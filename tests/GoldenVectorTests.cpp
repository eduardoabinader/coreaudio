#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

struct VectorExpectation
{
    const char* filename;
    std::size_t byteCount;
};

bool validateVector(const VectorExpectation& expectation)
{
    const std::string path = std::string(GIGANTE_GOLDEN_DIRECTORY)
        + "/" + expectation.filename;
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        std::cerr << "cannot open golden vector: " << path << '\n';
        return false;
    }

    std::size_t bytesRead = 0;
    std::size_t sampleIndex = 0;
    std::array<unsigned char, sizeof(float)> bytes {};
    while (input.read(reinterpret_cast<char*>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size())))
    {
        const uint32_t bits = static_cast<uint32_t>(bytes[0])
            | (static_cast<uint32_t>(bytes[1]) << 8u)
            | (static_cast<uint32_t>(bytes[2]) << 16u)
            | (static_cast<uint32_t>(bytes[3]) << 24u);
        float value = 0.0F;
        std::memcpy(&value, &bits, sizeof(value));
        if (!std::isfinite(value))
        {
            std::cerr << "non-finite sample in " << path
                      << " at index " << sampleIndex << '\n';
            return false;
        }
        bytesRead += bytes.size();
        ++sampleIndex;
    }

    if (!input.eof())
    {
        std::cerr << "partial float or read error in " << path << '\n';
        return false;
    }
    if (bytesRead != expectation.byteCount)
    {
        std::cerr << "unexpected byte count in " << path
                  << ": expected " << expectation.byteCount
                  << ", got " << bytesRead << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    static_assert(sizeof(float) == sizeof(uint32_t));

    constexpr std::array<VectorExpectation, 5> expectations {{
        { "transientTailBlock64.f32le", 883200 },
        { "transientTailBlock257.f32le", 883200 },
        { "liveChangesBlock7.f32le", 1152000 },
        { "liveChangesBlock512.f32le", 1152000 },
        { "invalidParameterRecovery.f32le", 864000 }
    }};

    for (const auto& expectation : expectations)
        if (!validateVector(expectation))
            return 1;

    std::cout << "validated 5 finite Kuaru golden vectors\n";
    return 0;
}
