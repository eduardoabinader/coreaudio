#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace gigante {

enum class HttpMethod { get, post, put, patch, del, options };

struct TransportTlsFiles {
    std::string certificatePath;
    std::string privateKeyPath;
    std::string clientCaPath;
};

struct TransportConfig {
    std::string host = "127.0.0.1";
    int port = 0;
    bool useTls = false;
    TransportTlsFiles tls;
    std::size_t baseThreads = 8;
    std::size_t maxThreads = 16;
    std::size_t maxQueuedConnections = 16;
    std::size_t maxPayloadBytes = 1024 * 1024;
    int readTimeoutMilliseconds = 2000;
    int writeTimeoutMilliseconds = 2000;
    int keepAliveTimeoutMilliseconds = 2000;
    std::size_t keepAliveMaxCount = 100;
};

struct TransportRequest {
    HttpMethod method = HttpMethod::get;
    std::string path;
    std::map<std::string, std::string> pathParameters;
    std::multimap<std::string, std::string> query;
    std::multimap<std::string, std::string> headers;
    std::string body;
};

class TransportWriter {
public:
    virtual ~TransportWriter() = default;
    virtual bool write(const char* data, std::size_t size) = 0;
    virtual bool isWritable() = 0;
    virtual void finish() = 0;
};

// A producer returning false ends the response as failed.
using StreamProducer = std::function<bool(TransportWriter&)>;
using SizedProducer = std::function<bool(std::uint64_t offset, std::uint64_t length, TransportWriter&)>;
using ProducerRelease = std::function<void(bool success)>;

struct TransportResponse {
    int status = 200;
    std::string contentType = "application/json";
    std::string body;
    std::multimap<std::string, std::string> headers;
    StreamProducer streamProducer;
    SizedProducer sizedProducer;
    std::uint64_t contentLength = 0;
    ProducerRelease release;
};

using RouteHandler = std::function<TransportResponse(const TransportRequest&)>;
using BodyChunkSink = std::function<bool(const char* data, std::size_t size)>;
using BodyChunkReader = std::function<bool(const BodyChunkSink&)>;
using UploadHandler = std::function<TransportResponse(const TransportRequest&, const BodyChunkReader&)>;
using PreRouteFilter = std::function<std::optional<TransportResponse>(const TransportRequest&)>;

// Supplies the body for a library-generated error, such as 404 or an escaped exception.
using ErrorResponder = std::function<TransportResponse(int status, const TransportRequest&)>;

struct TransportStartResult {
    bool started = false;
    std::string cause;
    int boundPort = 0;
};

class HttpTransport {
public:
    virtual ~HttpTransport() = default;

    // Patterns use {name} parameters. Register routes before start().
    virtual void addRoute(HttpMethod method, const std::string& pattern, RouteHandler handler) = 0;
    virtual void addUploadRoute(HttpMethod method, const std::string& pattern, UploadHandler handler) = 0;
    virtual void setPreRouteFilter(PreRouteFilter filter) = 0;
    virtual void setErrorResponder(ErrorResponder responder) = 0;

    virtual TransportStartResult start(const TransportConfig& config) = 0;

    // Returns false when work is still running after the bound. Repeat calls are safe.
    virtual bool stop(int timeoutMilliseconds) = 0;
    virtual const char* tlsBackend() const = 0;
};

} // namespace gigante
