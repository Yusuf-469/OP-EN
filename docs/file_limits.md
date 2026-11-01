# Tested file-size limits

Limits are enforced in `src/core/geometry_io.h` (`kMaxInputBytes`, `kMaxTriangles`, `kMaxNodes`); larger input
is rejected with a message, not truncated. Measured on the development PC (i7-13650HX, 23.8 GiB, Windows 11),
raw output in `results/file_size_limits.txt`.

| File | Size | C++ parse only | Full UI path (upload + parse + JSON) |
|---|---|---|---|
| STL 100,000 triangles | 23.7 MB | 0.06 s | 0.18 s |
| STL 500,000 triangles | 118.5 MB | 0.26 s | 2.2 s |
| CSV 1,000,000 nodes | 43.1 MB | 0.39 s | 2.6 s |
| CSV 2,000,000 nodes (limit) | 87.3 MB | 0.82 s | 5.4 s |
| CSV 2,000,001 nodes | - | rejected: "more than 2000000 nodes (tested limit)" | - |

Hard limits: 128 MB per file, 2,000,000 triangles, 2,000,000 nodes. Recommended for smooth interaction in the
3D view: up to ~500k triangles and ~1M nodes (browser WebGL memory, not the C++ backend, is the next bottleneck).

Reproduce: `powershell -File scripts/file_size_test.ps1` then `python tests/upload_size_test.py build/Launch.exe build/uew_cli.exe`.

Empty files are explained ("file is empty (0 bytes)"; "valid header but no data rows"; "solid contains no
facets") rather than shown as an empty view.
