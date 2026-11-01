# Prompt engineering review

How the prompts behind this submission were reviewed and improved. The AI tools generated code and drafts; the
candidate set the requirements, judged the output, and made the decisions listed in section 0. This review was
requested by the candidate and drafted with Claude Code (disclosed in `docs/ai_use_log.md`).
**Read it with the labels in mind:**

* **v1 (as used):** the master prompt from the first prototype attempt, drafted by the candidate and expanded with
  ChatGPT. It is stored unedited in
  `docs/prompts/master_prompt_v1.md`. The prompts sent in the Claude Code session that produced this submission are
  quoted word for word in `docs/ai_use_log.md`.
* **v2 (refined, written afterwards):** the staged prompts below were written *after* the work, as a review of
  what v1 lacked. **They were not sent to any tool** and are not presented as the prompts that produced this code.
  They show how the same work would be specified next time, based on the defects the project actually ran into.

## 0. The candidate's role: decisions the AI did not make

Each item is on record in the session or in the repository; none is reconstructed.

| Decision by the candidate | Why it mattered | Evidence |
|---|---|---|
| Drafted the requirements for v1 and had ChatGPT expand them into the master prompt (rubric and gates, priority order, anti-fabrication rules). | Set what "done" meant and which marks to protect first. | `docs/prompts/master_prompt_v1.md` |
| Judged the first prototype not good enough to submit and asked for a full rework, with the prototype as the reference for what to improve. | The prototype's defects (agreement checked on one node, Linux benchmarks, `Launch.exe` never run) were then fixed, not patched. | AI-use log, prompt 1; issue list under the defect table |
| Identified the delivery constraint (one PDF ≤ 1 MB) before any code was written and asked for a strategy. | Led to the hosted ZIP + SHA-256 + evidence-PDF design. | AI-use log, prompt 1 |
| Directed the use of specialist review tools (UI/accessibility rules, C++ review agent). | The C++ review found 10 robustness defects; the accessibility audit found 2 contrast failures. All were fixed with tests. | AI-use log, defect 12; `tests/contrast_check.py` |
| Chose the distribution: public GitHub repository and release; asked for clickable per-criterion links so reviewers can verify each claim. | Every rubric line in the PDF links to the exact submitted commit. | PDF cover; `scripts/build_submission_pdf.py` |
| Obtained an external review of the submission and had its two findings fixed: readability evidence missing from the PDF, and the build failing when CMake is run without the preset. | Added Appendix C (code excerpts) and an up-front toolchain check in the packaging script. | Commit history; `scripts/package_windows.ps1` |
| Asked for a strict, rubric-weighted score of the work and for the prompts to be reviewed (this document). | Gaps were listed openly instead of hidden. | This file |
| Kept authorship and AI use separate: sole author on every commit, AI named only in the disclosure files. | Matches the brief's disclosure rule. | `docs/ownership_statement.md` |

## 1. What v1 did well

* Hard anti-fabrication rules (no invented benchmarks, hardware, ANSYS provenance or test results). These held
  throughout: every number in `results/` came from running the C++ code.
* A priority order and a fallback rule ("implement a smaller honest version and document the limitation").
* The scoring rubric and shortlisting gates were given to the agent, so effort went where the marks are.
* A completion checklist, so "done" was defined.

## 2. What v1 lacked, and what it cost

