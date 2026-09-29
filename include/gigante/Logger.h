#pragma once

#include <cstdint>
#include <atomic>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>

#define GIGANTE_LOG_LEVEL_OFF 0
#define GIGANTE_LOG_LEVEL_ERROR 1
#define GIGANTE_LOG_LEVEL_WARN 2
#define GIGANTE_LOG_LEVEL_INFO 3
#define GIGANTE_LOG_LEVEL_DEBUG 4
#define GIGANTE_LOG_LEVEL_TRACE 5

#ifndef GIGANTE_LOG_COMPILED_LEVEL
#define GIGANTE_LOG_COMPILED_LEVEL GIGANTE_LOG_LEVEL_INFO
#endif

namespace gigante
{

enum class LogLevel : std::uint8_t
{
    off,
    error,
    warn,
    info,
    debug,
    trace
};

enum class LogColorMode : std::uint8_t
{
    automatic,
    always,
    never
};

enum class LogSinkType : std::uint8_t
{
    platformDefault,
    callback
};

struct LogField
{
    std::string_view key;
    std::string_view value;
};

struct LogConfiguration
{
    std::string name { "gigante" };
    LogSinkType sinkType { LogSinkType::platformDefault };
    LogColorMode colorMode { LogColorMode::automatic };
    LogLevel runtimeLevel { LogLevel::info };
    LogLevel flushLevel { LogLevel::error };
};

struct LogLevelState
{
    LogLevel requested { LogLevel::off };
    LogLevel compiled { LogLevel::off };
    LogLevel effective { LogLevel::off };
    bool limitedByBuild { false };
};

enum class LogWriteStatus : std::uint8_t
{
    written,
    filtered,
    backendFailure
};

struct LogWriteResult
{
    LogWriteStatus status { LogWriteStatus::backendFailure };
    std::string cause;
};

class SessionIdSequence final
{
public:
    SessionIdSequence() = default;

    SessionIdSequence(const SessionIdSequence&) = delete;
    SessionIdSequence& operator=(const SessionIdSequence&) = delete;

private:
    friend class Logger;
    [[nodiscard]] std::uint64_t next() noexcept;

    std::atomic<std::uint64_t> nextValue { 1 };
};

class Logger final
{
public:
    using Callback = std::function<void(LogLevel, std::string_view)>;

    static constexpr std::uint64_t noSessionId = 0;

    explicit Logger(
        LogConfiguration configuration,
        SessionIdSequence& sessionIds,
        Callback callback = {});
    ~Logger();

    Logger(Logger&&) noexcept;
    Logger& operator=(Logger&&) noexcept;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    [[nodiscard]] std::uint64_t newSessionId() noexcept;
    [[nodiscard]] LogLevelState setRuntimeLevel(LogLevel requested) noexcept;
    [[nodiscard]] LogLevelState levelState() const noexcept;

    [[nodiscard]] LogWriteResult log(
        LogLevel level,
        std::uint64_t sessionId,
        std::string_view event,
        std::initializer_list<LogField> fields = {});
    [[nodiscard]] LogWriteResult flush();

    [[nodiscard]] static LogLevel compiledLevel() noexcept;
    [[nodiscard]] static std::string formatLine(
        std::uint64_t sessionId,
        std::string_view event,
        std::initializer_list<LogField> fields = {});

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation;
};

} // namespace gigante

#if GIGANTE_LOG_COMPILED_LEVEL >= GIGANTE_LOG_LEVEL_ERROR
#define GIGANTE_LOG_ERROR(logger, ...) \
    static_cast<void>((logger).log(::gigante::LogLevel::error, __VA_ARGS__))
#else
#define GIGANTE_LOG_ERROR(logger, ...) ((void) 0)
#endif

#if GIGANTE_LOG_COMPILED_LEVEL >= GIGANTE_LOG_LEVEL_WARN
#define GIGANTE_LOG_WARN(logger, ...) \
    static_cast<void>((logger).log(::gigante::LogLevel::warn, __VA_ARGS__))
#else
#define GIGANTE_LOG_WARN(logger, ...) ((void) 0)
#endif

#if GIGANTE_LOG_COMPILED_LEVEL >= GIGANTE_LOG_LEVEL_INFO
#define GIGANTE_LOG_INFO(logger, ...) \
    static_cast<void>((logger).log(::gigante::LogLevel::info, __VA_ARGS__))
#else
#define GIGANTE_LOG_INFO(logger, ...) ((void) 0)
#endif

#if GIGANTE_LOG_COMPILED_LEVEL >= GIGANTE_LOG_LEVEL_DEBUG
#define GIGANTE_LOG_DEBUG(logger, ...) \
    static_cast<void>((logger).log(::gigante::LogLevel::debug, __VA_ARGS__))
#else
#define GIGANTE_LOG_DEBUG(logger, ...) ((void) 0)
#endif

#if GIGANTE_LOG_COMPILED_LEVEL >= GIGANTE_LOG_LEVEL_TRACE
#define GIGANTE_LOG_TRACE(logger, ...) \
    static_cast<void>((logger).log(::gigante::LogLevel::trace, __VA_ARGS__))
#else
#define GIGANTE_LOG_TRACE(logger, ...) ((void) 0)
#endif
