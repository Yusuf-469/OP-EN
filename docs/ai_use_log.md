# AI-use log

Disclosure as required by section 8 of the brief. This is a factual record of how I used AI tools on this project.
My original prompts were short and informal. The block quotes in sections 2-9 are cleaned-up write-ups of what I
asked for, written afterwards to state each request precisely; they are not the exact text I sent. The exact wording
is kept in the session record. Sections 10-11 quote my prompts as sent.

## Tools and models

| Tool | Model | What I used it for |
|---|---|---|
| ChatGPT (OpenAI) | label recorded in that prototype's own log as "GPT-6" (not independently verified) | Expanding my requirements into the master prompt (`docs/prompts/master_prompt_v1.md`), and building the first prototype (`UnifiedEngineeringWorkbench_Prototype`, not submitted). No code was copied from the prototype. |
| Claude Code (Anthropic), VS Code extension | Claude Opus 5.5 (`claude-opus-5-5`) | This submission: design, C++/JS/Python code, tests, documentation drafts, packaging, report layout, review passes. Session of 2026-11-01. |
| Claude Code skills and agents | `ui-ux-pro-max`, `ui-design-system`, `cpp-reviewer` | UI quality rules and contrast audit; a separate C++ review pass. |

## How the project went, prompt by prompt

### 1. The first attempt (ChatGPT)

I wrote down what the assignment needed and had ChatGPT turn it into a long master prompt (kept unedited in
`docs/prompts/master_prompt_v1.md`), then used it to build a first prototype. When I looked at the result it was
not something I could submit: the serial/parallel check compared only the last node of each case, the benchmark
numbers came from a Linux container rather than my Windows PC, most C++ sat on single very long lines, there was
no cancellation, and `Launch.exe` had never actually been built or run.

### 2. Starting again, with the delivery problem first

The submission form only takes one PDF under 1 MB, which a runnable app cannot fit into. I raised that before any
code was written, and asked for a full rework that used the prototype only as a list of what to fix:

> Act as the delivery lead for a graded Windows 11 C++ prototype. The submission form accepts exactly one PDF of at most
> 1 MB; the brief also requires a runnable ZIP with `Launch.exe`. Propose a delivery design that satisfies both, with
> the trade-offs. Then rebuild the project from scratch, using `<prototype folder>` only as a list of defects to avoid.
> Acceptance: every rubric item has evidence; no numbers that were not measured; the PDF build fails if it exceeds 1 MB.

What came out of it: the ZIP is hosted and the PDF carries the report, the evidence and the ZIP's SHA-256, so a
reviewer can prove the download is the submitted file. When asked, I chose my registration number as my ID on the
submission and to host the ZIP as a GitHub release (a Google Drive mirror was also planned but not set up).

### 3. Bringing in review tools

> Run three independent review passes on the current code and report findings before fixing anything: (1) UI against
> WCAG 2.2 AA (contrast, focus, 44 px targets, reduced motion); (2) C++ for lifetime, concurrency and exception-safety
> defects; (3) input validation at the HTTP boundary. For each finding give file:line, a failing test, then the fix.

This added a UI-quality pass, a WCAG contrast audit (two colours failed and were fixed) and a separate C++ review
that found ten robustness defects (row 12 in the table below). Each finding was checked before it was fixed.

### 4. Authorship and disclosure

> I am the sole author. Verify with `git log --format='%an %ae' | sort -u` and a search for tool names outside
> `docs/ai_use_log.md`.

Commits carry my name only; AI tools are named only here and in the ownership statement, as the brief asks.

### 5. Publishing

The code was pushed and release v1.0.0 created; the downloaded ZIP's hash was checked against the one in the PDF.

### 6. Making the PDF prove the work

> Make the submission PDF self-verifying: for each of the 12 rubric criteria, link the exact files on GitHub, pinned
> to the submitted commit, next to the PDF section that discusses them. Keep the cover to one page and the PDF under 1 MB.

Result: the cover's rubric map links every criterion to its files on GitHub, pinned to the submitted commit.

### 7. Asking for an honest score

> Score the submission against the official rubric (weights attached) as a strict assessor. For each criterion give
> the score, the evidence and the single change that would gain the most marks. Also remove Appendix A and renumber
> all cross-references.

The empty handwritten-appendix placeholder was removed from the PDF. The score came back with the weak areas named
(handwritten derivation missing, single-day commit history, unfinished parts of this log) rather than hidden.

### 8. An outside review, and a review of my own prompts

I had the submission reviewed externally and passed on its two findings (rows 13-14 below): the PDF gave no code
to judge readability by, and the build failed with an NMake/MSVC error when CMake was run without the preset.
I also asked for my prompts to be reviewed:

