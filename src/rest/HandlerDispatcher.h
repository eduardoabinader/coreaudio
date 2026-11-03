#pragma once

#include "RestMetrics.h"

#include "gigante/RestTypes.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

namespace gigante {

// Hands a handler to the project's dispatcher while the server thread waits with a
// deadline. A dispatcher adds safe handoff, not capacity: the server thread stays held.
class DispatchGate final : public std::enable_shared_from_this<DispatchGate> {
public:
    RestResponse run(RestDispatcher& dispatcher, std::function<RestResponse()> work, int deadlineMilliseconds, RestMetrics& metrics) {
        struct Call {
            std::mutex mutex;
            std::condition_variable finished;
            bool done = false;
            RestResponse response;
        };
        auto call = std::make_shared<Call>();
        auto gate = shared_from_this();
        std::function<void()> task = [call, gate, handlerWork = std::move(work)] {
            if (!gate->enter()) {
                return;
            }
            RestResponse response = handlerWork();
            gate->leave();
            std::lock_guard<std::mutex> lock(call->mutex);
            call->response = std::move(response);
            call->done = true;
            call->finished.notify_all();
        };
        bool posted = false;
        try {
            posted = dispatcher.tryPost(std::move(task));
        } catch (const std::exception&) {
            posted = false;
        } catch (...) {
            posted = false;
        }
        if (!posted) {
            metrics.dispatcherFull.fetch_add(1);
            return RestResponse::error(503, "dispatcherFull", "The server is busy. Try again.", true);
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(deadlineMilliseconds);
        std::unique_lock<std::mutex> lock(call->mutex);
        while (!call->done) {
            if (isClosed()) {
                return RestResponse::error(503, "serverStopping", "The server is stopping.", true);
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                metrics.dispatcherDeadlines.fetch_add(1);
                return RestResponse::error(504, "handlerDeadline", "The handler did not finish in time.", true);
            }
            call->finished.wait_for(lock, std::chrono::milliseconds(20));
        }
        return std::move(call->response);
    }

    void open() {
        std::lock_guard<std::mutex> lock(mutex);
        closed = false;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex);
        closed = true;
    }

    bool waitUntilIdle(std::chrono::steady_clock::time_point deadline) {
        std::unique_lock<std::mutex> lock(mutex);
        const auto noneRunning = [this] { return running == 0; };
        if (deadline == std::chrono::steady_clock::time_point::max()) {
            idle.wait(lock, noneRunning);
            return true;
        }
        return idle.wait_until(lock, deadline, noneRunning);
    }

private:
    bool enter() {
        std::lock_guard<std::mutex> lock(mutex);
        if (closed) {
            return false;
        }
        ++running;
        return true;
    }

    void leave() {
        std::lock_guard<std::mutex> lock(mutex);
        --running;
        idle.notify_all();
    }

    bool isClosed() {
        std::lock_guard<std::mutex> lock(mutex);
        return closed;
    }

    std::mutex mutex;
    std::condition_variable idle;
    std::size_t running = 0;
    bool closed = false;
};

} // namespace gigante
