#include "RestTestSupport.h"

#include "Sha256.h"

#include <gigante/RestResourceStore.h>

#include <sys/resource.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace gigante;
using namespace restTest;

namespace {

char patternByte(std::uint64_t offset) {
    return static_cast<char>((offset * 2654435761ULL) >> 13);
}

void fillPattern(std::uint64_t offset, char* buffer, std::size_t length) {
    for (std::size_t index = 0; index < length; ++index) {
        buffer[index] = patternByte(offset + index);
    }
}

std::string patternData(std::size_t size) {
    std::string data(size, '\0');
    fillPattern(0, &data[0], size);
    return data;
}

std::string digestOf(const std::string& data) {
    Sha256 digest;
    digest.update(data.data(), data.size());
    return digest.finish();
}

// Keeps resources in memory. Uploads are staged and replace a resource only on commit.
class MemoryStore final : public RestResourceStore {
public:
    struct Entry {
        std::string data;
        bool readable = true;
        bool writable = true;
    };

    std::mutex mutex;
    std::map<std::string, Entry> resources;
    std::set<std::string> askedIds;
    std::uint64_t free = 1ULL << 40;
    std::atomic<int> commits{0};
    std::atomic<int> discards{0};
    std::atomic<bool> failing{false};

    void put(const std::string& id, std::string data, bool readable = true, bool writable = true) {
        std::lock_guard<std::mutex> lock(mutex);
        resources[id] = {std::move(data), readable, writable};
    }

    std::vector<RestResourceInfo> list() override {
        if (failing.load()) {
            throw std::runtime_error("/secret/path/store.db is unavailable");
        }
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<RestResourceInfo> result;
        for (const auto& resource : resources) {
            result.push_back(info(resource.first, resource.second));
        }
        return result;
    }

    std::optional<RestResourceInfo> describe(const std::string& id) override {
        std::lock_guard<std::mutex> lock(mutex);
        askedIds.insert(id);
        const auto entry = resources.find(id);
        return entry == resources.end() ? std::nullopt : std::optional<RestResourceInfo>(info(id, entry->second));
    }

    std::unique_ptr<RestUploadSink> beginUpload(const std::string& id, std::uint64_t size) override;

    std::unique_ptr<RestDownloadSource> openRead(const std::string& id) override {
        class Source final : public RestDownloadSource {
        public:
            explicit Source(std::string bytes) : data(std::move(bytes)) {}
            std::uint64_t size() const override { return data.size(); }
            bool read(std::uint64_t offset, char* buffer, std::size_t length) override {
                if (offset + length > data.size()) {
                    return false;
                }
                std::memcpy(buffer, data.data() + offset, length);
                return true;
            }

        private:
            std::string data;
        };
        std::lock_guard<std::mutex> lock(mutex);
        askedIds.insert(id);
        const auto entry = resources.find(id);
        return entry == resources.end() ? nullptr : std::make_unique<Source>(entry->second.data);
    }

    std::uint64_t freeBytes() override {
        return free;
    }

    bool has(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex);
        return resources.count(id) != 0;
    }

    std::string data(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex);
        return resources[id].data;
    }

private:
    static RestResourceInfo info(const std::string& id, const Entry& entry) {
        RestResourceInfo result;
        result.id = id;
        result.size = entry.data.size();
        result.sha256 = digestOf(entry.data);
        result.readable = entry.readable;
        result.writable = entry.writable;
        return result;
    }
};

std::unique_ptr<RestUploadSink> MemoryStore::beginUpload(const std::string& id, std::uint64_t) {
    class Sink final : public RestUploadSink {
    public:
        Sink(MemoryStore& owner, std::string resource) : store(owner), id(std::move(resource)) {}
        bool write(const char* data, std::size_t size) override {
            staged.append(data, size);
            return true;
        }
        bool commit() override {
            store.put(id, staged);
            store.commits.fetch_add(1);
            return true;
        }
        void discard() override {
            store.discards.fetch_add(1);
        }

    private:
        MemoryStore& store;
        std::string id;
        std::string staged;
    };
    return std::make_unique<Sink>(*this, id);
}

// Never keeps payload bytes: a sink hashes them, and a source generates them.
class DigestOnlyStore final : public RestResourceStore {
public:
    std::uint64_t generatedSize = 0;
    std::mutex mutex;
    std::string committedDigest;
    std::uint64_t committedSize = 0;

