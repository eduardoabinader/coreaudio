#include "RestTestSupport.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

using namespace gigante;
using namespace restTest;

namespace {

void writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::string makeLogTree() {
    char pattern[] = "/tmp/giganteRestLogsXXXXXX";
    const char* created = mkdtemp(pattern);
    const std::string root = created == nullptr ? "" : created;
    mkdir((root + "/sub").c_str(), 0700);
    mkdir((root + "/a").c_str(), 0700);
    mkdir((root + "/a/b").c_str(), 0700);
    mkdir((root + "/a/b/c").c_str(), 0700);
    writeFile(root + "/app.log",
              "2026-11-03T10:00:00.123Z INFO started\n"
              "[2026-11-03 10:00:01] ERROR first failure\n"
              "no timestamp ERROR plain\n"
              "2026-11-03T10:00:02+01:00 INFO done\r\n");
    writeFile(root + "/notes.txt", "ERROR in notes\n");
    writeFile(root + "/data.bin", "ERROR in a binary file\n");
    writeFile(root + "/.hidden.log", "ERROR hidden\n");
    writeFile(root + "/sub/nested.log", "ERROR nested\nbad byte \xff ERROR\n");
    writeFile(root + "/a/b/c/deep.log", "ERROR too deep\n");
    writeFile("/tmp/giganteRestOutside.log", "ERROR outside the root\n");
    symlink("/tmp/giganteRestOutside.log", (root + "/linked.log").c_str());
    symlink("/tmp", (root + "/linkedDir").c_str());
    return root;
}

void removeLogTree(const std::string& root) {
    const std::string command = "rm -rf '" + root + "' /tmp/giganteRestOutside.log";
    if (std::system(command.c_str()) != 0) {
        std::printf("could not remove %s\n", root.c_str());
    }
}

void getLog() {
    const std::string root = makeLogTree();
    RestServer server(developmentServer());
    RestLogQueryLimits limits;
    limits.maxDepth = 2;
    server.enableLogQuery({root}, limits);
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));

    RestResult result = client.getLog("*", {"ERROR"});
    check(result.status == 200, "getLogAnswers", result.body);
    check(contains(result.body, "\"file\":\"app.log\",\"line\":2,\"timestamp\":\"2026-11-03 10:00:01\""), "bracketTimestamp", result.body);
    check(contains(result.body, "\"line\":3,\"timestamp\":null"), "missingTimestampIsNull");
    check(contains(result.body, "\"file\":\"notes.txt\"") && contains(result.body, "\"file\":\"sub/nested.log\""), "logSuffixesAndSubfolders");
    check(!contains(result.body, "binary file") && !contains(result.body, "hidden") && !contains(result.body, "outside") && !contains(result.body, "too deep"),
          "excludedFilesNeverRead", result.body);
    check(!contains(result.body, root) && !contains(result.body, "/tmp"), "rootNeverExposed");
    check(contains(result.body, "bad byte ? ERROR"), "invalidUtf8Replaced");
    check(contains(result.body, "\"truncated\":false"), "notTruncated");

    result = client.getLog("app.log", {"INFO", "done"});
    check(contains(result.body, "2026-11-03T10:00:02+01:00") && contains(result.body, "\"line\":4") && !contains(result.body, "started"), "filtersAllMatch", result.body);
    check(!contains(result.body, "\\r"), "carriageReturnStripped");

    result = client.getLog("*.log", {"ERROR"}, 1);
    check(contains(result.body, "\"truncated\":true") && contains(result.body, "bad byte") && !contains(result.body, "first failure"), "maxLinesKeepsNewest", result.body);

    for (const char* badGlob : {"../app.log", "/etc/hosts", "sub/nested.log", ".hidden.log", ""}) {
        result = client.getLog(badGlob);
        check(result.status == 400 && contains(result.body, "invalidFileGlob"), "pathGlobRefused", badGlob);
    }
    removeLogTree(root);
}

void byteCap() {
    const std::string root = makeLogTree();
    std::string many;
    for (int index = 0; index < 500; ++index) {
        many += "ERROR line " + std::to_string(index) + "\n";
    }
    writeFile(root + "/many.log", many);
    RestServer server(developmentServer());
    RestLogQueryLimits limits;
    limits.maxBytes = 2000;
    server.enableLogQuery({root}, limits);
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));
    const RestResult result = client.getLog("many.log", {}, 1000);
    check(result.status == 200 && result.body.size() < 2300 && contains(result.body, "\"truncated\":true") && contains(result.body, "ERROR line 499"), "byteCapApplied", std::to_string(result.body.size()));
    removeLogTree(root);
}

void commands() {
    std::mutex mutex;
    std::vector<RestLogMarker> markers;
    bool providerFails = false;
    RestServer server(developmentServer());
    server.setCommandStatusProvider([&] {
        if (providerFails) {
            throw std::runtime_error("/private/manager failed");
        }
        RestCommandRecord capture;
        capture.name = "capturePlayback";
        capture.alive = true;
        capture.processId = 4242;
        RestCommandRecord encoder;
        encoder.name = "encoder";
        encoder.exitCode = 3;
        encoder.detail = "exited";
        return std::vector<RestCommandRecord>{capture, encoder};
    });
    server.setLogMarkerWriter([&](const RestLogMarker& marker) {
        if (marker.command != "capturePlayback") {
            return false;
        }
        std::lock_guard<std::mutex> lock(mutex);
        markers.push_back(marker);
        return true;
    });
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));

    RestResult result = client.cmdStatus("capture*");
    check(result.status == 200 && contains(result.body, "\"name\":\"capturePlayback\",\"alive\":true,\"processId\":4242,\"exitCode\":null") && !contains(result.body, "encoder"), "cmdStatusGlob", result.body);
    result = client.cmdStatus();
    check(contains(result.body, "\"processId\":null,\"exitCode\":3"), "cmdStatusAll", result.body);

    result = client.appendLogMarker("capturePlayback", "artifactHeard", "click at 12.3 s");
    check(result.status == 200 && markers.size() == 1 && markers[0].origin == "client" && markers[0].text == "click at 12.3 s", "markerAppended", result.body);
    check(client.appendLogMarker("unknown", "event", "text").status == 404, "unknownCommand404");
    check(client.appendLogMarker("capturePlayback", "event", "two\nlines").status == 400, "markerInjectionRefused");
    check(client.appendLogMarker("capture Playback", "event", "text").status == 400, "markerNameValidated");
    check(client.appendLogMarker("capturePlayback", "event", std::string(5000, 'x')).status == 400, "markerTextBounded");
    check(client.call(RestMethod::post, "/core/v1/appendLogMarker", std::string(9000, ' ')).status == 413, "markerBodyLimit");

    providerFails = true;
    result = client.cmdStatus();
    check(result.status == 500 && !contains(result.body, "private"), "providerFailureHidden", result.body);
    check(client.getLog("*").status == 404, "getLogAbsentWhenNotRegistered");

    const RestResult capabilities = client.getCapabilities();
    check(contains(capabilities.body, "\"cmdStatus\"") && contains(capabilities.body, "\"appendLogMarker\"") && !contains(capabilities.body, "\"getLog\""), "capabilitiesListDiagnostics", capabilities.body);
}

} // namespace

int main() {
    getLog();
    byteCap();
    commands();
    return finish("RestDiagnosticsTests");
}
