#include "PlatformHostFactsSupport.h"
#include "PosixHostFactsSupport.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <sys/system_properties.h>

namespace gigante
{
namespace
{

std::optional<std::string> readAndroidBuildFingerprint()
{
    char value[PROP_VALUE_MAX]{};
    const int length = __system_property_get("ro.build.fingerprint", value);
    if (length <= 0)
        return std::nullopt;

    return std::string{value, static_cast<std::size_t>(length)};
}

HostFacts readAndroidHostFacts(
    const RealtimeSchedulingOutcome& realtimeScheduling)
{
    auto facts = platformDetail::readPosixHostFacts(
        "Android", realtimeScheduling);
    facts.buildFingerprint = readAndroidBuildFingerprint();

    // readPosixHostFacts()'s generic render-capability detection checks
    // for a Wayland/X11 display socket, neither of which exists on
    // Android (its own windowing system is SurfaceFlinger, reached
    // through android.view.Surface/ANativeWindow, not a POSIX display
    // socket) -- so it always reported no video capability here, which
    // was never actually true: every Android host has a real display.
    // Confirmed on real hardware (Pixel 9 Pro, gst-harness DEV_REQ
    // Phase 7b): a genuine GStreamer glimagesink pipeline renders visible
    // video via an android.view.Surface-backed ANativeWindow once given
    // one (see gstools/architecture.md "Phase 7b" for the full proof).
    if (std::find(facts.renderCapabilities.begin(), facts.renderCapabilities.end(),
            RenderCapability::video) == facts.renderCapabilities.end())
        facts.renderCapabilities.push_back(RenderCapability::video);

    return facts;
}

} // namespace

std::unique_ptr<HostFactsSource> createHostFactsSource()
{
    return platformDetail::createFunctionHostFactsSource(readAndroidHostFacts);
}

} // namespace gigante
