#include "DiagnosticsEndpoints.h"

#include "RestJson.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <optional>
#include <utility>

namespace gigante {

namespace {

constexpr std::size_t maxLineBytes = 4096;
constexpr std::size_t maxGlobBytes = 128;
constexpr std::size_t maxFilterBytes = 256;
constexpr std::size_t maxFilters = 8;
constexpr std::size_t maxMarkerTextBytes = 4096;

struct LogFile {
    std::string path;
    std::string relative;
};

struct LogLine {
    std::string file;
    std::size_t number = 0;
    std::string text;
};

bool hasLogSuffix(const std::string& name) {
    for (const char* suffix : {".log", ".txt", ".jsonl"}) {
        const std::string ending(suffix);
        if (name.size() > ending.size() && name.compare(name.size() - ending.size(), ending.size(), ending) == 0) {
            return true;
        }
    }
    return false;
}

// A file glob names files, never a path: no separators, no leading dot, no "..".
bool isSafeGlob(const std::string& glob) {
    if (glob.empty() || glob.size() > maxGlobBytes || glob.front() == '.' || glob.find("..") != std::string::npos) {
        return false;
    }
    for (const char current : glob) {
        if (current == '/' || current == '\\' || static_cast<unsigned char>(current) < 0x20) {
            return false;
        }
    }
    return true;
}

bool isMarkerName(const std::string& value) {
    if (value.empty() || value.size() > 64) {
        return false;
    }
    for (const char current : value) {
        if (std::isalnum(static_cast<unsigned char>(current)) == 0 && current != '-' && current != '_' && current != '.') {
            return false;
        }
    }
    return true;
}

// Symbolic links are never followed, whether they name files or directories.
void collectFiles(const std::string& directory, const std::string& relative, int depth, const RestLogQueryLimits& limits, const std::string& glob, std::vector<LogFile>& files) {
    if (depth > limits.maxDepth) {
        return;
    }
    DIR* handle = opendir(directory.c_str());
    if (handle == nullptr) {
        return;
    }
    std::vector<std::string> names;
    while (const dirent* entry = readdir(handle)) {
        const std::string name = entry->d_name;
        if (!name.empty() && name.front() != '.') {
            names.push_back(name);
        }
    }
    closedir(handle);
    std::sort(names.begin(), names.end());
    for (const std::string& name : names) {
        const std::string path = directory + "/" + name;
        const std::string childRelative = relative.empty() ? name : relative + "/" + name;
        struct stat info {};
        if (lstat(path.c_str(), &info) != 0 || S_ISLNK(info.st_mode)) {
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            collectFiles(path, childRelative, depth + 1, limits, glob, files);
        } else if (S_ISREG(info.st_mode) && hasLogSuffix(name) && DiagnosticsEndpoints::globMatch(glob, name)) {
            files.push_back({path, childRelative});
        }
    }
}

bool containsAll(const std::string& line, const std::vector<std::string>& filters) {
    return std::all_of(filters.begin(), filters.end(), [&line](const std::string& filter) { return line.find(filter) != std::string::npos; });
}

bool digitsAt(const std::string& text, std::size_t start, std::size_t count) {
    if (start + count > text.size()) {
        return false;
    }
    for (std::size_t index = start; index < start + count; ++index) {
        if (std::isdigit(static_cast<unsigned char>(text[index])) == 0) {
            return false;
        }
    }
    return true;
}

// Recognizes a leading YYYY-MM-DD[T ]hh:mm:ss, optionally bracketed, with fraction and zone.
std::string leadingTimestamp(const std::string& line) {
    const std::size_t start = !line.empty() && line.front() == '[' ? 1 : 0;
    if (line.size() < start + 19) {
        return "";
    }
    if (!digitsAt(line, start, 4) || line[start + 4] != '-' || !digitsAt(line, start + 5, 2) || line[start + 7] != '-' ||
        !digitsAt(line, start + 8, 2) || (line[start + 10] != 'T' && line[start + 10] != ' ') ||
        !digitsAt(line, start + 11, 2) || line[start + 13] != ':' || !digitsAt(line, start + 14, 2) || line[start + 16] != ':' ||
        !digitsAt(line, start + 17, 2)) {
        return "";
    }
    std::size_t end = start + 19;
    if (end < line.size() && (line[end] == '.' || line[end] == ',')) {
        ++end;
        while (end < line.size() && std::isdigit(static_cast<unsigned char>(line[end])) != 0) {
            ++end;
        }
    }
    if (end < line.size() && line[end] == 'Z') {
        ++end;
    } else if (end < line.size() && (line[end] == '+' || line[end] == '-') && digitsAt(line, end + 1, 2)) {
        end += 3;
        if (end < line.size() && line[end] == ':') {
            ++end;
        }
        if (digitsAt(line, end, 2)) {
            end += 2;
        }
    }
    return line.substr(start, end - start);
}

} // namespace

DiagnosticsEndpoints::DiagnosticsEndpoints(DiagnosticsSources diagnosticsSources, MiddlewareErrorBuilder builder, ResponseFinisher responseFinisher, ServiceLog serviceLog)
    : sources(std::move(diagnosticsSources)), errorBuilder(std::move(builder)), finisher(std::move(responseFinisher)), log(std::move(serviceLog)) {}

bool DiagnosticsEndpoints::globMatch(const std::string& pattern, const std::string& text) {
    std::size_t patternIndex = 0;
    std::size_t textIndex = 0;
    std::size_t starIndex = std::string::npos;
    std::size_t resumeIndex = 0;
    while (textIndex < text.size()) {
        if (patternIndex < pattern.size() && (pattern[patternIndex] == '?' || pattern[patternIndex] == text[textIndex])) {
            ++patternIndex;
            ++textIndex;
        } else if (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
            starIndex = patternIndex++;
            resumeIndex = textIndex;
        } else if (starIndex != std::string::npos) {
            patternIndex = starIndex + 1;
            textIndex = ++resumeIndex;
        } else {
            return false;
        }
    }
    while (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
        ++patternIndex;
    }
    return patternIndex == pattern.size();
}

std::vector<std::string> DiagnosticsEndpoints::routeNames() const {
    std::vector<std::string> names;
    if (!sources.logRoots.empty()) {
        names.emplace_back("getLog");
    }
    if (sources.commandStatus) {
        names.emplace_back("cmdStatus");
    }
    if (sources.markerWriter) {
        names.emplace_back("appendLogMarker");
    }
    return names;
}

void DiagnosticsEndpoints::registerRoutes(Router& router) {
    RouteInfo read;
    read.authRequired = true;
    read.idempotent = true;
    RouteInfo marker;
    marker.authRequired = true;
    marker.maxBodyBytes = 8192;
    if (!sources.logRoots.empty()) {
        router.addCommon(HttpMethod::get, "/core/v1/getLog", [this](const TransportRequest& request) { return getLog(request); }, read);
    }
    if (sources.commandStatus) {
        router.addCommon(HttpMethod::get, "/core/v1/cmdStatus", [this](const TransportRequest& request) { return cmdStatus(request); }, read);
    }
    if (sources.markerWriter) {
        router.addCommon(HttpMethod::post, "/core/v1/appendLogMarker", [this](const TransportRequest& request) { return appendLogMarker(request); }, marker);
    }
}

TransportResponse DiagnosticsEndpoints::json(const TransportRequest& request, std::string body) {
    TransportResponse response;
    response.body = std::move(body);
    finisher(response, request);
    return response;
}

TransportResponse DiagnosticsEndpoints::getLog(const TransportRequest& request) {
    const auto file = request.query.find("file");
    const std::string glob = file == request.query.end() ? "*" : file->second;
    if (request.query.count("file") > 1 || !isSafeGlob(glob)) {
        return errorBuilder(400, "invalidFileGlob", "file must be one file-name glob, without a path.", false, request);
    }
    std::vector<std::string> filters;
    const auto range = request.query.equal_range("filter");
    for (auto entry = range.first; entry != range.second; ++entry) {
        if (entry->second.empty() || entry->second.size() > maxFilterBytes || filters.size() == maxFilters) {
            return errorBuilder(400, "invalidFilter", "Each filter is plain text of 1 to 256 bytes, at most 8 of them.", false, request);
        }
        filters.push_back(entry->second);
    }
    std::size_t maxLines = 200;
    const auto requested = request.query.find("maxLines");
    if (requested != request.query.end()) {
        char* end = nullptr;
        const unsigned long long value = std::strtoull(requested->second.c_str(), &end, 10);
        if (requested->second.empty() || end == nullptr || *end != '\0' || value == 0) {
            return errorBuilder(400, "invalidMaxLines", "maxLines must be a positive number.", false, request);
        }
        maxLines = static_cast<std::size_t>(std::min<unsigned long long>(value, sources.limits.maxLines));
    }
    maxLines = std::min(maxLines, sources.limits.maxLines);

    std::vector<LogFile> files;
    for (std::size_t index = 0; index < sources.logRoots.size(); ++index) {
        // A root's own name never reaches the client; with several roots, an index prefixes the path.
        const std::string prefix = sources.logRoots.size() > 1 ? std::to_string(index) : "";
        collectFiles(sources.logRoots[index], prefix, 0, sources.limits, glob, files);
    }
    bool truncated = false;
    if (files.size() > sources.limits.maxFiles) {
        files.resize(sources.limits.maxFiles);
        truncated = true;
    }

    // The most recent matches are the useful ones, so keep the last maxLines.
    std::deque<LogLine> kept;
    for (const LogFile& logFile : files) {
        std::FILE* stream = std::fopen(logFile.path.c_str(), "rb");
        if (stream == nullptr) {
            continue;
        }
        std::string line;
        std::size_t number = 0;
        bool overlong = false;
        int character = 0;
        do {
            character = std::fgetc(stream);
            if (character != EOF && character != '\n') {
                if (line.size() < maxLineBytes) {
                    line += static_cast<char>(character);
                } else {
                    overlong = true;
                }
                continue;
            }
            if (character == EOF && line.empty()) {
                break;
            }
            ++number;
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (containsAll(line, filters)) {
                kept.push_back({logFile.relative, number, std::move(line)});
                if (kept.size() > maxLines) {
                    kept.pop_front();
                    truncated = true;
                }
            }
            truncated = truncated || overlong;
            overlong = false;
            line.clear();
        } while (character != EOF);
        std::fclose(stream);
    }

    std::vector<std::string> rendered;
    std::size_t bytes = 0;
    for (auto entry = kept.rbegin(); entry != kept.rend(); ++entry) {
        const std::string timestamp = leadingTimestamp(entry->text);
        std::string item = "{\"file\":" + jsonString(validUtf8(entry->file)) + ",\"line\":" + std::to_string(entry->number) +
                           ",\"timestamp\":" + (timestamp.empty() ? std::string("null") : jsonString(timestamp)) +
                           ",\"text\":" + jsonString(validUtf8(entry->text)) + "}";
        if (bytes + item.size() > sources.limits.maxBytes) {
            truncated = true;
            break;
        }
        bytes += item.size() + 1;
        rendered.push_back(std::move(item));
    }
    std::string body = "{\"files\":" + std::to_string(files.size()) + ",\"lines\":[";
    for (auto item = rendered.rbegin(); item != rendered.rend(); ++item) {
        body += (item == rendered.rbegin() ? "" : ",") + *item;
    }
    return json(request, body + "],\"truncated\":" + (truncated ? "true" : "false") + "}");
}

TransportResponse DiagnosticsEndpoints::cmdStatus(const TransportRequest& request) {
    const auto name = request.query.find("name");
    const std::string glob = name == request.query.end() ? "*" : name->second;
    if (glob.empty() || glob.size() > maxGlobBytes) {
        return errorBuilder(400, "invalidNameGlob", "name must be a glob of 1 to 128 bytes.", false, request);
    }
    std::vector<RestCommandRecord> records;
    try {
        records = sources.commandStatus();
    } catch (...) {
        log(RestLogLevel::error, "restCommandStatusFailed", "");
        return errorBuilder(500, "providerFailed", "The command status provider failed.", true, request);
    }
    std::string body = "{\"commands\":[";
    bool first = true;
    for (const RestCommandRecord& record : records) {
        if (!globMatch(glob, record.name)) {
            continue;
        }
        body += std::string(first ? "" : ",") + "{\"name\":" + jsonString(validUtf8(record.name)) +
                ",\"alive\":" + (record.alive ? "true" : "false") +
                ",\"processId\":" + (record.processId < 0 ? std::string("null") : std::to_string(record.processId)) +
                ",\"exitCode\":" + (record.exitCode ? std::to_string(*record.exitCode) : std::string("null")) +
                ",\"detail\":" + jsonString(validUtf8(record.detail)) + "}";
        first = false;
    }
    return json(request, body + "]}");
}

TransportResponse DiagnosticsEndpoints::appendLogMarker(const TransportRequest& request) {
    const std::optional<JsonObject> object = parseJsonObject(request.body);
    RestLogMarker marker;
    marker.origin = "client";
    if (!object || !jsonText(*object, "command", marker.command) || !jsonText(*object, "event", marker.event) || !jsonText(*object, "text", marker.text)) {
        return errorBuilder(400, "invalidMarker", "The body needs command, event and text.", false, request);
    }
    if (object->count("origin") != 0 && !jsonText(*object, "origin", marker.origin)) {
        return errorBuilder(400, "invalidMarker", "origin must be a string.", false, request);
    }
    const bool textIsSafe = marker.text.size() <= maxMarkerTextBytes &&
                            std::none_of(marker.text.begin(), marker.text.end(), [](char current) {
                                return static_cast<unsigned char>(current) < 0x20 && current != '\t';
                            });
    if (!isMarkerName(marker.command) || !isMarkerName(marker.event) || !isMarkerName(marker.origin) || !textIsSafe) {
        return errorBuilder(400, "invalidMarker", "Names are short identifiers; text is one line of at most 4096 bytes.", false, request);
    }
    marker.text = validUtf8(marker.text);
    bool appended = false;
    try {
        appended = sources.markerWriter(marker);
    } catch (...) {
        log(RestLogLevel::error, "restLogMarkerFailed", marker.command);
        return errorBuilder(500, "providerFailed", "The log marker writer failed.", true, request);
    }
    if (!appended) {
        return errorBuilder(404, "unknownCommand", "No command with that name is known.", false, request);
    }
    return json(request, "{\"appended\":true,\"command\":" + jsonString(marker.command) + "}");
}

} // namespace gigante
