# Performance debugging record: allocation contention (2026-11-01)

Machine: i7-13650HX (14 cores / 20 logical), 23.8 GiB, Windows 11 25H2, GCC 16.2.0 -O3, Balanced power plan, on AC.
Command: `uew_cli benchmark <dir> <cases> <mesh>` (seed 20261101, 1 warm-up + 5 timed repetitions). Raw wall times in ms.

## Before (commit c61a0d0): every solve heap-allocated its matrix, RHS and result vectors

| workload | variant | 5 samples (ms) | median speed-up |
|---|---|---|---|
| 400 x 20000 | serial | 169.1 172.3 170.5 165.9 165.3 | 1.00 |
| | pool p=2 | 92.4 88.2 86.0 85.5 86.5 | 1.95 |
| | pool p=3 | 57.8 57.3 58.5 94.5 180.6 | 2.89 |
| | pool p=4 | 158.1 147.4 134.1 123.6 108.3 | **1.26** |
| 40 x 200000 | serial | 299.6 295.1 286.1 295.7 295.1 | 1.00 |
| | pool p=2 | 403.5 430.4 458.2 458.1 424.3 | **0.69** |
| | pool p=4 | 230.9 233.7 237.8 223.2 241.7 | 1.26 |

Symptoms: p=4 slower than p=3; large-mesh pool slower than serial; very large spread between repetitions.

## Hypothesis

Each solve allocated ~4 x (N+1) doubles (1.6 MB each at N = 200000) and the result storage was freshly
allocated every repetition, so the timed region was dominated by first-touch page faults and contention in the
process heap / Windows memory manager, which serialises threads. Larger meshes -> larger allocations -> worse.

## Change

`SolverWorkspace` reused per worker thread; results written into storage allocated once before timing
(`allocateResults`), NaN-poisoned before each run so the agreement check still proves every value was rewritten.

## After (same machine, same commands)

| workload | variant | 5 samples (ms) | median speed-up |
|---|---|---|---|
| 400 x 20000 | serial | 148.0 149.2 154.6 154.8 148.4 | 1.00 |
| | pool p=2 | 81.7 77.6 76.4 76.2 77.4 | 1.94 |
| | pool p=3 | 51.1 51.8 50.7 50.4 51.1 | 2.93 |
| | pool p=4 | 39.8 39.5 40.2 38.9 38.5 | **3.80** |
| 40 x 200000 | serial | 157.4 157.2 158.2 167.7 167.9 | 1.00 |
| | pool p=2 | 81.4 80.0 82.9 79.2 79.4 | 1.98 |
| | pool p=4 | 43.1 43.0 44.0 43.5 43.0 | **3.65** |

Serial time also fell (169 -> 149 ms), confirming allocation was a cost even without threads. Hypothesis supported.

# Second issue: bimodal p=4 timings (Windows power throttling)

After the allocation fix, repeated full benchmarks showed p=4 samples that were either ~39 ms or ~135 ms
(about serial speed), switching part-way through a run; p=1..3 were unaffected. Four consecutive runs, p=4 samples (ms):

    run 1: 39 39 39 135 135
    run 2: 39 39 39 39 134
    run 3: 140 133 141 136 135
    run 4: 39 39 38 39 112

Hypothesis: Windows 11 EcoQoS power throttling of a background (windowless) process on this hybrid
P-core/E-core CPU moves its threads to efficiency cores at low clock. Test: opt the process out with
`SetProcessInformation(ProcessPowerThrottling, EXECUTION_SPEED control bit set, state bit clear)`
(`disablePowerThrottling()` in src/core/system_info.cpp), change nothing else, repeat:

    run 1: 39 38 39 39 39
    run 2: 40 39 39 38 38
    run 3: 39 38 40 40 38
    run 4: 39 39 40 40 39

20/20 samples now 38-40 ms, so the hypothesis is supported. Remaining limitation: single-thread runs still vary
(150-220 ms), consistent with one thread migrating between P- and E-cores; medians of 5 runs are reported.
