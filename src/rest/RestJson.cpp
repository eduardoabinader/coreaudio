#include "RestJson.h"

#include <cctype>
#include <cstdlib>
#include <limits>

namespace gigante {

namespace {

class JsonReader {
public:
    JsonReader(const std::string& source, int depthLimit) : text(source), maxDepth(depthLimit) {}

    std::optional<JsonObject> readDocument() {
        JsonObject object;
        skipSpace();
        if (!readObject(object, 1)) {
            return std::nullopt;
        }
        skipSpace();
        if (position != text.size()) {
            return std::nullopt;
        }
        return object;
    }

private:
    void skipSpace() {
        while (position < text.size() && (text[position] == ' ' || text[position] == '\t' || text[position] == '\n' || text[position] == '\r')) {
            ++position;
        }
    }

    bool consume(char expected) {
        skipSpace();
        if (position < text.size() && text[position] == expected) {
            ++position;
            return true;
        }
        return false;
    }

    bool readObject(JsonObject& object, int depth) {
        if (depth > maxDepth || !consume('{')) {
            return false;
        }
        if (consume('}')) {
            return true;
        }
        do {
            skipSpace();
            std::string key;
            JsonField field;
            if (!readString(key) || !consume(':') || !readValue(field, depth)) {
                return false;
            }
            if (!object.emplace(std::move(key), std::move(field)).second) {
                return false;
            }
        } while (consume(','));
        return consume('}');
    }

    bool readValue(JsonField& field, int depth) {
        skipSpace();
        if (position >= text.size()) {
            return false;
        }
        const char current = text[position];
        if (current == '"') {
            field.kind = JsonField::Kind::string;
            return readString(field.text);
        }
        if (current == '{' || current == '[') {
            const std::size_t start = position;
            if (!skipComposite(depth + 1)) {
                return false;
            }
            field.kind = JsonField::Kind::composite;
            field.text = text.substr(start, position - start);
            return true;
        }
        for (const char* literal : {"true", "false"}) {
            if (readLiteral(literal)) {
                field.kind = JsonField::Kind::boolean;
                field.text = literal;
                return true;
            }
        }
        if (readLiteral("null")) {
            field.kind = JsonField::Kind::null;
            return true;
        }
        const std::size_t start = position;
        if (!readNumber()) {
            return false;
        }
        field.kind = JsonField::Kind::number;
        field.text = text.substr(start, position - start);
        return true;
    }

    bool skipComposite(int depth) {
        if (depth > maxDepth) {
            return false;
        }
        if (text[position] == '{') {
            JsonObject ignored;
            return readObject(ignored, depth);
        }
        ++position;
        if (consume(']')) {
            return true;
        }
        do {
            JsonField ignored;
            if (!readValue(ignored, depth)) {
                return false;
            }
        } while (consume(','));
        return consume(']');
    }

    bool readLiteral(const char* literal) {
        const std::string word(literal);
        if (text.compare(position, word.size(), word) == 0) {
            position += word.size();
            return true;
        }
        return false;
    }

