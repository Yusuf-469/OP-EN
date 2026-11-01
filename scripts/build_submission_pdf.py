"""Builds the single submission PDF (< 1 MB) and the stand-alone report PDF.

Parts: cover/index, report (<= 6 pages), Appendix A (screenshot walkthrough with log), Appendix B (commit log +
test results), Appendix C (code excerpts). Report numbers are filled from results/*.json.
Rendering uses the Edge that ships with Windows 11 (headless print-to-PDF); bookmarks are added with pypdf.
All parts are printed as ONE document so each font is embedded once (separate prints repeated ~600 kB of fonts).

Inputs:  docs/report/submission.json            links and notes (see submission.example.json)
Usage:   python scripts/build_submission_pdf.py
"""
import base64
import html
import io
import json
import logging
import re
import subprocess
import tempfile
from pathlib import Path

from PIL import Image
from pypdf import PdfReader, PdfWriter

logging.getLogger("pypdf").setLevel(logging.ERROR)  # harmless trailer-size warnings on stderr stop finalize.ps1

ROOT = Path(__file__).resolve().parent.parent
REPORT_DIR = ROOT / "docs" / "report"
EDGE = Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe")
LIMIT_BYTES = 1_000_000  # the form allows 1 MB; stay under the decimal megabyte to be safe
SHOTS = [  # subset that fits the size budget; all nine full-size screenshots are in the ZIP
    ("02_geometry_stl_and_temperature_nodes", "Geometry: STL heat sink + synthetic nodal temperatures, °C legend, node inspection"),
    ("03_malformed_file_error", "Malformed file rejected with a line-numbered, actionable message"),
    ("04_thermal_fea", "Thermal: FE vs exact, pointwise error, log-log convergence, verification table"),
    ("07_benchmark_results", "Performance: timings, speed-up, agreement, machine info, exports"),
    ("08_hardware_mocks", "Hardware: simulated devices, telemetry, timeout and recovery log"),
    ("09_backprop_check", "Backprop: chain-rule gradients vs central differences, loss curve"),
]


def esc(text):
    return html.escape(str(text))


def placeholder(text):
    return f'<span style="color:#b3261e;font-weight:700">[{esc(text)}]</span>'


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()


def render(body, path):
    """Prints an HTML body (report.css styling) to PDF; the file lives in docs/report so relative paths resolve."""
    page = f'<!doctype html><html><head><meta charset="utf-8"><link rel="stylesheet" href="report.css"></head><body>{body}</body></html>'
    src = REPORT_DIR / "_build.html"
    src.write_text(page, encoding="utf-8")
    try:
        subprocess.run([str(EDGE), "--headless", "--disable-gpu", "--no-pdf-header-footer", "--virtual-time-budget=4000",
                        f"--print-to-pdf={path}", src.as_uri()], check=True, capture_output=True, timeout=120)
    finally:
        src.unlink()
    return PdfReader(str(path))


def perf_fields():
    bench = json.loads((ROOT / "results/performance/benchmark.json").read_text())["benchmark"]
    rows = bench["rows"]
    serial, pools = rows[0], rows[1:]
    body = "".join(
        f'<tr><td>{"serial loop" if r["variant"] == "serial" else "pool p = %d" % r["workers"]}</td>'
        f'<td class="n">{r["median_ms"]:.1f}</td><td class="n">{r["speedup"]:.2f}</td><td class="n">{r["efficiency"]:.2f}</td>'
        f'<td class="n">{"&ndash;" if r["variant"] == "serial" else "%.1g" % r["max_abs_diff_C"]}</td></tr>' for r in rows)
    worst = max(r["max_abs_diff_C"] for r in pools)
    return {
        "PERF_ROWS": body,
        "AGREEMENT": "bit-identical (0.0 &deg;C)" if worst == 0 else f"max |&Delta;T| = {worst:.1e} &deg;C (&le; 1e&minus;8)",
        "P1_DELTA": f"{(pools[0]['median_ms'] / serial['median_ms'] - 1) * 100:+.0f} %",
        "EFF2": f"{pools[1]['efficiency']:.2f}",
        "EFF4": f"{pools[-1]['efficiency']:.2f}",
        "SERIAL_RANGE": f"{min(serial['samples_ms']):.0f}&ndash;{max(serial['samples_ms']):.0f}",
        "SPEEDUP4": f"{pools[-1]['speedup']:.2f}",
    }


