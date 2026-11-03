#include "CppHttplibTransport.h"

#include <httplib.h>

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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

std::string toLibraryPattern(const std::string& pattern) {
    std::string result;
    result.reserve(pattern.size());
    for (const char current : pattern) {
        if (current == '{') {
            result.push_back(':');
        } else if (current != '}') {
            result.push_back(current);
        }
    }
    return result;
}

HttpMethod toMethod(const std::string& method) {
    if (method == "POST") return HttpMethod::post;
    if (method == "PUT") return HttpMethod::put;
    if (method == "PATCH") return HttpMethod::patch;
    if (method == "DELETE") return HttpMethod::del;
    if (method == "OPTIONS") return HttpMethod::options;
    return HttpMethod::get;
}

TransportRequest toRequest(const httplib::Request& request) {
    TransportRequest result;
    result.method = toMethod(request.method);
    result.path = request.path;
    result.body = request.body;
    for (const auto& parameter : request.path_params) {
        result.pathParameters.emplace(parameter.first, parameter.second);
    }
    for (const auto& parameter : request.params) {
        result.query.emplace(parameter.first, parameter.second);
    }
    for (const auto& header : request.headers) {
        result.headers.emplace(header.first, header.second);
    }
    return result;
}

void applyResponse(TransportResponse response, httplib::Response& target) {
    target.status = response.status;
    for (const auto& header : response.headers) {
        target.set_header(header.first, header.second);
    }
    if (response.streamProducer) {
        target.set_chunked_content_provider(
            response.contentType,
            [producer = std::move(response.streamProducer)](std::size_t, httplib::DataSink& sink) {
                SinkWriter writer(sink);
                return producer(writer);
            },
            std::move(response.release));
    } else if (response.sizedProducer) {
        target.set_content_provider(
            static_cast<std::size_t>(response.contentLength),
            response.contentType,
            [producer = std::move(response.sizedProducer)](std::size_t offset, std::size_t length, httplib::DataSink& sink) {
                SinkWriter writer(sink);
                return producer(offset, length, writer);
            },
            std::move(response.release));
    } else if (response.release) {
        // Using a provider means release runs only after the response has been sent.
        const auto body = std::make_shared<const std::string>(std::move(response.body));
        target.set_content_provider(
            body->size(),
            response.contentType,
            [body](std::size_t offset, std::size_t length, httplib::DataSink& sink) {
                return sink.write(body->data() + offset, length);
            },
            std::move(response.release));
    } else {
        target.set_content(response.body, response.contentType);
    }
}

} // namespace

class CppHttplibTransport::Implementation {
public:
    explicit Implementation(RestMetrics& restMetrics) : metrics(restMetrics) {}

    ~Implementation() {
        if (server) {
            server->stop();
        }
        if (listenThread.joinable()) {
            listenThread.join();
        }
    }

    TransportStartResult start(const TransportConfig& config) {
        TransportStartResult result;
        if (server) {
            result.cause = "server already started";
            return result;
        }
        std::unique_ptr<httplib::Server> candidate;
        if (config.useTls) {
#ifdef CPPHTTPLIB_MBEDTLS_SUPPORT
            // Mbed TLS builds this list on first use, without locking.
            mbedtls_ssl_list_ciphersuites();
#endif
#ifdef CPPHTTPLIB_SSL_ENABLED
            auto secure = std::make_unique<httplib::SSLServer>(
                config.tls.certificatePath.c_str(),
                config.tls.privateKeyPath.c_str(),
                config.tls.clientCaPath.empty() ? nullptr : config.tls.clientCaPath.c_str());
            if (!secure->is_valid()) {
                result.cause = "invalid TLS certificate, key or client CA";
                return result;
            }
            candidate = std::move(secure);
#else
            result.cause = "TLS requested, but this build has no TLS backend";
            return result;
#endif
        } else {
            candidate = std::make_unique<httplib::Server>();
        }
        configure(*candidate, config);
        registerRoutes(*candidate);
        int boundPort = config.port;
        if (config.port == 0) {
            boundPort = candidate->bind_to_any_port(config.host);
            if (boundPort < 0) {
                result.cause = "could not bind to " + config.host;
                return result;
            }
        } else if (!candidate->bind_to_port(config.host, config.port)) {
            result.cause = "could not bind to " + config.host + ":" + std::to_string(config.port);
            return result;
        }
        server = std::move(candidate);
        std::promise<void> finishedPromise;
        finished = finishedPromise.get_future();
        listenThread = std::thread([target = server.get(), promise = std::move(finishedPromise)]() mutable {
            target->listen_after_bind();
            promise.set_value();
        });
        server->wait_until_ready();
        result.started = true;
        result.boundPort = boundPort;
        return result;
    }

    bool stop(int timeoutMilliseconds) {
        if (!server) {
            return true;
        }
        server->stop();
        if (finished.wait_for(std::chrono::milliseconds(timeoutMilliseconds)) != std::future_status::ready) {
            return false;
        }
        listenThread.join();
        server.reset();
        return true;
    }

