#include "CopyService.h"

#include "RestJson.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <limits>
#include <optional>
#include <random>
#include <thread>
#include <utility>

namespace gigante {

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t chunkBytes = 64 * 1024;

std::string randomTransferId() {
    std::random_device device;
    char text[33];
    std::snprintf(text, sizeof(text), "%08x%08x%08x%08x", device(), device(), device(), device());
    return text;
}

bool isHexDigest(std::string& digest) {
    if (digest.size() != 64) {
        return false;
    }
    for (char& current : digest) {
        if (std::isxdigit(static_cast<unsigned char>(current)) == 0) {
            return false;
        }
        current = static_cast<char>(std::tolower(static_cast<unsigned char>(current)));
    }
    return true;
}

bool parseUnsigned(const std::string& text, std::uint64_t& value) {
    if (text.empty() || text.size() > 20) {
        return false;
    }
    std::uint64_t result = 0;
    for (const char current : text) {
        if (current < '0' || current > '9') {
            return false;
        }
        const auto digit = static_cast<std::uint64_t>(current - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
            return false;
        }
        result = result * 10 + digit;
    }
    value = result;
    return true;
}

std::string resourceJson(const RestResourceInfo& info) {
    return "{\"id\":" + jsonString(info.id) + ",\"size\":" + std::to_string(info.size) +
           ",\"sha256\":" + (info.sha256.empty() ? std::string("null") : jsonString(info.sha256)) +
           ",\"contentType\":" + jsonString(info.contentType) +
           ",\"readable\":" + (info.readable ? "true" : "false") + ",\"writable\":" + (info.writable ? "true" : "false") + "}";
}

} // namespace

CopyService::CopyService(
    std::shared_ptr<RestResourceStore> resourceStore,
    CapacityGate& capacityGate,
    CopyServiceConfig serviceConfig,
    MiddlewareErrorBuilder builder,
    ResponseFinisher responseFinisher,
    ServiceLog serviceLog)
    : store(std::move(resourceStore)),
      gate(capacityGate),
      config(serviceConfig),
      errorBuilder(std::move(builder)),
      finisher(std::move(responseFinisher)),
      log(std::move(serviceLog)) {}

CopyService::~CopyService() {
    close();
}

const char* CopyService::stateName(State state) {
    switch (state) {
        case State::created: return "created";
        case State::receiving: return "receiving";
        case State::paused: return "paused";
        case State::committed: return "committed";
        case State::failed: return "failed";
        case State::canceled: return "canceled";
    }
    return "unknown";
}

bool CopyService::isFinal(State state) {
    return state == State::committed || state == State::failed || state == State::canceled;
}

void CopyService::registerRoutes(Router& router) {
    RouteInfo read;
    read.authRequired = true;
    read.idempotent = true;
    RouteInfo small;
    small.authRequired = true;
    small.maxBodyBytes = 4096;
    RouteInfo body;
    body.authRequired = true;
    body.maxBodyBytes = static_cast<std::size_t>(std::min<std::uint64_t>(config.maxTransferBytes, std::numeric_limits<std::size_t>::max()));
    router.addCommon(HttpMethod::get, "/core/v1/resources", [this](const TransportRequest& request) { return list(request); }, read);
    router.addCommon(HttpMethod::get, "/core/v1/resources/{id}", [this](const TransportRequest& request) { return download(request); }, read);
    router.addCommon(HttpMethod::post, "/core/v1/copy", [this](const TransportRequest& request) { return create(request); }, small);
    router.addCommonUpload(HttpMethod::put, "/core/v1/copy/{transferId}", [this](const TransportRequest& request, const BodyChunkReader& reader) {
        return upload(request, reader);
    }, body);
    router.addCommon(HttpMethod::get, "/core/v1/copy/{transferId}", [this](const TransportRequest& request) { return status(request); }, read);
    router.addCommon(HttpMethod::del, "/core/v1/copy/{transferId}", [this](const TransportRequest& request) { return cancel(request); }, small);
}

TransportResponse CopyService::json(const TransportRequest& request, int status, std::string body) {
    TransportResponse response;
    response.status = status;
    response.body = std::move(body);
    finisher(response, request);
    return response;
}