    std::vector<RestResourceInfo> list() override {
        return {};
    }
    std::optional<RestResourceInfo> describe(const std::string& id) override {
        if (id != "generated") {
            return std::nullopt;
        }
        RestResourceInfo info;
        info.id = id;
        info.size = generatedSize;
        info.writable = true;
        return info;
    }
    std::unique_ptr<RestUploadSink> beginUpload(const std::string&, std::uint64_t) override {
        class Sink final : public RestUploadSink {
        public:
            explicit Sink(DigestOnlyStore& owner) : store(owner) {}
            bool write(const char* data, std::size_t size) override {
                digest.update(data, size);
                received += size;
                return true;
            }
            bool commit() override {
                std::lock_guard<std::mutex> lock(store.mutex);
                store.committedDigest = digest.finish();
                store.committedSize = received;
                return true;
            }
            void discard() override {}

        private:
            DigestOnlyStore& store;
            Sha256 digest;
            std::uint64_t received = 0;
        };
        return std::make_unique<Sink>(*this);
    }
    std::unique_ptr<RestDownloadSource> openRead(const std::string& id) override {
        class Source final : public RestDownloadSource {
        public:
            explicit Source(std::uint64_t length) : length(length) {}
            std::uint64_t size() const override { return length; }
            bool read(std::uint64_t offset, char* buffer, std::size_t count) override {
                fillPattern(offset, buffer, count);
                return true;
            }

        private:
            std::uint64_t length;
        };
        return id == "generated" ? std::make_unique<Source>(generatedSize) : nullptr;
    }
    std::uint64_t freeBytes() override {
        return 1ULL << 40;
    }
};

RestUploadSource sourceFor(const std::string& data) {
    return [&data](std::uint64_t offset, char* buffer, std::size_t length) {
        if (offset + length > data.size()) {
            return false;
        }
        std::memcpy(buffer, data.data() + offset, length);
        return true;
    };
}

RestCallOptions transferOptions() {
    RestCallOptions options;
    options.deadlineMilliseconds = 60000;
    return options;
}

std::string createTransfer(RestClient& client, const std::string& resource, std::size_t size, const std::string& digest, RestResult* result = nullptr) {
    const RestResult created = client.call(RestMethod::post, "/core/v1/copy", "{\"resource\":\"" + resource + "\",\"size\":" + std::to_string(size) + ",\"sha256\":\"" + digest + "\"}");
    if (result != nullptr) {
        *result = created;
    }
    const std::size_t start = created.body.find("\"transferId\":\"");
    return start == std::string::npos ? "" : created.body.substr(start + 14, 32);
}

RestResult putPart(RestClient& client, const std::string& transferId, std::uint64_t offset, const std::string& part) {
    RestCallOptions options;
    options.headers["X-Upload-Offset"] = std::to_string(offset);
    return client.call(RestMethod::put, "/core/v1/copy/" + transferId, part, "application/octet-stream", options);
}

long maxResidentMegabytes() {
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
    return usage.ru_maxrss / (1024 * 1024);
#else
    return usage.ru_maxrss / 1024;
#endif
}

void listAndDownload() {
    auto store = std::make_shared<MemoryStore>();
    const std::string data = patternData(1024 * 1024 + 17);
    store->put("capture.wav", data);
    store->put("secret.bin", "x", false, false);
    RestServer server(developmentServer());
    server.setResourceStore(store);
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));

    const RestResult listed = client.listResources();
    check(listed.status == 200 && contains(listed.body, "\"id\":\"capture.wav\"") && contains(listed.body, digestOf(data)), "copyListShowsResources", listed.body.substr(0, 200));
    const RestResult capabilities = client.getCapabilities();
    check(contains(capabilities.body, "\"copyDownload\"") && contains(capabilities.body, "\"copyList\""), "capabilitiesListCopy");

    std::string received;
    RestResult result = client.download("capture.wav", [&](const char* bytes, std::size_t size) {
        received.append(bytes, size);
        return true;
    }, 0, transferOptions());
    check(result.error == RestClientError::none && result.status == 200 && received == data && result.sha256 == digestOf(data), "downloadWhole", result.errorText);

    received.clear();
    result = client.download("capture.wav", [&](const char* bytes, std::size_t size) {
        received.append(bytes, size);
        return true;
    }, 1000, transferOptions());
    check(result.status == 206 && received == data.substr(1000) && result.bytesTransferred == data.size() - 1000, "downloadRangeResume", std::to_string(result.status));

    check(client.download("missing.bin", [](const char*, std::size_t) { return true; }).status == 404, "downloadUnknown404");
    check(client.download("secret.bin", [](const char*, std::size_t) { return true; }).status == 403, "downloadUnreadable403");
    const int dotDot = client.call(RestMethod::get, "/core/v1/resources/..").status;
    check(dotDot == 400 || dotDot == 404, "dotDotNeverReachesStore", std::to_string(dotDot));
    check(store->askedIds.count("..") == 0, "storeNeverSeesDotDot");

    store->failing.store(true);
    const RestResult failing = client.listResources();
    check(failing.status == 500 && !contains(failing.body, "secret"), "storeFailureHidden", failing.body);
}

