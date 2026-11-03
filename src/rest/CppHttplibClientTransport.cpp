#include "CppHttplibClientTransport.h"

#include <httplib.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace gigante {

namespace {

class SinkWriter final : public TransportWriter {
public:
    explicit SinkWriter(httplib::DataSink& dataSink) : sink(dataSink) {}

    bool write(const char* data, std::size_t size) override {
        return sink.write(data, size);
    }
    bool isWritable() override {
        return sink.is_writable();
    }
    void finish() override {
        sink.done();
    }

private:
    httplib::DataSink& sink;
};

// The library ignores SIGPIPE only in Server's constructor, so a client-only process
// would be killed when a peer closes during a write.
void protectFromSigpipe(httplib::Client& client) {
#ifdef SO_NOSIGPIPE
    client.set_socket_options([](socket_t descriptor) {
        const int enabled = 1;
        setsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
        setsockopt(descriptor, IPPROTO_TCP, TCP_NODELAY, &enabled, sizeof(enabled));
    });
#else
    (void)client;
    struct sigaction current {};
    if (sigaction(SIGPIPE, nullptr, &current) == 0 && current.sa_handler == SIG_DFL) {
        std::signal(SIGPIPE, SIG_IGN);
    }
#endif
}

ClientError toClientError(httplib::Error error) {
    switch (error) {
        case httplib::Error::Success:
            return ClientError::none;
        case httplib::Error::Connection:
        case httplib::Error::ConnectionTimeout:
            return ClientError::connection;
        case httplib::Error::Canceled:
            return ClientError::canceled;
#ifdef CPPHTTPLIB_SSL_ENABLED
        case httplib::Error::SSLServerVerification:
            return ClientError::tlsVerification;
        case httplib::Error::SSLServerHostnameVerification:
            return ClientError::hostnameVerification;
        case httplib::Error::SSLConnection:
        case httplib::Error::SSLLoadingCerts:
            return ClientError::tlsHandshake;
#endif
        default:
            return ClientError::other;
    }
}

ClientResponse toResponse(const httplib::Result& result) {
    ClientResponse response;
    response.error = toClientError(result.error());
    response.errorText = httplib::to_string(result.error());
    if (result) {
        response.status = result->status;
        response.body = result->body;
        for (const auto& header : result->headers) {
            response.headers.emplace(header.first, header.second);
        }
    }
    return response;
}

httplib::Headers toHeaders(const std::map<std::string, std::string>& headers) {
    httplib::Headers result;
    for (const auto& header : headers) {
        result.emplace(header.first, header.second);
    }
    return result;
}

} // namespace

class CppHttplibClientTransport::Implementation {
public:
    explicit Implementation(const ClientTransportConfig& config)
        : client(
              config.baseUrl,
              config.clientCertificatePath,
              config.clientPrivateKeyPath) {
#ifdef CPPHTTPLIB_SSL_ENABLED
        if (!config.caCertificatePath.empty()) {
            client.set_ca_cert_path(config.caCertificatePath);
        }
        client.enable_server_certificate_verification(config.verifyPeer);
        client.enable_server_hostname_verification(config.verifyHostname);
#endif
#ifdef CPPHTTPLIB_MBEDTLS_SUPPORT
        // Mbed TLS builds this list on first use, without locking.
        mbedtls_ssl_list_ciphersuites();
#endif
        protectFromSigpipe(client);
        client.set_follow_location(false);
        client.set_connection_timeout(std::chrono::milliseconds(config.connectTimeoutMilliseconds));
        client.set_read_timeout(std::chrono::milliseconds(config.readTimeoutMilliseconds));
        client.set_write_timeout(std::chrono::milliseconds(config.writeTimeoutMilliseconds));
    }

    httplib::Client client;
    std::mutex streamMutex;
    httplib::sse::SSEClient* activeStream = nullptr;
    std::atomic<bool> cancelled{false};
};

CppHttplibClientTransport::CppHttplibClientTransport(const ClientTransportConfig& config)
    : implementation(std::make_unique<Implementation>(config)) {}

CppHttplibClientTransport::~CppHttplibClientTransport() = default;

ClientResponse CppHttplibClientTransport::request(
    HttpMethod method,
    const std::string& path,
    const std::string& body,
    const std::string& contentType,
    const std::map<std::string, std::string>& headers) {
    httplib::Client& client = implementation->client;
    const httplib::Headers libraryHeaders = toHeaders(headers);
    switch (method) {
        case HttpMethod::get: return toResponse(client.Get(path, libraryHeaders));
        case HttpMethod::post: return toResponse(client.Post(path, libraryHeaders, body, contentType));
        case HttpMethod::put: return toResponse(client.Put(path, libraryHeaders, body, contentType));
        case HttpMethod::patch: return toResponse(client.Patch(path, libraryHeaders, body, contentType));
        case HttpMethod::del: return toResponse(client.Delete(path, libraryHeaders, body, contentType));
        case HttpMethod::options: return toResponse(client.Options(path, libraryHeaders));
    }
    return {};
}

