#include "FakeEndpointFactSource.h"

#include "gigante/EndpointPolicy.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

int failureCount = 0;

void expect(const bool condition, const char* const message)
{
    if (condition)
        return;

    ++failureCount;
    std::cerr << "FAIL: " << message << '\n';
}

gigante::AudioEndpointFacts makeEndpoint(
    std::string endpointId,
    const gigante::EndpointDirection direction,
    const gigante::EndpointBus bus,
    const std::uint32_t channelCount = 2U,
    std::vector<double> sampleRates = {44100.0, 48000.0})
{
    gigante::AudioEndpointFacts endpoint;
    endpoint.endpointId = std::move(endpointId);
    endpoint.platformId = "platform-device-1";
    endpoint.deviceName = "JUCE Device";
    endpoint.productName = "Product Name";
    endpoint.direction = direction;
    endpoint.bus = bus;
    endpoint.channelCount = channelCount;
    endpoint.sampleRates = std::move(sampleRates);
    return endpoint;
}

gigante::EndpointRolePolicy usbOutputPolicy()
{
    gigante::EndpointRolePolicy policy;
    policy.requiredDirection = gigante::EndpointDirection::output;
    policy.allowedBuses = {gigante::EndpointBus::usb};
    policy.minimumChannelCount = 2U;
    policy.minimumSampleRate = 48000.0;
    return policy;
}

gigante::EndpointSelectionRequest requestEndpoint(std::string endpointId)
{
    gigante::EndpointSelectionRequest request;
    request.requestedEndpointId = std::move(endpointId);
    return request;
}

bool hasReason(const gigante::EndpointDecision& decision,
               const gigante::EndpointRejectionReason reason)
{
    return std::find(decision.rejectionReasons.begin(),
                     decision.rejectionReasons.end(),
                     reason) != decision.rejectionReasons.end();
}

void testZeroOneAndSeveralCandidates()
{
    FakeEndpointFactSource source;
    const auto policy = usbOutputPolicy();
    const gigante::EndpointSelectionRequest noRequest;

    const auto zero = gigante::evaluateEndpointSelection(
        source.snapshot(), policy, noRequest);
    expect(zero.status
               == gigante::EndpointSelectionStatus::noEligibleEndpoints,
           "empty inventory must report zero eligible endpoints");
    expect(!zero.effectiveEndpointId.has_value(),
           "empty inventory must have no effective endpoint");

    source.replace({makeEndpoint("usb-a",
                                 gigante::EndpointDirection::output,
                                 gigante::EndpointBus::usb)});
    const auto one = gigante::evaluateEndpointSelection(
        source.snapshot(), policy, noRequest);
    expect(one.status
               == gigante::EndpointSelectionStatus::oneEligibleEndpoint,
           "one candidate must be reported explicitly");
    expect(!one.effectiveEndpointId.has_value(),
           "one candidate must not be selected implicitly");
    expect(one.decisions.size() == 1U
               && one.decisions[0].disposition
                   == gigante::EndpointDisposition::eligible,
           "unrequested single candidate must remain merely eligible");

    source.replace({makeEndpoint("usb-a",
                                 gigante::EndpointDirection::output,
                                 gigante::EndpointBus::usb),
                    makeEndpoint("usb-b",
                                 gigante::EndpointDirection::output,
                                 gigante::EndpointBus::usb)});
    const auto several = gigante::evaluateEndpointSelection(
        source.snapshot(), policy, noRequest);
    expect(several.status
               == gigante::EndpointSelectionStatus::multipleEligibleEndpoints,
           "several candidates must be reported explicitly");
    expect(!several.effectiveEndpointId.has_value(),
           "several candidates must not produce an implicit selection");

    const auto selected = gigante::evaluateEndpointSelection(
        source.snapshot(), policy, requestEndpoint("usb-b"));
    expect(selected.status == gigante::EndpointSelectionStatus::selected,
           "an eligible explicit request must be selected");
    expect(selected.requestedEndpointId == std::optional<std::string>{"usb-b"},
           "requested identity must be retained");
    expect(selected.effectiveEndpointId == std::optional<std::string>{"usb-b"},
           "effective identity must be distinct and explicit");
    expect(selected.decisions[0].disposition
               == gigante::EndpointDisposition::rejected
               && hasReason(selected.decisions[0],
                            gigante::EndpointRejectionReason::notRequested),
           "eligible unrequested candidate must retain its rejection reason");
    expect(selected.decisions[1].disposition
               == gigante::EndpointDisposition::selected,
           "requested candidate must be marked selected");
}