| # | Gap in v1 | Observed consequence | v2 fix |
|---|---|---|---|
| 1 | Seven roles in one sentence ("software engineer, C++ engineer, numerical specialist, UI/UX designer, tester, writer"). | Nobody owns the review; the output read as one long generation, not checked work. | One role per stage; review done as a separate pass with a separate role. |
| 2 | One 600-line prompt, no checkpoints. | The prototype claimed completion with `Launch.exe` never built or run. | Seven stage prompts, each ending in a gate: commands that must pass before the next stage starts. |
| 3 | No target environment facts (compiler, toolchain, machine). | Prototype benchmarks were measured in a Linux container, not on the Windows 11 PC. | Stage 0 states the machine, OS, toolchain (MinGW-w64 GCC + Ninja, no MSVC) and that numbers must come from that machine. |
| 4 | The hardest delivery constraint was missing: the form accepts **one PDF ≤ 1 MB**. | Packaging strategy had to be redesigned in the second attempt. | Stated in Stage 0 and enforced as an acceptance check in Stage 7. |
| 5 | Qualitative acceptance ("responsive frontend", "useful errors", "verify agreement"). | Prototype compared only the *last node* of each case for serial/parallel agreement. | Measurable checks: every node of every case compared, NaN must fail, error messages carry line numbers, UI responsiveness asserted by an E2E test. |
| 6 | Asked the AI to "prepare a legible handwritten derivation". | An AI cannot produce authentic handwriting; the instruction invites fabrication. | AI produces only check values; the derivation is done by the candidate. |
| 7 | Prescribed a folder tree and also said "modify if better". | Ambiguous: the tree or the rule? | State the invariant (numerics in C++ core, transport and UI separate) and let the tree follow. |
| 8 | No privacy or portability rules. | Absolute local paths had to be scrubbed from history later. | "No personal data, no machine-specific absolute paths" in Stage 0. |
| 9 | No performance diagnosis method. | Parallel slowdowns (allocation contention, Windows EcoQoS throttling) needed several attempts to explain. | Stage 4 demands one-change-at-a-time hypotheses with before/after numbers. |
| 10 | Emphasis by volume (capitals, "CRITICAL", "emergency"). | Tokens without information. | Constraints written once, as testable statements. |

## 3. v2 — staged prompts (refined, written afterwards, not sent)

Every stage uses the same frame: **Role · Context · Task · Constraints · Acceptance (commands) · Output.**

### Stage 0 — Session context (sent once, before the stages)

> **Role:** lead engineer accountable for a graded technical prototype.
> **Context:** PASS Science Works R&D internship assignment (brief attached). Windows 11 x64 laptop, 4 logical CPUs.
> Toolchain: CMake + Ninja + MinGW-w64 GCC; MSVC is *not* installed. Submission: one PDF ≤ 1 MB via a form, so the
> runnable ZIP is hosted and linked from the PDF with its SHA-256. Rubric and gates attached.
> **Rules:** all numerics, parsing, concurrency and device mocks in C++17; frontend only displays. Never invent a
> result, measurement, device or provenance. No personal data or absolute paths in any committed file. Each stage
> ends at its gate; report pass/fail with command output before moving on. Commit once per stage, after the gate.

### Stage 1 — Foundation

> **Task:** CMake project (`release` preset: Ninja + GCC, static), a C++ core library, a localhost HTTP server
> (cpp-httplib, 127.0.0.1 only) and a Win32 `Launch.exe` that starts the server and opens the browser.
> **Acceptance:** `cmake --preset release && cmake --build --preset release` succeeds; `objdump -p Launch.exe` lists
> only Windows system DLLs; `GET /api/health` returns 200 within 1 s of launch; a fresh copy runs from a folder with
> no compiler on PATH.
> **Output:** files changed, gate output, open risks.

### Stage 2 — Thermal FEA (14 points, gate ≥ 7)

> **Role:** numerical-methods engineer.
> **Task:** 1D steady conduction, L = 1 m, A = 1e-4 m², k = 10 W/mK, q = 1e4 W/m³, T(0) = 100 °C, T(L) = 20 °C; two-node
> linear elements; symmetric Dirichlet elimination; Thomas solver; N = 10, 20, 40, 80.
> **Acceptance:** max midpoint error equals qh²/8k to 1e-9 for every N; observed order 2.00 ± 0.01; nodal error
> < 1e-10 (superconvergence, explain why); boundary reactions sum to qAL = 1 W; invalid inputs (k ≤ 0, N < 1, NaN)
> rejected with a message. Unit tests for each; CSV + plots generated by the C++ CLI.
> **Constraint:** do not change the public solver API once Stage 4 depends on it.

### Stage 3 — Geometry and ANSYS-style CSV

