#include "CppHttplibClientTransport.h"
#include "CppHttplibTransport.h"
#include "ProbeReport.h"
#include "RestMetrics.h"
#include "TransferProbe.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace gigante;

namespace {

using Clock = std::chrono::steady_clock;

double elapsedSeconds(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

std::string headerValue(const TransportRequest& request, const std::string& name) {
    const auto entry = request.headers.find(name);
    return entry == request.headers.end() ? "" : entry->second;
}

std::uint64_t transferBytes = 1024ULL * 1024ULL * 1024ULL;

void registerRoutes(CppHttplibTransport& transport, std::atomic<int>& eventConnections) {
    transport.addUploadRoute(HttpMethod::put, "/copy/upload", [](const TransportRequest& request, const BodyChunkReader& reader) {
        Sha256 digest;
        std::uint64_t received = 0;
        const bool complete = reader([&](const char* data, std::size_t size) {
            digest.update(data, size);
            received += size;
            return true;
        });
        const std::string actual = digest.finish();
        TransportResponse response;
        if (!complete) {
            response.status = 400;
            response.body = "{\"code\":\"incomplete\"}";
        } else if (actual != headerValue(request, "X-Digest")) {
            response.status = 422;
            response.body = "{\"code\":\"digestMismatch\",\"retryable\":false}";
        } else {
            response.body = "{\"received\":" + std::to_string(received) + "}";
        }
        return response;
    });
    transport.addRoute(HttpMethod::get, "/copy/download", [](const TransportRequest&) {
        TransportResponse response;
        response.contentType = "application/octet-stream";
        response.contentLength = transferBytes;
        response.sizedProducer = [](std::uint64_t offset, std::uint64_t length, TransportWriter& writer) {
            std::vector<char> chunk(static_cast<std::size_t>(std::min<std::uint64_t>(length, 64 * 1024)));
            fillTransferData(offset, chunk.data(), chunk.size());
            return writer.write(chunk.data(), chunk.size());
        };
        return response;
    });
    transport.addRoute(HttpMethod::get, "/events/ordered", [](const TransportRequest&) {
        TransportResponse response;
        response.contentType = "text/event-stream";
        auto sent = std::make_shared<int>(0);
        response.streamProducer = [sent](TransportWriter& writer) {
            if (*sent == 200) {
                writer.finish();
                return true;
            }
            const std::string event = "event: tick\ndata: " + std::to_string((*sent)++) + "\n\n";
            return writer.write(event.data(), event.size());
        };
        return response;
    });
    transport.addRoute(HttpMethod::get, "/events/retryZero", [&eventConnections](const TransportRequest&) {
        eventConnections.fetch_add(1);
        TransportResponse response;
        response.contentType = "text/event-stream";
        response.streamProducer = [](TransportWriter& writer) {
            const std::string event = "retry: 0\ndata: go\n\n";
            writer.write(event.data(), event.size());
            writer.finish();
            return true;
        };
        return response;
    });
    transport.addRoute(HttpMethod::get, "/events/forever", [](const TransportRequest&) {
        TransportResponse response;
        response.contentType = "text/event-stream";
        response.streamProducer = [](TransportWriter& writer) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            const std::string event = "data: tick\n\n";
            return writer.write(event.data(), event.size());
        };
        return response;
    });
}

void transferChecks(const ClientTransportConfig& config, ProbeReport& report) {
    const std::string expected = transferDigest(0, transferBytes);
    const std::uint64_t baseline = ProbeReport::peakResidentBytes();

    {
        CppHttplibClientTransport client(config);
        const auto start = Clock::now();
        const ClientResponse response = client.upload(
            "/copy/upload",
            transferBytes,
            "application/octet-stream",
            {{"X-Digest", expected}},
            [](std::uint64_t offset, std::size_t size, TransportWriter& writer) {
                std::vector<char> chunk(std::min<std::size_t>(size, 64 * 1024));
                fillTransferData(offset, chunk.data(), chunk.size());
                return writer.write(chunk.data(), chunk.size());
            });
        const double seconds = elapsedSeconds(start);
        report.check("upload:digestVerifiedByServer", response.status == 200, "status=" + std::to_string(response.status) + " seconds=" + std::to_string(seconds) + " error=" + response.errorText + " " + response.body);
    }

    {
        CppHttplibClientTransport client(config);
        Sha256 digest;
        std::uint64_t received = 0;
        std::uint64_t lastProgress = 0;
        const auto start = Clock::now();
        const ClientResponse response = client.download(
            "/copy/download",
            0,
            [&](const char* data, std::size_t size) {
                digest.update(data, size);
                received += size;
                return true;
            },
            [&](std::uint64_t current, std::uint64_t) {
                lastProgress = current;
                return true;
            });
        const double seconds = elapsedSeconds(start);
        report.check("download:digestMatches", response.status == 200 && digest.finish() == expected, "status=" + std::to_string(response.status) + " bytes=" + std::to_string(received) + " seconds=" + std::to_string(seconds));
        report.check("download:progressReported", lastProgress == transferBytes);
    }

    const std::uint64_t growth = ProbeReport::peakResidentBytes() - std::min(baseline, ProbeReport::peakResidentBytes());
    report.check("transfer:memoryBounded", growth < 128ULL * 1024 * 1024, "peakGrowthMiB=" + std::to_string(growth / (1024 * 1024)));
    report.note("transfer:peakResident", std::to_string(ProbeReport::peakResidentBytes() / (1024 * 1024)) + " MiB");

    {
        CppHttplibClientTransport client(config);
        const std::uint64_t offset = transferBytes - 4 * 1024 * 1024;
        Sha256 digest;
        const ClientResponse response = client.download(
            "/copy/download",
            offset,
            [&](const char* data, std::size_t size) {
                digest.update(data, size);
                return true;
            },
            nullptr);
        report.check("download:rangeResumes", response.status == 206 && digest.finish() == transferDigest(offset, transferBytes - offset), "status=" + std::to_string(response.status));
    }

    {
        CppHttplibClientTransport client(config);
        const ClientResponse response = client.upload(
            "/copy/upload",
            64 * 1024,
            "application/octet-stream",
            {{"X-Digest", "wrong"}},
            [](std::uint64_t offset, std::size_t size, TransportWriter& writer) {
                std::vector<char> chunk(size);
                fillTransferData(offset, chunk.data(), size);
                return writer.write(chunk.data(), size);
            });
        report.check("upload:wrongDigestRefused", response.status == 422, "status=" + std::to_string(response.status));
    }

    {
        CppHttplibClientTransport client(config);
        std::atomic<bool> started{false};
        const auto start = Clock::now();
        std::thread canceller([&] {
            while (!started.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            client.cancel();
        });
        const ClientResponse response = client.download(
            "/copy/download",
            0,
            [&](const char*, std::size_t) {
                started.store(true);
                return true;
            },
            nullptr);
        canceller.join();
        report.check("cancel:downloadEndsFromAnotherThread", response.status != 200 && elapsedSeconds(start) < 30.0, "error=" + response.errorText + " seconds=" + std::to_string(elapsedSeconds(start)));
    }

    {
        CppHttplibClientTransport client(config);
        std::atomic<bool> started{false};
        const auto start = Clock::now();
        std::thread canceller([&] {
            while (!started.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            client.cancel();
        });
        const ClientResponse response = client.upload(
            "/copy/upload",
            transferBytes,
            "application/octet-stream",
            {{"X-Digest", expected}},
            [&](std::uint64_t, std::size_t size, TransportWriter& writer) {
                started.store(true);
                const std::string chunk(std::min<std::size_t>(size, 64 * 1024), 'c');
                return writer.write(chunk.data(), chunk.size());
            });
        canceller.join();
        report.check("cancel:uploadEndsFromAnotherThread", response.status != 200 && elapsedSeconds(start) < 30.0, "error=" + response.errorText + " seconds=" + std::to_string(elapsedSeconds(start)));
    }
}

void eventChecks(const ClientTransportConfig& config, std::atomic<int>& eventConnections, ProbeReport& report) {
    {
        CppHttplibClientTransport client(config);
        std::vector<int> values;
        EventStreamOptions options;
        options.maximumAttempts = 1;
        options.minimumReconnectMilliseconds = 100;
        client.subscribeEvents("/events/ordered", options, [&](const std::string& event, const std::string& data) {
            if (event == "tick") {
                values.push_back(std::atoi(data.c_str()));
            }
        });
        bool ordered = values.size() >= 200;
        for (std::size_t index = 0; ordered && index < 200; ++index) {
            ordered = values[index] == static_cast<int>(index);
        }
        report.check("sse:eventsArriveInOrder", ordered, "received=" + std::to_string(values.size()));
    }

    {
        CppHttplibClientTransport client(config);
        EventStreamOptions options;
        options.maximumAttempts = 5;
        options.minimumReconnectMilliseconds = 500;
        eventConnections.store(0);
        const auto start = Clock::now();
        const int connections = client.subscribeEvents("/events/retryZero", options, [](const std::string&, const std::string&) {});
        const double seconds = elapsedSeconds(start);
        report.check(
            "sse:retryZeroCannotCauseTightLoop",
            connections <= options.maximumAttempts + 1 && seconds >= 0.5 * (options.maximumAttempts - 1),
            "connections=" + std::to_string(connections) + " serverSaw=" + std::to_string(eventConnections.load()) + " seconds=" + std::to_string(seconds));
    }

    {
        CppHttplibClientTransport client(config);
        std::atomic<int> received{0};
        std::thread subscriber([&] {
            client.subscribeEvents("/events/forever", {}, [&](const std::string&, const std::string&) { received.fetch_add(1); });
        });
        while (received.load() < 3) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        const auto start = Clock::now();
        client.cancel();
        subscriber.join();
        report.check("sse:cancelEndsSubscription", elapsedSeconds(start) < 5.0, "seconds=" + std::to_string(elapsedSeconds(start)));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: clientProbe certsDirectory [transferMiB]\n";
        return 2;
    }
    const std::string certs = argv[1];
    if (argc > 2) {
        transferBytes = std::strtoull(argv[2], nullptr, 10) * 1024ULL * 1024ULL;
    }
    ProbeReport report("client");
    report.check("sha256:knownValue", [] {
        Sha256 digest;
        digest.update("abc", 3);
        return digest.finish() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    }());

    RestMetrics metrics;
    CppHttplibTransport transport(metrics);
    std::atomic<int> eventConnections{0};
    registerRoutes(transport, eventConnections);
    TransportConfig serverConfig;
    serverConfig.useTls = true;
    serverConfig.tls.certificatePath = certs + "/server.pem";
    serverConfig.tls.privateKeyPath = certs + "/server.key";
    serverConfig.maxPayloadBytes = static_cast<std::size_t>(transferBytes + 1024);
    serverConfig.readTimeoutMilliseconds = 5000;
    serverConfig.writeTimeoutMilliseconds = 5000;
    const TransportStartResult started = transport.start(serverConfig);
    report.check("serverStarts", started.started, started.cause);

    ClientTransportConfig config;
    config.baseUrl = "https://localhost:" + std::to_string(started.boundPort);
    config.caCertificatePath = certs + "/ca.pem";
    config.readTimeoutMilliseconds = 10000;
    config.writeTimeoutMilliseconds = 10000;

    eventChecks(config, eventConnections, report);
    transferChecks(config, report);
    report.check("serverStops", transport.stop(10000));
    return report.finish();
}
