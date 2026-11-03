#include "TransferClient.h"

#include "RestJson.h"
#include "Router.h"
#include "Sha256.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <vector>

namespace gigante {

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t chunkBytes = 64 * 1024;
constexpr std::size_t maxErrorBodyBytes = 4096;

RestClientError toRestError(ClientError error) {
    switch (error) {
        case ClientError::none: return RestClientError::none;
        case ClientError::connection: return RestClientError::connection;
        case ClientError::tlsVerification: return RestClientError::tlsVerification;
        case ClientError::hostnameVerification: return RestClientError::hostnameVerification;
        case ClientError::tlsHandshake: return RestClientError::tlsHandshake;
        case ClientError::canceled: return RestClientError::canceled;
        case ClientError::other: return RestClientError::other;
    }
    return RestClientError::other;
}

std::optional<std::string> responseHeader(const std::multimap<std::string, std::string>& headers, const std::string& name) {
    for (const auto& entry : headers) {
        if (entry.first.size() == name.size() &&
            std::equal(entry.first.begin(), entry.first.end(), name.begin(), [](char left, char right) {
                return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
            })) {
            return entry.second;
        }
    }
    return std::nullopt;
}

bool parseOffset(const std::string& text, std::uint64_t& value) {
    JsonObject object;
    object["value"] = JsonField{JsonField::Kind::number, text};
    return jsonUnsigned(object, "value", value);
}

int remainingMilliseconds(Clock::time_point deadline) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
    return remaining <= 0 ? 0 : static_cast<int>(std::min<long long>(remaining, 0x7fffffff));
}

RestResult failed(RestClientError error, std::string text) {
    RestResult result;
    result.error = error;
    result.errorText = std::move(text);
    return result;
}

} // namespace

TransferClient::TransferClient(ClientContext& clientContext) : context(clientContext) {}

void TransferClient::cancelRemote(const std::string& transferId, const RestCallOptions& options) {
    // Best effort, with a fresh connection that the client's cancel() cannot reach.
    CppHttplibClientTransport transport(context.transportConfig(2000));
    transport.request(HttpMethod::del, "/core/v1/copy/" + transferId, "", "application/json", options.headers);
}

bool TransferClient::queryOffset(const std::string& transferId, const RestCallOptions& options, int budgetMilliseconds, std::uint64_t generation, std::uint64_t& offset) {
    TrackedTransport tracked(context, context.transportConfig(budgetMilliseconds), generation);
    if (!tracked.isUsable()) {
        return false;
    }
    const ClientResponse response = tracked.transport().request(HttpMethod::get, "/core/v1/copy/" + transferId, "", "application/json", options.headers);
    const std::optional<JsonObject> object = response.status == 200 ? parseJsonObject(response.body) : std::nullopt;
    return object && jsonUnsigned(*object, "bytesReceived", offset);
}

RestResult TransferClient::upload(const std::string& resourceId, std::uint64_t size, const std::string& sha256, const RestUploadSource& source, const RestCallOptions& options) {
    if (!Router::isValidPathParameter(resourceId)) {
        return failed(RestClientError::other, "the resource ID is not valid");
    }
    if (!source) {
        return failed(RestClientError::other, "no upload source");
    }
    const std::uint64_t generation = context.generation();
    const auto deadline = Clock::now() + std::chrono::milliseconds(options.deadlineMilliseconds);
    RestResult result;
    std::string transferId;
    {
        TrackedTransport tracked(context, context.transportConfig(remainingMilliseconds(deadline)), generation);
        if (!tracked.isUsable()) {
            return failed(RestClientError::canceled, "the transfer was canceled");
        }
        const std::string body = "{\"resource\":" + jsonString(resourceId) + ",\"size\":" + std::to_string(size) + ",\"sha256\":" + jsonString(sha256) + "}";
        const ClientResponse created = tracked.transport().request(HttpMethod::post, "/core/v1/copy", body, "application/json", options.headers);
        result.attempts = 1;
        result.status = created.status;
        result.body = created.body;
        result.error = context.isCanceled(generation) ? RestClientError::canceled : toRestError(created.error);
        result.errorText = created.error == ClientError::none ? std::string() : created.errorText;
        const std::optional<JsonObject> object = created.status == 201 ? parseJsonObject(created.body) : std::nullopt;
        if (result.error != RestClientError::none || !object || !jsonText(*object, "transferId", transferId)) {
            return result;
        }
    }

    std::uint64_t offset = 0;
    const int attempts = std::max(1, options.maxAttempts);
    int backoffMilliseconds = 100;
    std::vector<char> buffer(chunkBytes);
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        const int budget = remainingMilliseconds(deadline);
        if (context.isCanceled(generation) || budget == 0) {
            cancelRemote(transferId, options);
            return context.isCanceled(generation) ? failed(RestClientError::canceled, "the transfer was canceled") : failed(RestClientError::deadline, "the call deadline passed");
        }
        std::map<std::string, std::string> headers = options.headers;
        headers["X-Upload-Offset"] = std::to_string(offset);
        bool sourceFailed = false;
        ClientResponse response;
        {
            TrackedTransport tracked(context, context.transportConfig(budget), generation);
            if (tracked.isUsable()) {
                const std::uint64_t start = offset;
                response = tracked.transport().upload(
                    "/core/v1/copy/" + transferId,
                    size - start,
                    "application/octet-stream",
                    headers,
                    [&](std::uint64_t bodyOffset, std::size_t length, TransportWriter& writer) {
                        const std::size_t count = std::min(length, buffer.size());
                        if (!source(start + bodyOffset, buffer.data(), count)) {
                            sourceFailed = true;
                            return false;
                        }
                        return writer.write(buffer.data(), count);
                    });
            }
        }
        result.attempts = attempt + 1;
        result.status = response.status;
        result.body = response.body;
        result.errorText = response.error == ClientError::none ? std::string() : response.errorText;
        result.error = toRestError(response.error);
        if (context.isCanceled(generation) || sourceFailed) {
            cancelRemote(transferId, options);
            return context.isCanceled(generation) ? failed(RestClientError::canceled, "the transfer was canceled") : failed(RestClientError::other, "the upload source stopped");
        }
        if (response.error == ClientError::none && response.status == 200) {
            const std::optional<JsonObject> object = parseJsonObject(response.body);
            std::string state;
            std::uint64_t received = 0;
            if (object && jsonText(*object, "state", state) && state == "committed") {
                result.bytesTransferred = size;
                result.sha256 = sha256;
                return result;
            }
            if (object && jsonUnsigned(*object, "bytesReceived", received)) {
                offset = received;
                continue;
            }
            return result;
        }
        const std::optional<std::string> serverOffset = responseHeader(response.headers, "X-Upload-Offset");
        std::uint64_t corrected = 0;
        if (response.status == 409 && serverOffset && parseOffset(*serverOffset, corrected)) {
            offset = corrected;
            continue;
        }
        const bool resumable = response.error != ClientError::none || response.status == 408 || response.status == 503;
        if (!resumable || attempt == attempts) {
            return result;
        }
        if (context.waitUntil(std::min(deadline, Clock::now() + std::chrono::milliseconds(backoffMilliseconds)), generation)) {
            continue;
        }
        backoffMilliseconds = std::min(backoffMilliseconds * 2, 5000);
        std::uint64_t received = 0;
        if (queryOffset(transferId, options, remainingMilliseconds(deadline), generation, received)) {
            offset = received;
        }
    }
    return result;
}

