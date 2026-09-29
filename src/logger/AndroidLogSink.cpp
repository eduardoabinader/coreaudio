#include "PlatformLogSink.h"

#include <spdlog/sinks/android_sink.h>

namespace gigante
{

std::shared_ptr<spdlog::sinks::sink> createPlatformLogSink(
    const LogConfiguration& configuration)
{
    return std::make_shared<spdlog::sinks::android_sink_mt>(configuration.name, true);
}

} // namespace gigante