> Review my master prompt as a prompt engineer: list each gap, what it caused in this project, and a fix. Then split
> it into staged prompts (Role, Context, Task, Constraints, Acceptance commands, Output), each ending in a gate.

The review is `docs/prompt_engineering.md`.

### 9. Presentation

> Redesign the submission PDF for a technical reviewer: a cover with title band, four headline results and link
> cards; consistent table and heading styles; page numbers. Constraints: report ≤ 6 pages, cover exactly one page,
> PDF < 1 MB, no external fonts. Show a rendered preview of the cover before finishing.

The cover and report were restyled; the report stayed within six pages and the PDF under 1 MB.

### 10. Learning the maths behind my own project

To be able to explain the work myself, I sent a series of learning prompts. Each one asked the AI to question me
rather than hand me the answer. Opening lines as sent:

1. "I want to understand the thermal FEA implementation in my Unified Engineering Workbench." (governing equation)
2. "Explain how the 1D two-node linear thermal element stiffness matrix is derived." (and asked to derive it myself)
3. "Explain how the uniform heat-generation term produces the element load vector in my thermal FEM implementation."
4. "Help me understand the analytical temperature solution:" (verification and midpoint error)
5. "I want to understand the backpropagation implementation in my C++ project." (chain rule)
6. "Walk me through one complete backpropagation training step." (with a numerical example)
7. "My project compares analytical backpropagation gradients against central finite-difference approximations."
8. "Act as a technical interviewer evaluating my understanding of the thermal FEM and backpropagation implementations."

### 11. Documenting what actually changed

I asked for an evidence-based comparison of the ChatGPT prototype against the final project, opening with:

> "# Task: Reconstruct My Actual Changes to the Unified Engineering Workbench"

and required that no change be attributed to me without evidence. The comparison (both versions inspected read-only)
found a full rewrite: no code statement is shared with the prototype, every prototype commit is authored by ChatGPT,
and every current source file was generated in the Claude Code session under my direction.

## What the AI produced

C++17 core (`src/core`), HTTP transport and Win32 launcher (`src/app`), CLI (`src/cli`), web UI (`src/web`),
tests (`tests/`), scripts, documentation drafts and the report layout. Every numeric result in `results/` was
produced by running the C++ code, not written by the AI.

## Independent verification

* 31 CTest cases (unit, numerical, negative, job registry, sample files), 31 HTTP integration checks, an E2E browser test
  (`tests/e2e_walkthrough.py`), file-size tests and a contrast audit; all pass on the submitted build (`evidence/`).
* Numerical results checked against closed-form theory, not against the AI's claims: midpoint error = qh²/8k
  exactly, order 2.000, energy balance 1.000 W = qAL, gradients vs central differences (~1e-10).
* Performance claims checked by repeated measurement; two hypotheses were tested by changing one thing at a time.
* The released ZIP was downloaded, extracted to a fresh folder with no compiler on PATH, and passed all 31 HTTP checks.

## Incorrect or unsuitable AI output that was identified (all real)

| # | What the AI produced | How it was caught | Resolution |
|---|---|---|---|
| 1 | Batch solver that heap-allocated every solve; p=4 was **slower** than p=3 (1.26x) and large meshes ran slower in parallel than serially. | Benchmark measurements | Per-worker reusable workspace + pre-allocated outputs: 3.8x at p=4. `results/performance/allocation_contention_before_after.md` |
| 2 | After fix 1, p=4 timings were bimodal (39 ms or 135 ms). | Repeated runs | Diagnosed as Windows EcoQoS power throttling; opt-out API made 20/20 samples stable. Same file. |
| 3 | `maxAbsDifference` used `std::max`, which silently ignores NaN, so a NaN result would **pass** the 1e-8 agreement check. | Code review while changing the benchmark | NaN now fails the check; test `agreement_detects_nan`. |
| 4 | File uploads sent without a content type; cpp-httplib caps form-urlencoded bodies at 8 KB, so the 14 kB sample STL failed with HTTP 413. | Integration test | Uploads sent as `application/octet-stream`. |
| 5 | A unit test expected a CSV mean of 50.5; the correct mean of (20.5, 80, 50) is 50.1667. | Test failure | Test corrected (the parser was right). |
| 6 | Diagram and mock described the J-Link **EDU Mini** as adapting to 1.2-5 V targets (true of other J-Link models). | Review while writing the pin-voltage table | Changed to 3.3 V target interface, 5 V tolerant; flagged for datasheet check. |
| 7 | Documentation drafts written in the first person describing debugging experiences and learning I had not had. | Review against "do not fabricate" | Rewritten in neutral, factual voice. |
| 8 | Peak rod temperature stated as 188.8 °C. | Recomputed by hand: dT/dx = 0 at x = 0.42 m | Corrected to 188.2 °C. |
| 9 | JS used `Math.max(...array)` for the hottest node (stack overflow above ~10^5 nodes) and then an `indexOf` on values serialised at different precision. | Self-review | Plain loop. |
| 10 | E2E test requested 4000 x 20000 cases, above the backend's 50 M-node memory cap, so the job was (correctly) refused. | E2E failure | Test uses a smaller batch. |
| 11 | JSON output used `snprintf` per number: 1M-node CSV took 7.0 s. | Upload size test | `std::to_chars`: 0.75 s. |
| 12 | Robustness defects in AI-written C++: a failed thread spawn left a job "running" forever; non-`std::exception` throws could terminate; data race on the launch-time value; thread-spawn failure in the pool could terminate; serial row omitted from `agreement_ok`; `nan` printed as convergence order when q = 0; no Host check (DNS rebinding); `+-5` parsed as a number; documented memory bound off by 2x. | Separate AI review pass (Claude Code `cpp-reviewer` agent), each finding checked before fixing | All fixed; new tests `jobs_busy_cancel_failure`, `convergence_order_roundoff`, `parsers_sign_and_range`, DNS-rebinding API check. |
| 13 | Submission PDF left the code-readability evidence cell empty, so a reviewer had to open the repository to judge the code. | External review I obtained | Appendix C prints excerpts straight from the source files (FE assembly/solver, worker pool). |
| 14 | Packaging script assumed the toolchain; run without it, CMake fell back to NMake/MSVC and failed with an unhelpful error. | External review I obtained | Script checks cmake/ninja/g++/git/objdump first and names what is missing; README warns against configuring without the preset. |

