#include <gigante/Logger.h>

#include "PlatformLogSink.h"

#include <atomic>
#include <exception>
#include <stdexcept>
#include <utility>

#include <spdlog/logger.h>
#include <spdlog/sinks/callback_sink.h>

namespace gigante
{
namespace
{

[[nodiscard]] constexpr LogLevel levelFromCompiledValue() noexcept
{
#if GIGANTE_LOG_COMPILED_LEVEL == GIGANTE_LOG_LEVEL_OFF
    return LogLevel::off;
#elif GIGANTE_LOG_COMPILED_LEVEL == GIGANTE_LOG_LEVEL_ERROR
    return LogLevel::error;
#elif GIGANTE_LOG_COMPILED_LEVEL == GIGANTE_LOG_LEVEL_WARN
    return LogLevel::warn;
#elif GIGANTE_LOG_COMPILED_LEVEL == GIGANTE_LOG_LEVEL_INFO
    return LogLevel::info;
#elif GIGANTE_LOG_COMPILED_LEVEL == GIGANTE_LOG_LEVEL_DEBUG
    return LogLevel::debug;
#elif GIGANTE_LOG_COMPILED_LEVEL == GIGANTE_LOG_LEVEL_TRACE
    return LogLevel::trace;
#else
#error "Unsupported GIGANTE_LOG_COMPILED_LEVEL"
#endif
}

[[nodiscard]] constexpr LogLevel effectiveLevel(
    const LogLevel compiled,
    const LogLevel requested) noexcept
{
    return static_cast<LogLevel>(std::min(
        static_cast<unsigned int>(compiled),
        static_cast<unsigned int>(requested)));
}

[[nodiscard]] spdlog::level::level_enum toSpdLevel(const LogLevel level) noexcept
{
    switch (level)
    {
        case LogLevel::error:
            return spdlog::level::err;
        case LogLevel::warn:
            return spdlog::level::warn;
        case LogLevel::info:
            return spdlog::level::info;
        case LogLevel::debug:
            return spdlog::level::debug;
        case LogLevel::trace:
            return spdlog::level::trace;
        case LogLevel::off:
            return spdlog::level::off;
    }

    return spdlog::level::off;
}

} // namespace

class Logger::Implementation final
{
public:
    Implementation(
        LogConfiguration configuration,
        SessionIdSequence& sessionIdsToUse,
        Callback callback)
        : sessionIds(sessionIdsToUse),
          requestedLevel(configuration.runtimeLevel)
    {
        std::shared_ptr<spdlog::sinks::sink> sink;
        if (configuration.sinkKind == LogSinkKind::callback)
        {
            if (! callback)
                throw std::invalid_argument("callback sink requires a callback");

            sink = std::make_shared<spdlog::sinks::callback_sink_mt>(
                [callback = std::move(callback)](const spdlog::details::log_msg& message)
                {
                    callback(
                        fromSpdLevel(message.level),
                        std::string_view(message.payload.data(), message.payload.size()));
                });
        }
        else
        {
            if (callback)
                throw std::invalid_argument("platform sink does not accept a callback");
            sink = createPlatformLogSink(configuration);
        }

        backend = std::make_unique<spdlog::logger>(configuration.name, std::move(sink));
        backend->set_pattern("%^%v%$");
        backend->set_level(toSpdLevel(currentState().effective));
        backend->flush_on(toSpdLevel(configuration.flushLevel));
    }

    [[nodiscard]] LogLevelState currentState() const noexcept
    {
        const auto requested = requestedLevel.load(std::memory_order_relaxed);
        const auto compiled = levelFromCompiledValue();
        const auto effective = effectiveLevel(compiled, requested);
        return { requested, compiled, effective, requested != effective };
    }

    [[nodiscard]] LogLevelState setLevel(const LogLevel requested) noexcept
    {
        requestedLevel.store(requested, std::memory_order_relaxed);
        const auto state = currentState();
        backend->set_level(toSpdLevel(state.effective));
        return state;
    }

    [[nodiscard]] static LogLevel fromSpdLevel(
        const spdlog::level::level_enum level) noexcept
    {
        switch (level)
        {
            case spdlog::level::critical:
            case spdlog::level::err:
                return LogLevel::error;
            case spdlog::level::warn:
                return LogLevel::warn;
            case spdlog::level::info:
                return LogLevel::info;
            case spdlog::level::debug:
                return LogLevel::debug;
            case spdlog::level::trace:
                return LogLevel::trace;
            case spdlog::level::off:
            case spdlog::level::n_levels:
                return LogLevel::off;
        }

        return LogLevel::off;
    }

    std::unique_ptr<spdlog::logger> backend;
    SessionIdSequence& sessionIds;
    std::atomic<LogLevel> requestedLevel { LogLevel::off };
};

Logger::Logger(
    LogConfiguration configuration,
    SessionIdSequence& sessionIds,
    Callback callback)
    : implementation(std::make_unique<Implementation>(
          std::move(configuration),
          sessionIds,
          std::move(callback)))
{
}

Logger::~Logger() = default;
Logger::Logger(Logger&&) noexcept = default;
Logger& Logger::operator=(Logger&&) noexcept = default;

std::uint64_t Logger::newSessionId() noexcept
{
    return implementation->sessionIds.next();
}

LogLevelState Logger::setRuntimeLevel(const LogLevel requested) noexcept
{
    return implementation->setLevel(requested);
}

LogLevelState Logger::levelState() const noexcept
{
    return implementation->currentState();
}

LogWriteResult Logger::log(
    const LogLevel level,
    const std::uint64_t sessionId,
    const std::string_view event,
    const std::initializer_list<LogField> fields)
{
    const auto state = levelState();
    if (level == LogLevel::off
        || static_cast<unsigned int>(level) > static_cast<unsigned int>(state.effective))
    {
        return { LogWriteStatus::filtered, {} };
    }

    try
    {
        implementation->backend->log(toSpdLevel(level), formatLine(sessionId, event, fields));
        return { LogWriteStatus::written, {} };
    }
    catch (const std::exception& exception)
    {
        return { LogWriteStatus::backendFailure, exception.what() };
    }
    catch (...)
    {
        return { LogWriteStatus::backendFailure, "unknown logging failure" };
    }
}

LogWriteResult Logger::flush()
{
    try
    {
        implementation->backend->flush();
        return { LogWriteStatus::written, {} };
    }
    catch (const std::exception& exception)
    {
        return { LogWriteStatus::backendFailure, exception.what() };
    }
    catch (...)
    {
        return { LogWriteStatus::backendFailure, "unknown flush failure" };
    }
}

LogLevel Logger::compiledLevel() noexcept
{
    return levelFromCompiledValue();
}

} // namespace gigante
