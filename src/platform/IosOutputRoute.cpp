#include "IosOutputRoute.h"
#include <algorithm>
#include <utility>

namespace gigante
{
namespace
{
bool isObservedSpeaker(const AudioLink& link)
{
    return link.transport == AudioTransport::builtIn && link.nativeTransport == "Speaker"
        && link.endpoint.direction == EndpointDirection::output
        && !link.endpoint.endpointId.empty() && link.endpoint.platformId
        && !link.endpoint.platformId->empty() && link.selected
        && link.availability == AudioLinkAvailability::available;
}
}
OutputRoute observeIosOutputs(std::vector<AudioLink> currentOutputs,
    std::optional<AudioLink>& knownSpeaker)
{
    OutputRoute route;
    if (currentOutputs.empty())
    {
        (void) route.markUnavailable(1, "AVAudioSession currentRoute has no output ports");
        return route;
    }
    auto nextSpeaker = knownSpeaker;
    for (const auto& link : currentOutputs)
        if (isObservedSpeaker(link)) nextSpeaker = link;
    if (nextSpeaker && isObservedSpeaker(*nextSpeaker)
        && std::none_of(currentOutputs.begin(), currentOutputs.end(), [&](const AudioLink& link)
           { return link.endpoint.endpointId == nextSpeaker->endpoint.endpointId; }))
    {
        auto inactive = *nextSpeaker;
        inactive.selected = false;
        // Previously observed hardware is known; current routability is not.
        inactive.availability = AudioLinkAvailability::unknown;
        currentOutputs.insert(currentOutputs.begin(), std::move(inactive));
    }
    const auto result = route.updateLinks(1, std::move(currentOutputs), OutputRouteBasis::effective,
        OutputInventoryStatus::partial, nextSpeaker
            ? "iOS exposes the current route; only current outputs and a previously observed speaker are known"
            : "iOS exposes the current route; inactive outputs, including an unobserved speaker, are unknown");
    if (result == OutputRouteUpdate::applied) knownSpeaker = std::move(nextSpeaker);
    return route;
}
}
