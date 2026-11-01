"""Builds the single submission PDF (< 1 MB) and the stand-alone report PDF.

Parts: cover/index, report (<= 6 pages), Appendix A (handwritten scans), Appendix B (screenshot walkthrough with
log), Appendix C (commit log + test results). Report numbers are filled from results/*.json.
Rendering uses the Edge that ships with Windows 11 (headless print-to-PDF); bookmarks are added with pypdf.
All parts are printed as ONE document so each font is embedded once (separate prints repeated ~600 kB of fonts).

Inputs:  docs/report/submission.json            links and notes (see submission.example.json)
         docs/handwritten/page1.jpg, page2.jpg  scans of the handwritten derivation
Usage:   python scripts/build_submission_pdf.py
"""
import base64
import html
import io
import json
import re
import subprocess
import tempfile
from pathlib import Path

from PIL import Image
from pypdf import PdfReader, PdfWriter

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


def appendix_a(quality):
    scans = [p for p in (ROOT / "docs/handwritten/page1.jpg", ROOT / "docs/handwritten/page2.jpg") if p.exists()]
    title = "<h1>Appendix A &mdash; Handwritten backpropagation (Task F)</h1>"
    if not scans:
        return title + (f'<p>{placeholder("Insert the two scanned pages as docs/handwritten/page1.jpg and page2.jpg, then rebuild")}</p>'
                        "<p>Expected: forward pass with numbers; chain-rule derivatives for w, b, v, c; one simultaneous gradient-descent "
                        "step from the original parameters; explanation of gradient direction. Check values: "
                        "<code>docs/backprop_worked_values.md</code>.</p>"), False
    pages = [f'<div style="text-align:center"><img src="{jpeg_uri(p, 1240, quality, gray=True)}" style="max-width:100%;max-height:{252 if i == 0 else 268}mm"></div>'
             for i, p in enumerate(scans)]
    return title + '<div class="page"></div>'.join(pages), True


def appendix_b(quality):
    walk = ROOT / "evidence/walkthrough"
    log = (walk / "walkthrough_log.txt").read_text(encoding="utf-8")
    figures = "".join(f'<figure><img src="{jpeg_uri(walk / (n + ".png"), 860, quality)}" style="width:150mm">'
                      f"<figcaption>{esc(c)}</figcaption></figure>" for n, c in SHOTS)
    return ("<h1>Appendix B &mdash; Screenshot walkthrough with logs</h1>"
            '<p class="meta">Produced by <code>tests/e2e_walkthrough.py</code> (Playwright driving Microsoft Edge against '
            "<code>Launch.exe</code>); every step is an assertion. All nine full-resolution screenshots: "
            "<code>evidence/walkthrough/</code> in the ZIP.</p>"
            f'<h3>Timestamped log</h3><pre style="font-size:7pt;white-space:pre-wrap">{esc(log)}</pre>{figures}')