> **Task:** strict ASCII STL and `node_id,x_m,y_m,z_m,temperature_C` parsers in C++; three.js viewer.
> **Acceptance:** each of 13 malformed fixtures rejected with a line-numbered message (binary STL, empty, 2 vertices,
> NaN, `inf`, `+-5`, duplicate id, below −273.15 °C, …); tested size limits written to `results/`; the sample CSV
> is labelled "SYNTHETIC SAMPLE — NOT AN ACTUAL ANSYS EXPORT" in file, UI and docs.

### Stage 4 — Multicore performance

> **Role:** performance engineer.
> **Task:** seeded batch of independent rods; serial and bounded `std::thread` pool (p = 1..4, never above logical
> CPUs); results stored by input index.
> **Acceptance:** max |T_serial − T_parallel| ≤ 1e-8 over **every node of every case**, NaN fails the check; warm-up +
> 5 repetitions, median reported; speed-up and efficiency with CPU, RAM, compiler and flags. If p = 4 is slower than
> p = 3 or timings are bimodal, state one hypothesis, change one thing, re-measure, and record before/after.

### Stage 5 — Embedded integration

> **Task:** C++ mocks for one MCU board, one FPGA board, one JTAG adapter (discover, capabilities, connect,
> disconnect, telemetry, recoverable timeout with bounded retry). Block diagram with power, ground, voltage domains,
> level shifting, connectors and JTAG signal ownership.
> **Acceptance:** every UI element and log line says "simulated"; pin/voltage table cites the board's datasheet
> value or is flagged "verify"; tests cover timeout → retry → recovery.

### Stage 6 — Backpropagation

> **Task:** C++ forward/backward pass for h = tanh(wx + b), ŷ = vh + c, J = ½(ŷ − y)², x = 1, y = 2, w = 0.5, b = 0,
> v = 1, c = 0, η = 0.1; no autodiff.
> **Acceptance:** each analytic gradient within 1e-5 of a central difference (ε = 1e-6); one simultaneous update from
> the original parameters; 100-step loss curve. Produce a table of check values only — the handwritten derivation
> is the candidate's own work.

### Stage 7 — Delivery

> **Task:** package ZIP from git HEAD (Launch.exe at root, source, tests, results, docs, git bundle); report ≤ 6 pages;
> submission PDF with links, SHA-256 and evidence appendices.
> **Acceptance:** PDF < 1,000,000 bytes; ZIP extracted on a clean folder with no compiler on PATH passes the API
> smoke test; downloaded release asset hash equals the hash printed in the PDF; no absolute local paths in the
> repository (`git grep` returns nothing).

## 4. v2 — debugging prompt pattern (refined, written afterwards, not sent)

Used for each defect in `docs/ai_use_log.md`. The pattern forces evidence before a fix:

> **Observed:** <exact symptom with numbers, e.g. "p = 4 median 135 ms vs p = 3 39 ms, bimodal over 20 runs">.
> **Expected:** <the measurable target>.
> **Constraints:** change one variable per experiment; do not touch unrelated code; keep existing tests green.
> **Deliver:** ranked hypotheses with the measurement that would falsify each → run the cheapest → the fix → a
> regression test that fails before and passes after → before/after numbers.

Applied retrospectively to the real incidents:

| Incident (real, see AI-use log) | Refined debugging prompt (written afterwards) |
|---|---|
| p = 4 slower than p = 3 | "Speed-up falls from 2.9× (p = 3) to 1.3× (p = 4) on a 4-CPU machine. Rank hypotheses (allocation contention, false sharing, oversubscription, throttling); profile allocations per solve; fix only the confirmed cause; re-run 20 times and report the median and spread." |
| Bimodal p = 4 timings | "20 runs at p = 4 split into 39 ms and 135 ms clusters with identical inputs. Test whether Windows power throttling (EcoQoS) is responsible by toggling only that setting; report both distributions." |
| NaN passed the agreement check | "Write a test where one parallel result is NaN; it must fail the 1e-8 agreement check. Then fix `maxAbsDifference` so it does." |
| HTTP 413 on a 14 kB upload | "A 14 kB STL upload returns 413. Find which limit applies (server body limit vs content-type-specific limit) before changing any limit; prefer fixing the request over raising the limit." |
