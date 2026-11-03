#pragma once

#include "ClientContext.h"

#include "gigante/RestTypes.h"

#include <cstdint>
#include <string>

namespace gigante {

// Streamed copy transfers. Uploads resume from the server's offset after a lost
// connection; downloads resume with Range. options.deadlineMilliseconds bounds the
// whole transfer, and options.maxAttempts the connections it may use.
class TransferClient final {
public:
    explicit TransferClient(ClientContext& context);

    RestResult upload(const std::string& resourceId, std::uint64_t size, const std::string& sha256, const RestUploadSource& source, const RestCallOptions& options);
    RestResult download(const std::string& resourceId, const RestDownloadSink& sink, std::uint64_t fromOffset, const RestCallOptions& options);

private:
    void cancelRemote(const std::string& transferId, const RestCallOptions& options);
    bool queryOffset(const std::string& transferId, const RestCallOptions& options, int budgetMilliseconds, std::uint64_t generation, std::uint64_t& offset);

    ClientContext& context;
};

} // namespace gigante
