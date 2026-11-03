#pragma once

#include "HttpClientTransport.h"

#include <cstdint>
#include <string>

namespace gigante {

struct LoadOutcome {
    int status = 0;
    ClientError error = ClientError::none;
    std::uint64_t bytes = 0;
    double seconds = 0.0;
};

// Simulates one remote client's behavior against a running server.
class LoadClient {
public:
    explicit LoadClient(ClientTransportConfig config);

    LoadOutcome request(const std::string& path);
    LoadOutcome openStream(const std::string& path, double seconds);
    LoadOutcome openStalledStream(const std::string& path, double stallSeconds);
    LoadOutcome uploadBytes(const std::string& path, std::uint64_t length);
    LoadOutcome uploadSlowly(const std::string& path, std::uint64_t length, int delayMilliseconds);

    // Plain sockets, so the behaviour is visible below any HTTP client.
    static double holdIdleConnections(const std::string& host, int port, int count, double seconds);
    static double sendHeadersSlowly(const std::string& host, int port, int delayMilliseconds, double limitSeconds);

private:
    ClientTransportConfig config;
};

} // namespace gigante
