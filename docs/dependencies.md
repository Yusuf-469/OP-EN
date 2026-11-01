# Dependencies, versions and licences

All runtime dependencies are **vendored at pinned versions** in the repository, so the build and the packaged
app need no network access. This file is the dependency lock record; SHA-256 prefixes identify the exact files.

## Shipped in the application

| Component | Version | Where | Licence | SHA-256 (first 16) | Used for |
|---|---|---|---|---|---|
| cpp-httplib | 0.18.3 | `third_party/cpp-httplib/httplib.h` | MIT (`third_party/licenses/cpp-httplib-LICENSE.txt`) | A0A0C13DC0866638 | localhost HTTP transport |
| three.js | 0.160.0 | `src/web/vendor/three.module.min.js` | MIT (`third_party/licenses/three.js-LICENSE.txt`) | 3E690AC7D180B0AA | WebGL rendering of STL/points |
| three.js OrbitControls | 0.160.0 | `src/web/vendor/OrbitControls.js` | MIT (same) | 5A44A9E86A2A0FB1 | rotate / pan / zoom |
| Fira Sans 400/600, Fira Code 400 (latin woff2) | @fontsource 5.0.18 | `src/web/vendor/fonts/` | SIL OFL 1.1 (`third_party/licenses/Fira-OFL-LICENSE.txt`) | - | UI typography |
| GCC runtime (libstdc++, libgcc), MinGW-w64 CRT, winpthreads | WinLibs GCC 16.2.0 UCRT r3 | statically linked into the .exe files | GPLv3 **with GCC Runtime Library Exception** (permits proprietary/static linking); MinGW-w64 runtime: ZPL / MIT-style | - | C++ standard library, threads |
| Windows system DLLs (KERNEL32, USER32, GDI32, SHELL32, WS2_32, api-ms-win-crt-*) | Windows 10/11 | part of the OS | - | - | not redistributed |

## Build, test and report tooling (not shipped inside Launch.exe)

| Tool | Version used | Purpose |
|---|---|---|
| CMake | 4.4.4 (minimum 3.20) | build system (`CMakeLists.txt`, `CMakePresets.json`) |
| Ninja | 1.13.2 | build generator |
| GCC (WinLibs MinGW-w64, UCRT, POSIX threads) | 16.2.0 | C++17 compiler, `-O3 -DNDEBUG -static` |
| Git | 2.52.0 | version control |
| Python | 3.11.9 | integration/E2E tests, figure plotting only (no backend logic) |
| matplotlib | 3.10.8 | report figures from C++-generated CSV |
| Playwright (Python) | 1.64.1 | E2E UI test driving installed Microsoft Edge 156 (no browser download) |

No package manager lock file is needed: there are no npm/pip runtime dependencies, and every C++/JS dependency is
a single pinned vendored file. Updating one means replacing the file, its licence and this table.
