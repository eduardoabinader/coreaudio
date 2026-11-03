#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace gigante {

enum class SecurityProfile { development, production };
enum class RestScheme { automatic, http, https };
enum class RestMethod { get, post, put, patch, del, options };
enum class RestLogLevel { error, warn, info };

using RestLogSink = std::function<void(RestLogLevel level, const std::string& event, const std::string& detail)>;

struct RestTlsFiles {
    std::string certificatePath;
    std::string privateKeyPath;
    std::string clientCaPath;
};

// One thread stays held per open connection, so maxThreads bounds open connections.
struct RestServerLimits {
    std::size_t baseThreads = 64;
    std::size_t maxThreads = 100;
    std::size_t maxQueuedConnections = 128;
    std::size_t maxBodyBytes = 1024 * 1024;
    int readTimeoutMilliseconds = 2000;
    int writeTimeoutMilliseconds = 2000;
    int keepAliveTimeoutMilliseconds = 1000;
    std::size_t keepAliveMaxCount = 100;
    int dispatcherDeadlineMilliseconds = 5000;

    // Each open stream or transfer holds a server thread. start() refuses limits where
    // the enabled caps plus reservedThreads exceed maxThreads.
    std::size_t maxStreams = 32;
    std::size_t maxTransfers = 16;
    std::size_t reservedThreads = 8;
    std::uint64_t maxTransferBytes = 4ULL * 1024 * 1024 * 1024;
    int uploadBodyDeadlineMilliseconds = 600000;
    int transferResumeWindowMilliseconds = 60000;
    std::uint64_t maxDownloadBytesPerSecond = 0;
};

struct RestServerConfig {
    explicit RestServerConfig(SecurityProfile securityProfile) : profile(securityProfile) {}

    SecurityProfile profile;
    RestScheme scheme = RestScheme::automatic;
    std::string host = "127.0.0.1";
    int port = 0;
    bool allowNonLoopbackPlaintext = false;
    RestTlsFiles tls;
    RestServerLimits limits;
    // Fixed when the project's binary is built; getIdentity and heartbeat report it.
    // The engine never builds anything at run time.
    std::string buildIdentity;
    std::string corsAllowOrigin;
    RestLogSink log;
};

struct RestClientConfig {
    explicit RestClientConfig(SecurityProfile securityProfile) : profile(securityProfile) {}

    SecurityProfile profile;
    std::string baseUrl;
    std::string caCertificatePath;
    std::string clientCertificatePath;
    std::string clientPrivateKeyPath;
    bool verifyPeer = true;
    bool verifyHostname = true;
    int connectTimeoutMilliseconds = 2000;
    int readTimeoutMilliseconds = 5000;
    int writeTimeoutMilliseconds = 5000;
    std::size_t maxConcurrentAsyncCalls = 4;
    std::size_t maxQueuedAsyncCalls = 32;
    RestLogSink log;
};

struct RestRequest {
    RestMethod method = RestMethod::get;
    std::string path;
    std::map<std::string, std::string> pathParameters;
    std::multimap<std::string, std::string> query;
    std::multimap<std::string, std::string> headers;
    std::string body;
    std::string requestId;
    std::string correlationId;

    std::string pathParameter(const std::string& name) const;
    std::optional<std::string> queryParameter(const std::string& name) const;
    std::optional<std::string> header(const std::string& name) const;
};

struct RestResponse {
    int status = 200;
    std::string contentType = "application/json";
    std::string body;
    std::multimap<std::string, std::string> headers;
    std::string errorCode;
    std::string errorMessage;
    bool retryable = false;

    static RestResponse json(std::string body, int status = 200);
    static RestResponse text(std::string body, int status = 200);

    // The server turns this into a versioned error body carrying the request ID.
    // The message is sent to clients, so it must not hold paths or internal details.
    static RestResponse error(int status, std::string code, std::string message, bool retryable = false);
};

enum class RestAuth { required, none };

struct RestRouteOptions {
    RestAuth auth = RestAuth::required;
    bool idempotent = false;
    std::size_t maxBodyBytes = 0;
};

// Handlers run concurrently on server threads unless a dispatcher is injected.
using RestHandler = std::function<RestResponse(const RestRequest&)>;

// Runs before the request body is read, so request.body is empty here.
using RestAuthenticator = std::function<bool(const RestRequest&)>;

