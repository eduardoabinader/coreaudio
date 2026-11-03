#include "gigante/OutputRoute.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

namespace
{
int failures = 0;
void expect(const bool condition, const char* message)
{
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
gigante::AudioLink output(std::string id, const bool selected)
{
    gigante::AudioLink link;
    link.endpoint.endpointId = std::move(id);
    link.endpoint.direction = gigante::EndpointDirection::output;
    link.endpoint.bus = gigante::AudioBus::usb;
    link.transport = gigante::AudioTransport::usb;
    link.availability = gigante::AudioLinkAvailability::available;
    link.selected = selected;
    return link;
}
}

int main()
{
    using namespace gigante;
    OutputRoute route;
    expect(route.status() == OutputRouteStatus::unavailable && route.links().empty(),
        "default route is not an observed output");
    route.setRequestedEndpointId("usb-old");
    auto first = output("speaker", true);
    expect(route.updateLinks(1, {first, output("usb-old", false)}, OutputRouteBasis::effective)
        == OutputRouteUpdate::applied, "publish initial complete effective links");
    first.endpoint.endpointId = "caller-mutated";
    expect(route.links().front().endpoint.endpointId == "speaker", "route owns link values");
    expect(route.links().front().selected, "request does not select an unobserved output");
    route.setRequestedEndpointId("usb-new");
    expect(route.generation() == 1 && route.links().front().selected,
        "changing intent does not change current observation");
    expect(route.updateLinks(2, {output("usb-new", true)}, OutputRouteBasis::effective)
        == OutputRouteUpdate::applied, "replacement update succeeds");
    expect(route.links().size() == 1 && route.links().front().endpoint.endpointId == "usb-new",
        "removed identities are not accumulated on replacement");
    expect(route.updateLinks(1, {output("usb-old", true)}, OutputRouteBasis::effective)
        == OutputRouteUpdate::staleGeneration, "late callback cannot restore old links");
    expect(route.links().front().endpoint.endpointId == "usb-new", "stale update leaves current route intact");

    expect(route.markFailed(3, "native route query denied") == OutputRouteUpdate::applied,
        "failed native observation is published");
    expect(route.status() == OutputRouteStatus::failed && route.context() == "native route query denied",
        "specific query failure is retained");
    expect(!route.links().front().selected
        && route.links().front().availability == AudioLinkAvailability::unknown,
        "historical identities cannot retain active/current flags after failure");
    expect(route.markUnavailable(2, "old error") == OutputRouteUpdate::staleGeneration,
        "old failure cannot replace a newer observation");
    expect(route.context() == "native route query denied", "old error does not overwrite current cause");
    expect(route.updateLinks(4, {output("speaker", true)}, OutputRouteBasis::effective)
        == OutputRouteUpdate::applied, "valid later observation recovers");
    expect(route.status() == OutputRouteStatus::available && route.context().empty(),
        "recovery clears only the superseded query error");
    expect(route.requestedEndpointId() == "usb-new", "observation preserves distinct requested intent");

    for (const auto basis : {OutputRouteBasis::inventory, OutputRouteBasis::requested, OutputRouteBasis::unknown})
    {
        OutputRoute observation;
        expect(observation.updateLinks(1, {output("speaker", true)}, basis) == OutputRouteUpdate::applied,
            "non-effective observations remain representable");
        expect(!observation.links().front().selected, "inventory/request cannot assert effective selection");
    }
    for (int invalid = 0; invalid < 3; ++invalid)
    {
        OutputRoute invalidRoute;
        expect(invalidRoute.updateLinks(1, {output("old", true)}, OutputRouteBasis::effective)
            == OutputRouteUpdate::applied, "seed prior valid route");
        auto link = output("replacement", true);
        if (invalid == 0) link.endpoint.endpointId.clear();
        if (invalid == 1) link.endpoint.direction = EndpointDirection::input;
        std::vector<AudioLink> links{link};
        if (invalid == 2) links.push_back(link);
        expect(invalidRoute.updateLinks(2, links, OutputRouteBasis::effective)
            == OutputRouteUpdate::invalidObservation, "invalid identities/directions are rejected");
        expect(invalidRoute.status() == OutputRouteStatus::failed
            && !invalidRoute.context().empty() && !invalidRoute.links().front().selected,
            "invalid new observation invalidates old selected links");
    }
    OutputRoute unavailable;
    expect(unavailable.markUnavailable(1, "no effective route") == OutputRouteUpdate::applied,
        "unavailable is distinct from query failure");
    expect(unavailable.status() == OutputRouteStatus::unavailable && unavailable.generation() == 1,
        "unavailable observation advances generation");
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    expect(unavailable.updateLinks(maximum, {}, OutputRouteBasis::effective) == OutputRouteUpdate::applied,
        "empty effective observation is representable, not inferred safe");
    expect(unavailable.markFailed(0, "wrapped") == OutputRouteUpdate::staleGeneration,
        "generation wrap cannot supersede the last observation");
    return failures == 0 ? 0 : 1;
}
