#include "ClientContext.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace gigante {

ClientContext::ClientContext(RestClientConfig config) : clientConfig(std::move(config)) {}

const RestClientConfig& ClientContext::config() const {
    return clientConfig;
}

ClientTransportConfig ClientContext::transportConfig(int budgetMilliseconds) const {
    ClientTransportConfig result;
    result.baseUrl = clientConfig.baseUrl;
    result.caCertificatePath = clientConfig.caCertificatePath;
    result.clientCertificatePath = clientConfig.clientCertificatePath;
    result.clientPrivateKeyPath = clientConfig.clientPrivateKeyPath;
    result.verifyPeer = clientConfig.verifyPeer;
    result.verifyHostname = clientConfig.verifyHostname;
    result.connectTimeoutMilliseconds = std::min(clientConfig.connectTimeoutMilliseconds, budgetMilliseconds);
    result.readTimeoutMilliseconds = std::min(clientConfig.readTimeoutMilliseconds, budgetMilliseconds);
    result.writeTimeoutMilliseconds = std::min(clientConfig.writeTimeoutMilliseconds, budgetMilliseconds);
    return result;
}

std::uint64_t ClientContext::generation() const {
    return cancelGeneration.load();
}

bool ClientContext::isCanceled(std::uint64_t generation) const {
    return cancelGeneration.load() != generation;
}

void ClientContext::cancel() {
    std::lock_guard<std::mutex> lock(mutex);
    cancelGeneration.fetch_add(1);
    for (HttpClientTransport* transport : active) {
        transport->cancel();
    }
    cancelled.notify_all();
}

bool ClientContext::waitUntil(Clock::time_point wakeAt, std::uint64_t generation) {
    std::unique_lock<std::mutex> lock(mutex);
    return cancelled.wait_until(lock, wakeAt, [&] { return isCanceled(generation); });
}

void ClientContext::log(RestLogLevel level, const std::string& event, const std::string& detail) const {
    if (!clientConfig.log) {
        return;
    }
    try {
        clientConfig.log(level, event, detail);
    } catch (const std::exception&) {
    } catch (...) {
    }
}

TrackedTransport::TrackedTransport(ClientContext& clientContext, const ClientTransportConfig& config, std::uint64_t generation)
    : context(clientContext), client(config) {
    std::lock_guard<std::mutex> lock(context.mutex);
    if (!context.isCanceled(generation)) {
        context.active.insert(&client);
        usable = true;
    }
}

TrackedTransport::~TrackedTransport() {
    std::lock_guard<std::mutex> lock(context.mutex);
    context.active.erase(&client);
}

bool TrackedTransport::isUsable() const {
    return usable;
}

HttpClientTransport& TrackedTransport::transport() {
    return client;
}

} // namespace gigante