TransportResponse CopyService::offsetConflict(
    const TransportRequest& request, const Transfer& transfer, int status, const std::string& code, const std::string& message, bool retryable) {
    TransportResponse response = errorBuilder(status, code, message, retryable, request);
    response.headers.emplace("X-Upload-Offset", std::to_string(transfer.bytesReceived.load()));
    return response;
}

std::string CopyService::describe(const Transfer& transfer) const {
    return "{\"transferId\":" + jsonString(transfer.id) + ",\"resource\":" + jsonString(transfer.resource) +
           ",\"size\":" + std::to_string(transfer.size) + ",\"bytesReceived\":" + std::to_string(transfer.bytesReceived.load()) +
           ",\"state\":" + jsonString(stateName(transfer.state)) +
           ",\"reason\":" + (transfer.reason.empty() ? std::string("null") : jsonString(transfer.reason)) +
           ",\"resumeWindowMilliseconds\":" + std::to_string(config.resumeWindowMilliseconds) + "}";
}

void CopyService::discard(std::unique_ptr<RestUploadSink> sink) {
    if (!sink) {
        return;
    }
    try {
        sink->discard();
    } catch (const std::exception&) {
        log(RestLogLevel::error, "restCopyDiscardFailed", "");
    } catch (...) {
        log(RestLogLevel::error, "restCopyDiscardFailed", "");
    }
}

void CopyService::purgeExpired() {
    std::vector<std::unique_ptr<RestUploadSink>> abandoned;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto now = Clock::now();
        const auto window = std::chrono::milliseconds(config.resumeWindowMilliseconds);
        for (auto entry = transfers.begin(); entry != transfers.end();) {
            Transfer& transfer = *entry->second;
            if (now - transfer.lastActivity < window || transfer.state == State::receiving) {
                ++entry;
            } else if (isFinal(transfer.state)) {
                entry = transfers.erase(entry);
            } else {
                transfer.state = State::failed;
                transfer.reason = "expired";
                transfer.lastActivity = now;
                abandoned.push_back(std::move(transfer.sink));
                ++entry;
            }
        }
    }
    for (auto& sink : abandoned) {
        discard(std::move(sink));
    }
}

void CopyService::close() {
    std::vector<std::unique_ptr<RestUploadSink>> abandoned;
    {
        std::lock_guard<std::mutex> lock(mutex);
        closed = true;
        for (auto& entry : transfers) {
            if (!isFinal(entry.second->state)) {
                abandoned.push_back(std::move(entry.second->sink));
            }
        }
        transfers.clear();
    }
    for (auto& sink : abandoned) {
        discard(std::move(sink));
    }
}

void CopyService::reopen() {
    std::lock_guard<std::mutex> lock(mutex);
    closed = false;
}

std::size_t CopyService::pendingTransfers() const {
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<std::size_t>(std::count_if(transfers.begin(), transfers.end(), [](const auto& entry) { return !isFinal(entry.second->state); }));
}

TransportResponse CopyService::list(const TransportRequest& request) {
    std::vector<RestResourceInfo> resources;
    try {
        resources = store->list();
    } catch (...) {
        log(RestLogLevel::error, "restCopyListFailed", "");
        return errorBuilder(500, "storeFailed", "The resource store could not answer.", true, request);
    }
    std::string body = "{\"resources\":[";
    bool first = true;
    for (const RestResourceInfo& info : resources) {
        if (!Router::isValidPathParameter(info.id)) {
            continue;
        }
        body += (first ? "" : ",") + resourceJson(info);
        first = false;
    }
    return json(request, 200, body + "]}");
}

