#pragma once

#include <memory>

namespace gigante {

class StreamSubscription;

// Owns one stream subscription. Destroying it cancels and joins: onClosed runs once,
// and no callback runs after the destructor returns. Must not run inside a callback.
class RestSubscription final {
public:
    explicit RestSubscription(std::unique_ptr<StreamSubscription> subscription);
    ~RestSubscription();

    RestSubscription(const RestSubscription&) = delete;
    RestSubscription& operator=(const RestSubscription&) = delete;

    void cancel();
    bool isActive() const;

private:
    std::unique_ptr<StreamSubscription> subscription;
};

} // namespace gigante
