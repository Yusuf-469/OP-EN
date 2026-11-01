#pragma once
// Host and build description reported next to every benchmark.

#include <cstdint>
#include <string>

namespace uew {

struct SystemInfo {
    std::string cpu_model;
    unsigned physical_cores = 0;  // 0 = unknown on this platform
    unsigned logical_cpus = 0;
    std::uint64_t ram_bytes = 0;
    std::string os;
    std::string compiler;
    std::string build_flags;
};

SystemInfo querySystemInfo();

// Windows 11 may apply EcoQoS power throttling to a background process and
// move its threads to efficiency cores at low clock, which made benchmark runs
// bimodal. This opts the process out (documented SetProcessInformation API).
// Returns false if the OS refused or the platform is not Windows.
bool disablePowerThrottling();

}  // namespace uew
