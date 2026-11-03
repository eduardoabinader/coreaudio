#pragma once

#include "gigante/RestTypes.h"

#include <string>

namespace gigante {

struct ServerSecurityDecision {
    bool allowed = false;
    std::string cause;
    std::string warning;
    bool useTls = false;
};

struct ClientSecurityDecision {
    bool allowed = false;
    std::string cause;
    std::string warning;
};

// The only place that decides HTTP versus HTTPS and whether verification may be off.
class SecurityPolicy final {
public:
    static ServerSecurityDecision checkServer(const RestServerConfig& config, bool hasAuthenticator, bool tlsAvailable, bool plaintextAllowed);
    static ClientSecurityDecision checkClient(const RestClientConfig& config, bool tlsAvailable, bool plaintextAllowed);
    static bool isLoopbackHost(const std::string& host);
};

} // namespace gigante