void testEveryEligibilityReasonIsRetained()
{
    auto endpoint = makeEndpoint("bad",
                                 gigante::EndpointDirection::input,
                                 gigante::EndpointBus::builtIn,
                                 1U,
                                 {44100.0});
    const gigante::EndpointInventory inventory{4U, {std::move(endpoint)}};
    const auto result = gigante::evaluateEndpointSelection(
        inventory, usbOutputPolicy(), {});

    expect(result.status
               == gigante::EndpointSelectionStatus::noEligibleEndpoints,
           "fully incompatible endpoint must not be eligible");
    expect(result.decisions.size() == 1U,
           "incompatible endpoint must retain one decision record");
    if (result.decisions.empty())
        return;

    const auto& decision = result.decisions[0];
    expect(hasReason(decision,
                     gigante::EndpointRejectionReason::directionMismatch),
           "direction mismatch reason must be retained");
    expect(hasReason(decision,
                     gigante::EndpointRejectionReason::busNotAllowed),
           "bus rejection reason must be retained");
    expect(hasReason(decision,
                     gigante::EndpointRejectionReason::insufficientChannels),
           "channel rejection reason must be retained");
    expect(hasReason(decision,
                     gigante::EndpointRejectionReason::insufficientSampleRate),
           "sample-rate rejection reason must be retained");
}

void testUnknownBusNeedsAnExplicitRequest()
{
    const gigante::EndpointInventory inventory{
        1U,
        {makeEndpoint("unknown-a",
                      gigante::EndpointDirection::output,
                      gigante::EndpointBus::unknown)}};
    const auto policy = usbOutputPolicy();

    const auto implicit = gigante::evaluateEndpointSelection(
        inventory, policy, {});
    expect(implicit.status
               == gigante::EndpointSelectionStatus::noEligibleEndpoints,
           "unknown bus must be ineligible without an explicit request");
    expect(hasReason(
               implicit.decisions[0],
               gigante::EndpointRejectionReason::unknownBusRequiresExplicitRequest),
           "unknown bus rejection must explain how it can be selected");

    const auto explicitResult = gigante::evaluateEndpointSelection(
        inventory, policy, requestEndpoint("unknown-a"));
    expect(explicitResult.status
               == gigante::EndpointSelectionStatus::selected,
           "explicit request must permit an otherwise valid unknown bus");
    expect(explicitResult.effectiveEndpointId
               == std::optional<std::string>{"unknown-a"},
           "explicit unknown-bus selection must be observable");
}

void testDuplicateIdentityRejectsEveryDuplicate()
{
    const gigante::EndpointInventory inventory{
        9U,
        {makeEndpoint("duplicate",
                      gigante::EndpointDirection::output,
                      gigante::EndpointBus::usb),
         makeEndpoint("duplicate",
                      gigante::EndpointDirection::output,
                      gigante::EndpointBus::usb)}};

    const auto result = gigante::evaluateEndpointSelection(
        inventory, usbOutputPolicy(), requestEndpoint("duplicate"));
    expect(result.status
               == gigante::EndpointSelectionStatus::requestedEndpointRejected,
           "ambiguous requested identity must be rejected");
    expect(!result.effectiveEndpointId.has_value(),
           "duplicate identity must never become effective");
    expect(result.decisions.size() == 2U,
           "both duplicate entries must retain decisions");
    for (const auto& decision : result.decisions)
    {
        expect(hasReason(decision,
                         gigante::EndpointRejectionReason::duplicateIdentity),
               "every duplicate must carry duplicateIdentity");
    }
}

void testLossNeverSubstitutesAnotherEndpoint()
{
    FakeEndpointFactSource source;
    const auto policy = usbOutputPolicy();
    source.replace({makeEndpoint("usb-a",
                                 gigante::EndpointDirection::output,
                                 gigante::EndpointBus::usb)});
    const auto first = gigante::evaluateEndpointSelection(
        source.snapshot(), policy, requestEndpoint("usb-a"));
    expect(first.status == gigante::EndpointSelectionStatus::selected,
           "initial requested endpoint must be selected");

    source.replace({makeEndpoint("usb-b",
                                 gigante::EndpointDirection::output,
                                 gigante::EndpointBus::usb)});
    const auto afterLoss = gigante::evaluateEndpointSelection(
        source.snapshot(), policy, requestEndpoint("usb-a"));
    expect(afterLoss.inventoryGeneration == 2U,
           "replacement must advance observable inventory generation");
    expect(afterLoss.status
               == gigante::EndpointSelectionStatus::requestedEndpointMissing,
           "loss must report the requested endpoint missing");
    expect(!afterLoss.effectiveEndpointId.has_value(),
           "loss must not silently substitute another endpoint");
    expect(afterLoss.decisions.size() == 1U
               && hasReason(afterLoss.decisions[0],
                            gigante::EndpointRejectionReason::notRequested),
           "replacement candidate must remain rejected until requested");

    const auto replacement = gigante::evaluateEndpointSelection(
        source.snapshot(), policy, requestEndpoint("usb-b"));
    expect(replacement.status == gigante::EndpointSelectionStatus::selected,
           "replacement must require a new explicit request");
}