TransportResponse CopyService::create(const TransportRequest& request) {
    purgeExpired();
    const std::optional<JsonObject> object = parseJsonObject(request.body);
    std::string resource;
    std::string digest;
    std::uint64_t size = 0;
    if (!object || !jsonText(*object, "resource", resource) || !jsonUnsigned(*object, "size", size) || !jsonText(*object, "sha256", digest)) {
        return errorBuilder(400, "invalidTransfer", "The body needs resource, size and sha256.", false, request);
    }
    if (!isHexDigest(digest)) {
        return errorBuilder(400, "invalidDigest", "sha256 must be 64 hexadecimal digits.", false, request);
    }
    if (!Router::isValidPathParameter(resource)) {
        return errorBuilder(404, "unknownResource", "No resource with that ID can be written.", false, request);
    }
    if (size > config.maxTransferBytes) {
        return errorBuilder(413, "transferTooLarge", "The transfer is over this server's size limit.", false, request);
    }
    std::optional<RestResourceInfo> existing;
    std::uint64_t free = 0;
    try {
        existing = store->describe(resource);
        free = store->freeBytes();
    } catch (...) {
        log(RestLogLevel::error, "restCopyStoreFailed", resource);
        return errorBuilder(500, "storeFailed", "The resource store could not answer.", true, request);
    }
    if (existing && !existing->writable) {
        return errorBuilder(403, "notWritable", "That resource cannot be written.", false, request);
    }
    if (size > free) {
        return errorBuilder(507, "insufficientStorage", "There is not enough free space for this transfer.", false, request);
    }
    auto transfer = std::make_shared<Transfer>();
    transfer->id = randomTransferId();
    transfer->resource = resource;
    transfer->size = size;
    transfer->expectedDigest = digest;
    transfer->lastActivity = Clock::now();
    std::string body;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (closed) {
            return errorBuilder(503, "serverStopping", "The server is stopping.", true, request);
        }
        std::size_t pending = 0;
        for (const auto& entry : transfers) {
            if (isFinal(entry.second->state)) {
                continue;
            }
            if (entry.second->resource == resource) {
                return errorBuilder(409, "resourceBusy", "Another transfer to that resource is open.", true, request);
            }
            ++pending;
        }
        if (pending >= config.maxPendingTransfers) {
            return errorBuilder(503, "tooManyTransfers", "Too many transfers are open. Try again later.", true, request);
        }
        transfers.emplace(transfer->id, transfer);
        body = describe(*transfer);
    }
    return json(request, 201, body);
}

