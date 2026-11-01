#include "hardware.h"

#include <cmath>

namespace uew {

const char* toString(DeviceKind kind) {
    switch (kind) {
        case DeviceKind::Microcontroller: return "microcontroller";
        case DeviceKind::Fpga: return "fpga";
        case DeviceKind::JtagAdapter: return "jtag-debug-adapter";
    }
    return "unknown";
}

const char* toString(LinkState state) {
    switch (state) {
        case LinkState::Disconnected: return "disconnected";
        case LinkState::Connected: return "connected";
        case LinkState::Error: return "error";
    }
    return "unknown";
}

void DeviceAdapter::connect() {
    pendingTimeouts_ = 0;
    state_ = LinkState::Connected;
}

void DeviceAdapter::disconnect() { state_ = LinkState::Disconnected; }

TelemetryFrame DeviceAdapter::readTelemetry() {
    if (state_ == LinkState::Error)
        throw std::logic_error("Device is in an error state; reconnect it before reading telemetry.");
    if (state_ != LinkState::Connected)
        throw std::logic_error("Device is not connected; connect it before reading telemetry.");
    if (pendingTimeouts_ > 0) {
        --pendingTimeouts_;
        throw DeviceTimeout("No telemetry reply within " + std::to_string(DeviceManager::kSimulatedTimeoutMs) +
                            " ms (simulated timeout).");
    }
    const std::uint32_t seq = ++sequence_;
    return {descriptor().id, seq, sample(seq)};
}

namespace {

// Small deterministic wobble so successive simulated frames differ.
double wobble(std::uint32_t seq, double amplitude) { return amplitude * std::sin(0.7 * seq); }

class MicrocontrollerMock : public DeviceAdapter {
public:
    DeviceDescriptor descriptor() const override {
        return {"mcu0", DeviceKind::Microcontroller, "STM32 Nucleo-F446RE (assumed)",
                "USB CDC virtual COM port via on-board ST-LINK, 115200 8N1", "3.3 V I/O (some pins 5 V tolerant)"};
    }
    std::vector<Capability> capabilities() const override {
        return {{"uart-telemetry", "Framed telemetry over virtual COM port"},
                {"adc", "12-bit ADC, internal temperature sensor"},
                {"gpio", "3.3 V GPIO on Arduino/Morpho headers"},
                {"swd-debug", "Debug via on-board ST-LINK (SWD), not via the shared JTAG adapter"}};
    }

protected:
    std::vector<TelemetryField> sample(std::uint32_t seq) const override {
        return {{"die_temperature", 31.5 + wobble(seq, 0.4), "C"},
                {"vdd", 3.30 + wobble(seq, 0.01), "V"},
                {"uptime", 1.0 * seq, "s"}};
    }
};

class FpgaMock : public DeviceAdapter {
public:
    DeviceDescriptor descriptor() const override {
        return {"fpga0", DeviceKind::Fpga, "Digilent Arty A7-35T (Xilinx XC7A35T, assumed)",
                "USB-UART via on-board FTDI FT2232H channel B", "3.3 V Pmod / bank I/O, 1.0 V VCCINT core"};
    }
    std::vector<Capability> capabilities() const override {
        return {{"uart-telemetry", "Soft UART in fabric reports XADC readings"},
                {"xadc", "On-die temperature and supply monitor"},
                {"jtag-config", "Bitstream load via JTAG (not performed: simulated, unprogrammed)"},
                {"pmod-io", "3.3 V Pmod headers; not 5 V tolerant"}};
    }

protected:
    std::vector<TelemetryField> sample(std::uint32_t seq) const override {
        return {{"xadc_temperature", 38.2 + wobble(seq, 0.6), "C"},
                {"vccint", 1.00 + wobble(seq, 0.004), "V"},
                {"fabric_clock", 100.0, "MHz"}};
    }
};

class JtagAdapterMock : public DeviceAdapter {
public:
    DeviceDescriptor descriptor() const override {
        return {"jtag0", DeviceKind::JtagAdapter, "SEGGER J-Link EDU Mini (assumed)",
                "USB 2.0 to host; 10-pin 1.27 mm Cortex Debug header to target",
                "3.3 V target interface (5 V tolerant), reads VTref; does not power the target"};
    }
    std::vector<Capability> capabilities() const override {
        return {{"jtag", "Drives TCK/TMS/TDI/nTRST, samples TDO of ONE target at a time"},
                {"swd", "SWDIO/SWCLK alternative for Arm targets"},
                {"vtref-sense", "Measures target I/O voltage before driving pins"},
                {"no-compute", "Debug interface only: runs no user workload"}};
    }

protected:
    std::vector<TelemetryField> sample(std::uint32_t seq) const override {
        return {{"vtref", 3.30 + wobble(seq, 0.01), "V"},
                {"tck_frequency", 4.0, "MHz"},
                {"chain_devices", 1.0, "count"}};
    }
};

}  // namespace

std::vector<DeviceAdapter*> DeviceManager::discover() {
    if (devices_.empty()) {
        devices_.push_back(std::make_unique<MicrocontrollerMock>());
        devices_.push_back(std::make_unique<FpgaMock>());
        devices_.push_back(std::make_unique<JtagAdapterMock>());
    }
    std::vector<DeviceAdapter*> found;
    for (auto& d : devices_) found.push_back(d.get());
    return found;
}

DeviceAdapter& DeviceManager::get(const std::string& id) {
    for (auto* d : discover())
        if (d->descriptor().id == id) return *d;
    throw std::invalid_argument("Unknown device id '" + id + "'. Run discovery first.");
}

TelemetryOutcome DeviceManager::readWithRetry(const std::string& id) {
    DeviceAdapter& device = get(id);
    TelemetryOutcome outcome;
    for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
        try {
            outcome.frame = device.readTelemetry();
            outcome.ok = true;
            outcome.attempts.push_back({attempt, false, "Telemetry frame received."});
            return outcome;
        } catch (const DeviceTimeout& e) {
            outcome.attempts.push_back({attempt, true, e.what()});
        }
    }
    device.markError();
    outcome.attempts.push_back({kMaxAttempts, true, "Retries exhausted; device marked as error. Reconnect to recover."});
    return outcome;
}

}  // namespace uew