void testDuplexDirectionsKeepSeparateIdentity()
{
    auto input = makeEndpoint("duplex-input",
                              gigante::EndpointDirection::input,
                              gigante::EndpointBus::usb);
    auto output = makeEndpoint("duplex-output",
                               gigante::EndpointDirection::output,
                               gigante::EndpointBus::usb);
    input.platformId = "same-duplex-device";
    output.platformId = "same-duplex-device";
    const gigante::EndpointInventory inventory{
        3U, {std::move(input), std::move(output)}};

    const auto result = gigante::evaluateEndpointSelection(
        inventory, usbOutputPolicy(), requestEndpoint("duplex-output"));
    expect(result.status == gigante::EndpointSelectionStatus::selected,
           "requested duplex output must be selected independently");
    expect(result.effectiveEndpointId
               == std::optional<std::string>{"duplex-output"},
           "duplex output identity must not collapse into platform id");
    expect(hasReason(result.decisions[0],
                     gigante::EndpointRejectionReason::directionMismatch),
           "duplex input must remain a direction-mismatched endpoint");
}

void testInvalidIdentityAndMissingRequest()
{
    const gigante::EndpointInventory inventory{
        5U,
        {makeEndpoint("",
                      gigante::EndpointDirection::output,
                      gigante::EndpointBus::usb),
         makeEndpoint("usb-a",
                      gigante::EndpointDirection::output,
                      gigante::EndpointBus::usb)}};
    const auto result = gigante::evaluateEndpointSelection(
        inventory, usbOutputPolicy(), requestEndpoint("missing"));
    expect(result.status
               == gigante::EndpointSelectionStatus::requestedEndpointMissing,
           "absent requested identity must be reported explicitly");
    expect(hasReason(result.decisions[0],
                     gigante::EndpointRejectionReason::invalidIdentity),
           "empty endpoint identity must be rejected specifically");
    expect(hasReason(result.decisions[1],
                     gigante::EndpointRejectionReason::notRequested),
           "valid alternative must not become an implicit fallback");
}

void testStableNames()
{
    expect(std::string{gigante::endpointDirectionName(
               gigante::EndpointDirection::input)} == "input",
           "input direction name must be stable");
    expect(std::string{gigante::endpointBusName(
               gigante::EndpointBus::builtIn)} == "builtIn",
           "built-in bus name must be stable");
    expect(std::string{gigante::endpointRejectionReasonName(
               gigante::EndpointRejectionReason::duplicateIdentity)}
               == "duplicateIdentity",
           "duplicate reason name must be stable");
    expect(std::string{gigante::endpointSelectionStatusName(
               gigante::EndpointSelectionStatus::requestedEndpointMissing)}
               == "requestedEndpointMissing",
           "missing-request status name must be stable");
}

void testInvalidPolicyIsTerminalAndSpecific()
{
    auto policy = usbOutputPolicy();
    policy.minimumSampleRate = std::numeric_limits<double>::quiet_NaN();
    const gigante::EndpointInventory inventory{
        6U,
        {makeEndpoint("usb-a",
                      gigante::EndpointDirection::output,
                      gigante::EndpointBus::usb)}};
    const auto request = requestEndpoint("usb-a");
    const auto result = gigante::evaluateEndpointSelection(
        inventory, policy, request);

    expect(result.status == gigante::EndpointSelectionStatus::invalidPolicy,
           "non-finite policy rate must report invalidPolicy");
    expect(result.requestedEndpointId == request.requestedEndpointId,
           "invalid policy result must retain requested configuration");
    expect(!result.effectiveEndpointId.has_value(),
           "invalid policy must not produce effective configuration");
    expect(result.decisions.empty(),
           "invalid policy must stop before evaluating endpoint facts");
}

} // namespace

int main()
{
    testZeroOneAndSeveralCandidates();
    testEveryEligibilityReasonIsRetained();
    testUnknownBusNeedsAnExplicitRequest();
    testDuplicateIdentityRejectsEveryDuplicate();
    testLossNeverSubstitutesAnotherEndpoint();
    testDuplexDirectionsKeepSeparateIdentity();
    testInvalidIdentityAndMissingRequest();
    testStableNames();
    testInvalidPolicyIsTerminalAndSpecific();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " endpoint-policy test(s) failed\n";
        return 1;
    }

    std::cout << "all endpoint-policy tests passed\n";
    return 0;
}
