#pragma once

#include "HttpClientTransport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace gigante {

class Sha256 {
public:
    Sha256();
    void update(const char* data, std::size_t size);
    std::string finish();

private:
    void processBlock(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state;
    std::array<std::uint8_t, 64> buffer{};
    std::size_t bufferedBytes = 0;
    std::uint64_t totalBytes = 0;
};

// Deterministic data, so both sides can digest a transfer with no file on disk.
void fillTransferData(std::uint64_t offset, char* data, std::size_t size);
std::string transferDigest(std::uint64_t start, std::uint64_t length);

} // namespace gigante
