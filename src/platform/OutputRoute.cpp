#include "gigante/OutputRoute.h"

#include <set>
#include <utility>

namespace gigante
{
OutputRouteUpdate OutputRoute::updateLinks(const std::uint64_t generationIn,
    std::vector<AudioLink> linksIn, const OutputRouteBasis basisIn,
    const OutputInventoryStatus inventoryStatusIn, std::string inventoryContextIn)
{
    if (generationIn <= currentGeneration)
        return OutputRouteUpdate::staleGeneration;

    std::set<std::string> identities;
    for (const auto& link : linksIn)
    {
        if (link.endpoint.endpointId.empty()
            || link.endpoint.direction != EndpointDirection::output
            || !identities.insert(link.endpoint.endpointId).second)
        {
            invalidate(generationIn, OutputRouteStatus::failed,
                "output links require unique nonempty output endpoint identities");
            return OutputRouteUpdate::invalidObservation;
        }
    }
    switch (basisIn)
    {
        case OutputRouteBasis::unknown:
        case OutputRouteBasis::inventory:
        case OutputRouteBasis::requested:
        case OutputRouteBasis::effective:
            break;
        default:
            invalidate(generationIn, OutputRouteStatus::failed,
                "invalid output observation basis");
            return OutputRouteUpdate::invalidObservation;
    }
    if (basisIn != OutputRouteBasis::effective)
        for (auto& link : linksIn)
            link.selected = false;

    if (inventoryStatusIn != OutputInventoryStatus::unknown
        && inventoryStatusIn != OutputInventoryStatus::partial
        && inventoryStatusIn != OutputInventoryStatus::complete)
    {
        invalidate(generationIn, OutputRouteStatus::failed, "invalid output inventory status");
        return OutputRouteUpdate::invalidObservation;
    }
    currentInventoryStatus = inventoryStatusIn;
    currentInventoryContext = std::move(inventoryContextIn);
    currentLinks = std::move(linksIn);
    currentGeneration = generationIn;
    currentBasis = basisIn;
    currentStatus = OutputRouteStatus::available;
    currentContext.clear();
    return OutputRouteUpdate::applied;
}

void OutputRoute::invalidate(const std::uint64_t generationIn,
    const OutputRouteStatus statusIn, std::string contextIn)
{
    // Historical identities may remain for diagnostics; none remain current/active.
    for (auto& link : currentLinks)
    {
        link.selected = false;
        link.availability = AudioLinkAvailability::unknown;
    }
    currentGeneration = generationIn;
    currentStatus = statusIn;
    currentBasis = OutputRouteBasis::unknown;
    currentContext = std::move(contextIn);
    currentInventoryStatus = OutputInventoryStatus::unknown;
    currentInventoryContext = currentContext;
}

OutputRouteUpdate OutputRoute::markUnavailable(const std::uint64_t generationIn,
    std::string contextIn)
{
    if (generationIn <= currentGeneration)
        return OutputRouteUpdate::staleGeneration;
    invalidate(generationIn, OutputRouteStatus::unavailable, std::move(contextIn));
    return OutputRouteUpdate::applied;
}

OutputRouteUpdate OutputRoute::markFailed(const std::uint64_t generationIn,
    std::string contextIn)
{
    if (generationIn <= currentGeneration)
        return OutputRouteUpdate::staleGeneration;
    invalidate(generationIn, OutputRouteStatus::failed, std::move(contextIn));
    return OutputRouteUpdate::applied;
}

void OutputRoute::setRequestedEndpointId(std::optional<std::string> endpointId)
{
    requestedId = std::move(endpointId);
}

const std::vector<AudioLink>& OutputRoute::links() const noexcept { return currentLinks; }
OutputRouteStatus OutputRoute::status() const noexcept { return currentStatus; }
OutputRouteBasis OutputRoute::basis() const noexcept { return currentBasis; }
std::uint64_t OutputRoute::generation() const noexcept { return currentGeneration; }
const std::string& OutputRoute::context() const noexcept { return currentContext; }
const std::optional<std::string>& OutputRoute::requestedEndpointId() const noexcept { return requestedId; }
OutputInventoryStatus OutputRoute::inventoryStatus() const noexcept { return currentInventoryStatus; }
const std::string& OutputRoute::inventoryContext() const noexcept { return currentInventoryContext; }
} // namespace gigante