void uploads() {
    auto store = std::make_shared<MemoryStore>();
    store->put("readonly.bin", "fixed", true, false);
    RestServerConfig config = developmentServer();
    config.limits.maxTransferBytes = 4 * 1024 * 1024;
    RestServer server(config);
    server.setResourceStore(store);
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));

    const std::string data = patternData(3 * 1024 * 1024 + 5);
    RestResult result = client.upload("upload.bin", data.size(), digestOf(data), sourceFor(data), transferOptions());
    check(result.error == RestClientError::none && result.status == 200 && store->data("upload.bin") == data && store->commits.load() == 1, "uploadCommits", result.body);
    check(result.bytesTransferred == data.size() && result.sha256 == digestOf(data), "uploadResult");

    result = client.upload("wrong.bin", data.size(), std::string(64, 'a'), sourceFor(data), transferOptions());
    check(result.status == 422 && !store->has("wrong.bin") && store->discards.load() == 1, "digestMismatchDiscards", std::to_string(result.status));

    RestResult created;
    createTransfer(client, "big.bin", 5 * 1024 * 1024, digestOf(""), &created);
    check(created.status == 413, "oversizeRefused", std::to_string(created.status));
    store->free = 10;
    createTransfer(client, "full.bin", 100, digestOf(""), &created);
    check(created.status == 507, "insufficientStorage", std::to_string(created.status));
    store->free = 1ULL << 40;
    createTransfer(client, "../escape", 10, digestOf(""), &created);
    check(created.status == 404 && store->askedIds.count("../escape") == 0, "pathLikeIdRefused", std::to_string(created.status));
    createTransfer(client, "readonly.bin", 10, digestOf(""), &created);
    check(created.status == 403, "notWritable", std::to_string(created.status));
    createTransfer(client, "x.bin", 10, "not-a-digest", &created);
    check(created.status == 400, "badDigestRefused", std::to_string(created.status));

    const std::string empty;
    result = client.upload("empty.bin", 0, digestOf(empty), sourceFor(empty), transferOptions());
    check(result.status == 200 && store->has("empty.bin"), "emptyUploadCommits", std::to_string(result.status));
}

void resumeProtocol() {
    auto store = std::make_shared<MemoryStore>();
    RestServer server(developmentServer());
    server.setResourceStore(store);
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));

    const std::string data = patternData(200000);
    const std::string transferId = createTransfer(client, "resumed.bin", data.size(), digestOf(data));
    check(transferId.size() == 32, "transferCreated", transferId);

    RestResult result = putPart(client, transferId, 5, data);
    check(result.status == 409 && contains(result.body, "offsetMismatch"), "wrongOffset409", result.body);
    RestResult conflict;
    createTransfer(client, "resumed.bin", data.size(), digestOf(data), &conflict);
    check(conflict.status == 409 && contains(conflict.body, "resourceBusy"), "oneTransferPerResource", conflict.body);

    result = putPart(client, transferId, 0, data.substr(0, 70000));
    check(result.status == 200 && contains(result.body, "\"bytesReceived\":70000") && contains(result.body, "\"state\":\"paused\""), "partialUploadPauses", result.body);
    const RestResult status = client.call(RestMethod::get, "/core/v1/copy/" + transferId);
    check(contains(status.body, "\"bytesReceived\":70000"), "copyStatusReportsOffset", status.body);
    check(!store->has("resumed.bin"), "nothingVisibleBeforeCommit");

    result = putPart(client, transferId, 70000, data.substr(70000) + "extra");
    check(result.status == 413, "bodyPastSizeRefused", std::to_string(result.status));
    result = putPart(client, transferId, 70000, data.substr(70000));
    check(result.status == 200 && contains(result.body, "committed") && store->data("resumed.bin") == data, "resumeCommits", result.body);
    result = putPart(client, transferId, data.size(), "");
    check(result.status == 409 && contains(result.body, "alreadyCommitted"), "committedIsFinal", result.body);

    const std::string canceledId = createTransfer(client, "canceled.bin", data.size(), digestOf(data));
    putPart(client, canceledId, 0, data.substr(0, 1000));
    result = client.call(RestMethod::del, "/core/v1/copy/" + canceledId);
    check(result.status == 200 && contains(result.body, "canceled") && store->discards.load() == 1, "copyCancelDiscards", result.body);
    result = putPart(client, canceledId, 1000, data.substr(1000));
    check(result.status == 410, "canceledTransferEnded", std::to_string(result.status));
    check(!store->has("canceled.bin"), "canceledLeavesNothing");

    const std::string abandonedId = createTransfer(client, "abandoned.bin", data.size(), digestOf(data));
    putPart(client, abandonedId, 0, data.substr(0, 1000));
    check(server.stop(), "stopWithOpenTransfer");
    check(store->discards.load() == 2 && !store->has("abandoned.bin"), "stopDiscardsUncommitted", std::to_string(store->discards.load()));
}