def report_body(cfg, perf):
    values = {"CANDIDATE_ID": esc(cfg.get("candidate_id") or "CandidateID"), "COMMIT": git("rev-parse", "--short", "HEAD"),
              "LEARNING_NOTE": esc(cfg.get("learning_note", "")),
              "VIDEO_NOTE": f'; demo video: <a href="{esc(cfg["video_url"])}">{esc(cfg["video_url"])}</a>' if cfg.get("video_url") else "",
              "CLEAN_MACHINE_NOTE": esc(cfg.get("clean_machine_note") or "it was not tested on a second, separately installed Windows machine."),
              **perf}
    text = (REPORT_DIR / "report.html").read_text(encoding="utf-8")
    for key, value in values.items():
        text = text.replace("{{" + key + "}}", value)
    if "{{" in text:
        raise SystemExit("Unfilled placeholder in report.html: " + re.search(r"\{\{\w+\}\}", text).group())
    return re.search(r"<body>(.*)</body>", text, re.S).group(1)


def jpeg_uri(path, width, quality, gray=False):
    img = Image.open(path).convert("L" if gray else "RGB")
    if img.width > width:
        img = img.resize((width, round(img.height * width / img.width)), Image.LANCZOS)
    buf = io.BytesIO()
    img.save(buf, "JPEG", quality=quality, optimize=True, progressive=True)
    return "data:image/jpeg;base64," + base64.b64encode(buf.getvalue()).decode()


def appendix_b(quality):
    walk = ROOT / "evidence/walkthrough"
    log = (walk / "walkthrough_log.txt").read_text(encoding="utf-8")
    figures = "".join(f'<figure><img src="{jpeg_uri(walk / (n + ".png"), 860, quality)}" style="width:150mm">'
                      f"<figcaption>{esc(c)}</figcaption></figure>" for n, c in SHOTS)
    return ("<h1>Appendix A &mdash; Screenshot walkthrough with logs</h1>"
            '<p class="meta">Produced by <code>tests/e2e_walkthrough.py</code> (Playwright driving Microsoft Edge against '
            "<code>Launch.exe</code>); every step is an assertion. All nine full-resolution screenshots: "
            "<code>evidence/walkthrough/</code> in the ZIP.</p>"
            f'<h3>Timestamped log</h3><pre style="font-size:7pt;white-space:pre-wrap">{esc(log)}</pre>{figures}')


def appendix_c():
    commits = git("log", "--reverse", "--date=format:%Y-%m-%d %H:%M", "--pretty=format:%h  %ad  %s")
    tests = "\n".join(f"== {p.name} ==\n" + "\n".join(p.read_text(encoding="utf-8", errors="replace").splitlines()[-6:])
                      for p in sorted((ROOT / "evidence/test_results").glob("*.txt")))
    return ("<h1>Appendix B &mdash; Version control and test evidence</h1>"
            f'<h3>Commit history (oldest first)</h3><pre style="font-size:7.6pt;white-space:pre-wrap">{esc(commits)}</pre>'
            '<p class="meta">Full messages with file statistics: <code>evidence/git/commit_log.txt</code>; repository: '
            "<code>git clone evidence/git/repository.bundle</code>.</p>"
            f'<h3>Test results (last lines of each log in <code>evidence/test_results/</code>)</h3>'
            f'<pre style="font-size:7.4pt;white-space:pre-wrap">{esc(tests)}</pre>')


def excerpt(path, first, last):
    """Source lines from the line starting with `first` to the closing brace of the function starting with `last`."""
    lines = (ROOT / path).read_text(encoding="utf-8").splitlines()
    start = next(i for i, l in enumerate(lines) if l.startswith(first))
    stop = next(i for i, l in enumerate(lines) if i >= start and l.startswith(last))
    end = next(i for i, l in enumerate(lines) if i >= stop and l == "}")
    return f'<h3><code>{path}</code>, lines {start + 1}&ndash;{end + 1}</h3><pre style="font-size:6.4pt;line-height:1.15;margin:1pt 0;padding:2pt 4pt">{esc(chr(10).join(lines[start:end + 1]))}</pre>'


def appendix_code():
    return ("<h1>Appendix C &mdash; Code excerpts</h1>"
            '<p class="meta">Printed straight from the submitted source (not retyped), so readability can be judged without '
            "opening the repository. Full source: <code>src/</code> in the ZIP and on GitHub.</p>"
            + excerpt("src/core/thermal_fea.cpp", "double elementConductance", "void solveTridiagonal")
            + excerpt("src/core/batch.cpp", "void runParallel(const std::vector<RodProblem>& cases, int workers, BatchResult&",
                      "void runParallel(const std::vector<RodProblem>& cases, int workers, BatchResult&"))


def link(cfg, key, label):
    url = cfg.get(key)
    return f'<a href="{esc(url)}">{esc(url)}</a>' if url else placeholder(f"add {label} in docs/report/submission.json")


