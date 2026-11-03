// Coverage-blind stand-in for libFuzzer: run with ASan and UBSan. Upstream's
// fuzz targets stay unchanged, but this driver finds less than coverage guidance.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);
extern "C" void __sanitizer_set_death_callback(void (*callback)()) __attribute__((weak));

namespace {

std::vector<std::uint8_t> currentInput;
std::string crashPath = "crash-input";

void saveCurrentInput() {
    std::ofstream file(crashPath, std::ios::binary);
    file.write(reinterpret_cast<const char*>(currentInput.data()), static_cast<std::streamsize>(currentInput.size()));
}

std::vector<std::vector<std::uint8_t>> loadCorpus(const std::string& directory) {
    std::vector<std::vector<std::uint8_t>> corpus;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        std::ifstream file(entry.path(), std::ios::binary);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (bytes.size() > 65536) {
            bytes.resize(65536);
        }
        corpus.push_back(std::move(bytes));
    }
    corpus.push_back({'G', 'E', 'T', ' ', '/', ' ', 'H', 'T', 'T', 'P', '/', '1', '.', '1', '\r', '\n', '\r', '\n'});
    corpus.push_back({'d', 'a', 't', 'a', ':', ' ', 'x', '\n', 'r', 'e', 't', 'r', 'y', ':', '0', '\n', '\n'});
    return corpus;
}

void mutate(std::vector<std::uint8_t>& input, const std::vector<std::vector<std::uint8_t>>& corpus, std::mt19937_64& random) {
    const int operations = 1 + static_cast<int>(random() % 8);
    for (int count = 0; count < operations; ++count) {
        const std::size_t size = input.size();
        switch (random() % 7) {
            case 0:
                if (size > 0) input[random() % size] ^= static_cast<std::uint8_t>(1U << (random() % 8));
                break;
            case 1:
                if (size > 0) input[random() % size] = static_cast<std::uint8_t>(random());
                break;
            case 2:
                input.insert(input.begin() + static_cast<std::ptrdiff_t>(size == 0 ? 0 : random() % size), static_cast<std::uint8_t>(random()));
                break;
            case 3:
                if (size > 0) input.erase(input.begin() + static_cast<std::ptrdiff_t>(random() % size));
                break;
            case 4:
                if (size > 4) {
                    const std::size_t start = random() % (size - 1);
                    const std::size_t length = 1 + random() % (size - start);
                    input.erase(input.begin() + static_cast<std::ptrdiff_t>(start), input.begin() + static_cast<std::ptrdiff_t>(start + length));
                }
                break;
            case 5: {
                const auto& other = corpus[random() % corpus.size()];
                if (!other.empty() && size < 65536) {
                    const std::size_t start = random() % other.size();
                    const std::size_t length = std::min<std::size_t>(other.size() - start, 1 + random() % 512);
                    input.insert(input.begin() + static_cast<std::ptrdiff_t>(size == 0 ? 0 : random() % size), other.begin() + static_cast<std::ptrdiff_t>(start), other.begin() + static_cast<std::ptrdiff_t>(start + length));
                }
                break;
            }
            default:
                if (size > 0 && size < 32768) {
                    const std::size_t start = random() % size;
                    input.insert(input.begin() + static_cast<std::ptrdiff_t>(start), 64 + random() % 512, input[start]);
                }
                break;
        }
    }
    if (input.size() > 65536) {
        input.resize(65536);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s corpusDirectory seconds [seed] [crashPath]\n", argv[0]);
        return 2;
    }
    const int seconds = std::atoi(argv[2]);
    std::mt19937_64 random(argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 1);
    if (argc > 4) {
        crashPath = argv[4];
    }
    if (&__sanitizer_set_death_callback != nullptr) {
        __sanitizer_set_death_callback(saveCurrentInput);
    }
    const auto corpus = loadCorpus(argv[1]);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    std::uint64_t iterations = 0;
    for (const auto& seedInput : corpus) {
        currentInput = seedInput;
        LLVMFuzzerTestOneInput(currentInput.data(), currentInput.size());
    }
    while (std::chrono::steady_clock::now() < deadline) {
        currentInput = corpus[random() % corpus.size()];
        mutate(currentInput, corpus, random);
        LLVMFuzzerTestOneInput(currentInput.data(), currentInput.size());
        ++iterations;
    }
    std::printf("FUZZ %s iterations=%llu seconds=%d corpus=%zu crashes=0\n", argv[0], static_cast<unsigned long long>(iterations), seconds, corpus.size());
    return 0;
}
