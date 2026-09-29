#include "gigante/EndpointPolicy.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

namespace gigante
{
namespace
{

bool containsBus(const std::vector<AudioBus>& buses, const AudioBus bus)
{
    return std::find(buses.begin(), buses.end(), bus) != buses.end();
}

bool supportsMinimumSampleRate(const AudioEndpoint& endpoint,
                               const double minimumSampleRate)
{
    return std::any_of(
        endpoint.sampleRates.begin(),
        endpoint.sampleRates.end(),
        [minimumSampleRate](const double sampleRate)
        {
            return std::isfinite(sampleRate)
                && sampleRate > 0.0
                && sampleRate >= minimumSampleRate;
        });
}

bool isExplicitlyRequested(const EndpointSelectionRequest& request,
                           const std::string& endpointId)
{
    return request.requestedEndpointId.has_value()
        && *request.requestedEndpointId == endpointId;
}

void reject(EndpointDecision& decision, const EndpointRejectionReason reason)
{
    decision.disposition = EndpointDisposition::rejected;
    decision.rejectionReasons.push_back(reason);
}

} // namespace

EndpointSelectionResult evaluateEndpointSelection(
    const EndpointInventory& inventory,
    const EndpointRolePolicy& policy,
    const EndpointSelectionRequest& request)
{
    EndpointSelectionResult result;
    result.inventoryGeneration = inventory.generation;
    result.requestedEndpointId = request.requestedEndpointId;

    if (!std::isfinite(policy.minimumSampleRate)
        || policy.minimumSampleRate < 0.0)
    {
        result.status = EndpointSelectionStatus::invalidPolicy;
        return result;
    }

    result.decisions.reserve(inventory.endpoints.size());

    std::unordered_map<std::string, std::size_t> identityCounts;
    identityCounts.reserve(inventory.endpoints.size());
    for (const auto& endpoint : inventory.endpoints)
    {
        if (!endpoint.endpointId.empty())
            ++identityCounts[endpoint.endpointId];
    }

    std::size_t eligibleCount = 0;
    bool requestedIdentityPresent = false;
    bool requestedIdentityEligible = false;

    for (std::size_t index = 0; index < inventory.endpoints.size(); ++index)
    {
        const auto& endpoint = inventory.endpoints[index];
        EndpointDecision decision;
        decision.inventoryIndex = index;
        decision.endpointId = endpoint.endpointId;
        decision.disposition = EndpointDisposition::eligible;

        const bool explicitlyRequested =
            isExplicitlyRequested(request, endpoint.endpointId);
        requestedIdentityPresent = requestedIdentityPresent || explicitlyRequested;

        if (endpoint.endpointId.empty())
            reject(decision, EndpointRejectionReason::invalidIdentity);
        else if (identityCounts[endpoint.endpointId] > 1U)
            reject(decision, EndpointRejectionReason::duplicateIdentity);

        if (endpoint.direction != policy.requiredDirection)
            reject(decision, EndpointRejectionReason::directionMismatch);

        if (endpoint.bus == AudioBus::unknown)
        {
            if (!explicitlyRequested)
            {
                reject(decision,
                       EndpointRejectionReason::unknownBusRequiresExplicitRequest);
            }
        }
        else if (!containsBus(policy.allowedBuses, endpoint.bus))
        {
            reject(decision, EndpointRejectionReason::busNotAllowed);
        }

        if (endpoint.channelCount < policy.minimumChannelCount)
            reject(decision, EndpointRejectionReason::insufficientChannels);

        if (!supportsMinimumSampleRate(endpoint, policy.minimumSampleRate))
            reject(decision, EndpointRejectionReason::insufficientSampleRate);

        const bool policyEligible = decision.rejectionReasons.empty();
        if (policyEligible)
            ++eligibleCount;

        if (request.requestedEndpointId.has_value())
        {
            if (explicitlyRequested && policyEligible)
            {
                decision.disposition = EndpointDisposition::selected;
                requestedIdentityEligible = true;
            }
            else if (!explicitlyRequested && policyEligible)
            {
                reject(decision, EndpointRejectionReason::notRequested);
            }
        }

        result.decisions.push_back(std::move(decision));
    }

    if (request.requestedEndpointId.has_value())
    {
        if (!requestedIdentityPresent)
        {
            result.status = EndpointSelectionStatus::requestedEndpointMissing;
        }
        else if (!requestedIdentityEligible)
        {
            result.status = EndpointSelectionStatus::requestedEndpointRejected;
        }
        else
        {
            result.status = EndpointSelectionStatus::selected;
            result.effectiveEndpointId = request.requestedEndpointId;
        }
        return result;
    }

    if (eligibleCount == 0U)
        result.status = EndpointSelectionStatus::noEligibleEndpoints;
    else if (eligibleCount == 1U)
        result.status = EndpointSelectionStatus::oneEligibleEndpoint;
    else
        result.status = EndpointSelectionStatus::multipleEligibleEndpoints;

    return result;
}

const char* endpointDirectionName(const EndpointDirection direction) noexcept
{
    switch (direction)
    {
        case EndpointDirection::input:
            return "input";
        case EndpointDirection::output:
            return "output";
    }
    return "unknown";
}

const char* audioBusName(const AudioBus bus) noexcept
{
    switch (bus)
    {
        case AudioBus::builtIn:
            return "builtIn";
        case AudioBus::usb:
            return "usb";
        case AudioBus::other:
            return "other";
        case AudioBus::unknown:
            return "unknown";
    }
    return "unknown";
}

const char* endpointRejectionReasonName(
    const EndpointRejectionReason reason) noexcept
{
    switch (reason)
    {
        case EndpointRejectionReason::invalidIdentity:
            return "invalidIdentity";
        case EndpointRejectionReason::duplicateIdentity:
            return "duplicateIdentity";
        case EndpointRejectionReason::directionMismatch:
            return "directionMismatch";
        case EndpointRejectionReason::busNotAllowed:
            return "busNotAllowed";
        case EndpointRejectionReason::unknownBusRequiresExplicitRequest:
            return "unknownBusRequiresExplicitRequest";
        case EndpointRejectionReason::insufficientChannels:
            return "insufficientChannels";
        case EndpointRejectionReason::insufficientSampleRate:
            return "insufficientSampleRate";
        case EndpointRejectionReason::notRequested:
            return "notRequested";
    }
    return "unknown";
}

const char* endpointSelectionStatusName(
    const EndpointSelectionStatus status) noexcept
{
    switch (status)
    {
        case EndpointSelectionStatus::invalidPolicy:
            return "invalidPolicy";
        case EndpointSelectionStatus::noEligibleEndpoints:
            return "noEligibleEndpoints";
        case EndpointSelectionStatus::oneEligibleEndpoint:
            return "oneEligibleEndpoint";
        case EndpointSelectionStatus::multipleEligibleEndpoints:
            return "multipleEligibleEndpoints";
        case EndpointSelectionStatus::requestedEndpointMissing:
            return "requestedEndpointMissing";
        case EndpointSelectionStatus::requestedEndpointRejected:
            return "requestedEndpointRejected";
        case EndpointSelectionStatus::selected:
            return "selected";
    }
    return "unknown";
}

} // namespace gigante