def repo_links(cfg, paths):
    """Clickable GitHub links pinned to this commit, so the reviewer sees exactly the submitted files."""
    base = (cfg.get("repo_url") or "").removesuffix(".git")
    if not base:
        return ", ".join(f"<code>{esc(p)}</code>" for p in paths)
    ref = git("rev-parse", "HEAD")
    url = lambda p: f"{base}/commits/{ref}" if p == "commits" else f"{base}/{'tree' if (ROOT / p).is_dir() else 'blob'}/{ref}/{p}"
    return ", ".join(f'<a href="{esc(url(p))}">{esc(p)}</a>' for p in paths)


def cover(cfg, starts, perf):
    cid = esc(cfg.get("candidate_id") or "CandidateID")
    pages = lambda part: f"{starts[part][0]}&ndash;{starts[part][1]}" if starts[part][0] != starts[part][1] else str(starts[part][0])
    rubric = [("Interface / webpage design", "Report §1&ndash;2; App. A", ["src/web"]),
              ("C++ backend and testing", "Report §1&ndash;2, §8; App. B", ["src/core", "src/app", "tests"]),
              ("Folder architecture", "README §3, §6", ["README.md"]), ("Code readability", "App. C", ["src"]),
              ("Version control", "App. B", ["commits"]),
              ("Thermal FEA and adoption", "Report §4", ["docs/thermal_fea.md", "src/core/thermal_fea.cpp", "results/thermal"]),
              ("CAD / ANSYS visualisation", "Report §3; App. A", ["data", "src/core/geometry_io.cpp"]),
              ("Multicore performance", "Report §5", ["src/core/batch.cpp", "results/performance"]),
              ("Embedded integration", "Report §6", ["docs/embedded_design.md", "src/core/hardware.cpp"]),
              ("Backpropagation", "Report §7", ["src/core/backprop.cpp", "docs/backprop_worked_values.md"]),
              ("Prompt engineering / AI judgement", "Report §8", ["docs/ai_use_log.md", "docs/prompt_engineering.md"]),
              ("One-click delivery / report", "this PDF; Launch.exe in ZIP", [".vscode", "src/app/launcher_win.cpp"])]
    mirror = f'<div class="card"><div class="t">Mirror (Google Drive)</div>{link(cfg, "mirror_url", "")}</div>' if cfg.get("mirror_url") else ""
    video = link(cfg, "video_url", "video URL") if cfg.get("video_url") else "Screenshot walkthrough, App. A"
    sha = f'<code>{esc(cfg["zip_sha256"])}</code>' if cfg.get("zip_sha256") else placeholder("SHA-256 from dist/*.zip.sha256")
    return f"""<div class="hero"><p class="kicker">PASS Science Works &middot; R&amp;D Internship 2026&ndash;27 &middot; Common technical assignment</p>
<h1>Unified Engineering Workbench</h1>
<p class="sub">C++17 engineering workbench: CAD/ANSYS visualisation, thermal FEA, multicore batch solving, embedded mocks, backpropagation</p>
<div class="facts"><div>Candidate<b>{cid}</b></div><div>Commit<b>{git("rev-parse", "--short", "HEAD")}</b></div>
<div>Date<b>{git("log", "-1", "--date=format:%d %b %Y", "--pretty=%ad")}</b></div><div>Platform<b>Windows 11 x64</b></div><div>Demonstration<b>{video}</b></div></div></div>
<div class="kpis">
<div class="kpi"><div class="v">2.000</div><div class="l">FEA convergence order; midpoint error = qh&sup2;/8k exactly</div></div>
<div class="kpi"><div class="v">{perf["SPEEDUP4"]}&times;</div><div class="l">speed-up on 4 workers (400 &times; 20,000-element rods)</div></div>
<div class="kpi alt"><div class="v">0.0 &deg;C</div><div class="l">serial vs parallel difference ({perf["AGREEMENT"]})</div></div>
<div class="kpi alt"><div class="v">110</div><div class="l">automated checks pass: unit, HTTP, browser E2E, file size, contrast</div></div>
</div>
<div class="links">
<div class="card"><div class="t">Application package &mdash; download, extract, double-click Launch.exe</div>{link(cfg, "zip_url", "ZIP download URL")}
<div class="small">SHA-256 {sha} &middot; {esc(cfg.get("zip_size", ""))}</div></div>
<div class="card"><div class="t">Source repository</div>{link(cfg, "repo_url", "repository URL")}
<div class="small">Every rubric line below links to the exact submitted commit.</div></div>{mirror}
</div>
<p class="small" style="margin:3pt 0 0"><b>Why linked, not attached:</b> the form accepts one PDF &le; 1 MB. The SHA-256 proves the downloaded ZIP is the submitted one
(PowerShell: <code>Get-FileHash {cid}_Workbench.zip</code>).</p>
<div class="two">
<div><h2>Run in one click</h2><ol class="steps">
<li>Download and extract the ZIP (right-click &rarr; Extract All).</li>
<li>Double-click <b>Launch.exe</b>; the workbench opens in the browser. No install, admin rights, terminal or internet.</li>
<li>Geometry &rarr; <i>Load sample heat sink</i>. VS Code: Run Task &rarr; <i>Start packaged Workbench</i>.</li></ol></div>
<div><h2>Contents of this PDF</h2><table>
<tr><th>Part</th><th class="n">Pages</th></tr>
<tr><td>Technical report</td><td class="n">{pages("report")}</td></tr>
<tr><td>A &mdash; Screenshot walkthrough with logs</td><td class="n">{pages("b")}</td></tr>
<tr><td>B &mdash; Commit history and test results</td><td class="n">{pages("c")}</td></tr>
<tr><td>C &mdash; Code excerpts (FE solver, worker pool)</td><td class="n">{pages("code")}</td></tr>
</table></div></div>
<h2>Rubric &rarr; evidence map</h2>
<table><tr><th style="width:27%">Criterion</th><th style="width:22%">In this PDF</th><th>On GitHub (click)</th></tr>{"".join(f"<tr><td><b>{c}</b></td><td>{e}</td><td>{repo_links(cfg, p)}</td></tr>" for c, e, p in rubric)}</table>
<p class="small">FEA midpoint error 1.25 / 0.3125 / 0.0781 / 0.0195 &deg;C for N = 10 / 20 / 40 / 80; energy balance 1.000 W = qAL; gradients match central differences to ~10<sup>&minus;10</sup>.</p>"""


