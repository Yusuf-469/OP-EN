#pragma once
// SIMULATED device adapters for the embedded-integration design (Task E).
// No real USB/serial/JTAG I/O happens here: each adapter models discovery,
// capability reporting, connect/disconnect and one telemetry message so the
// host workbench can be developed and tested before hardware is attached.

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace uew {

enum class DeviceKind { Microcontroller, Fpga, JtagAdapter };
enum class LinkState { Disconnected, Connected, Error };

const char* toString(DeviceKind kind);
const char* toString(LinkState state);

struct DeviceDescriptor {
    std::string id;         // stable id used by the UI/API
    DeviceKind kind;
    std::string model;      // assumed part, see docs/embedded_design.md
    std::string host_link;  // how the host would reach it
    std::string io_voltage;
};

struct Capability { std::string name, detail; };
struct TelemetryField { std::string name; double value; std::string unit; };

// The single telemetry message type shared by all devices.
struct TelemetryFrame {
    std::string device_id;
    std::uint32_t sequence = 0;
    std::vector<TelemetryField> fields;
};

class DeviceTimeout : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class DeviceAdapter {
public:
    virtual ~DeviceAdapter() = default;
    virtual DeviceDescriptor descriptor() const = 0;
    virtual std::vector<Capability> capabilities() const = 0;

    void connect();     // also clears an Error state (simulated re-enumeration)
    void disconnect();
    LinkState state() const { return state_; }

    // Throws std::logic_error when not connected, DeviceTimeout when a fault is injected.
    TelemetryFrame readTelemetry();

    // Test hook: the next `count` telemetry reads time out.
    void injectTimeouts(int count) { pendingTimeouts_ = count; }
    void markError() { state_ = LinkState::Error; }

protected:
    virtual std::vector<TelemetryField> sample(std::uint32_t sequence) const = 0;

private:
    LinkState state_ = LinkState::Disconnected;
    int pendingTimeouts_ = 0;
    std::uint32_t sequence_ = 0;
};

struct TelemetryAttempt {
    int attempt;
    bool timed_out;
    std::string message;
};

struct TelemetryOutcome {
    bool ok = false;
    TelemetryFrame frame;
    std::vector<TelemetryAttempt> attempts;
};

// Owns the adapters; adds retry-on-timeout so a transient failure recovers.
class DeviceManager {
public:
    static constexpr int kMaxAttempts = 3;
    static constexpr int kSimulatedTimeoutMs = 250;

    std::vector<DeviceAdapter*> discover();  // simulated bus scan: always finds the three devices
    DeviceAdapter& get(const std::string& id);
    TelemetryOutcome readWithRetry(const std::string& id);

private:
    std::vector<std::unique_ptr<DeviceAdapter>> devices_;
};

}  // namespace uew
