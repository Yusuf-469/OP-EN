"""End-to-end UI test and timestamped screenshot walkthrough.

Starts Launch.exe headless, drives the real UI in Microsoft Edge via
Playwright, asserts each step, and writes screenshots plus a timestamped log
to evidence/walkthrough/. Requires: pip install playwright==1.48.0 (uses the
Edge already installed on Windows 11; no browser download needed).

Usage: python tests/e2e_walkthrough.py <path-to-Launch.exe> [out_dir]
"""
import subprocess
import sys
import tempfile
import time
from datetime import datetime
from pathlib import Path

from playwright.sync_api import expect, sync_playwright

ROOT = Path(__file__).resolve().parent.parent
log_lines = []


def log(message):
    line = f"{datetime.now().isoformat(timespec='seconds')}  {message}"
    print(line)
    log_lines.append(line)


def shot(page, out, name, caption):
    page.screenshot(path=str(out / f"{name}.png"))
    log(f"screenshot {name}.png - {caption}")


def start_app(exe):
    port_file = Path(tempfile.mkdtemp()) / "port.txt"
    started = time.perf_counter()
    proc = subprocess.Popen([str(exe), "--no-browser", "--port-file", str(port_file)])
    while not (port_file.exists() and port_file.read_text().strip()):
        if time.perf_counter() - started > 15:
            raise RuntimeError("Launch.exe did not report a port within 15 s")
        time.sleep(0.05)
    return proc, f"http://127.0.0.1:{port_file.read_text().strip()}/"