ClientResponse CppHttplibClientTransport::upload(
    const std::string& path,
    std::uint64_t length,
    const std::string& contentType,
    const std::map<std::string, std::string>& headers,
    ClientChunkSource source) {
    return toResponse(implementation->client.Put(
        path,
        toHeaders(headers),
        static_cast<std::size_t>(length),
        [chunkSource = std::move(source)](std::size_t offset, std::size_t size, httplib::DataSink& sink) {
            SinkWriter writer(sink);
            return chunkSource(offset, size, writer);
        },
        contentType));
}

ClientResponse CppHttplibClientTransport::download(
    const std::string& path,
    std::uint64_t rangeStart,
    ClientChunkSink sink,
    ClientProgress progress) {
    httplib::Headers headers;
    if (rangeStart > 0) {
        headers.emplace("Range", "bytes=" + std::to_string(rangeStart) + "-");
    }
    return toResponse(implementation->client.Get(
        path,
        headers,
        [chunkSink = std::move(sink)](const char* data, std::size_t size) { return chunkSink(data, size); },
        [chunkProgress = std::move(progress)](std::size_t current, std::size_t total) {
            return chunkProgress ? chunkProgress(current, total) : true;
        }));
}

ClientResponse CppHttplibClientTransport::streamGet(
    const std::string& path,
    const std::map<std::string, std::string>& headers,
    ClientResponseStart onStart,
    ClientChunkSink sink) {
    int status = 0;
    std::multimap<std::string, std::string> responseHeaders;
    ClientResponse response = toResponse(implementation->client.Get(
        path,
        toHeaders(headers),
        [&](const httplib::Response& start) {
            // A cancel that raced the connect is caught here, as soon as the server answers.
            if (implementation->cancelled.load()) {
                return false;
            }
            status = start.status;
            for (const auto& header : start.headers) {
                responseHeaders.emplace(header.first, header.second);
            }
            return onStart ? onStart(status, responseHeaders) : true;
        },
        [this, chunkSink = std::move(sink)](const char* data, std::size_t size) {
            return !implementation->cancelled.load() && chunkSink(data, size);
        }));
    // An abandoned or cut-off response has no result object, but its start was seen.
    if (response.status == 0) {
        response.status = status;
        response.headers = std::move(responseHeaders);
    }
    return response;
}

int CppHttplibClientTransport::subscribeEvents(
    const std::string& path, const EventStreamOptions& options, ClientEventHandler handler) {
    httplib::sse::SSEClient stream(implementation->client, path);
    std::atomic<int> connections{0};
    auto lastOpen = std::chrono::steady_clock::time_point{};
    stream.set_reconnect_interval(options.minimumReconnectMilliseconds);
    stream.set_max_reconnect_attempts(options.maximumAttempts);
    stream.on_message([&handler](const httplib::sse::SSEMessage& message) {
        handler(message.event, message.data);
    });
    // The library resets its attempt count on every successful connect and takes
    // retry: from the server, so a hostile server could otherwise cause a tight
    // reconnect loop. Both limits are enforced here.
    stream.on_open([&] {
        const int count = connections.fetch_add(1) + 1;
        if (count > options.maximumAttempts) {
            stream.stop();
            return;
        }
        const auto minimumNext = lastOpen + std::chrono::milliseconds(options.minimumReconnectMilliseconds);
        const auto now = std::chrono::steady_clock::now();
        if (count > 1 && now < minimumNext) {
            std::this_thread::sleep_for(minimumNext - now);
        }
        lastOpen = std::chrono::steady_clock::now();
    });
    {
        std::lock_guard<std::mutex> lock(implementation->streamMutex);
        if (implementation->cancelled.load()) {
            return 0;
        }
        implementation->activeStream = &stream;
    }
    stream.start();
    {
        std::lock_guard<std::mutex> lock(implementation->streamMutex);
        implementation->activeStream = nullptr;
    }
    return connections.load();
}

void CppHttplibClientTransport::cancel() {
    std::lock_guard<std::mutex> lock(implementation->streamMutex);
    implementation->cancelled.store(true);
    if (implementation->activeStream != nullptr) {
        implementation->activeStream->stop();
    }
    implementation->client.stop();
}

} // namespace gigante
