#include <gigante/Logger.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

void require(const bool condition, const char* const message)
{
    if (! condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

[[nodiscard]] gigante::Logger makeCaptureLogger(
    std::vector<std::pair<gigante::LogLevel, std::string>>& lines,
    std::mutex& linesMutex,
    gigante::SessionIdSequence& sessionIds,
    const gigante::LogLevel runtimeLevel = gigante::LogLevel::trace)
{
    gigante::LogConfiguration configuration;
    configuration.name = "capture";
    configuration.sinkType = gigante::LogSinkType::callback;
    configuration.runtimeLevel = runtimeLevel;
    configuration.flushLevel = gigante::LogLevel::error;
    return gigante::Logger(
        std::move(configuration),
        sessionIds,
        [&lines, &linesMutex](const gigante::LogLevel level, const std::string_view line)
        {
            const std::lock_guard<std::mutex> lock(linesMutex);
            lines.emplace_back(level, line);
        });
}

void testGoldenFormat()
{
    const auto line = gigante::Logger::formatLine(
        9,
        "route confirmed",
        {
            { "output", "BLUETOOTH_SCO" },
            { "input", "BUILTIN_MIC" }
        });
    require(
        line == "session=9 event=route_confirmed input=BUILTIN_MIC output=BLUETOOTH_SCO",
        "AHIO golden line changed");

    const auto sanitized = gigante::Logger::formatLine(
        gigante::Logger::noSessionId,
        "line\rbreak\nspace here",
        {
            { "z key", "two words" },
            { "a\nkey", "line\rvalue" }
        });
    require(
        sanitized == "session=0 event=line_break_space_here a_key=line_value z_key=two_words",
        "sanitization or field ordering changed");

    const std::vector<gigante::LogField> dynamicFields {
        { "source", "java" },
        { "operation", "wake lock" }
    };
    const auto dynamicLine = gigante::Logger::formatLine(
        11,
        "bridge warning",
        std::span<const gigante::LogField>(dynamicFields));
    require(
        dynamicLine == "session=11 event=bridge_warning operation=wake_lock source=java",
        "dynamic-field formatting changed");
}

void testRuntimeAndBuildPrecedence()
{
    std::vector<std::pair<gigante::LogLevel, std::string>> lines;
    std::mutex linesMutex;
    gigante::SessionIdSequence sessionIds;
    auto logger = makeCaptureLogger(lines, linesMutex, sessionIds);

    const auto initial = logger.levelState();
    require(initial.requested == gigante::LogLevel::trace, "requested level not retained");
    require(initial.compiled == gigante::Logger::compiledLevel(), "compiled level mismatch");
    require(
        initial.effective == std::min(initial.compiled, initial.requested),
        "build level did not take precedence");
    require(
        initial.limitedByBuild == (initial.requested != initial.effective),
        "build limitation not reported");

    const auto lowered = logger.setRuntimeLevel(gigante::LogLevel::warn);
    require(lowered.requested == gigante::LogLevel::warn, "runtime request lost");
    require(
        lowered.effective == std::min(lowered.compiled, gigante::LogLevel::warn),
        "runtime level did not lower effective level");

    const auto filtered = logger.log(gigante::LogLevel::info, 1, "filtered");
    require(filtered.status == gigante::LogWriteStatus::filtered, "runtime filter failed");
    const auto written = logger.log(gigante::LogLevel::error, 1, "written");
    if (gigante::Logger::compiledLevel() == gigante::LogLevel::off)
    {
        require(written.status == gigante::LogWriteStatus::filtered, "OFF build emitted a line");
        require(lines.empty(), "OFF build invoked its sink");
    }
    else
    {
        require(written.status == gigante::LogWriteStatus::written, "enabled line was not written");
        require(lines.size() == 1, "unexpected callback count");
        require(lines.front().first == gigante::LogLevel::error, "callback level changed");
        require(lines.front().second == "session=1 event=written", "callback line changed");
    }
    require(logger.flush().status == gigante::LogWriteStatus::written, "flush failed");

    const auto disabled = logger.setRuntimeLevel(gigante::LogLevel::off);
    require(disabled.effective == gigante::LogLevel::off, "runtime off was ignored");
    require(
        logger.log(gigante::LogLevel::error, 1, "disabled").status
            == gigante::LogWriteStatus::filtered,
        "runtime off emitted a line");
}

void testSessionIds()
{
    std::vector<std::pair<gigante::LogLevel, std::string>> lines;
    std::mutex linesMutex;
    gigante::SessionIdSequence sessionIds;
    auto logger = makeCaptureLogger(lines, linesMutex, sessionIds);
    auto secondLogger = makeCaptureLogger(lines, linesMutex, sessionIds);
    require(gigante::Logger::noSessionId == 0, "no-session id changed");
    require(logger.newSessionId() == 1, "first session id changed");
    require(secondLogger.newSessionId() == 2, "session ids are not process-unique");
    require(logger.newSessionId() == 3, "session ids are not monotonic");
}

void testConfiguredCompileTimeCeiling()
{
    std::vector<std::pair<gigante::LogLevel, std::string>> lines;
    std::mutex linesMutex;
    gigante::SessionIdSequence sessionIds;
    auto logger = makeCaptureLogger(lines, linesMutex, sessionIds);
    int errorArguments = 0;
    int warnArguments = 0;
    int infoArguments = 0;
    int debugArguments = 0;
    int traceArguments = 0;

    GIGANTE_LOG_ERROR(logger, 1, std::to_string(++errorArguments));
    GIGANTE_LOG_WARN(logger, 1, std::to_string(++warnArguments));
    GIGANTE_LOG_INFO(logger, 1, std::to_string(++infoArguments));
    GIGANTE_LOG_DEBUG(logger, 1, std::to_string(++debugArguments));
    GIGANTE_LOG_TRACE(logger, 1, std::to_string(++traceArguments));

    const auto compiled = static_cast<unsigned int>(gigante::Logger::compiledLevel());
    require(errorArguments == (compiled >= GIGANTE_LOG_LEVEL_ERROR ? 1 : 0), "ERROR ceiling failed");
    require(warnArguments == (compiled >= GIGANTE_LOG_LEVEL_WARN ? 1 : 0), "WARN ceiling failed");
    require(infoArguments == (compiled >= GIGANTE_LOG_LEVEL_INFO ? 1 : 0), "INFO ceiling failed");
    require(debugArguments == (compiled >= GIGANTE_LOG_LEVEL_DEBUG ? 1 : 0), "DEBUG ceiling failed");
    require(traceArguments == (compiled >= GIGANTE_LOG_LEVEL_TRACE ? 1 : 0), "TRACE ceiling failed");
    require(lines.size() == compiled, "compiled ceiling emitted the wrong number of levels");
}

void testConcurrentLinesDoNotInterleave()
{
    if (gigante::Logger::compiledLevel() == gigante::LogLevel::off)
        return;

    std::vector<std::pair<gigante::LogLevel, std::string>> lines;
    std::mutex linesMutex;
    gigante::SessionIdSequence sessionIds;
    auto logger = makeCaptureLogger(lines, linesMutex, sessionIds, gigante::LogLevel::info);

    constexpr std::size_t threadCount = 4;
    constexpr std::size_t linesPerThread = 100;
    std::vector<std::thread> threads;
    for (std::size_t threadIndex = 0; threadIndex < threadCount; ++threadIndex)
    {
        threads.emplace_back(
            [&logger, threadIndex]
            {
                for (std::size_t lineIndex = 0; lineIndex < linesPerThread; ++lineIndex)
                {
                    const auto result = logger.log(
                        gigante::LogLevel::info,
                        static_cast<std::uint64_t>(threadIndex + 1),
                        "concurrent",
                        { { "line", std::to_string(lineIndex) } });
                    require(result.status == gigante::LogWriteStatus::written, "concurrent log failed");
                }
            });
    }
    for (auto& thread : threads)
        thread.join();

    require(lines.size() == threadCount * linesPerThread, "concurrent lines were lost");
    require(
        std::all_of(
            lines.begin(),
            lines.end(),
            [](const auto& line)
            {
                return line.second.find(" event=concurrent line=") != std::string::npos
                    && line.second.find('\n') == std::string::npos
                    && line.second.find('\r') == std::string::npos;
            }),
        "concurrent line was malformed");
}

void testInvalidSinkConfiguration()
{
    gigante::LogConfiguration configuration;
    configuration.sinkType = gigante::LogSinkType::callback;
    gigante::SessionIdSequence sessionIds;
    bool threw = false;
    try
    {
        gigante::Logger logger(configuration, sessionIds);
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    require(threw, "missing callback was accepted");
}

void testPlatformSinkLifecycle()
{
    gigante::LogConfiguration configuration;
    configuration.name = "gigante-log-test";
    configuration.sinkType = gigante::LogSinkType::platformDefault;
    configuration.colorMode = gigante::LogColorMode::never;
    configuration.runtimeLevel = gigante::LogLevel::off;
    gigante::SessionIdSequence sessionIds;
    gigante::Logger logger(std::move(configuration), sessionIds);
    require(logger.flush().status == gigante::LogWriteStatus::written, "platform flush failed");
}

} // namespace

int main()
{
    testGoldenFormat();
    testRuntimeAndBuildPrecedence();
    testSessionIds();
    testConfiguredCompileTimeCeiling();
    testConcurrentLinesDoNotInterleave();
    testInvalidSinkConfiguration();
    testPlatformSinkLifecycle();
    return 0;
}
