#pragma once

#include <gigante/Logger.h>

#include <memory>

namespace spdlog
{
namespace sinks
{
class sink;
}
}

namespace gigante
{

[[nodiscard]] std::shared_ptr<spdlog::sinks::sink> createPlatformLogSink(
    const LogConfiguration& configuration);

} // namespace gigante
