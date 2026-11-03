#include "ProbeReport.h"

#include <sys/resource.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <utility>

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

namespace gigante {

ProbeReport::ProbeReport(std::string name) : probeName(std::move(name)) {}

void ProbeReport::check(const std::string& name, bool result, const std::string& detail) {
    (result ? passed : failed) += 1;
    std::printf("%s %s %s%s%s\n", result ? "PASS" : "FAIL", probeName.c_str(), name.c_str(), detail.empty() ? "" : " : ", detail.c_str());
    std::fflush(stdout);
}

void ProbeReport::note(const std::string& name, const std::string& detail) {
    std::printf("NOTE %s %s : %s\n", probeName.c_str(), name.c_str(), detail.c_str());
    std::fflush(stdout);
}

int ProbeReport::finish() {
    std::printf("SUMMARY %s passed=%d failed=%d\n", probeName.c_str(), passed, failed);
    std::fflush(stdout);
    return failed == 0 ? 0 : 1;
}

std::size_t ProbeReport::threadCount() {
#if defined(__APPLE__)
    thread_act_array_t threads = nullptr;
    mach_msg_type_number_t count = 0;
    if (task_threads(mach_task_self(), &threads, &count) != KERN_SUCCESS) {
        return 0;
    }
    for (mach_msg_type_number_t index = 0; index < count; ++index) {
        mach_port_deallocate(mach_task_self(), threads[index]);
    }
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(threads), count * sizeof(thread_act_t));
    return count;
#else
    std::ifstream status("/proc/self/status");
    std::string key;
    std::size_t value = 0;
    while (status >> key) {
        if (key == "Threads:") {
            status >> value;
            return value;
        }
    }
    return 0;
#endif
}

std::uint64_t ProbeReport::residentBytes() {
#if defined(__APPLE__)
    mach_task_basic_info information{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&information), &count) != KERN_SUCCESS) {
        return 0;
    }
    return information.resident_size;
#else
    std::ifstream statm("/proc/self/statm");
    std::uint64_t total = 0;
    std::uint64_t resident = 0;
    statm >> total >> resident;
    return resident * 4096;
#endif
}

std::uint64_t ProbeReport::peakResidentBytes() {
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
#if defined(__APPLE__)
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
}

double ProbeReport::percentile(std::vector<double> values, double fraction) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1));
    return values[index];
}

} // namespace gigante
