#include "gigante/EffectsChain.h"
#include "gigante/RealtimeMetrics.h"
#include "gigante/Revision.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>
#include <string>
#include <vector>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace allocationProbe
{
thread_local bool tracking = false;
thread_local std::size_t allocationCount = 0;

void record() noexcept
{
    if (tracking)
        ++allocationCount;
}

#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void releaseMemory(void* memory) noexcept
{
    std::free(memory);
}

void* allocateAligned(std::size_t alignment, std::size_t size) noexcept
{
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    const std::size_t roundedSize =
        ((size + alignment - 1U) / alignment) * alignment;
    return std::aligned_alloc(alignment, roundedSize);
#endif
}

#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void releaseAlignedMemory(void* memory) noexcept
{
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}
}

void* operator new(std::size_t size)
{
    allocationProbe::record();
    if (void* const memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* memory) noexcept
{
    allocationProbe::releaseMemory(memory);
}

void operator delete[](void* memory) noexcept
{
    ::operator delete(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    allocationProbe::releaseMemory(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    ::operator delete(memory);
}

#if defined(__cpp_aligned_new)
void* operator new(std::size_t size, std::align_val_t alignment)
{
    allocationProbe::record();
    const std::size_t alignmentValue = static_cast<std::size_t>(alignment);
    if (void* const memory = allocationProbe::allocateAligned(alignmentValue, size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size, std::align_val_t alignment)
{
    return ::operator new(size, alignment);
}

void operator delete(void* memory, std::align_val_t) noexcept
{
    allocationProbe::releaseAlignedMemory(memory);
}

void operator delete[](void* memory, std::align_val_t alignment) noexcept
{
    ::operator delete(memory, alignment);
}

void operator delete(void* memory, std::size_t, std::align_val_t) noexcept
{
    allocationProbe::releaseAlignedMemory(memory);
}

void operator delete[](
    void* memory,
    std::size_t,
    std::align_val_t alignment) noexcept
{
    ::operator delete(memory, alignment);
}
#endif

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

    std::array<int, gigante::effectsStageCount> begun {};
    std::array<int, gigante::effectsStageCount> ended {};
    gigante::BlockObservation begin {};
    gigante::BlockObservation end {};
    int blockBeginCount = 0;
    int blockEndCount = 0;
};

class FakeTickClock final : public gigante::TickClock
{
public:
    std::uint64_t currentTick() noexcept override
    {
        const std::uint64_t result = nextTick;
        nextTick += tickStep;
        return result;
    }

    std::uint64_t nextTick = 0;
    std::uint64_t tickStep = 10;
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
#if defined(GIGANTE_TESTS_WITH_JUCE_DSP)
    REQUIRE(report.status.ok());
#else
    REQUIRE(report.status.code == gigante::ErrorCode::effectsNotImplemented);
    requireBitExact(std::vector<float>(8, 0.25F), shortSamples);
#endif
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

void testRealtimeMetricsAreDeterministicAndBounded()
{
    gigante::EffectsChain chain;
    REQUIRE(chain.prepare({ 48000.0, 2, 4 }).ok());

    FakeTickClock clock;
    gigante::RealtimeMetricsObserver observer(
        clock,
        { 48000, 1.0 });
    chain.reset(&observer);

    std::vector<float> left { -0.5F, 0.5F, -0.25F, 0.25F };
    std::vector<float> right { 0.0F, 1.0F, -1.0F, 0.0F };
    gigante::ProcessingContext context;
    context.firstSamplePosition = 73;
    REQUIRE(chain.process(viewOf(left, &right), {}, context, &observer).status.ok());

    const auto metrics = observer.snapshot();
    REQUIRE(metrics.blockCount == 1);
    REQUIRE(metrics.resetCount == 1);
    REQUIRE(metrics.firstSamplePosition == 73);
    REQUIRE(metrics.maximumCallbackTicks == 50);
    REQUIRE(metrics.callbackBudgetExceededCount == 1);
    REQUIRE(metrics.algorithmicLatencySamples == 0);
    REQUIRE(metrics.bypassed);
    REQUIRE(metrics.input.peak == 1.0F);
    REQUIRE(std::abs(metrics.input.rootMeanSquare - 0.572821974F) < 0.000001F);
    REQUIRE(metrics.input.finiteSampleCount == 8);
    REQUIRE(metrics.postChain.peak == metrics.input.peak);
    REQUIRE(metrics.output.rootMeanSquare == metrics.input.rootMeanSquare);

    const auto inputIndex = static_cast<std::size_t>(gigante::EffectsStage::input);
    const auto outputIndex = static_cast<std::size_t>(gigante::EffectsStage::output);
    REQUIRE(metrics.stages[inputIndex].callCount == 1);
    REQUIRE(metrics.stages[inputIndex].totalTicks == 10);
    REQUIRE(metrics.stages[inputIndex].maximumTicks == 10);
    REQUIRE(metrics.stages[outputIndex].callCount == 1);
    REQUIRE(metrics.stages[outputIndex].totalTicks == 10);

    for (std::size_t index = 1; index + 1 < gigante::effectsStageCount; ++index)
        REQUIRE(metrics.stages[index].callCount == 0);

    // The fixed SPSC publication queue drops rather than blocking or allocating.
    for (int block = 0; block < 10; ++block)
        REQUIRE(chain.process(viewOf(left, &right), {}, context, &observer).status.ok());
    observer.snapshot();
    REQUIRE(chain.process(viewOf(left, &right), {}, context, &observer).status.ok());
    const auto afterOverflow = observer.snapshot();
    REQUIRE(afterOverflow.blockCount == 12);
    REQUIRE(afterOverflow.droppedPublicationCount > 0);
}

void testRealtimeMetricsCountRecoveryAndFiniteLevels()
{
    gigante::EffectsChain chain;
    REQUIRE(chain.prepare({ 48000.0, 1, 4 }).ok());

    FakeTickClock clock;
    gigante::RealtimeMetricsObserver observer(
        clock,
        { 48000, 1.0 });
    std::vector<float> samples {
        0.5F,
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -0.5F
    };
    gigante::EffectsParameters parameters;
    parameters.driveAmount = std::numeric_limits<float>::quiet_NaN();

    const auto report = chain.process(viewOf(samples), parameters, {}, &observer);
    REQUIRE(report.status.ok());
    const auto metrics = observer.snapshot();
    REQUIRE(metrics.nonFiniteSampleCount == 2);
    REQUIRE(metrics.invalidParameterCount == 1);
    REQUIRE(metrics.recoveryCount == 2);
    REQUIRE(metrics.input.finiteSampleCount == 2);
    REQUIRE(metrics.input.peak == 0.5F);
    REQUIRE(metrics.input.rootMeanSquare == 0.5F);
    REQUIRE(metrics.output.finiteSampleCount == 4);
    REQUIRE(std::abs(metrics.output.rootMeanSquare - 0.353553385F) < 0.000001F);
}

void testRealtimeMetricsCoverEveryStage()
{
    FakeTickClock clock;
    gigante::RealtimeMetricsObserver observer(clock, { 1000000000, 0.0 });
    gigante::BlockObservation block;
    block.sampleCount = 64;
    observer.onBlockBegin(block);
    for (std::size_t index = 0; index < gigante::effectsStageCount; ++index)
    {
        const auto stage = static_cast<gigante::EffectsStage>(index);
        observer.onStageBegin(stage);
        observer.onStageEnd(stage);
    }
    observer.onBlockEnd(block);

    const auto metrics = observer.snapshot();
    REQUIRE(metrics.callbackBudgetExceededCount == 0);
    for (const auto& stage : metrics.stages)
    {
        REQUIRE(stage.callCount == 1);
        REQUIRE(stage.totalTicks == 10);
        REQUIRE(stage.maximumTicks == 10);
    }
}

void testPreparedProcessPerformsNoHeapAllocation()
{
    gigante::EffectsChain chain;
    gigante::EffectsParameters parameters;
    parameters.tremoloDepth = 0.4F;
    parameters.tremoloRateHz = 3.0F;
    parameters.filterFrequencyHz = 3200.0F;
    parameters.filterResonance = 0.7F;
    parameters.driveAmount = 0.25F;
    parameters.chorusMix = 0.3F;
    parameters.reverbMix = 0.2F;
    parameters.outputGainDb = -3.0F;
    REQUIRE(chain.prepare({ 48000.0, 2, 512 }, parameters).ok());

    FakeTickClock clock;
    gigante::RealtimeMetricsObserver observer(clock, { 48000, 0.0 });
    std::vector<float> left(512, 0.125F);
    std::vector<float> right(512, -0.125F);
    const auto buffer = viewOf(left, &right);
    gigante::ProcessingContext context;
#if defined(GIGANTE_TESTS_WITH_JUCE_DSP)
    context.mode = gigante::ProcessingMode::effects;
#endif

    // Warm any process-independent runtime paths before observing the callback.
    REQUIRE(chain.process(buffer, parameters, context, &observer).status.ok());
    observer.snapshot();

    allocationProbe::allocationCount = 0;
    allocationProbe::tracking = true;
    for (int block = 0; block < 32; ++block)
    {
        context.firstSamplePosition = static_cast<std::uint64_t>(block + 1) * 512U;
        const auto report = chain.process(buffer, parameters, context, &observer);
        if (!report.status.ok())
        {
            allocationProbe::tracking = false;
            REQUIRE(report.status.ok());
        }
    }
    allocationProbe::tracking = false;
    REQUIRE(allocationProbe::allocationCount == 0);
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
    const std::array<TestCase, 10> tests {{
        { "lifecycle and configuration", testLifecycleAndConfiguration },
        { "invalid prepare", testInvalidPrepareIsSpecificAndRecoverable },
        { "process validation", testProcessValidationAndExplicitUnimplementedMode },
        { "bit-exact bypass", testBypassIsBitExactForFiniteSamplesAndVariableBlocks },
        { "non-finite recovery", testNonFiniteRecoveryIsDeterministicAndCounted },
        { "reconfiguration", testReconfigurationChangesEffectiveContract },
        { "deterministic realtime metrics", testRealtimeMetricsAreDeterministicAndBounded },
        { "metrics recovery", testRealtimeMetricsCountRecoveryAndFiniteLevels },
        { "metrics stage coverage", testRealtimeMetricsCoverEveryStage },
        { "prepared process allocation", testPreparedProcessPerformsNoHeapAllocation }
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
