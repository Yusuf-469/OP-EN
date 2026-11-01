# MASTER EXECUTION PROMPT: PASS SCIENCE WORKS — UNIFIED ENGINEERING WORKBENCH

You are my autonomous senior software engineer, C++ engineer, numerical computing specialist, UI/UX designer, testing engineer and technical documentation assistant.

Your responsibility is to build a complete, working prototype for the PASS SCIENCE WORKS R&D INTERNSHIP 2026–2027 common engineering assignment.

## 1. CRITICAL CONTEXT

We have ONLY ONE DAY to complete this assignment.

This is an emergency development situation. You must optimize for:

1. Working functionality.
2. Technical correctness.
3. Compliance with the official assignment.
4. Demonstrable results.
5. Reproducible testing.
6. Clean architecture.
7. Submission readiness.

Do not spend hours on unnecessary visual effects, elaborate animations, additional features or production-scale architecture.

Do not attempt to build an unnecessarily complicated system.

Build the simplest technically correct implementation that satisfies the maximum number of rubric requirements.

You must work autonomously. Do not stop after generating a plan. Inspect the existing project, implement it, run it, test it, fix errors and prepare the deliverables.

If something is impossible within the deadline, implement a smaller honest version and document the limitation.

Never fabricate test results, benchmarks, hardware detection, physical device connections, ANSYS provenance or execution evidence.

---

## 2. NON-NEGOTIABLE TECHNICAL REQUIREMENTS

The assignment explicitly requires:

- C++17 or later for ALL backend logic.
- C++ for numerical computation.
- C++ for file processing.
- C++ for concurrency.
- C++ for the embedded-device mock adapters.
- Any frontend language is permitted.
- A web interface or desktop application is acceptable.
- Target platform: Windows 11 x64.
- A standalone ZIP-extracted application.
- Launch.exe must open the application without requiring terminal commands, administrator rights, package installation or a first-run build.

Frontend rendering libraries are permitted.

Keep frontend presentation separate from the C++ computational core.

Do not move numerical computations into JavaScript, Python or the frontend.

Preferred architecture:

Frontend → HTTP/API transport → C++ backend → C++ modules.

If a local HTTP server is used, it must be implemented using C++.

Prefer a lightweight C++ HTTP library rather than introducing unnecessary infrastructure.

---

## 3. FIRST ACTION: INSPECT THE EXISTING ENVIRONMENT

Before modifying anything:

1. Inspect all existing project files.
2. Identify the current framework and available dependencies.
3. Identify existing working functionality.
4. Check compiler availability.
5. Check CMake availability.
6. Check whether a C++ build already exists.
7. Identify existing frontend/backend integration.
8. Preserve useful existing work.

Do not blindly overwrite an existing project.

Create a short implementation plan and immediately begin execution.

If the project is empty, initialize the architecture yourself.

---

## 4. APPLICATION IDENTITY

Application name:

UNIFIED ENGINEERING WORKBENCH

Subtitle:

A Modular Engineering Simulation and Analysis Platform

The application must have four primary views:

1. Geometry
2. Thermal Analysis
3. Performance
4. Hardware Integration

Include a clean dashboard or navigation layout.

Keep the interface professional, modern, engineering-oriented and easy to demonstrate.

Avoid excessive text.

Every major operation should have:
- Clear input controls.
- Validation.
- Execution status.
- Progress or loading feedback.
- Error handling.
- Visualized results.
- Export capability wherever applicable.

---

# 5. TASK A — INTERFACE, BACKEND AND EXECUTION

Implement one coherent application.

Requirements:

- Geometry import.
- Thermal solver.
- Performance benchmarking.
- Hardware mock integration.
- File selection.
- Parameter validation.
- Progress/status.
- Actionable error messages.
- Result export.
- Responsive frontend during computations.
- Cancellation or safe completion.
- Separation of C++ core and presentation.

Prioritize a reliable execution flow over complex architecture.

---

