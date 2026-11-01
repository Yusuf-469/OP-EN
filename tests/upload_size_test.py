"""Times the full UI upload path (HTTP + C++ parse + JSON) for large files.

Usage: python tests/upload_size_test.py <Launch.exe> <uew_cli.exe>
Appends results to results/file_size_limits.txt.
"""
import subprocess
import sys
import tempfile
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CASES = [("make-large-stl", 100_000, "stl", "geometry/stl"), ("make-large-stl", 500_000, "stl", "geometry/stl"),
         ("make-large-csv", 1_000_000, "csv", "results/csv"), ("make-large-csv", 2_000_000, "csv", "results/csv")]


def main():
    launch, cli = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    tmp = Path(tempfile.mkdtemp())
    proc = subprocess.Popen([str(launch), "--no-browser", "--port-file", str(tmp / "port")])
    lines = []
    try:
        while not ((tmp / "port").exists() and (tmp / "port").read_text()):
            time.sleep(0.05)
        port = (tmp / "port").read_text()
        for command, count, ext, route in CASES:
            path = tmp / f"f{count}.{ext}"
            subprocess.run([str(cli), command, str(path), str(count)], check=True, capture_output=True)
            body = path.read_bytes()
            start = time.perf_counter()
            req = urllib.request.Request(f"http://127.0.0.1:{port}/api/{route}", data=body, method="POST",
                                         headers={"Content-Type": "application/octet-stream"})
            reply = urllib.request.urlopen(req, timeout=300).read()
            lines.append(f"API upload {ext.upper()} {count} items: {len(body) / 1e6:.1f} MB in, "
                         f"{len(reply) / 1e6:.1f} MB JSON out, {time.perf_counter() - start:.2f} s round trip")
            print(lines[-1])
            path.unlink()
    finally:
        proc.terminate()
    with open(ROOT / "results" / "file_size_limits.txt", "a", encoding="ascii") as out:
        out.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
