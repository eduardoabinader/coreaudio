#include "RestTestSupport.h"

#include <atomic>
#include <string>
#include <vector>

using namespace gigante;
using namespace restTest;

namespace {

std::string certs;

RestServerConfig productionServer() {
    RestServerConfig config(SecurityProfile::production);
    config.limits.baseThreads = 8;
    config.limits.maxThreads = 16;
    config.tls.certificatePath = certs + "/server.pem";
    config.tls.privateKeyPath = certs + "/server.key";
    return config;
}

RestClientConfig productionClient(const std::string& url) {
    RestClientConfig config(SecurityProfile::production);
    config.baseUrl = url;
    config.caCertificatePath = certs + "/ca.pem";
    return config;
}

void allowAll(RestServer& server) {
    server.setAuthenticator([](const RestRequest&) { return true; });
}

int claimFreePort() {
    RestServer probe(restTest::developmentServer());
    const int port = probe.start().port;
    probe.stop();
    return port;
}

void serverRefusals() {
    {
        RestServerConfig config = productionServer();
        config.scheme = RestScheme::http;
        RestServer server(config);
        allowAll(server);
        const RestStartResult result = server.start();
        check(!result.started && contains(result.cause, "requires HTTPS"), "productionRefusesHttp", result.cause);
    }
    {
        RestServerConfig config = productionServer();
        config.tls = RestTlsFiles{};
        RestServer server(config);
        allowAll(server);
        const RestStartResult result = server.start();
        check(!result.started && contains(result.cause, "certificate"), "productionNeedsCertificate", result.cause);
    }
    {
        RestServer server(productionServer());
        const RestStartResult result = server.start();
        check(!result.started && contains(result.cause, "authenticator"), "productionNeedsAuthenticator", result.cause);
    }
    {
        RestServerConfig config = productionServer();
        config.tls.certificatePath = certs + "/garbage.pem";
        config.port = claimFreePort();
        RestServer server(config);
        allowAll(server);
        const RestStartResult result = server.start();
        check(!result.started, "badCertificateRefused", result.cause);
        check(server.state() != RestServerState::running, "badCertificateNotRunning");
        RestClient plain(restTest::developmentClient(config.port));
        RestCallOptions quick;
        quick.deadlineMilliseconds = 1000;
        check(plain.call(RestMethod::get, "/core/v1/health", "", "application/json", quick).status == 0, "badCertificateNoHttpFallback");
    }
    {
        RestServerConfig config = restTest::developmentServer();
        config.host = "0.0.0.0";
        RestServer server(config);
        const RestStartResult result = server.start();
        check(!result.started && contains(result.cause, "allowNonLoopbackPlaintext"), "wildcardPlaintextNeedsOptIn", result.cause);
    }
    {
        RestServerConfig config = restTest::developmentServer();
        config.host = "";
        RestServer server(config);
        check(!server.start().started, "emptyHostRefused");
    }
    {
        RestServerConfig config = restTest::developmentServer();
        config.host = "0.0.0.0";
        config.allowNonLoopbackPlaintext = true;
        std::vector<std::string> warnings;
        config.log = [&warnings](RestLogLevel level, const std::string& event, const std::string&) {
            if (level == RestLogLevel::warn) {
                warnings.push_back(event);
            }
        };
        RestServer server(config);
        check(server.start().started && warnings.size() == 1, "optedInPlaintextWarns");
    }
}

void clientRefusals() {
    RestClient plain(productionClient("http://localhost:1"));
    check(contains(plain.configurationError(), "https"), "productionClientRefusesHttp", plain.configurationError());
    const RestResult refused = plain.heartbeat();
    check(refused.error == RestClientError::refusedByPolicy && refused.attempts == 0, "refusedClientSendsNothing");

    RestClientConfig unverified = productionClient("https://localhost:1");
    unverified.verifyPeer = false;
    check(!RestClient(unverified).configurationError().empty(), "productionClientRefusesNoVerification");

    RestClientConfig noName = productionClient("https://localhost:1");
    noName.verifyHostname = false;
    check(!RestClient(noName).configurationError().empty(), "productionClientRefusesNoHostnameCheck");

    RestClientConfig badUrl = productionClient("ftp://localhost");
    check(!RestClient(badUrl).configurationError().empty(), "unknownSchemeRefused");
}

void productionServerBehaviour() {
    RestServer server(productionServer());
    server.setAuthenticator([](const RestRequest& request) { return request.header("Authorization").value_or("") == "Bearer good"; });
    const RestStartResult started = server.start();
    check(started.started && started.scheme == "https", "productionUsesHttps", started.cause);
    const std::string url = "https://localhost:" + std::to_string(started.port);
    RestClient client(productionClient(url));
    check(client.health().status == 200, "healthOverTls");
    check(client.status().status == 401, "statusNeedsCredentials");
    RestCallOptions authorized;
    authorized.idempotent = true;
    authorized.headers["Authorization"] = "Bearer good";
    const RestResult status = client.call(RestMethod::get, "/core/v1/status", "", "application/json", authorized);
    check(status.status == 200 && contains(status.body, "\"effective\":{\"scheme\":\"https\""), "statusWithCredentials", status.body);

    RestClient wrongHost(productionClient("https://127.0.0.1:" + std::to_string(started.port)));
    check(wrongHost.health().error == RestClientError::hostnameVerification, "wrongHostRejected");
    RestClientConfig untrustedConfig = productionClient(url);
    untrustedConfig.caCertificatePath = certs + "/otherCa.pem";
    RestClient untrusted(untrustedConfig);
    check(untrusted.health().error == RestClientError::tlsVerification, "untrustedCaRejected");
    RestClient plainToTls(restTest::developmentClient(started.port));
    RestCallOptions once;
    once.maxAttempts = 1;
    check(plainToTls.call(RestMethod::get, "/core/v1/health", "", "application/json", once).status != 200, "plainClientGetsNothing");
    check(server.stop(), "productionStops");
}

void mutualTls() {
    RestServerConfig config = productionServer();
    config.tls.clientCaPath = certs + "/ca.pem";
    RestServer server(config);
    allowAll(server);
    const RestStartResult started = server.start();
    const std::string url = "https://localhost:" + std::to_string(started.port);
    RestCallOptions once;
    once.maxAttempts = 1;
    RestClient without(productionClient(url));
    check(without.call(RestMethod::get, "/core/v1/health", "", "application/json", once).status != 200, "mutualTlsNeedsCertificate");
    RestClientConfig wrong = productionClient(url);
    wrong.clientCertificatePath = certs + "/otherClient.pem";
    wrong.clientPrivateKeyPath = certs + "/otherClient.key";
    RestClient wrongClient(wrong);
    check(wrongClient.call(RestMethod::get, "/core/v1/health", "", "application/json", once).status != 200, "mutualTlsRefusesUntrusted");
    RestClientConfig good = productionClient(url);
    good.clientCertificatePath = certs + "/client.pem";
    good.clientPrivateKeyPath = certs + "/client.key";
    RestClient goodClient(good);
    check(goodClient.health().status == 200, "mutualTlsAcceptsTrusted");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }
    certs = argv[1];
    serverRefusals();
    clientRefusals();
    productionServerBehaviour();
    mutualTls();
    return finish("RestSecurityTests");
}
