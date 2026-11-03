#pragma once

#include <cstdint>
#include <cstdio>
#include <map>
#include <optional>
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

// A parsed member of a JSON object. Nested arrays and objects are kept as raw text.
struct JsonField {
    enum class Kind { string, number, boolean, null, composite };
    Kind kind = Kind::null;
    std::string text;
};

using JsonObject = std::map<std::string, JsonField>;

// Parses one top-level object, strictly: duplicate keys, trailing text and nesting
// deeper than maxDepth are refused.
std::optional<JsonObject> parseJsonObject(const std::string& text, int maxDepth = 8);

bool jsonText(const JsonObject& object, const std::string& key, std::string& value);
bool jsonUnsigned(const JsonObject& object, const std::string& key, std::uint64_t& value);

// Replaces bytes that are not valid UTF-8 with '?', so the result can go into jsonString.
std::string validUtf8(const std::string& value);

} // namespace gigante