def geometry_steps(page, out):
    page.click("#stl-sample")
    expect(page.locator("#stl-info")).to_contain_text("Triangles")
    page.click("#csv-sample")
    expect(page.locator("#csv-info")).to_contain_text("Hottest")
    log("PASS sample STL and synthetic CSV loaded, legend shown: " + page.locator("#legend-ticks").inner_text().replace("\n", " / "))
    box = page.locator("#viewer canvas").bounding_box()
    for i in range(400):  # click across the view until a node is picked
        page.mouse.click(box["x"] + box["width"] * (0.2 + 0.6 * (i % 20) / 19),
                         box["y"] + box["height"] * (0.2 + 0.6 * (i // 20) / 19))
        if page.locator("#inspect").is_visible():
            break
    expect(page.locator("#inspect")).to_contain_text("°C")
    log("PASS node inspection: " + page.locator("#inspect").inner_text().replace("\n", " | "))
    shot(page, out, "02_geometry_stl_and_temperature_nodes", "STL heat sink + coloured nodes, legend, node inspection")

    page.set_input_files("#csv-file", str(ROOT / "tests/fixtures/bad_nan_temperature.csv"))
    expect(page.locator("#status")).to_contain_text("not a finite number")
    log("PASS malformed CSV rejected: " + page.locator("#status").inner_text())
    page.set_input_files("#stl-file", str(ROOT / "tests/fixtures/bad_binary.stl"))
    expect(page.locator("#status")).to_contain_text("binary STL")
    log("PASS binary STL rejected: " + page.locator("#status").inner_text())
    shot(page, out, "03_malformed_file_error", "actionable error for a malformed file; previous data kept")


def thermal_steps(page, out):
    page.click("#tab-thermal")
    expect(page.locator("#thermal-table")).to_contain_text("0.01953")
    log("PASS thermal study N=10..80, midpoint error at N=80 = 0.01953 C (theory qh^2/8k)")
    shot(page, out, "04_thermal_fea", "FE vs exact plot, log-log convergence, verification table")
    page.fill("#p-k", "-5")
    page.click("#thermal-form button[type=submit]")
    expect(page.locator("#thermal-error")).to_contain_text("greater than zero")
    log("PASS invalid k rejected: " + page.locator("#thermal-error").inner_text())
    shot(page, out, "05_parameter_validation", "invalid input highlighted with message")
    page.click("#thermal-reset")


def performance_steps(page, out):
    page.click("#tab-performance")
    page.fill("#b-cases", "400")
    page.fill("#b-mesh", "20000")
    page.click("#bench-start")
    expect(page.locator("#bench-cancel")).to_be_enabled()
    page.wait_for_timeout(700)
    page.click("#tab-backprop")  # UI must stay responsive while C++ threads compute
    page.click("#tab-performance")
    shot(page, out, "06_benchmark_running", "progress bar and stage while the UI stays responsive")
    expect(page.locator("#bench-table")).to_contain_text("Agreement check passed", timeout=180_000)
    log("PASS benchmark: " + page.locator("#bench-table p").first.inner_text())
    shot(page, out, "07_benchmark_results", "timings, speed-up, efficiency, agreement, system info")

    page.fill("#b-cases", "2000")  # long enough to cancel mid-run, within the result-memory cap
    page.click("#bench-start")
    page.wait_for_timeout(500)
    page.click("#bench-cancel")
    expect(page.locator("#bench-stage")).to_contain_text("Cancelled", timeout=60_000)
    expect(page.locator("#bench-table")).to_contain_text("Agreement check passed")
    log("PASS cancellation: job cancelled, previous results kept intact")
    page.fill("#b-cases", "400")


def hardware_steps(page, out):
    page.click("#tab-hardware")
    expect(page.locator("#hw-devices .device")).to_have_count(3)
    for dev in ("mcu0", "fpga0", "jtag0"):
        page.locator(f"#dev-{dev} button", has_text="Connect").click()
        page.locator(f"#dev-{dev} button", has_text="Read telemetry").click()
    page.locator("#dev-jtag0 button", has_text="Simulate timeout").click()
    page.locator("#dev-jtag0 button", has_text="Read telemetry").click()
    expect(page.locator("#hw-log")).to_contain_text("jtag0 attempt 2: Telemetry frame received")
    log("PASS transient timeout on jtag0 recovered by retry")
    page.locator("#dev-fpga0 button", has_text="Simulate persistent fault").click()
    page.locator("#dev-fpga0 button", has_text="Read telemetry").click()
    expect(page.locator("#dev-fpga0 .state")).to_contain_text("error")
    page.locator("#dev-fpga0 button", has_text="Reconnect").click()
    page.locator("#dev-fpga0 button", has_text="Read telemetry").click()
    expect(page.locator("#dev-fpga0 .state")).to_contain_text("connected")
    log("PASS persistent fault -> error state -> reconnect -> telemetry recovered")
    shot(page, out, "08_hardware_mocks", "three simulated devices, telemetry, timeout and recovery log")


def main():
    exe = Path(sys.argv[1]).resolve()
    out = Path(sys.argv[2] if len(sys.argv) > 2 else ROOT / "evidence" / "walkthrough")
    out.mkdir(parents=True, exist_ok=True)
    proc, url = start_app(exe)
    errors = []
    try:
        with sync_playwright() as pw:
            browser = pw.chromium.launch(channel="msedge", headless=True)
            page = browser.new_page(viewport={"width": 1440, "height": 900})
            # The 400 replies to the deliberately malformed uploads are expected; anything else is a failure.
            page.on("console", lambda m: m.type == "error" and "status of 400" not in m.text and errors.append(m.text))
            page.on("pageerror", lambda e: errors.append(str(e)))
            page.goto(url)
            expect(page.locator("#status")).to_contain_text("Workbench ready", timeout=20_000)
            log(f"PASS UI ready at {url}")
            shot(page, out, "01_launch_ready", "backend connected, Geometry view")
            geometry_steps(page, out)
            thermal_steps(page, out)
            performance_steps(page, out)
            hardware_steps(page, out)
            page.click("#tab-backprop")
            expect(page.locator("#bp-table")).to_contain_text("✓")
            shot(page, out, "09_backprop_check", "hand-derived gradients vs central differences, loss curve")
            browser.close()
    finally:
        proc.terminate()
    log("PASS no browser console errors" if not errors else f"FAIL console errors: {errors}")
    (out / "walkthrough_log.txt").write_text("\n".join(log_lines) + "\n", encoding="utf-8")
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