    bool readDigits() {
        const std::size_t start = position;
        while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])) != 0) {
            ++position;
        }
        return position > start;
    }

    bool readNumber() {
        if (position < text.size() && text[position] == '-') {
            ++position;
        }
        if (position < text.size() && text[position] == '0') {
            ++position;
        } else if (!readDigits()) {
            return false;
        }
        if (position < text.size() && text[position] == '.') {
            ++position;
            if (!readDigits()) {
                return false;
            }
        }
        if (position < text.size() && (text[position] == 'e' || text[position] == 'E')) {
            ++position;
            if (position < text.size() && (text[position] == '+' || text[position] == '-')) {
                ++position;
            }
            if (!readDigits()) {
                return false;
            }
        }
        return true;
    }

    bool readHex(unsigned& value) {
        if (position + 4 > text.size()) {
            return false;
        }
        value = 0;
        for (int index = 0; index < 4; ++index) {
            const char current = text[position++];
            value <<= 4;
            if (current >= '0' && current <= '9') {
                value |= static_cast<unsigned>(current - '0');
            } else if (current >= 'a' && current <= 'f') {
                value |= static_cast<unsigned>(current - 'a' + 10);
            } else if (current >= 'A' && current <= 'F') {
                value |= static_cast<unsigned>(current - 'A' + 10);
            } else {
                return false;
            }
        }
        return true;
    }

    static void appendUtf8(std::string& out, unsigned codePoint) {
        if (codePoint < 0x80) {
            out += static_cast<char>(codePoint);
        } else if (codePoint < 0x800) {
            out += static_cast<char>(0xc0 | (codePoint >> 6));
            out += static_cast<char>(0x80 | (codePoint & 0x3f));
        } else if (codePoint < 0x10000) {
            out += static_cast<char>(0xe0 | (codePoint >> 12));
            out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (codePoint & 0x3f));
        } else {
            out += static_cast<char>(0xf0 | (codePoint >> 18));
            out += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f));
            out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (codePoint & 0x3f));
        }
    }

    bool readString(std::string& out) {
        if (position >= text.size() || text[position] != '"') {
            return false;
        }
        ++position;
        while (position < text.size()) {
            const char current = text[position++];
            if (current == '"') {
                return true;
            }
            if (static_cast<unsigned char>(current) < 0x20) {
                return false;
            }
            if (current != '\\') {
                out += current;
                continue;
            }
            if (position >= text.size()) {
                return false;
            }
            const char escaped = text[position++];
            switch (escaped) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned codePoint = 0;
                    if (!readHex(codePoint)) {
                        return false;
                    }
                    if (codePoint >= 0xdc00 && codePoint <= 0xdfff) {
                        return false;
                    }
                    if (codePoint >= 0xd800 && codePoint <= 0xdbff) {
                        unsigned low = 0;
                        if (text.compare(position, 2, "\\u") != 0) {
                            return false;
                        }
                        position += 2;
                        if (!readHex(low) || low < 0xdc00 || low > 0xdfff) {
                            return false;
                        }
                        codePoint = 0x10000 + ((codePoint - 0xd800) << 10) + (low - 0xdc00);
                    }
                    appendUtf8(out, codePoint);
                    break;
                }
                default:
                    return false;
            }
        }
        return false;
    }

    const std::string& text;
    int maxDepth;
    std::size_t position = 0;
};

} // namespace

std::optional<JsonObject> parseJsonObject(const std::string& text, int maxDepth) {
    return JsonReader(text, maxDepth).readDocument();
}

bool jsonText(const JsonObject& object, const std::string& key, std::string& value) {
    const auto entry = object.find(key);
    if (entry == object.end() || entry->second.kind != JsonField::Kind::string) {
        return false;
    }
    value = entry->second.text;
    return true;
}

bool jsonUnsigned(const JsonObject& object, const std::string& key, std::uint64_t& value) {
    const auto entry = object.find(key);
    if (entry == object.end() || entry->second.kind != JsonField::Kind::number) {
        return false;
    }
    const std::string& digits = entry->second.text;
    if (digits.empty() || digits.size() > 20) {
        return false;
    }
    std::uint64_t result = 0;
    for (const char current : digits) {
        if (current < '0' || current > '9') {
            return false;
        }
        const auto digit = static_cast<std::uint64_t>(current - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
            return false;
        }
        result = result * 10 + digit;
    }
    value = result;
    return true;
}

std::string validUtf8(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    std::size_t index = 0;
    while (index < value.size()) {
        const auto lead = static_cast<unsigned char>(value[index]);
        std::size_t length = 0;
        unsigned minimum = 0;
        if (lead < 0x80) {
            result += static_cast<char>(lead);
            ++index;
            continue;
        } else if (lead >= 0xc2 && lead <= 0xdf) {
            length = 2;
            minimum = 0x80;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            length = 3;
            minimum = 0x800;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            length = 4;
            minimum = 0x10000;
        }
        bool valid = length != 0 && index + length <= value.size();
        unsigned codePoint = length == 2 ? (lead & 0x1f) : (length == 3 ? (lead & 0x0f) : (lead & 0x07));
        for (std::size_t offset = 1; valid && offset < length; ++offset) {
            const auto next = static_cast<unsigned char>(value[index + offset]);
            valid = (next & 0xc0) == 0x80;
            codePoint = (codePoint << 6) | (next & 0x3f);
        }
        valid = valid && codePoint >= minimum && codePoint <= 0x10ffff && (codePoint < 0xd800 || codePoint > 0xdfff);
        if (valid) {
            result.append(value, index, length);
            index += length;
        } else {
            result += '?';
            ++index;
        }
    }
    return result;
}

} // namespace gigante
