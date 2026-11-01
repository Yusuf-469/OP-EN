"""Integration test: starts Launch.exe headless and exercises every API route
with valid and invalid input. Standard library only.

Usage: python tests/api_smoke_test.py <path-to-Launch.exe>
"""
import json
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tests" / "fixtures"
failures = []


def call(base, method, path, body=None, headers=None):
    # Raw file bodies must not be labelled form-urlencoded (httplib caps those at 8 KB).
    headers = {"Content-Type": "application/octet-stream", **(headers or {})}
    req = urllib.request.Request(base + path, data=body, method=method, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            return resp.status, resp.read()
    except urllib.error.HTTPError as err:
        return err.code, err.read()


def check(name, condition, detail=""):
    print(("PASS " if condition else "FAIL ") + name + (f"  ({detail})" if detail and not condition else ""))
    if not condition:
        failures.append(name)


def run(base):
    status, body = call(base, "GET", "/")
    check("serves index.html", status == 200 and b"Unified Engineering Workbench" in body)

    status, body = call(base, "GET", "/api/thermal/study?N=10,20,40,80")
    meshes = json.loads(body)["meshes"]
    check("thermal study", status == 200 and abs(meshes[0]["max_midpoint_error_C"] - 1.25) < 1e-9)
    status, body = call(base, "GET", "/api/thermal/study?k=-1")
    check("thermal rejects k<=0 with 400", status == 400 and b"conductivity" in body, body)
    status, body = call(base, "GET", "/api/thermal/study?L=abc")
    check("thermal rejects text", status == 400 and b"finite number" in body, body)
    status, body = call(base, "GET", "/api/thermal/export?file=convergence")
    check("thermal CSV export", status == 200 and body.startswith(b"N,h_m,"))

    stl = (ROOT / "data" / "heat_sink.stl").read_bytes()
    status, body = call(base, "POST", "/api/geometry/stl", stl)
    check("sample STL parses", status == 200 and json.loads(body)["triangles"] > 0)
    csv = (ROOT / "data" / "heat_sink_temperature_synthetic.csv").read_bytes()
    status, body = call(base, "POST", "/api/results/csv", csv)
    check("sample CSV parses", status == 200 and json.loads(body)["count"] > 0)
    for fixture in sorted(FIXTURES.glob("bad_*")):
        route = "/api/geometry/stl" if fixture.suffix == ".stl" else "/api/results/csv"
        status, body = call(base, "POST", route, fixture.read_bytes())
        check(f"rejects {fixture.name}", status == 400 and b"error" in body, body)

    status, body = call(base, "GET", "/api/backprop")
    data = json.loads(body)
    check("backprop gradient check", all(abs(data["analytic"][p] - data["finite_difference"][p]) <= 1e-5 for p in "wbvc"))

    status, body = call(base, "POST", "/api/hardware/jtag0/telemetry")
    check("telemetry before connect is 409", status == 409, body)
    call(base, "POST", "/api/hardware/jtag0/connect")
    call(base, "POST", "/api/hardware/jtag0/inject-timeout")
    status, body = call(base, "POST", "/api/hardware/jtag0/telemetry")
    data = json.loads(body)
    check("timeout recovered by retry", data["telemetry_ok"] and data["attempts"][0]["timed_out"])

    status, body = call(base, "POST", "/api/benchmark?cases=40&mesh=2000&workers=2")
    job = json.loads(body)["job_id"]
    status, body = call(base, "POST", "/api/benchmark?cases=40&mesh=2000")
    check("second concurrent job refused with 409", status == 409, body)
    for _ in range(300):
        state = json.loads(call(base, "GET", f"/api/jobs/{job}")[1])
        if state["state"] != "running":
            break
        time.sleep(0.1)
    check("benchmark completes with agreement", state["state"] == "done" and state["result"]["benchmark"]["agreement_ok"])
    status, body = call(base, "GET", f"/api/jobs/{job}/files/benchmark_summary.csv")
    check("benchmark CSV export", status == 200 and body.startswith(b"variant,workers"))

    status, body = call(base, "POST", "/api/benchmark?cases=1200&mesh=20000&workers=4")
    job = json.loads(body)["job_id"]
    time.sleep(0.3)
    call(base, "POST", f"/api/jobs/{job}/cancel")
    for _ in range(300):
        state = json.loads(call(base, "GET", f"/api/jobs/{job}")[1])
        if state["state"] != "running":
            break
        time.sleep(0.1)
    check("cancel stops job without results", state["state"] == "cancelled" and "result" not in state, state["state"])

    status, body = call(base, "GET", "/api/health", headers={"Origin": "http://evil.example"})
    check("cross-origin request refused", status == 403)
    status, body = call(base, "GET", "/api/system", headers={"Host": "rebind.example:80"})
    check("DNS-rebinding Host header refused", status == 403)
    status, body = call(base, "GET", "/api/system")
    check("system info", status == 200 and json.loads(body)["system"]["logical_cpus"] >= 1)


def main():
    exe = Path(sys.argv[1]).resolve()
    port_file = Path(tempfile.mkdtemp()) / "port.txt"
    proc = subprocess.Popen([str(exe), "--no-browser", "--port-file", str(port_file)])
    try:
        for _ in range(100):
            if port_file.exists() and port_file.read_text().strip():
                break
            time.sleep(0.1)
        base = f"http://127.0.0.1:{port_file.read_text().strip()}"
        run(base)
    finally:
        proc.terminate()
    print(f"{'FAILED' if failures else 'ALL PASSED'}: {len(failures)} failure(s)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
