# AI-use log

Disclosure as required by section 8 of the brief. Entries are factual records of what happened; nothing is
reconstructed or invented. Sections marked **[candidate]** must be completed by the candidate personally.

## Tools and models

| Tool | Model | Used for |
|---|---|---|
| ChatGPT (OpenAI) | label recorded in that prototype's own log as "GPT-6" (not independently verified) | An earlier prototype (`UnifiedEngineeringWorkbench_Prototype`, not submitted). Used only as a reference for what to improve; no code was copied from it. |
| Claude Code (Anthropic), VS Code extension | Claude Opus 5.5 (`claude-opus-5-5`) | This submission: design, C++/JS/Python code, tests, documentation drafts, packaging, report layout. Session date 2026-11-01. |
| Claude Code skill `ui-ux-pro-max` | (local rule database) | UI review: style, font pairing (Fira Sans + Fira Code), 3D-view accessibility fallback. |

## Representative prompts (verbatim)

1. Initial request (attached: the three-page assignment PDF):
   > "I NEED THIS BUT NOTE THAT IN THE GOOGLE FORM ONLY 1 FORMAT IS ALLOWED FOR SUBMISSION THATS PDF FORMAT THAT TOO
   > UNDER 1 MB SO WHAT WILL BE OUR APPROACH? AND I WANT THE ASSIGNMENT TO SCORE 100/100 I WILL ALSO PROVIDE U A FOLDER
   > PATH IN WHICH I MADE ATTEMPT TO DO THIS | BUT I NEED A COMPLETE AND BETTER REWORK LEARN FROM IT
   > "<local path to the prototype folder>" AND MAKE IT BETTER"
   > (local path redacted: personal data)
2. Mid-session refinement:
   > "USE THE SKILLS THAT CAN ENHANCE THIS PROJECT"

**[candidate]** Add any further prompts you used (e.g. questions asked to understand the FE weak form or the chain
rule) with the date.

## What the AI produced

C++17 core (`src/core`), HTTP transport and Win32 launcher (`src/app`), CLI (`src/cli`), web UI (`src/web`),
tests (`tests/`), scripts, documentation drafts and the report layout. Every numeric result in `results/` was
produced by running the C++ code, not written by the AI.

## Independent verification

* 31 CTest cases (unit, numerical, negative, job registry, sample files), 31 HTTP integration checks, an E2E browser test
  (`tests/e2e_walkthrough.py`), file-size tests; all pass on the submitted build (`evidence/`).
* Numerical results checked against closed-form theory, not against the AI's claims: midpoint error = qh²/8k
  exactly, order 2.000, energy balance 1.000 W = qAL, gradients vs central differences (~1e-10).
* Performance claims checked by repeated measurement; two hypotheses were tested by changing one thing at a time.

## Incorrect or unsuitable AI output that was identified (all real, from this session)

| # | What the AI produced | How it was caught | Resolution |
|---|---|---|---|
| 1 | Batch solver that heap-allocated every solve; p=4 was **slower** than p=3 (1.26x) and large meshes ran slower in parallel than serially. | Benchmark measurements | Per-worker reusable workspace + pre-allocated outputs: 3.8x at p=4. `results/performance/allocation_contention_before_after.md` |
| 2 | After fix 1, p=4 timings were bimodal (39 ms or 135 ms). | Repeated runs | Diagnosed as Windows EcoQoS power throttling; opt-out API made 20/20 samples stable. Same file. |
| 3 | `maxAbsDifference` used `std::max`, which silently ignores NaN, so a NaN result would **pass** the 1e-8 agreement check. | Code review while changing the benchmark | NaN now fails the check; test `agreement_detects_nan`. |
| 4 | File uploads sent without a content type; cpp-httplib caps form-urlencoded bodies at 8 KB, so the 14 kB sample STL failed with HTTP 413. | Integration test | Uploads sent as `application/octet-stream`. |
| 5 | A unit test expected a CSV mean of 50.5; the correct mean of (20.5, 80, 50) is 50.1667. | Test failure | Test corrected (the parser was right). |
| 6 | Diagram and mock described the J-Link **EDU Mini** as adapting to 1.2-5 V targets (true of other J-Link models). | Review while writing the pin-voltage table | Changed to 3.3 V target interface, 5 V tolerant; flagged for datasheet check. |
| 7 | Documentation drafts written in the first person describing debugging experiences and learning the candidate had not had. | Review against "do not fabricate" | Rewritten in neutral, factual voice; candidate to add own reflections. |
| 8 | Peak rod temperature stated as 188.8 °C. | Recomputed by hand: dT/dx = 0 at x = 0.42 m | Corrected to 188.2 °C. |
| 9 | JS used `Math.max(...array)` for the hottest node (stack overflow above ~10^5 nodes) and then an `indexOf` on values serialised at different precision. | Self-review | Plain loop. |
| 10 | E2E test requested 4000 x 20000 cases, above the backend's 50 M-node memory cap, so the job was (correctly) refused. | E2E failure | Test uses 2000 cases. |
| 11 | JSON output used `snprintf` per number: 1M-node CSV took 7.0 s. | Upload size test | `std::to_chars`: 0.75 s. |
| 12 | Robustness defects in AI-written C++: a failed thread spawn left a job "running" forever; non-`std::exception` throws could terminate; data race on the launch-time value; thread-spawn failure in the pool could terminate; serial row omitted from `agreement_ok`; `nan` printed as convergence order when q = 0; no Host check (DNS rebinding); `+-5` parsed as a number; documented memory bound off by 2x. | Separate AI review pass (Claude Code `cpp-reviewer` agent), each finding checked by hand before fixing | All fixed; new tests `jobs_busy_cancel_failure`, `convergence_order_roundoff`, `parsers_sign_and_range`, DNS-rebinding API check. |

Issues found in the earlier ChatGPT prototype (reason for the rework): serial/parallel agreement compared only the
last node of each case; benchmark numbers came from a Linux container, not the target Windows PC; most C++ was
written as single very long lines; there was no cancellation; `Launch.exe` had never been built or run.

## Candidate's own edits and understanding **[candidate]**

List what you changed yourself after reviewing (files, reason), and confirm you can explain: the element
stiffness/load derivation and Dirichlet elimination, why nodal error is ~1e-13, the worker pool and why results
stay in input order, the two performance fixes, and every line of the backprop chain rule.
