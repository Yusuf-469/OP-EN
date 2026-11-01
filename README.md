# Unified Engineering Workbench

A small Windows 11 engineering workbench: ASCII STL and nodal-temperature CSV viewer, 1D thermal finite-element
solver with analytical verification, serial vs multi-threaded batch benchmark, simulated embedded-hardware
adapters, and a hand-derived backpropagation check. **All backend logic, numerics, file parsing and concurrency
are C++17**; the UI is plain HTML/CSS/JavaScript served by the C++ backend on `127.0.0.1`.

PASS Science Works R&D Internship 2026-2027, common technical assignment. Report: [`docs/report/report.pdf`](docs/report/report.pdf).

## 1. Run it (one click)

**Prerequisites:** Windows 11 x64 and a web browser (Edge is built in). Nothing to install; no internet, no admin rights.

1. Extract `CandidateID_Workbench.zip` (right-click > Extract All). Extract the whole folder, not just the exe.
2. Double-click **`Launch.exe`**. The workbench opens in your default browser, and a small window shows the
   local address. Closing that window (or **Stop and exit**) stops the app.
3. Try **Geometry > Load sample heat sink**, then **Load sample temperatures**.

`Launch.exe` is statically linked (MinGW-w64, GCC runtime under the GCC Runtime Library Exception); it imports only
DLLs that ship with Windows (`evidence/launch_exe_dll_dependencies.txt`). Logs go to `logs/workbench.log`
(or `%TEMP%\uew_workbench.log` if the folder is read-only).

**VS Code / Antigravity:** open the extracted folder. Either *Terminal > Run Task > Start packaged Workbench*
(built-in, no extension needed) or *Run and Debug >* **Run packaged Workbench (Launch.exe)** *> F5* (needs the
Microsoft C/C++ extension `ms-vscode.cpptools`). Both start the same `Launch.exe`.

## 2. What each view does

| View | Task | What you can do |
|---|---|---|
| Geometry | B | Open ASCII STL (rotate / pan / zoom / fit, keyboard camera buttons) and nodal CSV (viridis point cloud, °C legend, click a node to inspect). Malformed files get a line-numbered error. |
| Thermal Analysis | C | Edit L, A, k, q, T(0), T(L), meshes; plots FE vs exact, pointwise error, log-log convergence, verification table, energy balance; CSV export. |
| Performance | D | Batch benchmark (serial vs bounded `std::thread` pool, p = 1..4), live progress, **Cancel**, timings/speed-up/efficiency/agreement, CSV/JSON export, system info and launch-to-ready time. |
| Hardware | E | Block diagram; discover / connect / disconnect / telemetry for simulated MCU, FPGA and JTAG adapter; inject transient or persistent timeouts and recover. |
| Backprop check | F | Forward pass, chain-rule gradients vs central differences, one update, 100-step loss curve. |

## 3. Architecture

```
Browser UI (src/web: HTML/CSS/ES modules, three.js)        presentation
        |  fetch /api/*  (127.0.0.1 only, cross-origin refused)
src/app  ApiServer (cpp-httplib) + JobRegistry + Launch.exe   transport: routing, jobs, no numerics
        |  plain C++ calls
src/core  thermal_fea | batch | geometry_io | backprop |       C++17 core: all computation,
          hardware | system_info | report_writers             parsing, concurrency, CSV/JSON
src/cli   uew_cli: regenerates results/ and data/ from the same core
```

* Long jobs (benchmark) run on a job thread; the UI polls progress and can cancel. A cancelled/failed job
  publishes no partial result; the previous result stays on screen.
* The core has no HTTP or UI dependency and is unit-tested directly; `uew_cli` and `Launch.exe` share it.

## 4. Supported formats

| Format | Rules (enforced in C++, `src/core/geometry_io.cpp`) |
|---|---|
| ASCII STL | `solid` ... `facet normal` / `outer loop` / 3 x `vertex` / `endloop` / `endfacet` ... `endsolid`. Exactly 3 vertices per facet, finite numbers only. Binary STL, empty files and solids with no facets are rejected with an explanation. Units are not stored in STL; choose mm / m / in in the UI (sample is mm). |
| Nodal CSV | Header exactly `node_id,x_m,y_m,z_m,temperature_C`; 5 fields per row; positive unique integer `node_id`; finite coordinates in metres; temperature in °C, not below -273.15. UTF-8 BOM, CRLF and blank lines accepted. |

Tested limits (`results/file_size_limits.txt`, `docs/file_limits.md`): 128 MB per file, 2,000,000 triangles,
2,000,000 nodes. 500k-triangle STL: 2.2 s upload-to-display data; 2M-node CSV: 5.4 s.

## 5. Build and test from source

Prerequisites: CMake >= 3.20, Ninja, MinGW-w64 GCC >= 11 (tested: WinLibs GCC 16.2.0 UCRT, `winget install BrechtSanders.WinLibs.POSIX.UCRT`),
Python 3.11 for the integration/E2E tests and figures (`pip install playwright==1.64.1 matplotlib`).

```powershell
cmake --preset release            # configure (build/)
cmake --build --preset release    # Launch.exe, uew_cli.exe, uew_tests.exe
ctest --preset release            # 31 CTest cases: unit, numerical, negative, job registry, sample files
python tests/api_smoke_test.py build/Launch.exe      # 31 HTTP integration checks
python tests/e2e_walkthrough.py build/Launch.exe     # UI end-to-end in Edge + screenshots
powershell -File scripts/regenerate_results.ps1      # results/*.csv, *.json, plots
powershell -File scripts/package_windows.ps1 -CandidateId <ID>   # dist/<ID>_Workbench.zip
```

A `msvc` preset (Visual Studio 2022 Build Tools) is included but was not used for the submitted build.

## 6. Folder map

| Path | Contents |
|---|---|
| `src/core`, `src/app`, `src/cli`, `src/web` | C++ core, transport + launcher, CLI, frontend |
| `tests/` | `test_core.cpp` (CTest), `api_smoke_test.py`, `e2e_walkthrough.py`, `upload_size_test.py`, `fixtures/bad_*` |
| `data/` | Self-created sample STL and synthetic CSV + `PROVENANCE.md` |
| `results/` | Raw CSV/JSON from the C++ CLI, plots, performance debugging record, file-size limits |
| `docs/` | Report, thermal FEA notes, embedded design, AI-use log, ownership, dependencies, effort log |
| `evidence/` | Screenshot walkthrough + log, git bundle and commit log (in the package) |
| `third_party/` | cpp-httplib v0.18.3 header and licences (three.js, Fira fonts vendored in `src/web/vendor`) |

## 7. Known limitations

* Windows 11 x64 only for the packaged app (the core and tests are portable C++17 but only Windows was built and tested).
* `Launch.exe` was tested on the development PC from a freshly extracted ZIP with no compiler on `PATH`; it was **not**
  tested on a second, separately installed Windows machine.
* The UI opens in the default browser instead of a native window; a browser is required (Edge ships with Windows 11).
* Hardware is **simulated**: no USB, serial, JTAG, firmware or FPGA programming. Part numbers are design assumptions.
* The sample CSV is **synthetic** (formula in `data/PROVENANCE.md`), not an ANSYS export. No mesh-to-result registration.
* Thermal solver: 1D, steady, constant properties, Dirichlet ends only.
* Benchmark: single-thread timings vary ±20 % on this hybrid P/E-core laptop CPU; medians of 5 runs are reported.
* Upload cancellation stops waiting in the UI; the C++ parse of that request still completes and is discarded.