## My own contribution and understanding

**What I did** (each item is on record in the session or the repository):

* Rejected the ChatGPT prototype after reviewing it and decided on a full rebuild rather than a patch.
* Identified the one-PDF, 1 MB submission limit and required a delivery design for it before any code was written.
* Directed separate review passes (UI/accessibility, C++ robustness) and required every fix to come with a test.
* Required the PDF to link each rubric criterion to the submitted files, and obtained an external review whose two
  findings were then fixed.
* Asked for a strict rubric-based self-assessment and acted on it.
* Worked through the maths of my own implementation (section 10).

**What I did not do:** I did not write or hand-edit the source code. The implementation was generated by AI tools
under my direction, as disclosed above.

**How I explain the core parts:**

* **Thermal FEA.** The rod is split into N two-node linear elements of length h = L/N. Each element adds a stiffness
  matrix (kA/h)[1 −1; −1 1], describing heat flow between its two nodes, and a load vector (qAh/2)[1; 1], which splits
  the heat generated in the element equally between them. Assembly gives a tridiagonal system, solved with the
  Thomas algorithm in O(N).
* **Boundary conditions.** T(0) = 100 °C and T(L) = 20 °C are known, so each known value's column is moved to the
  right-hand side and its row is replaced by T = value. Without them the system is singular: temperature would only
  be fixed up to a constant.
* **Nodal vs midpoint error.** For this 1D problem linear elements are exact at the nodes, so the nodal error
  (~1e-13) is only rounding. Between nodes a straight line approximates a parabola; the largest gap is at element
  midpoints and equals qh²/8k. Halving h quarters it: 1.25, 0.3125, 0.0781, 0.0195 °C for N = 10, 20, 40, 80 (order 2).
* **Worker pool.** A fixed number of threads take case indices from an atomic counter, so no case is solved twice.
  Each worker writes its result into slot i of a pre-allocated array, so the output stays in input order whichever
  thread finishes first, and serial and parallel results are identical (difference 0.0 °C).
* **The two performance fixes.** Allocating memory on every solve made threads contend for the heap, so 4 workers
  ran slower than 3; each worker now reuses its own workspace. Windows power throttling (EcoQoS) made timings flip
  between 39 and 135 ms; the process now opts out. Result for 400 cases × 20,000 elements: serial median 160.6 ms,
  4 workers 40.7 ms, speed-up 3.95×.
* **Backpropagation.** The model is z = wx + b, h = tanh(z), ŷ = vh + c, J = ½(ŷ − y)², with x = 1, y = 2, w = 0.5,
  b = 0, v = 1, c = 0. Forward pass: h = 0.4621, ŷ = 0.4621, J = 1.1825. With e = ŷ − y = −1.5379 the chain rule gives
  ∂J/∂c = e, ∂J/∂v = e·h = −0.7107, ∂J/∂w = e·v·(1 − h²)·x = −1.2095 and ∂J/∂b = −1.2095, where (1 − h²) is the
  derivative of tanh. All four are taken at the original parameters and applied together with η = 0.1, which moves
  every parameter up because the prediction is too low. Central finite differences agree with each gradient to
  ~1e-10, which shows the derivative code matches the loss function, though not that the model is a good one.
