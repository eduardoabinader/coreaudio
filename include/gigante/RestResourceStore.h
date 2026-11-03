#pragma once

#include "gigante/RestTypes.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gigante {

// Receives an upload. commit() makes the whole resource visible at once; discard()
// removes everything staged. Exactly one of them is called.
class RestUploadSink {
public:
    virtual ~RestUploadSink() = default;
    virtual bool write(const char* data, std::size_t size) = 0;
    virtual bool commit() = 0;
    virtual void discard() = 0;
};

class RestDownloadSource {
public:
    virtual ~RestDownloadSource() = default;
    virtual std::uint64_t size() const = 0;
    virtual bool read(std::uint64_t offset, char* buffer, std::size_t length) = 0;
};

// Implemented by the project. The engine only ever passes resource IDs made of
// letters, digits, '-', '_', '.' and '~', never a path. Calls may be concurrent.
class RestResourceStore {
public:
    virtual ~RestResourceStore() = default;
    virtual std::vector<RestResourceInfo> list() = 0;
    virtual std::optional<RestResourceInfo> describe(const std::string& id) = 0;
    virtual std::unique_ptr<RestUploadSink> beginUpload(const std::string& id, std::uint64_t size) = 0;
    virtual std::unique_ptr<RestDownloadSource> openRead(const std::string& id) = 0;
    virtual std::uint64_t freeBytes() = 0;
};

} // namespace gigante
