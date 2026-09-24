#include "gigante/EffectsChain.h"
#include "gigante/Revision.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{

class TestFailure
{
public:
    TestFailure(const char* expression, const char* file, int line)
        : expressionValue(expression), fileValue(file), lineValue(line)
    {
    }

    const char* expression() const noexcept { return expressionValue; }
    const char* file() const noexcept { return fileValue; }
    int line() const noexcept { return lineValue; }

private:
    const char* expressionValue;
    const char* fileValue;
    int lineValue;
};

#define REQUIRE(expression) \
    do \
    { \
        if (!(expression)) \
            throw TestFailure(#expression, __FILE__, __LINE__); \
    } while (false)

struct RecordingObserver final : gigante::EffectsStageObserver
{
    void onBlockBegin(const gigante::BlockObservation& value) noexcept override
    {
        ++blockBeginCount;
        begin = value;
    }

    void onStageBegin(gigante::EffectsStage stage) noexcept override
    {
        begun[stageIndex(stage)] += 1;
    }

    void onStageEnd(gigante::EffectsStage stage) noexcept override
    {
        ended[stageIndex(stage)] += 1;
    }

    void onBlockEnd(const gigante::BlockObservation& value) noexcept override
    {
        ++blockEndCount;
        end = value;
    }

    static std::size_t stageIndex(gigante::EffectsStage stage) noexcept
    {
        return static_cast<std::size_t>(stage);
    }

    std::array<int, 14> begun {};
    std::array<int, 14> ended {};
    gigante::BlockObservation begin {};
    gigante::BlockObservation end {};
    int blockBeginCount = 0;
    int blockEndCount = 0;
};

gigante::AudioBufferView viewOf(
    std::vector<float>& first,
    std::vector<float>* second = nullptr)
{
    static thread_local std::array<float*, 2> channels {};
    channels[0] = first.data();
    channels[1] = second != nullptr ? second->data() : nullptr;
    return { channels.data(), second != nullptr ? 2U : 1U, first.size() };
}

void requireBitExact(const std::vector<float>& expected, const std::vector<float>& actual)
{
    REQUIRE(expected.size() == actual.size());
    REQUIRE(std::memcmp(
        expected.data(),
        actual.data(),
        expected.size() * sizeof(float)) == 0);
}

void testLifecycleAndConfiguration()
{
    gigante::EffectsChain chain;
    REQUIRE(chain.lifecycleState() == gigante::LifecycleState::unprepared);

    const gigante::PrepareSpecification requested { 48000.0, 2, 256 };
    REQUIRE(chain.prepare(requested).ok());
    REQUIRE(chain.lifecycleState() == gigante::LifecycleState::prepared);
    REQUIRE(chain.requestedConfiguration().sampleRate == 48000.0);
    REQUIRE(chain.requestedConfiguration().channelCount == 2);
    REQUIRE(chain.requestedConfiguration().maximumBlockSize == 256);
    REQUIRE(chain.effectiveConfiguration().sampleRate == 48000.0);
    REQUIRE(chain.effectiveConfiguration().channelCount == 2);
    REQUIRE(chain.effectiveConfiguration().maximumBlockSize == 256);
    REQUIRE(chain.algorithmicLatencySamples() == 0);

    chain.reset();
    chain.reset();
    REQUIRE(chain.lifecycleState() == gigante::LifecycleState::prepared);

    REQUIRE(chain.prepare(requested).ok());
    REQUIRE(chain.lifecycleState() == gigante::LifecycleState::prepared);

    chain.release();
    chain.release();
    REQUIRE(chain.lifecycleState() == gigante::LifecycleState::unprepared);
    REQUIRE(chain.effectiveConfiguration().channelCount == 0);
}

void testInvalidPrepareIsSpecificAndRecoverable()
{
    gigante::EffectsChain chain;

    auto result = chain.prepare({ 0.0, 2, 64 });
    REQUIRE(result.code == gigante::ErrorCode::invalidSampleRate);
    REQUIRE(result.state == gigante::LifecycleState::unprepared);

    result = chain.prepare({ 48000.0, 0, 64 });
    REQUIRE(result.code == gigante::ErrorCode::invalidChannelCount);
    result = chain.prepare({ 48000.0, 3, 64 });
    REQUIRE(result.code == gigante::ErrorCode::invalidChannelCount);

    result = chain.prepare({ 48000.0, 2, 0 });
    REQUIRE(result.code == gigante::ErrorCode::invalidMaximumBlockSize);

    result = chain.prepare({ 44100.0, 1, 32 });
    REQUIRE(result.ok());
    REQUIRE(result.state == gigante::LifecycleState::prepared);
}

void testProcessValidationAndExplicitUnimplementedMode()
{
    gigante::EffectsChain chain;
    gigante::EffectsParameters parameters;
    gigante::ProcessingContext context;
    std::vector<float> samples(16, 0.25F);

    auto report = chain.process(viewOf(samples), parameters, context);
    REQUIRE(report.status.code == gigante::ErrorCode::notPrepared);
    REQUIRE(report.status.state == gigante::LifecycleState::unprepared);

    REQUIRE(chain.prepare({ 48000.0, 1, 8 }).ok());
    report = chain.process(viewOf(samples), parameters, context);
    REQUIRE(report.status.code == gigante::ErrorCode::blockExceedsMaximum);
    REQUIRE(report.status.requestedValue == 16);
    REQUIRE(report.status.effectiveValue == 8);

    std::vector<float> shortSamples(8, 0.25F);
    std::vector<float> second(8, 0.25F);
    report = chain.process(viewOf(shortSamples, &second), parameters, context);
    REQUIRE(report.status.code == gigante::ErrorCode::channelCountMismatch);
    REQUIRE(report.status.requestedValue == 2);
    REQUIRE(report.status.effectiveValue == 1);

    context.mode = gigante::ProcessingMode::effects;
    report = chain.process(viewOf(shortSamples), parameters, context);
    REQUIRE(report.status.code == gigante::ErrorCode::effectsNotImplemented);
    requireBitExact(std::vector<float>(8, 0.25F), shortSamples);
}

void testBypassIsBitExactForFiniteSamplesAndVariableBlocks()
{
    gigante::EffectsChain chain;
    REQUIRE(chain.prepare({ 96000.0, 2, 17 }).ok());

    gigante::EffectsParameters parameters;
    gigante::ProcessingContext context;
    std::uint64_t firstSample = 0;

    for (const std::size_t blockSize : { 1U, 2U, 3U, 7U, 17U, 5U })
    {
        std::vector<float> left(blockSize);
        std::vector<float> right(blockSize);
        for (std::size_t i = 0; i < blockSize; ++i)
        {
            left[i] = static_cast<float>(i + 1U) / 19.0F;
            right[i] = -static_cast<float>(i + 3U) / 23.0F;
        }
        const auto expectedLeft = left;
        const auto expectedRight = right;
        context.firstSamplePosition = firstSample;

        const auto report = chain.process(viewOf(left, &right), parameters, context);
        REQUIRE(report.status.ok());
        REQUIRE(report.bypassed);
        REQUIRE(report.algorithmicLatencySamples == 0);
        REQUIRE(report.nonFiniteSampleCount == 0);
        requireBitExact(expectedLeft, left);
        requireBitExact(expectedRight, right);
        firstSample += blockSize;
    }
}

void testNonFiniteRecoveryIsDeterministicAndCounted()
{
    gigante::EffectsChain chain;
    REQUIRE(chain.prepare({ 48000.0, 2, 4 }).ok());

    std::vector<float> left {
        0.25F,
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -0.25F
    };
    std::vector<float> right {
        -std::numeric_limits<float>::infinity(),
        0.5F,
        -0.5F,
        std::numeric_limits<float>::quiet_NaN()
    };
    RecordingObserver observer;
    gigante::ProcessingContext context;
    context.firstSamplePosition = 91;

    const auto report = chain.process(
        viewOf(left, &right), {}, context, &observer);

    REQUIRE(report.status.ok());
    REQUIRE(report.nonFiniteSampleCount == 4);
    REQUIRE(chain.totalNonFiniteSampleCount() == 4);
    REQUIRE(left[0] == 0.25F);
    REQUIRE(left[1] == 0.0F);
    REQUIRE(left[2] == 0.0F);
    REQUIRE(left[3] == -0.25F);
    REQUIRE(right[0] == 0.0F);
    REQUIRE(right[1] == 0.5F);
    REQUIRE(right[2] == -0.5F);
    REQUIRE(right[3] == 0.0F);

    REQUIRE(observer.blockBeginCount == 1);
    REQUIRE(observer.blockEndCount == 1);
    REQUIRE(observer.begin.firstSamplePosition == 91);
    REQUIRE(observer.end.nonFiniteSampleCount == 4);
    REQUIRE(observer.end.algorithmicLatencySamples == 0);
    REQUIRE(observer.end.bypassed);
    REQUIRE(observer.begun[RecordingObserver::stageIndex(gigante::EffectsStage::input)] == 1);
    REQUIRE(observer.ended[RecordingObserver::stageIndex(gigante::EffectsStage::input)] == 1);
    REQUIRE(observer.begun[RecordingObserver::stageIndex(gigante::EffectsStage::output)] == 1);
    REQUIRE(observer.ended[RecordingObserver::stageIndex(gigante::EffectsStage::output)] == 1);

    chain.reset();
    REQUIRE(chain.totalNonFiniteSampleCount() == 0);
}

void testReconfigurationChangesEffectiveContract()
{
    gigante::EffectsChain chain;
    REQUIRE(chain.prepare({ 44100.0, 1, 4 }).ok());
    REQUIRE(chain.prepare({ 48000.0, 2, 8 }).ok());
    REQUIRE(chain.requestedConfiguration().sampleRate == 48000.0);
    REQUIRE(chain.effectiveConfiguration().sampleRate == 48000.0);
    REQUIRE(chain.effectiveConfiguration().channelCount == 2);
    REQUIRE(chain.effectiveConfiguration().maximumBlockSize == 8);

    std::vector<float> left(8, 0.1F);
    std::vector<float> right(8, -0.1F);
    REQUIRE(chain.process(viewOf(left, &right), {}, {}).status.ok());
}

using TestFunction = void (*)();

struct TestCase
{
    const char* name;
    TestFunction function;
};

} // namespace

int main()
{
    const std::array<TestCase, 6> tests {{
        { "lifecycle and configuration", testLifecycleAndConfiguration },
        { "invalid prepare", testInvalidPrepareIsSpecificAndRecoverable },
        { "process validation", testProcessValidationAndExplicitUnimplementedMode },
        { "bit-exact bypass", testBypassIsBitExactForFiniteSamplesAndVariableBlocks },
        { "non-finite recovery", testNonFiniteRecoveryIsDeterministicAndCounted },
        { "reconfiguration", testReconfigurationChangesEffectiveContract }
    }};

    int failures = 0;
    for (const auto& test : tests)
    {
        try
        {
            test.function();
            std::cout << "PASS " << test.name << '\n';
        }
        catch (const TestFailure& failure)
        {
            ++failures;
            std::cerr << "FAIL " << test.name << ": " << failure.expression()
                      << " at " << failure.file() << ':' << failure.line() << '\n';
        }
    }

    if (std::string(gigante::revision).empty())
    {
        ++failures;
        std::cerr << "FAIL revision identity is empty\n";
    }

    return failures == 0 ? 0 : 1;
}
