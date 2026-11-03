// parse_sse_line is private upstream; this target reaches it without modifying the pin.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define private public
#include <httplib.h>
#undef private

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    static httplib::Client client("http://127.0.0.1:1");
    httplib::sse::SSEClient stream(client, "/events");
    httplib::sse::SSEMessage message;
    int retryMilliseconds = 0;
    bool hasData = false;
    bool hasId = false;
    std::string input(reinterpret_cast<const char*>(data), size);
    std::size_t start = 0;
    while (start <= input.size()) {
        const std::size_t end = input.find_first_of("\r\n", start);
        const std::string line = input.substr(start, end == std::string::npos ? std::string::npos : end - start);
        stream.parse_sse_line(line, message, retryMilliseconds, hasData, hasId);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return 0;
}
