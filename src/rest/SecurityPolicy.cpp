#include "SecurityPolicy.h"

namespace gigante {

bool SecurityPolicy::isLoopbackHost(const std::string& host) {
    return host == "127.0.0.1" || host == "::1" || host == "localhost";
}

ServerSecurityDecision SecurityPolicy::checkServer(
    const RestServerConfig& config, bool hasAuthenticator, bool tlsAvailable, bool plaintextAllowed) {
    ServerSecurityDecision decision;
    const bool production = config.profile == SecurityProfile::production;
    if (config.host.empty() || config.host.size() > 255) {
        decision.cause = "listener host must be set explicitly";
        return decision;
    }
    const bool useTls = config.scheme == RestScheme::https || (config.scheme == RestScheme::automatic && production);
    if (useTls) {
        if (!tlsAvailable) {
            decision.cause = "HTTPS requested, but this build has no TLS backend";
            return decision;
        }
        if (config.tls.certificatePath.empty() || config.tls.privateKeyPath.empty()) {
            decision.cause = "HTTPS requires a certificate and private key";
            return decision;
        }
    } else {
        if (production) {
            decision.cause = "the production profile requires HTTPS";
            return decision;
        }
        if (!plaintextAllowed) {
            decision.cause = "plaintext support is removed from this build";
            return decision;
        }
        if (!isLoopbackHost(config.host)) {
            if (!config.allowNonLoopbackPlaintext) {
                decision.cause = "plaintext on a non-loopback address needs allowNonLoopbackPlaintext";
                return decision;
            }
            decision.warning = "plaintext listener on non-loopback address " + config.host;
        }
    }
    if (production && !hasAuthenticator) {
        decision.cause = "the production profile requires an authenticator";
        return decision;
    }
    decision.allowed = true;
    decision.useTls = useTls;
    return decision;
}

ClientSecurityDecision SecurityPolicy::checkClient(const RestClientConfig& config, bool tlsAvailable, bool plaintextAllowed) {
    ClientSecurityDecision decision;
    const bool production = config.profile == SecurityProfile::production;
    const bool secure = config.baseUrl.rfind("https://", 0) == 0;
    const bool plain = config.baseUrl.rfind("http://", 0) == 0;
    if (!secure && !plain) {
        decision.cause = "the base URL must start with http:// or https://";
        return decision;
    }
    if (plain && production) {
        decision.cause = "the production profile requires an https:// URL";
        return decision;
    }
    if (plain && !plaintextAllowed) {
        decision.cause = "plaintext support is removed from this build";
        return decision;
    }
    if (secure && !tlsAvailable) {
        decision.cause = "https:// requested, but this build has no TLS backend";
        return decision;
    }
    if (!config.verifyPeer || !config.verifyHostname) {
        if (production) {
            decision.cause = "the production profile does not allow verification to be disabled";
            return decision;
        }
        decision.warning = "TLS verification partly disabled for " + config.baseUrl;
    }
    decision.allowed = true;
    return decision;
}

} // namespace gigante
