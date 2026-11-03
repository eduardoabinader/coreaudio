#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace gigante {

// Independent of the TLS backend, so builds without TLS can still verify transfers.
class Sha256 {
public:
    Sha256();
    void update(const char* data, std::size_t size);
    // Returns the lowercase hex digest. The object is spent afterwards.
    std::string finish();

private:
    void processBlock(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state;
    std::array<std::uint8_t, 64> buffer{};
    std::size_t bufferedBytes = 0;
    std::uint64_t totalBytes = 0;
};

} // namespace gigante
