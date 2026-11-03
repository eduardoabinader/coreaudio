#include "IosOutputRoute.h"
#include <iostream>
#include <stdexcept>

namespace
{
using namespace gigante;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
AudioLink port(std::string id, AudioTransport transport, std::string type)
{
    AudioLink link;
    link.endpoint.endpointId = id;
    link.endpoint.platformId = id;
    link.endpoint.deviceName = id;
    link.endpoint.direction = EndpointDirection::output;
    link.transport = transport;
    link.nativeTransport = type;
    link.availability = AudioLinkAvailability::available;
    link.selected = true;
    return link;
}
void scenarios()
{
    std::optional<AudioLink> known;
    auto bt = port("headphone-native-uid", AudioTransport::bluetoothA2dp, "BluetoothA2DPOutput");
    auto route = observeIosOutputs({bt}, known);
    check(route.links().size() == 1 && !known && route.inventoryStatus() == OutputInventoryStatus::partial,
          "Bluetooth cold start must not manufacture speaker identity or complete inventory");
    auto speaker = port("speaker-native-uid", AudioTransport::builtIn, "Speaker");
    route = observeIosOutputs({speaker}, known);
    check(route.links().size() == 1 && known && route.links()[0].selected,
          "native speaker observation learns exact identity without duplicate row");
    route = observeIosOutputs({bt}, known);
    check(route.links().size() == 2 && route.links()[0].endpoint.platformId == "speaker-native-uid"
          && !route.links()[0].selected && route.links()[0].availability == AudioLinkAvailability::unknown
          && route.links()[1].selected, "inactive known speaker is retained as unselected with unknown routability");
    route = observeIosOutputs({speaker}, known);
    check(route.links().size() == 1 && route.links()[0].selected,
          "disconnect removes external Bluetooth rather than accumulating it in known outputs");
    auto receiver = port("receiver-native-uid", AudioTransport::builtIn, "Receiver");
    std::optional<AudioLink> noSpeaker;
    route = observeIosOutputs({receiver}, noSpeaker);
    check(!noSpeaker && route.links().size() == 1, "receiver is not a speaker capability");
    auto fake = bt;
    fake.endpoint.deviceName = "Speaker";
    route = observeIosOutputs({fake}, noSpeaker);
    check(!noSpeaker && route.links().size() == 1, "display names cannot establish speaker hardware");
    route = observeIosOutputs({speaker, speaker}, noSpeaker);
    check(route.status() == OutputRouteStatus::failed && !noSpeaker, "invalid native observation must not populate known outputs");
    route = observeIosOutputs({}, known);
    check(route.status() == OutputRouteStatus::unavailable && route.links().empty()
          && route.inventoryStatus() == OutputInventoryStatus::unknown, "empty route must not fabricate active speaker");
    route = observeIosOutputs({speaker, bt}, known);
    check(route.links().size() == 2 && route.links()[0].selected && route.links()[1].selected,
          "multiple actual outputs retain their observed selections");
}
}
int main()
{
    try { scenarios(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    return 0;
}