# 6. TASK B — CAD / ANSYS DATA VISUALIZATION

Implement two independent import workflows.

## A. ASCII STL viewer

Support ASCII STL files.

Implement the parser in C++.

Extract:
- Triangle vertices.
- Facet normals where available.
- Triangle count.
- Bounding box.

Visualize the geometry in an interactive 3D viewer.

Support:
- Rotation.
- Pan.
- Zoom.
- Fit to view.
- Triangle count.
- File information.

Use an appropriate frontend 3D rendering library.

Do not claim support for binary STL or native CAD/ANSYS project formats.

## B. ANSYS nodal temperature CSV viewer

Required schema:

node_id,x_m,y_m,z_m,temperature_C

Implement parsing and validation in C++.

Display:
- 3D point cloud.
- Temperature-based coloring.
- Temperature legend.
- Temperature units.
- Node inspection.
- Minimum temperature.
- Maximum temperature.
- Average temperature.
- Number of nodes.

Allow users to import CSV files.

Create one small synthetic example CSV.

Clearly label it:

"SYNTHETIC SAMPLE — NOT AN ACTUAL ANSYS EXPORT."

Provide a provenance and schema note.

## Validation

Reject:
- Malformed rows.
- Missing columns.
- Non-numeric values.
- NaN.
- Infinity.
- Empty files.

Return useful error messages.

Document tested file-size limits.

---

# 7. TASK C — STEADY-STATE THERMAL FINITE ELEMENT ANALYSIS

THIS IS A HIGH-PRIORITY COMPONENT.

Implement the actual numerical solver in C++.

Problem definition:

Uniform one-dimensional rod.

Parameters:

L = 1 m
A = 1e-4 m²
k = 10 W/(m·K)
q = 10,000 W/m³

Boundary conditions:

T(0) = 100 °C
T(L) = 20 °C

Governing equation:

-d/dx(kA dT/dx) = qA

Assumptions:

- Constant material properties.
- Uniform heat generation.
- Insulated lateral surfaces.
- Steady-state conduction.

## Finite element implementation

Use:
- Two-node linear elements.
- Uniform mesh.
- Element stiffness assembly.
- Element load vector assembly.
- Correct application of prescribed temperatures.
- A reliable linear-system solver.

Run four mesh configurations:

N = 10
N = 20
N = 40
N = 80

## Analytical solution

Implement the analytical reference:

T_exact(x) = 100 - 80x/L + [q/(2k)]x(L-x)

Compare FE and analytical solutions.

Calculate maximum absolute error at ELEMENT MIDPOINTS using FE interpolation.

Do not calculate the error only at nodes.

## Results

Generate:

1. Temperature versus position plot.
2. Analytical versus FE comparison.
3. Mesh convergence plot.
4. Error comparison for all four meshes.
5. Exportable numerical CSV.

All graphs must have:
- Axis labels.
- Units.
- Legends.
- Meaningful titles.

Display:
- Number of elements.
- Number of nodes.
- Maximum midpoint error.
- Solver status.

Explain the numerical assumptions and boundary conditions in the interface or report.

Include one debugging insight based on an actual issue encountered or a deliberate validation experiment.

---

# 8. TASK D — MULTICORE / MULTITHREADED EXECUTION

Implement REAL CPU-parallel computation in C++.

Do not merely run the computation in a background UI thread.

Create a batch of independent thermal simulations.

## Batch generation

Use:
- Fixed random seed.
- Configurable case count.
- Configurable mesh size.
- Varied positive conductivity values.
- Varied positive heat generation values.

Generate identical workloads for serial and parallel execution.

## Execution modes

Implement:

1. Serial execution.
2. Parallel execution with 1 worker.
3. Parallel execution with 2 workers.
4. Parallel execution with up to 4 workers.

Never exceed available logical CPUs.

Use a bounded worker pool or standard C++ threads.

Maintain deterministic input order.

## Benchmarking

Measure actual computation time.

