#include "gigante/HostFacts.h"
#include "gigante/Revision.h"

#include <iostream>
#include <string>

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

void testCurrentPlatformFacts()
{
    gigante::RealtimeSchedulingOutcome scheduling;
    scheduling.state = gigante::RealtimeSchedulingState::applied;
    scheduling.requestedPolicy = "fifo";
    scheduling.requestedPriority = 7;
    scheduling.observedPolicy = "fifo";
    scheduling.observedPriority = 7;
    scheduling.detail = "test supplied outcome";

    const auto source = gigante::createHostFactsSource();
    expect(source != nullptr, "the selected platform source must be created");
    if (source == nullptr)
        return;

    const auto facts = source->read(scheduling);
    expect(facts.platformName == GIGANTE_EXPECTED_PLATFORM_NAME,
           "CMake must link the expected concrete platform source");
    expect(facts.logicalCoreCount.has_value()
               && *facts.logicalCoreCount > 0U,
           "logical core count must be available and positive");
    expect(facts.physicalMemoryBytes.has_value()
               && *facts.physicalMemoryBytes > 0U,
           "physical memory must be available and positive");
    expect(facts.architecture.has_value() && !facts.architecture->empty(),
           "architecture must be available");
    expect(facts.buildFingerprint.has_value()
               && !facts.buildFingerprint->empty(),
           "platform build fingerprint must be available");
    expect(facts.sourceRevision == gigante::revision,
           "source revision must use explicit build identity");

    expect(facts.realtimeScheduling.state == scheduling.state,
           "scheduling state must be preserved");
    expect(facts.realtimeScheduling.requestedPolicy
               == scheduling.requestedPolicy,
           "requested scheduling policy must be preserved");
    expect(facts.realtimeScheduling.requestedPriority
               == scheduling.requestedPriority,
           "requested scheduling priority must be preserved");
    expect(facts.realtimeScheduling.observedPolicy
               == scheduling.observedPolicy,
           "observed scheduling policy must be preserved");
    expect(facts.realtimeScheduling.observedPriority
               == scheduling.observedPriority,
           "observed scheduling priority must be preserved");
    expect(facts.realtimeScheduling.detail == scheduling.detail,
           "scheduling detail must be preserved");

    const auto description = gigante::formatHostFacts(facts);
    expect(description.find(facts.platformName + "/") == 0U,
           "formatted identity must start with the platform name");
    expect(description.find("/cores=") != std::string::npos,
           "formatted identity must contain core count");
    expect(description.find("/ram=") != std::string::npos,
           "formatted identity must contain physical memory");
    expect(description.find("/build=" + facts.sourceRevision)
               != std::string::npos,
           "formatted identity must contain source revision");
}

void testLifecycleIsExplicitAndRepeatable()
{
    const auto first = gigante::createHostFactsSource();
    const auto second = gigante::createHostFactsSource();
    expect(first != nullptr && second != nullptr,
           "repeated source creation must succeed");
    expect(first.get() != second.get(),
           "source creation must not return a hidden singleton");
    if (first == nullptr || second == nullptr)
        return;

    const gigante::RealtimeSchedulingOutcome notRequested;
    const auto firstFacts = first->read(notRequested);
    const auto secondFacts = second->read(notRequested);
    expect(firstFacts.platformName == secondFacts.platformName,
           "repeated reads must keep platform name");
    expect(firstFacts.architecture == secondFacts.architecture,
           "repeated reads must keep architecture");
    expect(firstFacts.sourceRevision == secondFacts.sourceRevision,
           "repeated reads must keep source revision");
    expect(firstFacts.realtimeScheduling.state
               == gigante::RealtimeSchedulingState::notRequested,
           "default scheduling outcome must remain explicitly not requested");
}

void testUnavailableFactsRemainExplicit()
{
    gigante::HostFacts facts;
    facts.platformName = "Linux";
    facts.sourceRevision = "test-revision";

    const auto description = gigante::formatHostFacts(facts);
    expect(!facts.logicalCoreCount.has_value(),
           "missing core count must not receive a fallback");
    expect(!facts.physicalMemoryBytes.has_value(),
           "missing physical memory must not receive a fallback");
    expect(!facts.architecture.has_value(),
           "missing architecture must not receive a fallback");
    expect(!facts.buildFingerprint.has_value(),
           "missing build fingerprint must not receive a fallback");
    expect(description
               == "Linux/unavailable/cores=unavailable/ram=unavailable"
                  "/build=test-revision",
           "formatted missing facts must remain explicitly unavailable");
}

void testNamesAreStable()
{
    expect(std::string{gigante::realtimeSchedulingStateName(
               gigante::RealtimeSchedulingState::notRequested)}
               == "notRequested",
           "not-requested scheduling name must be stable");
    expect(std::string{gigante::realtimeSchedulingStateName(
               gigante::RealtimeSchedulingState::applied)} == "applied",
           "applied scheduling name must be stable");
    expect(std::string{gigante::realtimeSchedulingStateName(
               gigante::RealtimeSchedulingState::rejected)} == "rejected",
           "rejected scheduling name must be stable");
    expect(std::string{gigante::realtimeSchedulingStateName(
               gigante::RealtimeSchedulingState::unavailable)}
               == "unavailable",
           "unavailable scheduling name must be stable");
}

} // namespace

int main()
{
    testCurrentPlatformFacts();
    testLifecycleIsExplicitAndRepeatable();
    testUnavailableFactsRemainExplicit();
    testNamesAreStable();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " host-facts test(s) failed\n";
        return 1;
    }

    std::cout << "all host-facts tests passed\n";
    return 0;
}
