#include "CapacityGate.h"
#include "CppHttplibTransport.h"
#include "LoadClient.h"
#include "ProbeReport.h"
#include "RestMetrics.h"
#include "Router.h"
#include "StreamHub.h"

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace gigante;

namespace {

using Clock = std::chrono::steady_clock;
using Results = std::map<std::string, double>;

const std::size_t streamCap = 70;
const std::size_t transferCap = 20;
const int bodyDeadlineMilliseconds = 3000;

std::string certs;
double scale = 1.0;
int writeTimeoutMilliseconds = 2000;
std::size_t basePoolThreads = 16;
std::size_t queuedConnections = 32;

class CapacityServer {
public:
    CapacityServer(DeliveryPolicy policy, std::size_t chunkBytes, int publishIntervalMilliseconds, std::size_t poolThreads = 100)
        : maxThreads(poolThreads),
          gate(streamCap, transferCap, metrics),
          hub(metrics),
          transport(metrics),
          router(transport),
          chunkSize(chunkBytes),
          publishInterval(publishIntervalMilliseconds) {
        StreamDescriptor descriptor;
        descriptor.id = "live";
        descriptor.contentType = "application/octet-stream";
        descriptor.policy = policy;
        hub.declare(descriptor);
        router.add(HttpMethod::get, "/probe/health", [](const TransportRequest&) {
            TransportResponse response;
            response.body = "{\"status\":\"ok\"}";
            return response;
        });
        router.add(HttpMethod::get, "/probe/rest", [](const TransportRequest&) {
            TransportResponse response;
            response.body = std::string(512, 'r');
            return response;
        });
        router.add(HttpMethod::get, "/probe/stream", [this](const TransportRequest&) {
            auto lease = gate.tryAcquireStream();
            if (!lease) {
                return busyResponse();
            }
            return hub.makeResponse("live", hub.subscribe("live", std::move(*lease)));
        });
        router.addUpload(HttpMethod::put, "/probe/upload", [this](const TransportRequest&, const BodyChunkReader& reader) {
            auto lease = gate.tryAcquireTransfer();
            if (!lease) {
                return busyResponse();
            }
            const auto deadline = Clock::now() + std::chrono::milliseconds(bodyDeadlineMilliseconds);
            std::uint64_t received = 0;
            const bool complete = reader([&](const char*, std::size_t size) {
                received += size;
                return Clock::now() < deadline;
            });
            TransportResponse response;
            response.status = complete ? 200 : 408;
            response.body = "{\"received\":" + std::to_string(received) + "}";
            return response;
        });
    }

    ~CapacityServer() {
        stop();
    }

    TransportStartResult start(bool useTls) {
        TransportConfig config;
        config.useTls = useTls;
        config.tls.certificatePath = certs + "/server.pem";
        config.tls.privateKeyPath = certs + "/server.key";
        config.baseThreads = basePoolThreads;
        config.maxThreads = maxThreads;
        config.maxQueuedConnections = queuedConnections;
        config.readTimeoutMilliseconds = 2000;
        config.writeTimeoutMilliseconds = writeTimeoutMilliseconds;
        config.keepAliveTimeoutMilliseconds = 1000;
        const TransportStartResult result = transport.start(config);
        if (result.started && publishInterval > 0) {
            publisher = std::thread([this] {
                const std::string chunk(chunkSize, 'p');
                while (!publisherStop.load()) {
                    const auto before = Clock::now();
                    hub.publish("live", chunk);
                    const double publishMilliseconds = std::chrono::duration<double, std::milli>(Clock::now() - before).count();
                    if (publishMilliseconds > worstPublishMilliseconds.load()) {
                        worstPublishMilliseconds.store(publishMilliseconds);
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(publishInterval));
                }
            });
        }
        return result;
    }

    void stop() {
        publisherStop.store(true);
        if (publisher.joinable()) {
            publisher.join();
        }
        gate.refuseNewLeases();
        hub.cancelAll();
        transport.stop(10000);
    }

    std::size_t maxThreads;
    RestMetrics metrics;
    CapacityGate gate;
    std::atomic<double> worstPublishMilliseconds{0.0};

private:
    static TransportResponse busyResponse() {
        TransportResponse response;
        response.status = 503;
        response.body = "{\"code\":\"capacityReached\",\"retryable\":true}";
        return response;
    }

