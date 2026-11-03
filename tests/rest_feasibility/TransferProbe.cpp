#include "TransferProbe.h"

#include <string>
#include <vector>

namespace gigante {

void fillTransferData(std::uint64_t offset, char* data, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
        const std::uint64_t position = offset + index;
        data[index] = static_cast<char>((position * 2654435761U) >> 13 ^ (position >> 20));
    }
}

std::string transferDigest(std::uint64_t start, std::uint64_t length) {
    Sha256 digest;
    std::vector<char> chunk(64 * 1024);
    std::uint64_t offset = start;
    const std::uint64_t end = start + length;
    while (offset < end) {
        const auto size = static_cast<std::size_t>(std::min<std::uint64_t>(chunk.size(), end - offset));
        fillTransferData(offset, chunk.data(), size);
        digest.update(chunk.data(), size);
        offset += size;
    }
    return digest.finish();
}

} // namespace gigante