Perform:
- Warm-up.
- Five measured repetitions.
- Median timing.

Exclude:
- File loading.
- Rendering.
- Compilation.

Report separately:
- Launch-to-ready time.
- Compute time.

Calculate:

Speedup = T1 / Tp

Efficiency = (T1 / Tp) / p

Report:
- CPU model.
- Physical core count if available.
- Logical core count.
- RAM if available.
- Compiler.
- Optimization flags.
- Workload configuration.
- Worker count.
- Median runtime.
- Speedup.
- Efficiency.

Verify serial and parallel results agree.

Maximum absolute temperature difference must be:

<= 1e-8 °C

Do not invent benchmark numbers.

If parallel execution is slower, explain the overhead honestly.

---

# 9. TASK E — EMBEDDED SYSTEM INTEGRATION

Create a modular host-connected embedded hardware integration design.

Three device types are mandatory:

1. One microcontroller board.
2. One FPGA board.
3. One JTAG debug adapter.

Physical devices are NOT required.

## Interface implementation

Create C++ mock adapters for each device type.

Each adapter must support:

- Device discovery.
- Capability reporting.
- Connect.
- Disconnect.
- One telemetry message.
- Recoverable failure.

Demonstrate all three device types in the frontend.

Include one simulated timeout or recoverable connection failure.

Clearly label every simulated device and behavior.

Do not pretend that physical hardware is connected.

## Engineering documentation

Provide:

- Labeled block diagram.
- Power connections.
- Grounding.
- Voltage domains.
- Level shifting.
- Connectors.
- Host communication.
- JTAG signal ownership.
- Device-specific pinout assumptions.

Explain why a JTAG adapter is a debugging interface and not a generic interchangeable compute board.

Do not assume that all devices share the same connector, voltage or protocol.

Include a safe integration plan.

---

# 10. TASK F — HANDWRITTEN BACKPROPAGATION

This component must include BOTH mathematical derivation and C++ implementation.

Problem:

x = 1
y = 2

h = tanh(wx + b)

ŷ = vh + c

J = 0.5(ŷ - y)²

Initial parameters:

w = 0.5
b = 0
v = 1
c = 0

Learning rate:

η = 0.1

## Mathematical derivation

Calculate:

1. Forward pass.
2. Hidden activation.
3. Prediction.
4. Loss.
5. Chain-rule derivatives for w, b, v and c.
6. Simultaneous gradient-descent update.

Use gradients from the ORIGINAL parameter values.

Include numerical values.

Explain gradient direction.

Prepare a legible handwritten derivation as a maximum two-page appendix. It must be genuinely reviewed and verified by the candidate.

## C++ implementation

Implement the same calculation without automatic differentiation.

Use central finite differences.

epsilon = 1e-6

Check each initial gradient.

Required absolute error:

<= 1e-5

Train for 100 subsequent steps using the same learning rate.

Plot loss over training steps.

Explain the loss curve.

Do not use automatic differentiation libraries.

---

# 11. USER INTERFACE REQUIREMENTS

Build a polished but practical engineering interface.

Suggested layout:

SIDEBAR:
- Overview
- Geometry
- Thermal Analysis
- Performance
- Hardware Integration
- Documentation

OVERVIEW:
- Project introduction.
- Module status.
- Quick actions.
- Recent results.
- System information.

GEOMETRY:
- STL upload.
- CSV upload.
- 3D visualization.
- File metadata.
- Temperature point-cloud view.

THERMAL ANALYSIS:
- Problem parameters.
- Mesh selection.
- Run solver.
- Analytical comparison.
- Convergence graph.
- CSV export.

PERFORMANCE:
- Batch configuration.
- Serial benchmark.
- Parallel benchmark.
- Worker selection.
- Speedup graph.
- Efficiency graph.
- Raw results.

HARDWARE:
- Microcontroller.
- FPGA.
- JTAG adapter.
- Connection state.
- Capabilities.
- Telemetry.
- Simulated failure/recovery.

