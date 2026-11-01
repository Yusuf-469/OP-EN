# AI-use log

Disclosure as required by section 8 of the brief. This is a factual record of how I used AI tools on this project.
Prompts in quotation marks are copied exactly as I typed them (capitals and typos included). Under several of them is
a refined version I would use next time, clearly marked as written afterwards; those were not sent to any tool. Sections marked **[candidate]** are still for me to complete personally.

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

> "I NEED THIS BUT NOTE THAT IN THE GOOGLE FORM ONLY 1 FORMAT IS ALLOWED FOR SUBMISSION THATS PDF FORMAT THAT TOO
> UNDER 1 MB SO WHAT WILL BE OUR APPROACH? AND I WANT THE ASSIGNMENT TO SCORE 100/100 I WILL ALSO PROVIDE U A FOLDER
> PATH IN WHICH I MADE ATTEMPT TO DO THIS | BUT I NEED A COMPLETE AND BETTER REWORK LEARN FROM IT
> "<local path to the prototype folder>" AND MAKE IT BETTER"
> (local path redacted: personal data)

**Refined version (written afterwards, not sent):**
> Act as the delivery lead for a graded Windows 11 C++ prototype. The submission form accepts exactly one PDF of at most
> 1 MB; the brief also requires a runnable ZIP with `Launch.exe`. Propose a delivery design that satisfies both, with
> the trade-offs. Then rebuild the project from scratch, using `<prototype folder>` only as a list of defects to avoid.
> Acceptance: every rubric item has evidence; no numbers that were not measured; the PDF build fails if it exceeds 1 MB.

What came out of it: the ZIP is hosted and the PDF carries the report, the evidence and the ZIP's SHA-256, so a
reviewer can prove the download is the submitted file. When asked, I chose my registration number as the candidate
ID and to host the ZIP as a GitHub release (a Google Drive mirror was also planned but not set up).

### 3. Bringing in review tools

> "USE THE SKILLS THAT CAN ENHANCE THIS PROJECT"

**Refined version (written afterwards, not sent):**
> Run three independent review passes on the current code and report findings before fixing anything: (1) UI against
> WCAG 2.2 AA (contrast, focus, 44 px targets, reduced motion); (2) C++ for lifetime, concurrency and exception-safety
> defects; (3) input validation at the HTTP boundary. For each finding give file:line, a failing test, then the fix.

This added a UI-quality pass, a WCAG contrast audit (two colours failed and were fixed) and a separate C++ review
that found ten robustness defects (row 12 in the table below). Each finding was checked before it was fixed.

### 4. Authorship and disclosure

> "I AM THE SOLE AUTHOR FOR THIS MAKE SURE TO REMOVE YOUR NAME FROM REPO AND FOLLOW THE FILE REQ FOR DISCLOSURE OF
> AI ALSO MY PERSONAL MAID ID IS- <email>"
> (email redacted: personal data)

**Refined version (written afterwards, not sent):**
> I am the sole author. Rewrite git history so every commit is authored by me, remove any tool attribution from
> commits and source files, and name the AI tools only in the disclosure files the brief requires. Verify with
> `git log --format='%an %ae' | sort -u` and a search for tool names outside `docs/ai_use_log.md`.

Commits carry my name only; AI tools are named only here and in the ownership statement, as the brief asks.

### 5. Publishing

> "DO U WANT A GITHUB REPO LINK FOR IT?"

> "HANDWRITTEN NOT NEEDED FOR NOW
> I HAVE GIVEN U GIT LINK  JUST DO IT"

The code was pushed and release v1.0.0 created; the downloaded ZIP's hash was checked against the one in the PDF.

### 6. Making the PDF prove the work

> "WHERE IS THE PDF THAT I HAVE TO SUBMIT?"

> "IN THE PDF WHY DONT WE PUT GITHUB LINKS OR ANY NECESSARY STUFF
> SO THAT THEY KNOW I HAVE COMPLETED EVERYTHING"

**Refined version (written afterwards, not sent):**
> Make the submission PDF self-verifying: for each of the 12 rubric criteria, link the exact files on GitHub, pinned
> to the submitted commit, next to the PDF section that discusses them. Keep the cover to one page and the PDF under 1 MB.

Result: the cover's rubric map links every criterion to its files on GitHub, pinned to the submitted commit.

### 7. Asking for an honest score

> "remove the appendix a
> also answer this in chat
> out of 100 how much will u give this task
> be brutally honest
> mark it acc to company metric"

**Refined version (written afterwards, not sent):**
> Score the submission against the official rubric (weights attached) as a strict assessor. For each criterion give
> the score, the evidence and the single change that would gain the most marks. Also remove Appendix A and renumber
> all cross-references.

The empty handwritten-appendix placeholder was removed from the PDF. The score came back with the weak areas named
(handwritten derivation missing, single-day commit history, unfinished parts of this log) rather than hidden.

### 8. An outside review, and a review of my own prompts

I had the submission reviewed externally and passed on its two findings (rows 13-14 below): the PDF gave no code
to judge readability by, and the build failed with an NMake/MSVC error when CMake was run without the preset.
I also asked for my prompts to be reviewed:

> "i want u to improve my prompts
> i will get u a prompt
> u can just put it stage by stage"

**Refined version (written afterwards, not sent):**
> Review my master prompt as a prompt engineer: list each gap, what it caused in this project, and a fix. Then split
> it into staged prompts (Role, Context, Task, Constraints, Acceptance commands, Output), each ending in a gate.

The review is `docs/prompt_engineering.md`. It keeps my original prompt unedited and marks the refined, staged
versions as written afterwards; they were not sent to any tool.

### 9. Presentation

> "improve the formatting and make it more appealing
> the pdf feels too plain rn"

**Refined version (written afterwards, not sent):**
> Redesign the submission PDF for a technical reviewer: a cover with title band, four headline results and link
> cards; consistent table and heading styles; page numbers. Constraints: report ≤ 6 pages, cover exactly one page,
> PDF < 1 MB, no external fonts. Show a rendered preview of the cover before finishing.

The cover and report were restyled; the report stayed within six pages and the PDF under 1 MB.

**[candidate]** Add any further prompts you used (e.g. questions asked to understand the FE weak form or the chain
rule) with the date.

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

## My own edits and understanding **[candidate]**

List what you changed yourself after reviewing (files, reason), and confirm you can explain: the element
stiffness/load derivation and Dirichlet elimination, why nodal error is ~1e-13, the worker pool and why results
stay in input order, the two performance fixes, and every line of the backprop chain rule.