TransportResponse CopyService::upload(const TransportRequest& request, const BodyChunkReader& reader) {
    purgeExpired();
    const std::string id = request.pathParameters.at("transferId");
    std::shared_ptr<Transfer> transfer;
    std::optional<CapacityLease> lease;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto entry = transfers.find(id);
        if (entry == transfers.end()) {
            return errorBuilder(404, "unknownTransfer", "No open transfer has that ID.", false, request);
        }
        transfer = entry->second;
        if (transfer->state == State::committed) {
            return errorBuilder(409, "alreadyCommitted", "This transfer is already committed.", false, request);
        }
        if (isFinal(transfer->state)) {
            return errorBuilder(410, "transferEnded", "This transfer has ended and must be created again.", false, request);
        }
        if (transfer->state == State::receiving) {
            return offsetConflict(request, *transfer, 409, "uploadInProgress", "Another upload to this transfer is running.", true);
        }
        std::uint64_t offset = 0;
        const std::optional<std::string> offsetHeader = requestHeader(request, "X-Upload-Offset");
        if (!offsetHeader || !parseUnsigned(*offsetHeader, offset)) {
            return offsetConflict(request, *transfer, 400, "missingUploadOffset", "X-Upload-Offset must give the starting byte.", false);
        }
        if (offset != transfer->bytesReceived.load()) {
            return offsetConflict(request, *transfer, 409, "offsetMismatch", "Resume from the offset in X-Upload-Offset.", true);
        }
        std::uint64_t length = 0;
        const std::optional<std::string> lengthHeader = requestHeader(request, "Content-Length");
        if (!lengthHeader || !parseUnsigned(*lengthHeader, length)) {
            return errorBuilder(411, "lengthRequired", "An upload needs a Content-Length header.", false, request);
        }
        if (length > transfer->size - offset) {
            return offsetConflict(request, *transfer, 413, "transferTooLarge", "The body runs past the declared size.", false);
        }
        lease = gate.tryAcquireTransfer();
        if (!lease) {
            return errorBuilder(503, "transferCapReached", "Too many transfers are running. Try again later.", true, request);
        }
        transfer->state = State::receiving;
        transfer->lastActivity = Clock::now();
    }

    if (!transfer->sink) {
        try {
            transfer->sink = store->beginUpload(transfer->resource, transfer->size);
        } catch (...) {
            transfer->sink.reset();
        }
        if (!transfer->sink) {
            std::lock_guard<std::mutex> lock(mutex);
            transfer->state = State::failed;
            transfer->reason = "storeRefused";
            transfer->lastActivity = Clock::now();
            return errorBuilder(500, "storeFailed", "The resource store refused the upload.", false, request);
        }
    }

    enum class Stop { none, canceled, deadline, overflow, writeFailed };
    Stop stop = Stop::none;
    const auto deadline = Clock::now() + std::chrono::milliseconds(config.uploadBodyDeadlineMilliseconds);
    bool complete = false;
    try {
        complete = reader([&](const char* data, std::size_t size) {
            if (transfer->cancelRequested.load()) {
                stop = Stop::canceled;
                return false;
            }
            if (Clock::now() > deadline) {
                stop = Stop::deadline;
                return false;
            }
            const std::uint64_t received = transfer->bytesReceived.load();
            if (size > transfer->size - received) {
                stop = Stop::overflow;
                return false;
            }
            bool written = false;
            try {
                written = transfer->sink->write(data, size);
            } catch (...) {
                written = false;
            }
            if (!written) {
                stop = Stop::writeFailed;
                return false;
            }
            transfer->digest.update(data, size);
            transfer->bytesReceived.store(received + size);
            return true;
        });
    } catch (...) {
        complete = false;
    }
    lease->release();

    std::unique_ptr<RestUploadSink> dropped;
    TransportResponse response;
    {
        std::lock_guard<std::mutex> lock(mutex);
        transfer->lastActivity = Clock::now();
        if (stop == Stop::canceled || transfer->cancelRequested.load()) {
            transfer->state = State::canceled;
            transfer->reason = "canceled";
            dropped = std::move(transfer->sink);
            response = errorBuilder(410, "transferCanceled", "The transfer was canceled.", false, request);
        } else if (stop == Stop::writeFailed || stop == Stop::overflow) {
            transfer->state = State::failed;
            transfer->reason = stop == Stop::overflow ? "overflow" : "storeWriteFailed";
            dropped = std::move(transfer->sink);
            response = stop == Stop::overflow
                ? errorBuilder(413, "transferTooLarge", "The body runs past the declared size.", false, request)
                : errorBuilder(500, "storeWriteFailed", "The resource store could not write the data.", false, request);
        } else if (!complete) {
            transfer->state = State::paused;
            response = offsetConflict(request, *transfer, 408, "uploadInterrupted", "The upload stopped early. Resume from X-Upload-Offset.", true);
        } else if (transfer->bytesReceived.load() < transfer->size) {
            transfer->state = State::paused;
            response = json(request, 200, describe(*transfer));
        } else {
            response = TransportResponse{};
            response.status = 0;
        }
    }
    discard(std::move(dropped));
    if (response.status != 0) {
        return response;
    }
    return finishUpload(request, transfer);
}

TransportResponse CopyService::finishUpload(const TransportRequest& request, const std::shared_ptr<Transfer>& transfer) {
    const std::string actual = transfer->digest.finish();
    std::unique_ptr<RestUploadSink> sink;
    {
        std::lock_guard<std::mutex> lock(mutex);
        sink = std::move(transfer->sink);
    }
    bool committed = false;
    if (actual == transfer->expectedDigest) {
        try {
            committed = sink->commit();
        } catch (...) {
            committed = false;
        }
    } else {
        discard(std::move(sink));
    }
    std::lock_guard<std::mutex> lock(mutex);
    transfer->lastActivity = Clock::now();
    if (actual != transfer->expectedDigest) {
        transfer->state = State::failed;
        transfer->reason = "digestMismatch";
        log(RestLogLevel::warn, "restCopyDigestMismatch", transfer->resource);
        return errorBuilder(422, "digestMismatch", "The received data does not match the declared sha256.", false, request);
    }
    if (!committed) {
        transfer->state = State::failed;
        transfer->reason = "commitFailed";
        return errorBuilder(500, "commitFailed", "The resource store could not commit the upload.", false, request);
    }
    transfer->state = State::committed;
    log(RestLogLevel::info, "restCopyCommitted", transfer->resource);
    return json(request, 200, describe(*transfer));
}

TransportResponse CopyService::status(const TransportRequest& request) {
    purgeExpired();
    std::lock_guard<std::mutex> lock(mutex);
    const auto entry = transfers.find(request.pathParameters.at("transferId"));
    if (entry == transfers.end()) {
        return errorBuilder(404, "unknownTransfer", "No open transfer has that ID.", false, request);
    }
    return json(request, 200, describe(*entry->second));
}

