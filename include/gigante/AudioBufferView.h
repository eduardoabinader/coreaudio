#pragma once

#include <cstddef>

namespace gigante
{

struct AudioBufferView
{
    float* const* channels = nullptr;
    std::size_t channelCount = 0;
    std::size_t sampleCount = 0;
};

struct AudioBufferReadView
{
    const float* const* channels = nullptr;
    std::size_t channelCount = 0;
    std::size_t sampleCount = 0;
};

} // namespace gigante