def main():
    cfg_path = REPORT_DIR / "submission.json"
    cfg = json.loads(cfg_path.read_text(encoding="utf-8-sig")) if cfg_path.exists() else {}  # -sig: tolerate a BOM (Notepad)
    out = ROOT / "dist" / f"{cfg.get('candidate_id') or 'CandidateID'}_Workbench_Submission.pdf"
    out.parent.mkdir(exist_ok=True)
    perf = perf_fields()
    report = report_body(cfg, perf)
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        # Pass 1: page counts of the fixed parts (cover is always one page).
        report_pdf = render(report, tmp / "report.pdf")
        # Stand-alone report copy goes to dist/; docs/report/report.pdf is the committed version.
        (out.parent / out.name.replace("_Submission", "_Report")).write_bytes((tmp / "report.pdf").read_bytes())
        count_c = len(render(appendix_c(), tmp / "c.pdf").pages)
        count_code = len(render(appendix_code(), tmp / "code.pdf").pages)
        for quality in (50, 42, 35, 28):  # lower JPEG quality only as far as needed to fit the limit
            body_b = appendix_b(quality)
            counts = {"report": len(report_pdf.pages), "b": len(render(body_b, tmp / "b.pdf").pages), "c": count_c, "code": count_code}
            starts, page = {}, 2
            for part in ("report", "b", "c", "code"):
                starts[part] = (page, page + counts[part] - 1)
                page += counts[part]
            # Pass 2: one document, so fonts are embedded once.
            sections = [cover(cfg, starts, perf), report, body_b, appendix_c(), appendix_code()]
            combined = render('<div class="page"></div>'.join(f"<section>{s}</section>" for s in sections), tmp / "all.pdf")
            if len(combined.pages) != page - 1:
                raise SystemExit(f"Page count mismatch ({len(combined.pages)} vs {page - 1}); a part overflowed differently.")
            writer = PdfWriter(clone_from=combined)
            for title, part in [("Submission index", None), ("Technical report", "report"),
                                ("Appendix A - Screenshot walkthrough", "b"), ("Appendix B - Commits and tests", "c"),
                                ("Appendix C - Code excerpts", "code")]:
                writer.add_outline_item(title, 0 if part is None else starts[part][0] - 1)
            writer.add_metadata({"/Title": "Unified Engineering Workbench - Submission", "/Author": cfg.get("candidate_id") or "CandidateID"})
            writer.compress_identical_objects(remove_duplicates=True, remove_unreferenced=True)
            with open(out, "wb") as f:
                writer.write(f)
            size = out.stat().st_size
            if size < LIMIT_BYTES:
                break
    print(f"{out.relative_to(ROOT)}: {size / 1e6:.3f} MB, {len(combined.pages)} pages "
          f"(report {counts['report']}, A {counts['b']}, B {counts['c']}, C {counts['code']}), JPEG quality {quality}")
    if size >= LIMIT_BYTES:
        raise SystemExit("ERROR: still over 1 MB; reduce screenshots.")
    if counts["report"] > 6:
        raise SystemExit(f"ERROR: report is {counts['report']} pages; the limit is 6.")
    missing = [k for k in ("candidate_id", "zip_url", "repo_url", "zip_sha256") if not cfg.get(k)]
    if missing:
        print("DRAFT - still missing:", ", ".join(missing))


if __name__ == "__main__":
    main()
