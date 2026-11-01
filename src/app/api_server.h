#pragma once
// Transport layer: a localhost HTTP server that serves the web UI and maps
// /api/* requests onto the C++ core. No numerical or parsing logic lives here.

#include <functional>
#include <memory>
#include <string>

namespace uew {

class ApiServer {
public:
    using Logger = std::function<void(const std::string&)>;
    using Clock = std::function<double()>;  // milliseconds since the process was created

    // sampleRoot (may be empty) is served read-only at /samples.
    ApiServer(std::string webRoot, std::string sampleRoot, Logger log, Clock msSinceLaunch);
    ~ApiServer();

    // Binds 127.0.0.1 on a free port and serves on a background thread.
    // Returns the port; throws std::runtime_error if binding fails.
    int start();
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace uew