TransportResponse CopyService::cancel(const TransportRequest& request) {
    std::unique_ptr<RestUploadSink> dropped;
    TransportResponse response;
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto entry = transfers.find(request.pathParameters.at("transferId"));
        if (entry == transfers.end()) {
            return errorBuilder(404, "unknownTransfer", "No open transfer has that ID.", false, request);
        }
        Transfer& transfer = *entry->second;
        if (transfer.state == State::committed) {
            return errorBuilder(409, "alreadyCommitted", "This transfer is already committed.", false, request);
        }
        transfer.cancelRequested.store(true);
        if (transfer.state == State::receiving) {
            // The upload thread discards when it sees the request, at its next chunk.
            return json(request, 202, describe(transfer));
        }
        if (!isFinal(transfer.state)) {
            transfer.state = State::canceled;
            transfer.reason = "canceled";
            transfer.lastActivity = Clock::now();
            dropped = std::move(transfer.sink);
        }
        response = json(request, 200, describe(transfer));
    }
    discard(std::move(dropped));
    return response;
}

TransportResponse CopyService::download(const TransportRequest& request) {
    const std::string id = request.pathParameters.at("id");
    std::optional<RestResourceInfo> info;
    try {
        info = store->describe(id);
    } catch (...) {
        log(RestLogLevel::error, "restCopyStoreFailed", id);
        return errorBuilder(500, "storeFailed", "The resource store could not answer.", true, request);
    }
    if (!info) {
        return errorBuilder(404, "unknownResource", "No resource has that ID.", false, request);
    }
    if (!info->readable) {
        return errorBuilder(403, "notReadable", "That resource cannot be read.", false, request);
    }
    std::optional<CapacityLease> acquired = gate.tryAcquireTransfer();
    if (!acquired) {
        return errorBuilder(503, "transferCapReached", "Too many transfers are running. Try again later.", true, request);
    }
    auto lease = std::make_shared<CapacityLease>(std::move(*acquired));
    std::shared_ptr<RestDownloadSource> source;
    try {
        source = store->openRead(id);
    } catch (...) {
        source.reset();
    }
    if (!source) {
        return errorBuilder(404, "unknownResource", "No resource has that ID.", false, request);
    }
    TransportResponse response;
    response.contentType = info->contentType.empty() ? "application/octet-stream" : info->contentType;
    response.headers.emplace("Accept-Ranges", "bytes");
    if (!info->sha256.empty()) {
        response.headers.emplace("X-Content-Sha256", info->sha256);
    }
    std::uint64_t size = 0;
    try {
        size = source->size();
    } catch (...) {
        return errorBuilder(500, "storeFailed", "The resource store could not answer.", true, request);
    }
    response.release = [lease, source](bool) { lease->release(); };
    if (size == 0) {
        finisher(response, request);
        return response;
    }
    const std::uint64_t rate = config.maxDownloadBytesPerSecond;
    // Small chunks at low rates keep each pause short, so a stop is not held up.
    const std::size_t chunk = rate == 0 ? chunkBytes : static_cast<std::size_t>(std::max<std::uint64_t>(1024, std::min<std::uint64_t>(chunkBytes, rate / 10)));
    auto buffer = std::make_shared<std::vector<char>>(chunk);
    auto started = std::make_shared<std::optional<Clock::time_point>>();
    auto sent = std::make_shared<std::uint64_t>(0);
    response.contentLength = size;
    response.sizedProducer = [source, buffer, started, sent, rate](std::uint64_t offset, std::uint64_t length, TransportWriter& writer) {
        const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(length, buffer->size()));
        bool read = false;
        try {
            read = source->read(offset, buffer->data(), count);
        } catch (...) {
            read = false;
        }
        if (!read) {
            return false;
        }
        if (rate != 0) {
            if (!*started) {
                *started = Clock::now();
            }
            const auto due = **started + std::chrono::microseconds(*sent * 1000000 / rate);
            std::this_thread::sleep_until(due);
        }
        *sent += count;
        return writer.write(buffer->data(), count);
    };
    finisher(response, request);
    return response;
}

} // namespace gigante
