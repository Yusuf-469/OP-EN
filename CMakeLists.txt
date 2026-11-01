cmake_minimum_required(VERSION 3.20)
project(UnifiedEngineeringWorkbench VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# MinGW: link libstdc++/libgcc/winpthread statically so Launch.exe runs on a
# clean Windows machine without extra DLLs (GCC Runtime Library Exception).
if(MINGW)
  add_link_options(-static)
endif()

# ---------------------------------------------------------------- C++ core
add_library(uew_core STATIC
  src/core/thermal_fea.cpp
  src/core/batch.cpp
  src/core/geometry_io.cpp
  src/core/backprop.cpp
  src/core/hardware.cpp
  src/core/system_info.cpp
  src/core/report_writers.cpp)
target_include_directories(uew_core PUBLIC src/core)
find_package(Threads REQUIRED)
target_link_libraries(uew_core PUBLIC Threads::Threads)

string(TOUPPER "${CMAKE_BUILD_TYPE}" UEW_BUILD_TYPE_UPPER)
set(UEW_FLAGS "${CMAKE_BUILD_TYPE}: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${UEW_BUILD_TYPE_UPPER}}")
string(STRIP "${UEW_FLAGS}" UEW_FLAGS)
target_compile_definitions(uew_core PRIVATE UEW_BUILD_FLAGS="${UEW_FLAGS}")

# ------------------------------------------------- command-line tool (results)
add_executable(uew_cli src/cli/uew_cli.cpp src/cli/sample_data.cpp)
target_link_libraries(uew_cli PRIVATE uew_core)

# ------------------------------------- Windows app: transport + launcher
if(WIN32 AND EXISTS ${CMAKE_SOURCE_DIR}/src/app/launcher_win.cpp)
  add_executable(Launch WIN32
    src/app/launcher_win.cpp
    src/app/api_server.cpp
    src/app/job_registry.cpp)
  target_include_directories(Launch PRIVATE src/app third_party/cpp-httplib)
  target_compile_definitions(Launch PRIVATE _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  target_link_libraries(Launch PRIVATE uew_core ws2_32 shell32 user32 gdi32)
  if(MINGW)
    target_link_options(Launch PRIVATE -municode)
  endif()
endif()

# ------------------------------------------------------------------- tests
enable_testing()
add_executable(uew_tests tests/test_core.cpp)
target_link_libraries(uew_tests PRIVATE uew_core)
set(UEW_TESTS
  thermal_boundary_conditions thermal_nodal_exactness thermal_midpoint_error_theory
  thermal_convergence_order thermal_energy_balance thermal_no_source_linear thermal_rejects_invalid
  batch_reproducible batch_serial_parallel_agree batch_input_order batch_cancellation
  batch_worker_error batch_rejects_invalid benchmark_report median
  agreement_detects_nan workspace_solve_matches
  stl_valid stl_malformed csv_valid csv_malformed
  backprop_initial_values backprop_finite_difference backprop_training
  hardware_discover_connect hardware_timeout_recovery)
foreach(test_name IN LISTS UEW_TESTS)
  add_test(NAME ${test_name} COMMAND uew_tests ${test_name})
endforeach()
# Sample data shipped in data/ must parse cleanly.
add_test(NAME sample_stl_parses COMMAND uew_cli parse-stl ${CMAKE_SOURCE_DIR}/data/heat_sink.stl)
add_test(NAME sample_csv_parses COMMAND uew_cli parse-csv ${CMAKE_SOURCE_DIR}/data/heat_sink_temperature_synthetic.csv)
