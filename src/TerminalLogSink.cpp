#include "PlatformLogSink.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace gigante
{
namespace
{

[[nodiscard]] spdlog::color_mode toSpdColorMode(const LogColorMode mode) noexcept
{
    switch (mode)
    {
        case LogColorMode::automatic:
            return spdlog::color_mode::automatic;
        case LogColorMode::always:
            return spdlog::color_mode::always;
        case LogColorMode::never:
            return spdlog::color_mode::never;
    }

    return spdlog::color_mode::never;
}

} // namespace

std::shared_ptr<spdlog::sinks::sink> createPlatformLogSink(
    const LogConfiguration& configuration)
{
    return std::make_shared<spdlog::sinks::stderr_color_sink_mt>(
        toSpdColorMode(configuration.colorMode));
}

} // namespace gigante
