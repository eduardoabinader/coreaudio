#pragma once

#include <gigante/RestClient.h>
#include <gigante/RestServer.h>

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>

namespace restTest {

inline int failures = 0;

inline void check(bool passed, const std::string& name, const std::string& detail = "") {
    if (!passed) {
        ++failures;
        std::printf("FAIL %s %s\n", name.c_str(), detail.c_str());
    }
}

inline int finish(const char* suite) {
    std::printf("%s: %d failures\n", suite, failures);
    return failures == 0 ? 0 : 1;
}

inline bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

inline bool waitUntil(const std::function<bool()>& condition, int milliseconds = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return condition();
}

inline gigante::RestServerConfig developmentServer() {
    gigante::RestServerConfig config(gigante::SecurityProfile::development);
    config.limits.baseThreads = 8;
    config.limits.maxThreads = 32;
    config.limits.maxQueuedConnections = 32;
    // start() refuses caps that, with reservedThreads, exceed maxThreads.
    config.limits.maxStreams = 8;
    config.limits.maxTransfers = 8;
    return config;
}

inline gigante::RestClientConfig developmentClient(int port) {
    gigante::RestClientConfig config(gigante::SecurityProfile::development);
    config.baseUrl = "http://127.0.0.1:" + std::to_string(port);
    return config;
}

} // namespace restTest
