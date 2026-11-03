#pragma once

#include "HttpTransport.h"
#include "RestMetrics.h"

#include <memory>

namespace gigante {

class CppHttplibTransport final : public HttpTransport {
public:
    explicit CppHttplibTransport(RestMetrics& metrics);
    ~CppHttplibTransport() override;

    void addRoute(HttpMethod method, const std::string& pattern, RouteHandler handler) override;
    void addUploadRoute(HttpMethod method, const std::string& pattern, UploadHandler handler) override;
    void setPreRouteFilter(PreRouteFilter filter) override;
    void setErrorResponder(ErrorResponder responder) override;
    TransportStartResult start(const TransportConfig& config) override;
    bool stop(int timeoutMilliseconds) override;
    const char* tlsBackend() const override;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation;
};

} // namespace gigante
