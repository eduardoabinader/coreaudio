#include "LoadClient.h"

#include "CppHttplibClientTransport.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace gigante {

namespace {

using Clock = std::chrono::steady_clock;

double elapsedSeconds(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

int connectPlain(const std::string& host, int port) {
    const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (descriptor < 0) {
        return -1;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    inet_pton(AF_INET, host.c_str(), &address.sin_addr);
    if (connect(descriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(descriptor);
        return -1;
    }
#ifdef SO_NOSIGPIPE
    const int enabled = 1;
    setsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
    return descriptor;
}

} // namespace

LoadClient::LoadClient(ClientTransportConfig clientConfig) : config(std::move(clientConfig)) {}

LoadOutcome LoadClient::request(const std::string& path) {
    const auto start = Clock::now();
    CppHttplibClientTransport transport(config);
    const ClientResponse response = transport.request(HttpMethod::get, path, "", "", {});
    return {response.status, response.error, response.body.size(), elapsedSeconds(start)};
}

LoadOutcome LoadClient::openStream(const std::string& path, double seconds) {
    const auto start = Clock::now();
    std::uint64_t bytes = 0;
    CppHttplibClientTransport transport(config);
    const ClientResponse response = transport.download(
        path,
        0,
        [&](const char*, std::size_t size) {
            bytes += size;
            return elapsedSeconds(start) < seconds;
        },
        nullptr);
    return {response.status, response.error, bytes, elapsedSeconds(start)};
}

LoadOutcome LoadClient::openStalledStream(const std::string& path, double stallSeconds) {
    const auto start = Clock::now();
    std::uint64_t bytes = 0;
    CppHttplibClientTransport transport(config);
    const ClientResponse response = transport.download(
        path,
        0,
        [&](const char*, std::size_t size) {
            bytes += size;
            std::this_thread::sleep_for(std::chrono::duration<double>(stallSeconds));
            return false;
        },
        nullptr);
    return {response.status, response.error, bytes, elapsedSeconds(start)};
}

LoadOutcome LoadClient::uploadBytes(const std::string& path, std::uint64_t length) {
    const auto start = Clock::now();
    const std::string chunk(16 * 1024, 'x');
    CppHttplibClientTransport transport(config);
    const ClientResponse response = transport.upload(
        path,
        length,
        "application/octet-stream",
        {},
        [&chunk](std::uint64_t, std::size_t size, TransportWriter& writer) {
            return writer.write(chunk.data(), std::min(size, chunk.size()));
        });
    return {response.status, response.error, length, elapsedSeconds(start)};
}

LoadOutcome LoadClient::uploadSlowly(const std::string& path, std::uint64_t length, int delayMilliseconds) {
    const auto start = Clock::now();
    CppHttplibClientTransport transport(config);
    const ClientResponse response = transport.upload(
        path,
        length,
        "application/octet-stream",
        {},
        [delayMilliseconds](std::uint64_t, std::size_t, TransportWriter& writer) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMilliseconds));
            return writer.write("x", 1);
        });
    return {response.status, response.error, length, elapsedSeconds(start)};
}

double LoadClient::holdIdleConnections(const std::string& host, int port, int count, double seconds) {
    const auto start = Clock::now();
    std::vector<int> descriptors;
    for (int index = 0; index < count; ++index) {
        const int descriptor = connectPlain(host, port);
        if (descriptor >= 0) {
            descriptors.push_back(descriptor);
        }
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    for (const int descriptor : descriptors) {
        close(descriptor);
    }
    return elapsedSeconds(start);
}

double LoadClient::sendHeadersSlowly(const std::string& host, int port, int delayMilliseconds, double limitSeconds) {
    const auto start = Clock::now();
    const int descriptor = connectPlain(host, port);
    if (descriptor < 0) {
        return 0.0;
    }
    const std::string request = "GET /probe/health HTTP/1.1\r\nHost: " + host + "\r\nX-Padding: " + std::string(4096, 'a') + "\r\n\r\n";
    std::size_t index = 0;
    while (index < request.size() && elapsedSeconds(start) < limitSeconds) {
        if (send(descriptor, &request[index], 1, 0) <= 0) {
            break;
        }
        char byte = 0;
        const int flags = fcntl(descriptor, F_GETFL, 0);
        fcntl(descriptor, F_SETFL, flags | O_NONBLOCK);
        const ssize_t received = recv(descriptor, &byte, 1, 0);
        fcntl(descriptor, F_SETFL, flags);
        if (received == 0 || received > 0) {
            break;
        }
        ++index;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMilliseconds));
    }
    close(descriptor);
    return elapsedSeconds(start);
}

} // namespace gigante
