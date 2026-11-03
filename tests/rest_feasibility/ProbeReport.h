#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gigante {

class ProbeReport {
public:
    explicit ProbeReport(std::string probeName);

    void check(const std::string& name, bool passed, const std::string& detail = "");
    void note(const std::string& name, const std::string& detail);

    // Prints the summary and returns a process exit code.
    int finish();

    static std::size_t threadCount();
    static std::uint64_t residentBytes();
    static std::uint64_t peakResidentBytes();
    static double percentile(std::vector<double> values, double fraction);

private:
    std::string probeName;
    int passed = 0;
    int failed = 0;
};

} // namespace gigante