// Hands handlers to a project thread, such as a Qt or JUCE message thread. It must
// not run on an audio thread. tryPost must not block; false means the queue is full.
class RestDispatcher {
public:
    virtual ~RestDispatcher() = default;
    virtual bool tryPost(std::function<void()> task) = 0;
};

enum class RestRouteResult { added, reservedPrefix, duplicate, invalidPattern, alreadyStarted };

enum class RestServerState { created, running, draining, stopped };

struct RestStartResult {
    bool started = false;
    std::string cause;
    int port = 0;
    std::string scheme;
};

// Each callback returns whether the action succeeded, and runs after the 202 is
// sent, on the server's service worker thread. An empty callback is not offered.
struct RestServiceCallbacks {
    std::function<bool()> start;
    std::function<bool()> stop;
    std::function<bool()> restart;
};

struct RestServerCounters {
    std::uint64_t requests = 0;
    std::uint64_t authenticationFailures = 0;
    std::uint64_t dispatcherFull = 0;
    std::uint64_t dispatcherDeadlines = 0;
    std::uint64_t handlerExceptions = 0;
    std::uint64_t resourceExhaustion = 0;
    std::uint64_t transportErrors = 0;
    std::uint64_t rejectedStreams = 0;
    std::uint64_t rejectedTransfers = 0;
    std::uint64_t droppedChunks = 0;
    std::uint64_t refusedChunks = 0;
    std::uint64_t evictedSubscribers = 0;
};

enum class RestDeliveryPolicy { reliable, realtime, broadcast };
enum class RestStreamLane { sse, binary };

// reliable never drops silently, realtime drops the oldest data and counts it, and
// broadcast evicts only a slow subscriber. The SSE lane assigns increasing event IDs;
// a reconnect sends a gap event with the number missed, and nothing is replayed.
struct RestStreamDescriptor {
    std::string id;
    RestStreamLane lane = RestStreamLane::sse;
    std::string contentType;
    std::string format;
    std::string formatVersion;
    RestDeliveryPolicy policy = RestDeliveryPolicy::realtime;
    std::size_t maxQueuedBytesPerSubscriber = 256 * 1024;
    int heartbeatMilliseconds = 15000;
};

enum class RestPublishStatus { delivered, refused, closed, invalidEvent };

struct RestPublishResult {
    RestPublishStatus status = RestPublishStatus::delivered;
    std::size_t subscribers = 0;
    std::size_t dropped = 0;
    std::size_t evicted = 0;
    std::uint64_t eventId = 0;
};

struct RestResourceInfo {
    std::string id;
    std::uint64_t size = 0;
    std::string sha256;
    std::string contentType = "application/octet-stream";
    bool readable = true;
    bool writable = false;
};

struct RestLogQueryLimits {
    std::size_t maxLines = 1000;
    std::size_t maxBytes = 1024 * 1024;
    std::size_t maxFiles = 64;
    int maxDepth = 4;
};

struct RestCommandRecord {
    std::string name;
    bool alive = false;
    std::int64_t processId = -1;
    std::optional<int> exitCode;
    std::string detail;
};

struct RestLogMarker {
    std::string command;
    std::string origin;
    std::string event;
    std::string text;
};

struct RestStreamEvent {
    std::string event;
    std::string data;
    std::string id;
};

enum class RestSubscriptionEnd { canceled, streamEnded, attemptsExhausted, rejected, refusedByPolicy };

struct RestSubscriptionOptions {
    int minimumReconnectMilliseconds = 1000;
    int maximumAttempts = 5;
    int idleTimeoutMilliseconds = 30000;
    std::string lastEventId;
    bool binary = false;
    std::map<std::string, std::string> headers;
};

// Supplies exactly `length` bytes starting at `offset`; false stops the upload.
using RestUploadSource = std::function<bool(std::uint64_t offset, char* buffer, std::size_t length)>;
// Receives downloaded bytes in order; false stops the download.
using RestDownloadSink = std::function<bool(const char* data, std::size_t size)>;

enum class RestClientError { none, refusedByPolicy, connection, tlsVerification, hostnameVerification, tlsHandshake, canceled, deadline, other };

struct RestCallOptions {
    bool idempotent = false;
    int deadlineMilliseconds = 10000;
    int maxAttempts = 3;
    std::map<std::string, std::string> headers;
};

struct RestResult {
    RestClientError error = RestClientError::none;
    int status = 0;
    std::string body;
    std::string errorText;
    int attempts = 0;
    std::uint64_t bytesTransferred = 0;
    std::string sha256;
};

} // namespace gigante