    StreamHub hub;
    CppHttplibTransport transport;
    Router router;
    std::size_t chunkSize;
    int publishInterval;
    std::thread publisher;
    std::atomic<bool> publisherStop{false};
};

ClientTransportConfig clientConfig(int port, bool useTls) {
    ClientTransportConfig config;
    config.baseUrl = std::string(useTls ? "https" : "http") + "://localhost:" + std::to_string(port);
    if (useTls) {
        config.caCertificatePath = certs + "/ca.pem";
    }
    config.connectTimeoutMilliseconds = 4000;
    config.readTimeoutMilliseconds = 4000;
    config.writeTimeoutMilliseconds = 4000;
    return config;
}

class HealthPoller {
public:
    explicit HealthPoller(ClientTransportConfig clientSettings) : settings(std::move(clientSettings)) {
        worker = std::thread([this] {
            while (!done.load()) {
                LoadClient client(settings);
                const LoadOutcome outcome = client.request("/probe/health");
                if (outcome.status == 200) {
                    latencies.push_back(outcome.seconds * 1000.0);
                } else {
                    ++failures;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        });
    }

    void finish(Results& results) {
        done.store(true);
        worker.join();
        results["healthOk"] = static_cast<double>(latencies.size());
        results["healthFailures"] = failures;
        results["healthP50Ms"] = ProbeReport::percentile(latencies, 0.50);
        results["healthP95Ms"] = ProbeReport::percentile(latencies, 0.95);
        results["healthP99Ms"] = ProbeReport::percentile(latencies, 0.99);
        results["healthMaxMs"] = ProbeReport::percentile(latencies, 1.0);
    }

private:
    ClientTransportConfig settings;
    std::thread worker;
    std::atomic<bool> done{false};
    std::vector<double> latencies;
    int failures = 0;
};

Results runMixedClients(int port, bool useTls, int streams, int uploads, int restClients, double seconds) {
    const ClientTransportConfig config = clientConfig(port, useTls);
    std::atomic<int> streamsOk{0};
    std::atomic<int> streamsBusy{0};
    std::atomic<int> streamsOther{0};
    std::atomic<int> uploadsOk{0};
    std::atomic<int> uploadsBusy{0};
    std::atomic<int> uploadsOther{0};
    std::atomic<int> restOk{0};
    std::atomic<int> restOther{0};
    std::atomic<std::uint64_t> streamBytes{0};
    const auto end = Clock::now() + std::chrono::duration<double>(seconds);
    std::vector<std::thread> workers;
    HealthPoller health(config);
    for (int index = 0; index < streams; ++index) {
        workers.emplace_back([&] {
            LoadClient client(config);
            const LoadOutcome outcome = client.openStream("/probe/stream", seconds);
            streamBytes += outcome.bytes;
            if (outcome.status == 503) {
                ++streamsBusy;
            } else if (outcome.bytes > 0) {
                ++streamsOk;
            } else {
                ++streamsOther;
            }
        });
    }
    for (int index = 0; index < uploads; ++index) {
        workers.emplace_back([&] {
            LoadClient client(config);
            while (Clock::now() < end) {
                const LoadOutcome outcome = client.uploadBytes("/probe/upload", 256 * 1024);
                if (outcome.status == 200) {
                    ++uploadsOk;
                } else if (outcome.status == 503) {
                    ++uploadsBusy;
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                } else {
                    ++uploadsOther;
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                }
            }
        });
    }
    for (int index = 0; index < restClients; ++index) {
        workers.emplace_back([&] {
            LoadClient client(config);
            while (Clock::now() < end) {
                const LoadOutcome outcome = client.request("/probe/rest");
                if (outcome.status == 200) {
                    ++restOk;
                } else {
                    ++restOther;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    Results results;
    health.finish(results);
    results["streamsOk"] = streamsOk.load();
    results["streamsBusy"] = streamsBusy.load();
    results["streamsOther"] = streamsOther.load();
    results["uploadsOk"] = uploadsOk.load();
    results["uploadsBusy"] = uploadsBusy.load();
    results["uploadsOther"] = uploadsOther.load();
    results["restOk"] = restOk.load();
    results["restOther"] = restOther.load();
    results["streamMiB"] = static_cast<double>(streamBytes.load()) / (1024.0 * 1024.0);
    return results;
}

Results runIdleFlood(int port, bool useTls, int connections, double seconds) {
    const ClientTransportConfig config = clientConfig(port, useTls);
    HealthPoller health(config);
    LoadClient::holdIdleConnections("127.0.0.1", port, connections, seconds);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    Results results;
    health.finish(results);
    return results;
}

Results runStalledStreams(int port, bool useTls, int clients, double stallSeconds) {
    const ClientTransportConfig config = clientConfig(port, useTls);
    HealthPoller health(config);
    std::vector<std::thread> workers;
    for (int index = 0; index < clients; ++index) {
        workers.emplace_back([&] {
            LoadClient client(config);
            client.openStalledStream("/probe/stream", stallSeconds);
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    Results results;
    health.finish(results);
    return results;
}

Results runSlowBodies(int port, bool useTls, int clients) {
    ClientTransportConfig config = clientConfig(port, useTls);
    config.readTimeoutMilliseconds = 30000;
    config.writeTimeoutMilliseconds = 30000;
    std::mutex mutex;
    double worst = 0.0;
    int rejected = 0;
    std::vector<std::thread> workers;
    for (int index = 0; index < clients; ++index) {
        workers.emplace_back([&] {
            LoadClient client(config);
            const LoadOutcome outcome = client.uploadSlowly("/probe/upload", 1024 * 1024, 200);
            std::lock_guard<std::mutex> lock(mutex);
            worst = std::max(worst, outcome.seconds);
            if (outcome.status != 200) {
                ++rejected;
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    return {{"worstBodySeconds", worst}, {"rejected", static_cast<double>(rejected)}};
}

Results runSlowHeaders(int port, int clients, double limitSeconds) {
    std::mutex mutex;
    double worst = 0.0;
    std::vector<std::thread> workers;
    for (int index = 0; index < clients; ++index) {
        workers.emplace_back([&] {
            const double held = LoadClient::sendHeadersSlowly("127.0.0.1", port, 500, limitSeconds);
            std::lock_guard<std::mutex> lock(mutex);
            worst = std::max(worst, held);
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    return {{"worstHeaderSeconds", worst}};
}

template <typename ClientWork>
Results runScenario(CapacityServer& server, bool useTls, const std::string& name, ProbeReport& report, ClientWork work) {
    int portPipe[2];
    int resultPipe[2];
    if (pipe(portPipe) != 0 || pipe(resultPipe) != 0) {
        report.check(name + ":pipes", false);
        return {};
    }
    const pid_t child = fork();
    if (child == 0) {
        close(portPipe[1]);
        close(resultPipe[0]);
        int port = 0;
        if (read(portPipe[0], &port, sizeof(port)) != static_cast<ssize_t>(sizeof(port))) {
            _exit(3);
        }
        const Results results = work(port);
        std::ostringstream output;
        for (const auto& entry : results) {
            output << entry.first << " " << entry.second << "\n";
        }
        const std::string text = output.str();
        if (write(resultPipe[1], text.data(), text.size()) < 0) {
            _exit(4);
        }
        _exit(0);
    }
    close(portPipe[0]);
    close(resultPipe[1]);
    const TransportStartResult started = server.start(useTls);
    report.check(name + ":serverStarts", started.started, started.cause);
    if (write(portPipe[1], &started.boundPort, sizeof(started.boundPort)) < 0) {
        report.check(name + ":portSent", false);
    }
    close(portPipe[1]);

    std::atomic<bool> sampling{true};
    std::size_t maxThreads = 0;
    std::size_t maxStreams = 0;
    std::uint64_t maxResident = 0;
    std::size_t zeroStreamSamples = 0;
    double firstZeroSeconds = -1.0;
    const auto samplingStart = Clock::now();
    const std::uint64_t baselineResident = ProbeReport::residentBytes();
    std::thread sampler([&] {
        while (sampling.load()) {
            maxThreads = std::max(maxThreads, ProbeReport::threadCount());
            maxResident = std::max(maxResident, ProbeReport::residentBytes());
            const std::size_t streams = server.gate.activeStreams();
            maxStreams = std::max(maxStreams, streams);
            if (maxStreams > 0 && streams == 0) {
                ++zeroStreamSamples;
                if (firstZeroSeconds < 0.0) {
                    firstZeroSeconds = std::chrono::duration<double>(Clock::now() - samplingStart).count();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    std::string text;
    char buffer[4096];
    ssize_t count = 0;
    while ((count = read(resultPipe[0], buffer, sizeof(buffer))) > 0) {
        text.append(buffer, static_cast<std::size_t>(count));
    }
    close(resultPipe[0]);
    int childStatus = 0;
    waitpid(child, &childStatus, 0);
    sampling.store(false);
    sampler.join();
    const auto stopStart = Clock::now();
    server.stop();
    const double stopSeconds = std::chrono::duration<double>(Clock::now() - stopStart).count();

    Results results;
    std::istringstream input(text);
    std::string key;
    double value = 0.0;
    while (input >> key >> value) {
        results[key] = value;
    }
    const RestMetricsSnapshot metrics = server.metrics.snapshot();
    results["serverMaxThreads"] = static_cast<double>(maxThreads);
    results["serverMaxStreams"] = static_cast<double>(maxStreams);
    results["serverZeroStreamSeconds"] = static_cast<double>(zeroStreamSamples) * 0.1;
    results["streamsAllFreedAfterSeconds"] = firstZeroSeconds;
    results["serverRssGrowthMiB"] = static_cast<double>(maxResident > baselineResident ? maxResident - baselineResident : 0) / (1024.0 * 1024.0);
    results["serverStopSeconds"] = stopSeconds;
    results["rejectedStreams"] = static_cast<double>(metrics.rejectedStreams);
    results["rejectedTransfers"] = static_cast<double>(metrics.rejectedTransfers);
    results["resourceExhaustion"] = static_cast<double>(metrics.resourceExhaustion);
    results["transportErrors"] = static_cast<double>(metrics.transportErrors);
    results["droppedChunks"] = static_cast<double>(metrics.droppedChunks);
    results["requests"] = static_cast<double>(metrics.requests);
    results["worstPublishMs"] = server.worstPublishMilliseconds.load();
    results["leakedStreamLeases"] = static_cast<double>(server.gate.activeStreams());
    results["leakedTransferLeases"] = static_cast<double>(server.gate.activeTransfers());
    std::ostringstream summary;
    for (const auto& entry : results) {
        summary << entry.first << "=" << entry.second << " ";
    }
    report.note(name, summary.str());
    report.check(name + ":clientProcessExited", WIFEXITED(childStatus) && WEXITSTATUS(childStatus) == 0, "status=" + std::to_string(childStatus));
    report.check(name + ":noLeakedLeases", results["leakedStreamLeases"] == 0 && results["leakedTransferLeases"] == 0);
    report.check(name + ":threadsBounded", maxThreads <= server.maxThreads + 8, "max=" + std::to_string(maxThreads));
    return results;
}

void hundredClients(bool useTls, ProbeReport& report) {
    CapacityServer server(DeliveryPolicy::broadcast, 1024, 50);
    const double seconds = 20.0 * scale;
    Results results = runScenario(server, useTls, "hundred", report, [=](int port) {
        return runMixedClients(port, useTls, 70, 20, 9, seconds);
    });
    report.check("hundred:healthAnswers", results["healthFailures"] == 0 && results["healthOk"] > 0);
    report.check("hundred:noStreamRejected", results["rejectedStreams"] == 0 && results["streamsOk"] == 70, "ok=" + std::to_string(results["streamsOk"]));
    report.check("hundred:noExhaustion", results["resourceExhaustion"] == 0);
}

void hundredClientsHeadroom(bool useTls, ProbeReport& report) {
    CapacityServer server(DeliveryPolicy::broadcast, 1024, 50, 140);
    const double seconds = 20.0 * scale;
    Results results = runScenario(server, useTls, "hundredPool140", report, [=](int port) {
        return runMixedClients(port, useTls, 70, 20, 9, seconds);
    });
    report.check("hundredPool140:healthAnswers", results["healthFailures"] == 0 && results["healthOk"] > 0);
    report.check("hundredPool140:noStreamRejected", results["rejectedStreams"] == 0 && results["streamsOk"] == 70, "ok=" + std::to_string(results["streamsOk"]));
    report.check("hundredPool140:noExhaustion", results["resourceExhaustion"] == 0, "count=" + std::to_string(results["resourceExhaustion"]));
}

void hundredClientsBurstSizing(bool useTls, ProbeReport& report) {
    basePoolThreads = 64;
    queuedConnections = 128;
    CapacityServer server(DeliveryPolicy::broadcast, 1024, 50);
    const double seconds = 20.0 * scale;
    Results results = runScenario(server, useTls, "hundredBurst", report, [=](int port) {
        return runMixedClients(port, useTls, 70, 20, 9, seconds);
    });
    report.check("hundredBurst:healthAnswers", results["healthFailures"] == 0 && results["healthOk"] > 0);
    report.check("hundredBurst:noStreamRejected", results["rejectedStreams"] == 0 && results["streamsOk"] == 70, "ok=" + std::to_string(results["streamsOk"]));
    report.check("hundredBurst:noExhaustion", results["resourceExhaustion"] == 0, "count=" + std::to_string(results["resourceExhaustion"]));
}

void overloadClientsBurstSizing(bool useTls, ProbeReport& report) {
    basePoolThreads = 64;
    queuedConnections = 128;
    CapacityServer server(DeliveryPolicy::broadcast, 1024, 50);
    const double seconds = 15.0 * scale;
    Results results = runScenario(server, useTls, "overloadBurst", report, [=](int port) {
        return runMixedClients(port, useTls, 100, 30, 19, seconds);
    });
    report.check("overloadBurst:healthAnswers", results["healthFailures"] == 0 && results["healthOk"] > 0, "failures=" + std::to_string(results["healthFailures"]));
    report.check("overloadBurst:streamsRejectedCleanly", results["streamsBusy"] >= 30, "busy=" + std::to_string(results["streamsBusy"]));
    report.check("overloadBurst:noExhaustion", results["resourceExhaustion"] == 0, "count=" + std::to_string(results["resourceExhaustion"]));
}

void overloadClients(bool useTls, ProbeReport& report) {
    CapacityServer server(DeliveryPolicy::broadcast, 1024, 50);
    const double seconds = 15.0 * scale;
    Results results = runScenario(server, useTls, "overload150", report, [=](int port) {
        return runMixedClients(port, useTls, 100, 30, 19, seconds);
    });
    report.check("overload150:healthAnswers", results["healthFailures"] == 0 && results["healthOk"] > 0, "failures=" + std::to_string(results["healthFailures"]));
    report.check("overload150:streamsRejectedCleanly", results["streamsBusy"] >= 30, "busy=" + std::to_string(results["streamsBusy"]));
    report.check("overload150:noneBeyondCap", results["serverMaxStreams"] <= static_cast<double>(streamCap));
}

void idleFlood(bool useTls, ProbeReport& report) {
    CapacityServer server(DeliveryPolicy::realtime, 1024, 0);
    Results results = runScenario(server, useTls, "idleFlood150", report, [=](int port) {
        return runIdleFlood(port, useTls, 150, 8.0);
    });
    report.check("idleFlood150:healthAnswers", results["healthFailures"] == 0, "failures=" + std::to_string(results["healthFailures"]) + " maxMs=" + std::to_string(results["healthMaxMs"]));
}

void idleFloodWidePool(bool useTls, ProbeReport& report) {
    CapacityServer server(DeliveryPolicy::realtime, 1024, 0, 200);
    Results results = runScenario(server, useTls, "idleFlood150Pool200", report, [=](int port) {
        return runIdleFlood(port, useTls, 150, 8.0);
    });
    report.check("idleFlood150Pool200:healthAnswers", results["healthFailures"] == 0, "failures=" + std::to_string(results["healthFailures"]) + " rssMiB=" + std::to_string(results["serverRssGrowthMiB"]));
}

void stalledStreams(bool useTls, ProbeReport& report) {
    CapacityServer server(DeliveryPolicy::realtime, 64 * 1024, 5);
    Results results = runScenario(server, useTls, "stalled20", report, [=](int port) {
        return runStalledStreams(port, useTls, 20, 10.0 * scale);
    });
    report.check("stalled20:leasesFreedWhileClientsStalled", results["streamsAllFreedAfterSeconds"] >= 0.0 && results["streamsAllFreedAfterSeconds"] < 7.0, "freedAfterSeconds=" + std::to_string(results["streamsAllFreedAfterSeconds"]));
    report.check("stalled20:memoryBounded", results["serverRssGrowthMiB"] < 128.0, "growthMiB=" + std::to_string(results["serverRssGrowthMiB"]));
    report.check("stalled20:publishNeverBlocks", results["worstPublishMs"] < 100.0, "worstMs=" + std::to_string(results["worstPublishMs"]));
}

void stalledStreamsShortTimeout(bool useTls, ProbeReport& report) {
    writeTimeoutMilliseconds = 500;
    CapacityServer server(DeliveryPolicy::realtime, 64 * 1024, 5);
    Results results = runScenario(server, useTls, "stalled20Write500", report, [=](int port) {
        return runStalledStreams(port, useTls, 20, 10.0 * scale);
    });
    report.check("stalled20Write500:leasesFreedWhileClientsStalled", results["streamsAllFreedAfterSeconds"] >= 0.0 && results["streamsAllFreedAfterSeconds"] < 7.0, "freedAfterSeconds=" + std::to_string(results["streamsAllFreedAfterSeconds"]));
}

void slowBodies(bool useTls, ProbeReport& report) {
    CapacityServer server(DeliveryPolicy::realtime, 1024, 0);
    Results results = runScenario(server, useTls, "slowBody5", report, [=](int port) {
        return runSlowBodies(port, useTls, 5);
    });
    report.check("slowBody5:totalDeadlineEnforced", results["worstBodySeconds"] < 3.0 + 3.0 && results["rejected"] == 5, "worstSeconds=" + std::to_string(results["worstBodySeconds"]));
}

void slowHeaders(ProbeReport& report) {
    CapacityServer server(DeliveryPolicy::realtime, 1024, 0);
    const double limitSeconds = 30.0 * scale;
    Results results = runScenario(server, false, "slowHeaders5", report, [=](int port) {
        return runSlowHeaders(port, 5, limitSeconds);
    });
    report.check("slowHeaders5:threadHoldBounded", results["worstHeaderSeconds"] < limitSeconds - 1.0, "heldSeconds=" + std::to_string(results["worstHeaderSeconds"]));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: capacityProbe certsDirectory [tls|plain] [timeScale] [scenario]\n";
        return 2;
    }
    certs = argv[1];
    const bool useTls = argc < 3 || std::string(argv[2]) == "tls";
    if (argc > 3) {
        scale = std::atof(argv[3]);
    }
    const std::string only = argc > 4 ? argv[4] : "";
    rlimit limits{};
    getrlimit(RLIMIT_NOFILE, &limits);
    limits.rlim_cur = std::min<rlim_t>(limits.rlim_max, 4096);
    setrlimit(RLIMIT_NOFILE, &limits);

    const std::vector<std::pair<std::string, std::function<void(ProbeReport&)>>> scenarios = {
        {"hundred", [=](ProbeReport& report) { hundredClients(useTls, report); }},
        {"hundredHeadroom", [=](ProbeReport& report) { hundredClientsHeadroom(useTls, report); }},
        {"hundredBurst", [=](ProbeReport& report) { hundredClientsBurstSizing(useTls, report); }},
        {"overload", [=](ProbeReport& report) { overloadClients(useTls, report); }},
        {"overloadBurst", [=](ProbeReport& report) { overloadClientsBurstSizing(useTls, report); }},
        {"idleFlood", [=](ProbeReport& report) { idleFlood(useTls, report); }},
        {"idleFloodWide", [=](ProbeReport& report) { idleFloodWidePool(useTls, report); }},
        {"stalled", [=](ProbeReport& report) { stalledStreams(useTls, report); }},
        {"stalledShort", [=](ProbeReport& report) { stalledStreamsShortTimeout(useTls, report); }},
        {"slowBody", [=](ProbeReport& report) { slowBodies(useTls, report); }},
        {"slowHeaders", [](ProbeReport& report) { slowHeaders(report); }},
    };
    int failures = 0;
    for (const auto& scenario : scenarios) {
        if (!only.empty() && only != scenario.first) {
            continue;
        }
        // Each scenario gets a fresh process, so no TLS state crosses a fork.
        const pid_t runner = fork();
        if (runner == 0) {
            ProbeReport report("capacity");
            scenario.second(report);
            _exit(report.finish());
        }
        int status = 0;
        waitpid(runner, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            ++failures;
        }
    }
    std::printf("CAPACITY failedScenarios=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
