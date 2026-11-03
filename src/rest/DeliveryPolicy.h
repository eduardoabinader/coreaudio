#pragma once

namespace gigante {

// reliable never drops silently; realtime drops the oldest data and counts it;
// broadcast shares chunks between subscribers and evicts only a slow subscriber.
enum class DeliveryPolicy { reliable, realtime, broadcast };

} // namespace gigante
