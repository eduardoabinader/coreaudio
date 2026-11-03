#include "TransferProbe.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace gigante {

namespace {

const std::uint32_t roundConstants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

std::uint32_t rotateRight(std::uint32_t value, int count) {
    return (value >> count) | (value << (32 - count));
}

} // namespace

Sha256::Sha256()
    : state{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19} {}

void Sha256::processBlock(const std::uint8_t* block) {
    std::uint32_t schedule[64];
    for (int index = 0; index < 16; ++index) {
        schedule[index] = (static_cast<std::uint32_t>(block[index * 4]) << 24) |
                          (static_cast<std::uint32_t>(block[index * 4 + 1]) << 16) |
                          (static_cast<std::uint32_t>(block[index * 4 + 2]) << 8) |
                          static_cast<std::uint32_t>(block[index * 4 + 3]);
    }
    for (int index = 16; index < 64; ++index) {
        const std::uint32_t first = rotateRight(schedule[index - 15], 7) ^ rotateRight(schedule[index - 15], 18) ^ (schedule[index - 15] >> 3);
        const std::uint32_t second = rotateRight(schedule[index - 2], 17) ^ rotateRight(schedule[index - 2], 19) ^ (schedule[index - 2] >> 10);
        schedule[index] = schedule[index - 16] + first + schedule[index - 7] + second;
    }
    std::array<std::uint32_t, 8> words = state;
    for (int index = 0; index < 64; ++index) {
        const std::uint32_t sumOne = rotateRight(words[4], 6) ^ rotateRight(words[4], 11) ^ rotateRight(words[4], 25);
        const std::uint32_t choice = (words[4] & words[5]) ^ (~words[4] & words[6]);
        const std::uint32_t first = words[7] + sumOne + choice + roundConstants[index] + schedule[index];
        const std::uint32_t sumZero = rotateRight(words[0], 2) ^ rotateRight(words[0], 13) ^ rotateRight(words[0], 22);
        const std::uint32_t majority = (words[0] & words[1]) ^ (words[0] & words[2]) ^ (words[1] & words[2]);
        const std::uint32_t second = sumZero + majority;
        words = {first + second, words[0], words[1], words[2], words[3] + first, words[4], words[5], words[6]};
    }
    for (std::size_t index = 0; index < state.size(); ++index) {
        state[index] += words[index];
    }
}

void Sha256::update(const char* data, std::size_t size) {
    totalBytes += size;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(data);
    while (size > 0) {
        if (bufferedBytes == 0 && size >= buffer.size()) {
            processBlock(bytes);
            bytes += buffer.size();
            size -= buffer.size();
            continue;
        }
        const std::size_t count = std::min(size, buffer.size() - bufferedBytes);
        std::memcpy(buffer.data() + bufferedBytes, bytes, count);
        bufferedBytes += count;
        bytes += count;
        size -= count;
        if (bufferedBytes == buffer.size()) {
            processBlock(buffer.data());
            bufferedBytes = 0;
        }
    }
}

std::string Sha256::finish() {
    const std::uint64_t totalBits = totalBytes * 8;
    const char marker = static_cast<char>(0x80);
    update(&marker, 1);
    const char zero = 0;
    while (bufferedBytes != 56) {
        update(&zero, 1);
    }
    char length[8];
    for (int index = 0; index < 8; ++index) {
        length[index] = static_cast<char>(totalBits >> (56 - index * 8));
    }
    update(length, 8);
    std::string digest;
    char text[9];
    for (const std::uint32_t word : state) {
        std::snprintf(text, sizeof(text), "%08x", word);
        digest += text;
    }
    return digest;
}

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
