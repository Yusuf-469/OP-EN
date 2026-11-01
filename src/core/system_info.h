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

}  // namespace uew
