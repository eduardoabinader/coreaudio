#include "ServiceRegistry.h"

#include "RestJson.h"

#include <exception>
#include <utility>

namespace gigante {

namespace {

const std::size_t maxQueuedWork = 32;

const std::function<bool()>& callbackFor(const RestServiceCallbacks& callbacks, ServiceAction action) {
    switch (action) {
        case ServiceAction::start: return callbacks.start;
        case ServiceAction::stop: return callbacks.stop;
        case ServiceAction::restart: return callbacks.restart;
    }
    return callbacks.start;
}

const char* runningState(ServiceAction action) {
    switch (action) {
        case ServiceAction::start: return "starting";
        case ServiceAction::stop: return "stopping";
        case ServiceAction::restart: return "restarting";
    }
    return "idle";
}

const char* finishedState(ServiceAction action) {
    return action == ServiceAction::stop ? "stopped" : "running";
}

} // namespace

ServiceRegistry::~ServiceRegistry() {
    close(std::chrono::steady_clock::time_point::max());
}

bool ServiceRegistry::add(const std::string& name, RestServiceCallbacks callbacks) {
    std::lock_guard<std::mutex> lock(mutex);
    return services.emplace(name, Service{std::move(callbacks), "idle", "none"}).second;
}

bool ServiceRegistry::isEmpty() const {
    std::lock_guard<std::mutex> lock(mutex);
    return services.empty();
}

ServiceSubmitResult ServiceRegistry::check(const std::string& name, ServiceAction action) const {
    std::lock_guard<std::mutex> lock(mutex);
    const auto service = services.find(name);
    if (service == services.end()) {
        return ServiceSubmitResult::unknownService;
    }
    if (!callbackFor(service->second.callbacks, action)) {
        return ServiceSubmitResult::notOffered;
    }
    if (!accepting) {
        return ServiceSubmitResult::closed;
    }
    return queue.size() >= maxQueuedWork ? ServiceSubmitResult::queueFull : ServiceSubmitResult::accepted;
}

ServiceSubmitResult ServiceRegistry::submit(const std::string& name, ServiceAction action) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto service = services.find(name);
    if (service == services.end()) {
        return ServiceSubmitResult::unknownService;
    }
    if (!callbackFor(service->second.callbacks, action)) {
        return ServiceSubmitResult::notOffered;
    }
    if (!accepting) {
        return ServiceSubmitResult::closed;
    }
    if (queue.size() >= maxQueuedWork) {
        return ServiceSubmitResult::queueFull;
    }
    queue.push_back({name, action});
    wake.notify_one();
    return ServiceSubmitResult::accepted;
}

std::string ServiceRegistry::statusJson() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::string result = "[";
    for (const auto& service : services) {
        if (result.size() > 1) {
            result += ",";
        }
        result += "{\"name\":" + jsonString(service.first) + ",\"state\":" + jsonString(service.second.state) +
                  ",\"lastResult\":" + jsonString(service.second.lastResult) + "}";
    }
    return result + "]";
}

void ServiceRegistry::open() {
    std::lock_guard<std::mutex> lock(mutex);
    accepting = true;
    workerExit = false;
    if (!worker.joinable()) {
        worker = std::thread([this] { run(); });
    }
}

bool ServiceRegistry::close(std::chrono::steady_clock::time_point deadline) {
    std::unique_lock<std::mutex> lock(mutex);
    accepting = false;
    const auto drained = [this] { return queue.empty() && !busy; };
    if (deadline == std::chrono::steady_clock::time_point::max()) {
        idle.wait(lock, drained);
    } else if (!idle.wait_until(lock, deadline, drained)) {
        return false;
    }
    workerExit = true;
    wake.notify_all();
    lock.unlock();
    if (worker.joinable()) {
        worker.join();
    }
    return true;
}

void ServiceRegistry::run() {
    std::unique_lock<std::mutex> lock(mutex);
    for (;;) {
        wake.wait(lock, [this] { return workerExit || !queue.empty(); });
        if (queue.empty()) {
            return;
        }
        const Work work = queue.front();
        queue.pop_front();
        busy = true;
        Service& service = services.at(work.name);
        service.state = runningState(work.action);
        const std::function<bool()> callback = callbackFor(service.callbacks, work.action);
        lock.unlock();
        bool succeeded = false;
        try {
            succeeded = callback();
        } catch (const std::exception&) {
            succeeded = false;
        } catch (...) {
            succeeded = false;
        }
        lock.lock();
        service.state = succeeded ? finishedState(work.action) : "failed";
        service.lastResult = succeeded ? "succeeded" : "failed";
        busy = false;
        idle.notify_all();
    }
}

} // namespace gigante