    std::vector<std::pair<HttpMethod, std::pair<std::string, RouteHandler>>> routes;
    std::vector<std::pair<HttpMethod, std::pair<std::string, UploadHandler>>> uploadRoutes;
    PreRouteFilter preRouteFilter;
    ErrorResponder errorResponder;

private:
    void configure(httplib::Server& target, const TransportConfig& config) {
        const std::size_t baseThreads = config.baseThreads;
        const std::size_t maxThreads = config.maxThreads;
        const std::size_t maxQueued = config.maxQueuedConnections;
        target.new_task_queue = [baseThreads, maxThreads, maxQueued] {
            return new httplib::ThreadPool(baseThreads, maxThreads, maxQueued);
        };
        target.set_read_timeout(std::chrono::milliseconds(config.readTimeoutMilliseconds));
        target.set_write_timeout(std::chrono::milliseconds(config.writeTimeoutMilliseconds));
        target.set_keep_alive_timeout(std::chrono::milliseconds(config.keepAliveTimeoutMilliseconds));
        target.set_keep_alive_max_count(config.keepAliveMaxCount);
        target.set_payload_max_length(config.maxPayloadBytes);
        target.set_error_logger([this](const httplib::Error& error, const httplib::Request*) {
            if (error == httplib::Error::ResourceExhaustion) {
                metrics.resourceExhaustion.fetch_add(1);
            } else {
                metrics.transportErrors.fetch_add(1);
            }
        });
        target.set_exception_handler([this](const httplib::Request& request, httplib::Response& response, std::exception_ptr) {
            metrics.handlerExceptions.fetch_add(1);
            response.status = 500;
            if (errorResponder) {
                applyResponse(errorResponder(500, toRequest(request)), response);
            } else {
                response.set_content("{\"code\":\"internalError\",\"retryable\":false}", "application/json");
            }
        });
        if (errorResponder) {
            target.set_error_handler([this](const httplib::Request& request, httplib::Response& response) {
                if (response.body.empty() && response.content_length_ == 0) {
                    applyResponse(errorResponder(response.status, toRequest(request)), response);
                }
            });
        }
        if (preRouteFilter) {
            target.set_pre_routing_handler([this](const httplib::Request& request, httplib::Response& response) {
                auto filtered = preRouteFilter(toRequest(request));
                if (!filtered) {
                    return httplib::Server::HandlerResponse::Unhandled;
                }
                applyResponse(std::move(*filtered), response);
                return httplib::Server::HandlerResponse::Handled;
            });
        }
    }

    void registerRoutes(httplib::Server& target) {
        for (const auto& route : routes) {
            const std::string pattern = toLibraryPattern(route.second.first);
            const RouteHandler handler = route.second.second;
            const auto wrapped = [this, handler](const httplib::Request& request, httplib::Response& response) {
                metrics.requests.fetch_add(1);
                applyResponse(handler(toRequest(request)), response);
                // The library slices a provider's output only when the status is 206.
                if (!request.ranges.empty() && response.status == 200 && response.content_length_ > 0) {
                    response.status = 206;
                }
            };
            switch (route.first) {
                case HttpMethod::get: target.Get(pattern, wrapped); break;
                case HttpMethod::post: target.Post(pattern, wrapped); break;
                case HttpMethod::put: target.Put(pattern, wrapped); break;
                case HttpMethod::patch: target.Patch(pattern, wrapped); break;
                case HttpMethod::del: target.Delete(pattern, wrapped); break;
                case HttpMethod::options: target.Options(pattern, wrapped); break;
            }
        }
        for (const auto& route : uploadRoutes) {
            const std::string pattern = toLibraryPattern(route.second.first);
            const UploadHandler handler = route.second.second;
            const auto wrapped = [this, handler](
                                     const httplib::Request& request,
                                     httplib::Response& response,
                                     const httplib::ContentReader& contentReader) {
                metrics.requests.fetch_add(1);
                const BodyChunkReader reader = [&contentReader](const BodyChunkSink& sink) {
                    return contentReader([&sink](const char* data, std::size_t size) { return sink(data, size); });
                };
                applyResponse(handler(toRequest(request), reader), response);
            };
            switch (route.first) {
                case HttpMethod::post: target.Post(pattern, wrapped); break;
                case HttpMethod::put: target.Put(pattern, wrapped); break;
                case HttpMethod::patch: target.Patch(pattern, wrapped); break;
                case HttpMethod::del: target.Delete(pattern, wrapped); break;
                case HttpMethod::get:
                case HttpMethod::options:
                    break;
            }
        }
    }

    RestMetrics& metrics;
    std::unique_ptr<httplib::Server> server;
    std::thread listenThread;
    std::future<void> finished;
};

CppHttplibTransport::CppHttplibTransport(RestMetrics& metrics)
    : implementation(std::make_unique<Implementation>(metrics)) {}

CppHttplibTransport::~CppHttplibTransport() = default;

void CppHttplibTransport::addRoute(HttpMethod method, const std::string& pattern, RouteHandler handler) {
    implementation->routes.push_back({method, {pattern, std::move(handler)}});
}

void CppHttplibTransport::addUploadRoute(HttpMethod method, const std::string& pattern, UploadHandler handler) {
    implementation->uploadRoutes.push_back({method, {pattern, std::move(handler)}});
}

void CppHttplibTransport::setPreRouteFilter(PreRouteFilter filter) {
    implementation->preRouteFilter = std::move(filter);
}

void CppHttplibTransport::setErrorResponder(ErrorResponder responder) {
    implementation->errorResponder = std::move(responder);
}

TransportStartResult CppHttplibTransport::start(const TransportConfig& config) {
    return implementation->start(config);
}

bool CppHttplibTransport::stop(int timeoutMilliseconds) {
    return implementation->stop(timeoutMilliseconds);
}

const char* CppHttplibTransport::tlsBackend() const {
#if defined(CPPHTTPLIB_OPENSSL_SUPPORT)
    return "openssl";
#elif defined(CPPHTTPLIB_MBEDTLS_SUPPORT)
    return "mbedtls";
#else
    return "none";
#endif
}

} // namespace gigante