Use a consistent design system.

Do not sacrifice functional clarity for visual complexity.

---

# 12. TESTING AND VALIDATION

Create actual C++ tests.

Minimum testing categories:

## Geometry
- Valid ASCII STL.
- Invalid STL.
- Empty file.
- Malformed CSV.
- Non-finite temperature.

## Thermal
- Correct boundary temperatures.
- Positive temperature values for the specified case.
- FE versus analytical solution.
- Mesh convergence.
- Correct midpoint error.
- Invalid parameters.

## Parallel
- Serial/parallel agreement.
- Deterministic output ordering.
- Multiple worker counts.
- Empty workload.
- Recoverable worker failure where applicable.

## Hardware
- Discovery.
- Connection.
- Disconnection.
- Telemetry.
- Timeout.
- Recovery.

## Backpropagation
- Forward pass.
- Gradient calculations.
- Finite-difference verification.
- Training loss.

Run the tests.

Fix failures.

Do not report tests as passing unless they were actually executed.

---

# 13. REQUIRED FOLDER STRUCTURE

Organize the final project approximately as follows:

UnifiedEngineeringWorkbench/
│
├── Launch.exe
├── README.md
├── CMakeLists.txt
├── CMakePresets.json
│
├── src/
│   ├── core/
│   ├── geometry/
│   ├── thermal/
│   ├── performance/
│   ├── hardware/
│   ├── backprop/
│   ├── transport/
│   └── frontend/
│
├── data/
│   ├── sample_geometry.stl
│   ├── sample_temperature.csv
│   └── provenance.md
│
├── tests/
│
├── results/
│   ├── thermal/
│   ├── performance/
│   └── backprop/
│
├── docs/
│   ├── architecture.md
│   ├── embedded_design.md
│   ├── assumptions.md
│   └── limitations.md
│
├── report/
│   ├── technical_report.pdf
│   └── handwritten_appendix.pdf
│
├── evidence/
│   ├── screenshots/
│   ├── logs/
│   └── demo/
│
└── .git/

Modify the structure if the existing project has a better working architecture.

Do not create empty folders merely to make the structure look complete.

---

# 14. DOCUMENTATION AND SUBMISSION

Prepare the following:

## README

Include:
- Application overview.
- Prerequisites.
- One-click launch instructions.
- Architecture.
- Supported file formats.
- Build instructions.
- Test instructions.
- Known limitations.

## Technical report

Maximum six pages, excluding the two-page handwritten appendix.

Cover:
- Design choices.
- C++ architecture.
- Thermal formulation.
- FEA verification.
- Mesh convergence.
- Parallel performance.
- Embedded-system design.
- Backpropagation.
- Test outcomes.
- Limitations.

Only use actual measured results.

## AI usage disclosure

Prepare an AI-use log containing:

- AI tools/models used.
- Representative prompts.
- Relevant outputs.
- Human modifications.
- Independent validation.
- At least one unsuitable or incorrect suggestion identified and tested.

Never fabricate AI interactions.

If no incorrect suggestion was encountered, perform a deliberate challenge test and document the outcome.

## Version control

Use meaningful commits.

Preserve actual development history.

Include a readable commit log.

## Demonstration

Prepare a 3–5 minute demonstration plan or timestamped screenshot walkthrough with logs.

Demonstrate actual functionality, not static mockups.

---

# 15. PRIORITY ORDER FOR THE ONE-DAY DEADLINE

This is critical.

Work in the following order.

PHASE 1 — FOUNDATION
- Inspect project.
- Set up C++ backend.
- Establish frontend/backend communication.
- Build application shell.
- Verify application launches.

PHASE 2 — THERMAL SOLVER
- Implement FEA.
- Validate analytical solution.
- Run all four meshes.
- Export CSV.
- Generate plots.

PHASE 3 — GEOMETRY
- ASCII STL parser.
- CSV parser.
- 3D rendering.
- Sample datasets.

