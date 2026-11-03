#pragma once

#include "gigante/RestTypes.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace gigante {

enum class ServiceAction { start, stop, restart };
enum class ServiceSubmitResult { accepted, unknownService, notOffered, queueFull, closed };

// Runs a project's service callbacks one at a time on its own worker thread, so they
// never hold a connection's thread, and records each outcome for status.
class ServiceRegistry final {
public:
    ServiceRegistry() = default;
    ~ServiceRegistry();

    bool add(const std::string& name, RestServiceCallbacks callbacks);
    bool isEmpty() const;

    ServiceSubmitResult check(const std::string& name, ServiceAction action) const;
    ServiceSubmitResult submit(const std::string& name, ServiceAction action);
    std::string statusJson() const;

    void open();

    // Stops taking work. Returns false if a callback is still running at the deadline.
    bool close(std::chrono::steady_clock::time_point deadline);

private:
    struct Service {
        RestServiceCallbacks callbacks;
        std::string state = "idle";
        std::string lastResult = "none";
    };
    struct Work {
        std::string name;
        ServiceAction action;
    };

    void run();

    mutable std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable idle;
    std::map<std::string, Service> services;
    std::deque<Work> queue;
    std::thread worker;
    bool accepting = false;
    bool busy = false;
    bool workerExit = false;
};

} // namespace gigante
