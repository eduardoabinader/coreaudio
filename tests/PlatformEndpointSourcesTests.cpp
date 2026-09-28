#include "gigante/PlatformEndpointSources.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
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

void testStatusNames()
{
    expect(std::string{gigante::platformEndpointSourceStatusName(
               gigante::PlatformEndpointSourceStatus::available)}
               == "available",
           "available status must have a stable name");
    expect(std::string{gigante::platformEndpointSourceStatusName(
               gigante::PlatformEndpointSourceStatus::backendUnavailable)}
               == "backendUnavailable",
           "backendUnavailable status must have a stable name");
    expect(std::string{gigante::platformEndpointSourceStatusName(
               gigante::PlatformEndpointSourceStatus::platformNotImplemented)}
               == "platformNotImplemented",
           "platformNotImplemented status must have a stable name");
    expect(std::string{gigante::platformEndpointSourceStatusName(
               gigante::PlatformEndpointSourceStatus::initializationFailed)}
               == "initializationFailed",
           "initializationFailed status must have a stable name");
}

void testFactoryResult()
{
    auto sources = gigante::createPlatformEndpointSources();
#if defined(GIGANTE_TEST_HAS_PLATFORM_ENDPOINT_SOURCES)
    expect(sources.available(),
           "platform build with JUCE must expose both endpoint sources");
    expect(sources.status
               == gigante::PlatformEndpointSourceStatus::available,
           "platform build with JUCE must report available status");
    expect(!sources.context.empty(),
           "available sources must describe their concrete backend");
    if (!sources.available())
        return;

    const auto inventory = sources.factSource->snapshot();
    expect(inventory.generation > 0U,
           "first platform snapshot must have a positive generation");
    std::set<std::string> identities;
    for (const auto& endpoint : inventory.endpoints)
    {
        expect(!endpoint.endpointId.empty(),
               "each endpoint must have a non-empty identity");
        expect(identities.insert(endpoint.endpointId).second,
               "endpoint identities must be unique within a snapshot");
        expect(!endpoint.deviceName.empty(),
               "each endpoint must retain its JUCE device name");
        if (endpoint.platformId)
        {
            expect(!endpoint.platformId->empty(),
                   "present platform ids must not be empty");
        }
        if (endpoint.productName)
        {
            expect(!endpoint.productName->empty(),
                   "present product names must not be empty");
        }
#if defined(GIGANTE_TEST_LINUX_ENDPOINT_SOURCE)
        expect(!endpoint.platformId.has_value(),
               "Linux must not recreate JUCE enumeration to infer a platform id");
        expect(!endpoint.productName.has_value(),
               "Linux must not recreate JUCE enumeration to infer a product name");
        expect(endpoint.bus == gigante::AudioBus::unknown,
               "Linux bus must remain unknown when JUCE does not expose it");
#endif
        expect(std::is_sorted(endpoint.sampleRates.begin(),
                              endpoint.sampleRates.end()),
               "sample rates must be sorted");
        expect(std::adjacent_find(endpoint.sampleRates.begin(),
                                  endpoint.sampleRates.end())
                   == endpoint.sampleRates.end(),
               "sample rates must not contain duplicates");
        for (const double rate : endpoint.sampleRates)
        {
            expect(std::isfinite(rate) && rate > 0.0,
                   "sample rates must be finite and positive");
        }
    }

    const auto startResult = sources.changeSource->start([] {});
    expect(startResult.status != gigante::EndpointChangeStartStatus::failed,
           "platform change source must start or preserve a specific "
           "unavailable/permission cause");
    expect(!startResult.context.empty(),
           "platform change-source result must retain context");
    sources.changeSource->stop();
    sources.changeSource->stop();
    std::cout << "Observed " << inventory.endpoints.size()
              << " platform endpoint(s)\n";
#else
    expect(!sources.available(),
           "JUCE-free build must not report platform sources as available");
    expect(sources.status
               == gigante::PlatformEndpointSourceStatus::backendUnavailable,
           "JUCE-free build must preserve the missing-backend cause");
    expect(!sources.context.empty(),
           "missing backend must retain diagnostic context");
    expect(sources.factSource == nullptr,
           "missing backend must not expose a fact source");
    expect(sources.changeSource == nullptr,
           "missing backend must not expose a change source");
#endif
}

} // namespace

int main()
{
    testStatusNames();
    testFactoryResult();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " platform endpoint source test(s) failed\n";
        return 1;
    }

    std::cout << "Platform endpoint source tests passed\n";
    return 0;
}