PHASE 4 — PERFORMANCE
- Serial batch execution.
- Parallel worker pool.
- Benchmarking.
- Agreement tests.

PHASE 5 — HARDWARE
- Three mock adapters.
- Telemetry.
- Failure and recovery.
- Block diagram.

PHASE 6 — BACKPROPAGATION
- Mathematical calculations.
- C++ implementation.
- Finite-difference checks.
- Loss plot.

PHASE 7 — DELIVERY
- Run tests.
- Fix critical defects.
- Generate report.
- Generate documentation.
- Prepare evidence.
- Package application.
- Verify Launch.exe.

If time becomes critically limited, prioritize a working C++ backend, thermal solver, standalone launch and minimum valid implementation of the remaining tasks.

Never leave the entire project unfinished because one advanced feature is consuming too much time.

---

# 16. OFFICIAL EVALUATION RUBRIC

Optimize implementation according to the actual rubric:

| Criterion | Points |
| Interface / webpage design | 8 |
| C++ backend and testing | 12 |
| Folder architecture | 6 |
| Code readability | 6 |
| Version control | 5 |
| Thermal FEA and adoption | 14 |
| CAD / ANSYS visualization | 10 |
| Multicore performance | 10 |
| Embedded integration | 8 |
| Backpropagation | 7 |
| Prompt engineering / AI judgement | 8 |
| One-click delivery / report | 6 |
| TOTAL | 100 |

Pay particular attention to the shortlisting gates:

- At least 6/12 in C++ backend.
- At least 7/14 in thermal FEA.
- At least 3/6 in delivery/report.
- Working C++ backend.
- Successful standalone launch.

A visually impressive application without working engineering functionality is unacceptable.

---

# 17. WORKING RULES FOR THE AI AGENT

1. Do not ask unnecessary questions.
2. Do not wait for permission to implement obvious requirements.
3. Do not repeatedly explain what you intend to do.
4. Write actual files and working code.
5. Compile frequently.
6. Test incrementally.
7. Fix errors immediately.
8. Reuse existing dependencies where possible.
9. Avoid unnecessary dependencies.
10. Avoid overengineering.
11. Never substitute fake outputs for numerical calculations.
12. Never label simulated hardware as physical hardware.
13. Never fabricate benchmark results.
14. Never claim a packaged application works until tested.
15. Maintain a clear list of completed, incomplete and blocked requirements.

If an implementation approach fails, switch to a simpler technically correct approach.

If a feature cannot be completed, document it and move forward.

---

# 18. FINAL COMPLETION CHECKLIST

Before declaring the project complete, verify:

[ ] Application launches.
[ ] Frontend and C++ backend communicate.
[ ] C++17 or later is used.
[ ] ASCII STL import works.
[ ] Temperature CSV import works.
[ ] 3D visualization works.
[ ] Thermal solver works.
[ ] Four mesh configurations work.
[ ] Analytical comparison works.
[ ] Midpoint error is calculated correctly.
[ ] Mesh convergence is demonstrated.
[ ] Serial computation works.
[ ] Parallel computation uses actual worker threads.
[ ] Benchmark measurements are reproducible.
[ ] Serial/parallel agreement is verified.
[ ] Three hardware mocks work.
[ ] Recoverable failure is demonstrated.
[ ] Backpropagation is implemented.
[ ] Finite-difference gradient checks pass.
[ ] Loss graph is generated.
[ ] Tests are executed.
[ ] Documentation is prepared.
[ ] Technical report is prepared.
[ ] AI disclosure is prepared.
[ ] Version-control evidence exists.
[ ] Demonstration evidence exists.
[ ] Launch.exe is packaged and tested.
[ ] ZIP contents are correct.

## FINAL INSTRUCTION

Start by inspecting the existing workspace.

Then immediately implement the project.

Your objective is not to produce the most complicated application.

Your objective is to produce the highest-quality, technically correct and independently verifiable engineering prototype possible within ONE DAY.

Do not stop at planning. BUILD IT.
