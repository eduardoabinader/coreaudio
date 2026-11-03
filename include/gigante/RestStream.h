#pragma once

#include "gigante/RestTypes.h"

#include <memory>
#include <string>

namespace gigante {

class StreamHub;

// A declared stream. Publishing never blocks on subscribers and is safe from any
// non-realtime thread. It may outlive the server: tryPublish then returns closed.
class RestStream final {
public:
    RestStream(std::shared_ptr<StreamHub> hub, std::string id);

    RestStream(const RestStream&) = delete;
    RestStream& operator=(const RestStream&) = delete;

    // On the SSE lane, event names are letters, digits, '-' and '_', and "gap" and
    // "end" are reserved. The binary lane ignores the event name.
    RestPublishResult tryPublish(const std::string& data, const std::string& event = "");

    // Ends every subscription with an end event; later publishing returns closed.
    void close();

    const std::string& id() const;

private:
    std::shared_ptr<StreamHub> hub;
    std::string streamId;
};

} // namespace gigante
