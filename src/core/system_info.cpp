#include "system_info.h"

#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef UEW_BUILD_FLAGS
#define UEW_BUILD_FLAGS "unknown"
#endif

namespace uew {
namespace {

std::string compilerName() {
#if defined(__clang__)
    return "Clang " __clang_version__;
#elif defined(__GNUC__)
    return "GCC " __VERSION__;
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_FULL_VER);
#else
    return "unknown C++17 compiler";
#endif
}

#ifdef _WIN32
std::string readRegistryString(const char* subKey, const char* value) {
    char buffer[256] = {};
    DWORD size = sizeof buffer;
    if (RegGetValueA(HKEY_LOCAL_MACHINE, subKey, value, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS)
        return {};
    return buffer;
}

unsigned countPhysicalCores() {
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    std::vector<unsigned char> buffer(length);
    auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data());
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &length)) return 0;
    unsigned cores = 0;
    for (DWORD offset = 0; offset < length; ++cores) {
        offset += reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data() + offset)->Size;
    }
    return cores;
}

std::string windowsVersion() {
    const char* key = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    const std::string build = readRegistryString(key, "CurrentBuild");
    // ProductName still says "Windows 10" on Windows 11; build >= 22000 is Windows 11.
    const std::string name = !build.empty() && std::stoi(build) >= 22000 ? "Windows 11" : "Windows 10 or earlier";
    return name + " " + readRegistryString(key, "DisplayVersion") + " (build " + build + ")";
}
#endif

}  // namespace

SystemInfo querySystemInfo() {
    SystemInfo info;
    info.logical_cpus = std::thread::hardware_concurrency();
    info.compiler = compilerName();
    info.build_flags = UEW_BUILD_FLAGS;
#ifdef _WIN32
    info.cpu_model = readRegistryString("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString");
    info.physical_cores = countPhysicalCores();
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof mem;
    if (GlobalMemoryStatusEx(&mem)) info.ram_bytes = mem.ullTotalPhys;
    info.os = windowsVersion();
#else
    info.cpu_model = "unknown (non-Windows build)";
    info.os = "non-Windows";
#endif
    return info;
}

}  // namespace uew
