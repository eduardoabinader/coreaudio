#include "CppHttplibClientTransport.h"
#include "CppHttplibTransport.h"
#include "ProbeReport.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

using namespace gigante;

namespace {

std::string certs;

std::string certPath(const std::string& name) {
    return certs + "/" + name;
}

TransportConfig serverConfig(const std::string& name, const std::string& clientCa = "") {
    TransportConfig config;
    config.useTls = true;
    config.tls.certificatePath = certPath(name + ".pem");
    config.tls.privateKeyPath = certPath(name + ".key");
    config.tls.clientCaPath = clientCa;
    return config;
}

std::unique_ptr<CppHttplibTransport> makeServer(RestMetrics& metrics) {
    auto server = std::make_unique<CppHttplibTransport>(metrics);
    server->addRoute(HttpMethod::get, "/health", [](const TransportRequest&) {
        TransportResponse response;
        response.body = "{\"status\":\"ok\"}";
        return response;
    });
    server->addRoute(HttpMethod::get, "/redirect", [](const TransportRequest&) {
        TransportResponse response;
        response.status = 302;
        response.headers.emplace("Location", "http://127.0.0.1:9/plain");
        return response;
    });
    return server;
}

ClientResponse fetch(const std::string& url, const std::string& ca, const std::string& cert = "", const std::string& key = "", const std::string& path = "/health") {
    ClientTransportConfig config;
    config.baseUrl = url;
    config.caCertificatePath = ca;
    config.clientCertificatePath = cert;
    config.clientPrivateKeyPath = key;
    CppHttplibClientTransport client(config);
    return client.request(HttpMethod::get, path, "", "", {});
}

int unusedPort() {
    const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(descriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    socklen_t size = sizeof(address);
    getsockname(descriptor, reinterpret_cast<sockaddr*>(&address), &size);
    const int port = ntohs(address.sin_port);
    close(descriptor);
    return port;
}

bool portAcceptsConnections(int port) {
    const int descriptor = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const bool connected = connect(descriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
    close(descriptor);
    return connected;
}

std::string describe(const ClientResponse& response) {
    return "status=" + std::to_string(response.status) + " error=" + response.errorText;
}

int serve(const std::string& name, bool mutualTls) {
    RestMetrics metrics;
    auto server = makeServer(metrics);
    const TransportStartResult started = server->start(serverConfig(name, mutualTls ? certPath("ca.pem") : ""));
    if (!started.started) {
        std::cerr << "start failed: " << started.cause << "\n";
        return 1;
    }
    std::cout << "PORT " << started.boundPort << std::endl;
    std::string line;
    std::getline(std::cin, line);
    server->stop(5000);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: tlsProbe certsDirectory [serve|serveMutual]\n";
        return 2;
    }
    certs = argv[1];
    if (argc > 2) {
        return serve("server", std::string(argv[2]) == "serveMutual");
    }

    ProbeReport report("tls");
    RestMetrics metrics;
    report.note("backend", CppHttplibTransport(metrics).tlsBackend());

    for (const auto& badInput : {std::make_pair(std::string("garbage.pem"), std::string("server.key")),
                                 std::make_pair(std::string("server.pem"), std::string("garbage.pem")),
                                 std::make_pair(std::string("server.pem"), std::string("otherCa.key")),
                                 std::make_pair(std::string("missing.pem"), std::string("missing.key"))}) {
        auto server = makeServer(metrics);
        TransportConfig config = serverConfig("server");
        config.tls.certificatePath = certPath(badInput.first);
        config.tls.privateKeyPath = certPath(badInput.second);
        config.port = unusedPort();
        const TransportStartResult started = server->start(config);
        report.check(
            "badCredentialsRefused:" + badInput.first + "+" + badInput.second,
            !started.started && !portAcceptsConnections(config.port),
            started.cause);
    }

    auto server = makeServer(metrics);
    const TransportStartResult started = server->start(serverConfig("server"));
    report.check("validServerStarts", started.started, started.cause);
    const std::string port = std::to_string(started.boundPort);
    const std::string localhostUrl = "https://localhost:" + port;

    const ClientResponse trusted = fetch(localhostUrl, certPath("ca.pem"));
    report.check("trustedCaAccepted", trusted.error == ClientError::none && trusted.status == 200, describe(trusted));

    const ClientResponse ipLiteral = fetch("https://127.0.0.1:" + port, certPath("ca.pem"));
    report.check("ipLiteralNotInCertificateRejected", ipLiteral.status == 0 && ipLiteral.error != ClientError::none, describe(ipLiteral));

    const ClientResponse untrusted = fetch(localhostUrl, certPath("otherCa.pem"));
    report.check("untrustedCaRejected", untrusted.status == 0 && untrusted.error != ClientError::none, describe(untrusted));

    const ClientResponse redirect = fetch(localhostUrl, certPath("ca.pem"), "", "", "/redirect");
    report.check("redirectToPlainHttpNotFollowed", redirect.status == 302, describe(redirect));

    const ClientResponse plainClient = fetch("http://localhost:" + port, "");
    report.check("plainClientGetsNoData", plainClient.status != 200, describe(plainClient));
    server->stop(5000);

    auto expiredServer = makeServer(metrics);
    const TransportStartResult expiredStarted = expiredServer->start(serverConfig("expired"));
    report.note("expiredCertificateAcceptedAtServerStart", expiredStarted.started ? "yes" : "no");
    if (expiredStarted.started) {
        const ClientResponse expired = fetch("https://localhost:" + std::to_string(expiredStarted.boundPort), certPath("ca.pem"));
        report.check("expiredCertificateRejectedByClient", expired.status == 0, describe(expired));
        expiredServer->stop(5000);
    }

    auto plainServer = makeServer(metrics);
    TransportConfig plainConfig;
    const TransportStartResult plainStarted = plainServer->start(plainConfig);
    const ClientResponse tlsToPlain = fetch("https://localhost:" + std::to_string(plainStarted.boundPort), certPath("ca.pem"));
    report.check("tlsClientRefusesPlainServer", tlsToPlain.status == 0, describe(tlsToPlain));
    plainServer->stop(5000);

    auto mutualServer = makeServer(metrics);
    const TransportStartResult mutualStarted = mutualServer->start(serverConfig("server", certPath("ca.pem")));
    const std::string mutualUrl = "https://localhost:" + std::to_string(mutualStarted.boundPort);
    const ClientResponse noClientCert = fetch(mutualUrl, certPath("ca.pem"));
    report.check("mutualTlsRefusesNoCertificate", noClientCert.status != 200, describe(noClientCert));
    const ClientResponse wrongClientCert = fetch(mutualUrl, certPath("ca.pem"), certPath("otherClient.pem"), certPath("otherClient.key"));
    report.check("mutualTlsRefusesUntrustedCertificate", wrongClientCert.status != 200, describe(wrongClientCert));
    const ClientResponse goodClientCert = fetch(mutualUrl, certPath("ca.pem"), certPath("client.pem"), certPath("client.key"));
    report.check("mutualTlsAcceptsTrustedCertificate", goodClientCert.status == 200, describe(goodClientCert));
    mutualServer->stop(5000);

    auto sixServer = makeServer(metrics);
    TransportConfig sixConfig = serverConfig("server");
    sixConfig.host = "::1";
    const TransportStartResult sixStarted = sixServer->start(sixConfig);
    report.check("ipv6Binds", sixStarted.started, sixStarted.cause);
    if (sixStarted.started) {
        ClientTransportConfig sixClient;
        sixClient.baseUrl = "https://[::1]:" + std::to_string(sixStarted.boundPort);
        sixClient.caCertificatePath = certPath("ca.pem");
        sixClient.verifyHostname = false;
        CppHttplibClientTransport client(sixClient);
        const ClientResponse response = client.request(HttpMethod::get, "/health", "", "", {});
        report.check("ipv6Serves", response.status == 200, describe(response));
        sixServer->stop(5000);
    }
    return report.finish();
}
