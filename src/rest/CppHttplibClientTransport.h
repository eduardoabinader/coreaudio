#pragma once

#include "HttpClientTransport.h"

#include <memory>

namespace gigante {

class CppHttplibClientTransport final : public HttpClientTransport {
public:
    explicit CppHttplibClientTransport(const ClientTransportConfig& config);
    ~CppHttplibClientTransport() override;

    ClientResponse request(
        HttpMethod method,
        const std::string& path,
        const std::string& body,
        const std::string& contentType,
        const std::map<std::string, std::string>& headers) override;
    ClientResponse upload(
        const std::string& path,
        std::uint64_t length,
        const std::string& contentType,
        const std::map<std::string, std::string>& headers,
        ClientChunkSource source) override;
    ClientResponse download(
        const std::string& path,
        std::uint64_t rangeStart,
        ClientChunkSink sink,
        ClientProgress progress) override;
    ClientResponse streamGet(
        const std::string& path,
        const std::map<std::string, std::string>& headers,
        ClientResponseStart onStart,
        ClientChunkSink sink) override;
    int subscribeEvents(const std::string& path, const EventStreamOptions& options, ClientEventHandler handler) override;
    void cancel() override;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation;
};

} // namespace gigante
