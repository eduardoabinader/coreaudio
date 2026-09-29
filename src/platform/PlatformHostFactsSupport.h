#pragma once

#include "gigante/HostFacts.h"

#include <memory>

namespace gigante::platformDetail
{

using HostFactsReader = HostFacts (*)(const RealtimeSchedulingOutcome&);

class FunctionHostFactsSource final : public HostFactsSource
{
public:
    explicit FunctionHostFactsSource(const HostFactsReader readerIn) noexcept
        : reader(readerIn)
    {
    }

    [[nodiscard]] HostFacts read(
        const RealtimeSchedulingOutcome& realtimeScheduling) const override
    {
        return reader(realtimeScheduling);
    }

private:
    HostFactsReader reader;
};

inline std::unique_ptr<HostFactsSource> createFunctionHostFactsSource(
    const HostFactsReader reader)
{
    return std::make_unique<FunctionHostFactsSource>(reader);
}

} // namespace gigante::platformDetail
