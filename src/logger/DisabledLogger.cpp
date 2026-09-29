#include <gigante/Logger.h>

#include <atomic>
#include <stdexcept>
#include <utility>

namespace gigante
{
class Logger::Implementation final
{
public:
    Implementation(
        const LogConfiguration& configuration,
        SessionIdSequence& sessionIdsToUse,
        const Callback& callback)
        : sessionIds(sessionIdsToUse),
          requestedLevel(configuration.runtimeLevel)
    {
        if (configuration.sinkType == LogSinkType::callback && ! callback)
            throw std::invalid_argument("callback sink requires a callback");
        if (configuration.sinkType == LogSinkType::platformDefault && callback)
            throw std::invalid_argument("platform sink does not accept a callback");
    }

    SessionIdSequence& sessionIds;
    std::atomic<LogLevel> requestedLevel { LogLevel::off };
};

Logger::Logger(
    LogConfiguration configuration,
    SessionIdSequence& sessionIds,
    Callback callback)
    : implementation(std::make_unique<Implementation>(configuration, sessionIds, callback))
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
    implementation->requestedLevel.store(requested, std::memory_order_relaxed);
    return levelState();
}

LogLevelState Logger::levelState() const noexcept
{
    const auto requested = implementation->requestedLevel.load(std::memory_order_relaxed);
    return { requested, LogLevel::off, LogLevel::off, requested != LogLevel::off };
}

LogWriteResult Logger::log(
    const LogLevel,
    const std::uint64_t,
    const std::string_view,
    const std::initializer_list<LogField>)
{
    return { LogWriteStatus::filtered, {} };
}

LogWriteResult Logger::flush()
{
    return { LogWriteStatus::written, {} };
}

LogLevel Logger::compiledLevel() noexcept
{
    return LogLevel::off;
}

} // namespace gigante
