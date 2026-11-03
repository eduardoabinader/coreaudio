#pragma once

#include <cstdio>
#include <string>

namespace gigante {

inline std::string jsonString(const std::string& value) {
    std::string result = "\"";
    result.reserve(value.size() + 2);
    for (const char current : value) {
        switch (current) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(current) < 0x20) {
                    char escaped[8];
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(current)));
                    result += escaped;
                } else {
                    result += current;
                }
        }
    }
    result += '"';
    return result;
}

inline std::string jsonError(const std::string& code, const std::string& message, const std::string& requestId, bool retryable) {
    return "{\"version\":1,\"code\":" + jsonString(code) + ",\"message\":" + jsonString(message) +
           ",\"requestId\":" + jsonString(requestId) + ",\"retryable\":" + (retryable ? "true" : "false") + "}";
}

} // namespace gigante
