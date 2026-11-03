#pragma once

#include "HttpClientTransport.h"
#include "Sha256.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace gigante {

// Deterministic data, so both sides can digest a transfer with no file on disk.
void fillTransferData(std::uint64_t offset, char* data, std::size_t size);
std::string transferDigest(std::uint64_t start, std::uint64_t length);

} // namespace gigante