void resumeWindowExpiry() {
    auto store = std::make_shared<MemoryStore>();
    RestServerConfig config = developmentServer();
    config.limits.transferResumeWindowMilliseconds = 100;
    RestServer server(config);
    server.setResourceStore(store);
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));
    const std::string data = patternData(5000);
    const std::string transferId = createTransfer(client, "late.bin", data.size(), digestOf(data));
    putPart(client, transferId, 0, data.substr(0, 100));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const RestResult result = putPart(client, transferId, 100, data.substr(100));
    check(result.status == 410 && store->discards.load() == 1, "abandonedAfterWindow", std::to_string(result.status));
}

void capacityAndPacing() {
    auto store = std::make_shared<MemoryStore>();
    const std::string data = patternData(256 * 1024);
    store->put("paced.bin", data);
    RestServerConfig config = developmentServer();
    config.limits.maxTransfers = 1;
    config.limits.maxDownloadBytesPerSecond = 512 * 1024;
    RestServer server(config);
    server.setResourceStore(store);
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));

    std::atomic<bool> first{false};
    RestResult paced;
    const auto before = std::chrono::steady_clock::now();
    std::thread downloader([&] {
        paced = client.download("paced.bin", [&](const char*, std::size_t) {
            first.store(true);
            return true;
        }, 0, transferOptions());
    });
    waitUntil([&] { return first.load(); });
    RestCallOptions once;
    once.maxAttempts = 1;
    const RestResult refused = client.download("paced.bin", [](const char*, std::size_t) { return true; }, 0, once);
    check(refused.status == 503, "transferCapReached", std::to_string(refused.status));
    check(client.health().status == 200, "healthAnswersAtTransferCap");
    downloader.join();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
    check(paced.status == 200 && paced.bytesTransferred == data.size(), "pacedDownloadCompletes", paced.errorText);
    check(elapsed >= 350, "bandwidthLimitApplied", std::to_string(elapsed));
    check(server.counters().rejectedTransfers >= 1, "rejectedTransfersCounted");

    RestServerConfig crowded = developmentServer();
    crowded.limits.maxThreads = 16;
    crowded.limits.maxTransfers = 10;
    crowded.limits.reservedThreads = 8;
    RestServer oversubscribed(crowded);
    oversubscribed.setResourceStore(store);
    const RestStartResult refusedStart = oversubscribed.start();
    check(!refusedStart.started && contains(refusedStart.cause, "maxThreads"), "startRefusesOversubscribedCaps", refusedStart.cause);
}

void largeTransfer() {
    const std::uint64_t size = 1ULL << 30;
    auto store = std::make_shared<DigestOnlyStore>();
    store->generatedSize = size;
    RestServerConfig config = developmentServer();
    RestServer server(config);
    server.setResourceStore(store);
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));
    Sha256 expected;
    std::vector<char> buffer(1024 * 1024);
    for (std::uint64_t offset = 0; offset < size; offset += buffer.size()) {
        fillPattern(offset, buffer.data(), buffer.size());
        expected.update(buffer.data(), buffer.size());
    }
    const std::string digest = expected.finish();
    const long before = maxResidentMegabytes();

    RestCallOptions options = transferOptions();
    options.deadlineMilliseconds = 300000;
    const RestResult uploaded = client.upload("generated", size, digest, [](std::uint64_t offset, char* bytes, std::size_t length) {
        fillPattern(offset, bytes, length);
        return true;
    }, options);
    check(uploaded.status == 200 && store->committedDigest == digest && store->committedSize == size, "oneGibUpload", uploaded.body);

    std::uint64_t received = 0;
    const RestResult downloaded = client.download("generated", [&](const char*, std::size_t length) {
        received += length;
        return true;
    }, 0, options);
    check(downloaded.status == 200 && received == size && downloaded.sha256 == digest, "oneGibDownload", downloaded.errorText);
    const long growth = maxResidentMegabytes() - before;
    check(growth < 128, "transferMemoryBounded", std::to_string(growth) + " MiB");
}

} // namespace

int main() {
    listAndDownload();
    uploads();
    resumeProtocol();
    resumeWindowExpiry();
    capacityAndPacing();
    largeTransfer();
    return finish("RestCopyTests");
}
