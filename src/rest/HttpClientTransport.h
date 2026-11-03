#pragma once

#include "HttpTransport.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace gigante {

struct ClientTransportConfig {
    std::string baseUrl;
    std::string caCertificatePath;
    std::string clientCertificatePath;
    std::string clientPrivateKeyPath;
    bool verifyPeer = true;
    bool verifyHostname = true;
    int connectTimeoutMilliseconds = 2000;
    int readTimeoutMilliseconds = 5000;
    int writeTimeoutMilliseconds = 5000;
};

enum class ClientError { none, connection, tlsVerification, hostnameVerification, tlsHandshake, canceled, other };

struct ClientResponse {
    ClientError error = ClientError::none;
    int status = 0;
    std::string body;
    std::string errorText;
    std::multimap<std::string, std::string> headers;
};

struct EventStreamOptions {
    int minimumReconnectMilliseconds = 1000;
    int maximumAttempts = 5;
};

using ClientChunkSource = std::function<bool(std::uint64_t offset, std::size_t size, TransportWriter&)>;
using ClientChunkSink = std::function<bool(const char* data, std::size_t size)>;
using ClientProgress = std::function<bool(std::uint64_t current, std::uint64_t total)>;
using ClientEventHandler = std::function<void(const std::string& event, const std::string& data)>;
// Sees the status and headers before any body byte; false abandons the response.
using ClientResponseStart = std::function<bool(int status, const std::multimap<std::string, std::string>& headers)>;

class HttpClientTransport {
public:
    virtual ~HttpClientTransport() = default;

    virtual ClientResponse request(
        HttpMethod method,
        const std::string& path,
        const std::string& body,
        const std::string& contentType,
        const std::map<std::string, std::string>& headers) = 0;
    virtual ClientResponse upload(
        const std::string& path,
        std::uint64_t length,
        const std::string& contentType,
        const std::map<std::string, std::string>& headers,
        ClientChunkSource source) = 0;
    virtual ClientResponse download(
        const std::string& path,
        std::uint64_t rangeStart,
        ClientChunkSink sink,
        ClientProgress progress) = 0;

    // A GET whose body, whatever its status, goes to the sink as it arrives.
    virtual ClientResponse streamGet(
        const std::string& path,
        const std::map<std::string, std::string>& headers,
        ClientResponseStart onStart,
        ClientChunkSink sink) = 0;

    // Blocks until cancel(), or until reconnect attempts are used up. Returns the
    // number of connections opened.
    virtual int subscribeEvents(const std::string& path, const EventStreamOptions& options, ClientEventHandler handler) = 0;

    // Safe from another thread; ends the current request.
    virtual void cancel() = 0;
};

} // namespace gigante