RestResult TransferClient::download(const std::string& resourceId, const RestDownloadSink& sink, std::uint64_t fromOffset, const RestCallOptions& options) {
    if (!Router::isValidPathParameter(resourceId)) {
        return failed(RestClientError::other, "the resource ID is not valid");
    }
    if (!sink) {
        return failed(RestClientError::other, "no download sink");
    }
    const std::uint64_t generation = context.generation();
    const auto deadline = Clock::now() + std::chrono::milliseconds(options.deadlineMilliseconds);
    const int attempts = std::max(1, options.maxAttempts);
    int backoffMilliseconds = 100;
    std::uint64_t offset = fromOffset;
    Sha256 digest;
    std::string declaredDigest;
    RestResult result;
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        const int budget = remainingMilliseconds(deadline);
        if (context.isCanceled(generation)) {
            return failed(RestClientError::canceled, "the transfer was canceled");
        }
        if (budget == 0) {
            return failed(RestClientError::deadline, "the call deadline passed");
        }
        std::map<std::string, std::string> headers = options.headers;
        if (offset > 0) {
            headers["Range"] = "bytes=" + std::to_string(offset) + "-";
        }
        int status = 0;
        bool sinkStopped = false;
        bool rangeIgnored = false;
        std::string errorBody;
        ClientResponse response;
        {
            TrackedTransport tracked(context, context.transportConfig(budget), generation);
            if (!tracked.isUsable()) {
                return failed(RestClientError::canceled, "the transfer was canceled");
            }
            response = tracked.transport().streamGet(
                "/core/v1/resources/" + resourceId,
                headers,
                [&](int responseStatus, const std::multimap<std::string, std::string>& responseHeaders) {
                    status = responseStatus;
                    if (status == 200 && offset > 0) {
                        rangeIgnored = true;
                        return false;
                    }
                    if (status == 200) {
                        declaredDigest = responseHeader(responseHeaders, "X-Content-Sha256").value_or("");
                    }
                    return true;
                },
                [&](const char* data, std::size_t size) {
                    if (status != 200 && status != 206) {
                        errorBody.append(data, std::min(size, maxErrorBodyBytes - std::min(errorBody.size(), maxErrorBodyBytes)));
                        return true;
                    }
                    digest.update(data, size);
                    offset += size;
                    if (!sink(data, size)) {
                        sinkStopped = true;
                        return false;
                    }
                    return true;
                });
        }
        result.attempts = attempt;
        result.status = status;
        result.bytesTransferred = offset - fromOffset;
        result.errorText = response.error == ClientError::none ? std::string() : response.errorText;
        result.error = toRestError(response.error);
        if (context.isCanceled(generation)) {
            return failed(RestClientError::canceled, "the transfer was canceled");
        }
        if (sinkStopped || rangeIgnored) {
            result.error = RestClientError::other;
            result.errorText = sinkStopped ? "the download sink stopped" : "the server ignored the Range request";
            return result;
        }
        if (response.error == ClientError::none && (status == 200 || status == 206)) {
            result.sha256 = digest.finish();
            if (fromOffset == 0 && !declaredDigest.empty() && declaredDigest != result.sha256) {
                result.error = RestClientError::other;
                result.errorText = "the downloaded data does not match the declared sha256";
            }
            return result;
        }
        result.body = errorBody;
        const bool resumable = response.error != ClientError::none || status == 503;
        if (!resumable || attempt == attempts) {
            return result;
        }
        context.waitUntil(std::min(deadline, Clock::now() + std::chrono::milliseconds(backoffMilliseconds)), generation);
        backoffMilliseconds = std::min(backoffMilliseconds * 2, 5000);
    }
    return result;
}

} // namespace gigante