def appendix_c():
    commits = git("log", "--reverse", "--date=format:%Y-%m-%d %H:%M", "--pretty=format:%h  %ad  %s")
    tests = "\n".join(f"== {p.name} ==\n" + "\n".join(p.read_text(encoding="utf-8", errors="replace").splitlines()[-6:])
                      for p in sorted((ROOT / "evidence/test_results").glob("*.txt")))
    return ("<h1>Appendix C &mdash; Version control and test evidence</h1>"
            f'<h3>Commit history (oldest first)</h3><pre style="font-size:7.6pt;white-space:pre-wrap">{esc(commits)}</pre>'
            '<p class="meta">Full messages with file statistics: <code>evidence/git/commit_log.txt</code>; repository: '
            "<code>git clone evidence/git/repository.bundle</code>.</p>"
            f'<h3>Test results (last lines of each log in <code>evidence/test_results/</code>)</h3>'
            f'<pre style="font-size:7.4pt;white-space:pre-wrap">{esc(tests)}</pre>')


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
    rubric = [("Interface / webpage design", "Report §1&ndash;2; App. B", ["src/web"]),
              ("C++ backend and testing", "Report §1&ndash;2, §8; App. C", ["src/core", "src/app", "tests"]),
              ("Folder architecture", "README §3, §6", ["README.md"]), ("Code readability", "", ["src"]),
              ("Version control", "App. C", ["commits"]),
              ("Thermal FEA and adoption", "Report §4", ["docs/thermal_fea.md", "src/core/thermal_fea.cpp", "results/thermal"]),
              ("CAD / ANSYS visualisation", "Report §3; App. B", ["data", "src/core/geometry_io.cpp"]),
              ("Multicore performance", "Report §5", ["src/core/batch.cpp", "results/performance"]),
              ("Embedded integration", "Report §6", ["docs/embedded_design.md", "src/core/hardware.cpp"]),
              ("Backpropagation", "Report §7; App. A", ["src/core/backprop.cpp", "docs/backprop_worked_values.md"]),
              ("Prompt engineering / AI judgement", "Report §8", ["docs/ai_use_log.md"]),
              ("One-click delivery / report", "this PDF; Launch.exe in ZIP", [".vscode", "src/app/launcher_win.cpp"])]
    mirror = f'<tr><th>Mirror download (Google Drive)</th><td>{link(cfg, "mirror_url", "")}</td></tr>' if cfg.get("mirror_url") else ""
    video = link(cfg, "video_url", "video URL") if cfg.get("video_url") else "Timestamped screenshot walkthrough with logs: Appendix B (accepted equivalent)"
    sha = f'<code>{esc(cfg["zip_sha256"])}</code>' if cfg.get("zip_sha256") else placeholder("SHA-256 from dist/*.zip.sha256")
    return f"""<h1>Unified Engineering Workbench &mdash; Submission</h1>
<p class="meta">PASS Science Works R&amp;D Internship 2026-27 &middot; common technical assignment &middot; Candidate <b>{cid}</b> &middot; commit {git("rev-parse", "--short", "HEAD")} &middot; {git("log", "-1", "--date=format:%Y-%m-%d", "--pretty=%ad")}</p>
<div class="note"><b>Why the application is linked, not attached.</b> The submission form accepts one PDF of at most 1 MB. The runnable package
(Launch.exe, source, tests, data, results, docs, git bundle) is <b>{cid}_Workbench.zip</b> at the link below; this PDF carries the report and the key
evidence. The SHA-256 lets you confirm the downloaded ZIP is exactly the one submitted (PowerShell: <code>Get-FileHash {cid}_Workbench.zip</code>).</div>
<table>
<tr><th style="width:27%">Application package (ZIP)</th><td>{link(cfg, "zip_url", "ZIP download URL")}<br>SHA-256: {sha} {esc(cfg.get("zip_size", ""))}</td></tr>
{mirror}
<tr><th>Source repository</th><td>{link(cfg, "repo_url", "repository URL")}</td></tr>
<tr><th>Demonstration</th><td>{video}</td></tr>
</table>
<h2>Run in one click</h2>
<p>1. Download and extract the ZIP (right-click &rarr; Extract All). 2. Double-click <b>Launch.exe</b>: the workbench opens in the default browser
(Windows 11 x64; no install, admin rights, terminal or internet). 3. Geometry &rarr; <i>Load sample heat sink</i>. In VS Code / Antigravity: open the folder
&rarr; Terminal &rarr; Run Task &rarr; <i>Start packaged Workbench</i>.</p>
<h2>Contents of this PDF</h2>
<table>
<tr><th>Part</th><th class="n">Pages</th></tr>
<tr><td>Technical report: design, FEA verification, performance, embedded design, backprop, tests, limitations, AI use</td><td class="n">{pages("report")}</td></tr>
<tr><td>Appendix A &mdash; handwritten backpropagation derivation</td><td class="n">{pages("a")}</td></tr>
<tr><td>Appendix B &mdash; timestamped screenshot walkthrough with logs</td><td class="n">{pages("b")}</td></tr>
<tr><td>Appendix C &mdash; commit history and test results</td><td class="n">{pages("c")}</td></tr>
</table>
<h2>Headline results</h2>
<p>FEA midpoint error 1.25 / 0.3125 / 0.0781 / 0.0195 &deg;C for N = 10 / 20 / 40 / 80, equal to qh&sup2;/8k (order 2.000); energy balance 1.000 W = qAL.
400 &times; 20,000-element batch: speed-up {perf["SPEEDUP4"]}&times; on 4 workers, serial/parallel agreement {perf["AGREEMENT"]}.
Gradients match central differences to ~10<sup>&minus;10</sup>. 110 automated checks pass (C++ unit/numerical/negative, HTTP integration, browser E2E, file size, WCAG contrast).</p>
<h2>Rubric &rarr; evidence map</h2>
<table><tr><th>Criterion</th><th>In this PDF</th><th>On GitHub (click)</th></tr>{"".join(f"<tr><td>{c}</td><td>{e}</td><td>{repo_links(cfg, p)}</td></tr>" for c, e, p in rubric)}</table>"""


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
        for quality in (50, 42, 35, 28):  # lower JPEG quality only as far as needed to fit the limit
            body_a, has_scans = appendix_a(quality)
            body_b = appendix_b(quality)
            counts = {"report": len(report_pdf.pages), "a": len(render(body_a, tmp / "a.pdf").pages),
                      "b": len(render(body_b, tmp / "b.pdf").pages), "c": count_c}
            starts, page = {}, 2
            for part in ("report", "a", "b", "c"):
                starts[part] = (page, page + counts[part] - 1)
                page += counts[part]
            # Pass 2: one document, so fonts are embedded once.
            sections = [cover(cfg, starts, perf), report, body_a, body_b, appendix_c()]
            combined = render('<div class="page"></div>'.join(f"<section>{s}</section>" for s in sections), tmp / "all.pdf")
            if len(combined.pages) != page - 1:
                raise SystemExit(f"Page count mismatch ({len(combined.pages)} vs {page - 1}); a part overflowed differently.")
            writer = PdfWriter(clone_from=combined)
            for title, part in [("Submission index", None), ("Technical report", "report"), ("Appendix A - Handwritten derivation", "a"),
                                ("Appendix B - Screenshot walkthrough", "b"), ("Appendix C - Commits and tests", "c")]:
                writer.add_outline_item(title, 0 if part is None else starts[part][0] - 1)
            writer.add_metadata({"/Title": "Unified Engineering Workbench - Submission", "/Author": cfg.get("candidate_id") or "CandidateID"})
            writer.compress_identical_objects(remove_duplicates=True, remove_unreferenced=True)
            with open(out, "wb") as f:
                writer.write(f)
            size = out.stat().st_size
            if size < LIMIT_BYTES:
                break
    print(f"{out.relative_to(ROOT)}: {size / 1e6:.3f} MB, {len(combined.pages)} pages "
          f"(report {counts['report']}, A {counts['a']}, B {counts['b']}, C {counts['c']}), JPEG quality {quality}")
    if size >= LIMIT_BYTES:
        raise SystemExit("ERROR: still over 1 MB; reduce screenshots or scan resolution.")
    if counts["report"] > 6:
        raise SystemExit(f"ERROR: report is {counts['report']} pages; the limit is 6.")
    missing = [k for k in ("candidate_id", "zip_url", "repo_url", "zip_sha256") if not cfg.get(k)] + ([] if has_scans else ["handwritten scans"])
    if missing:
        print("DRAFT - still missing:", ", ".join(missing))


if __name__ == "__main__":
    main()
