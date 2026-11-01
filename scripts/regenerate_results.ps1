param([string]$Cli = "build/uew_cli.exe")
# Regenerates every file in results/ from the C++ CLI (no hand-edited numbers).
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force results/thermal, results/backprop, results/performance | Out-Null
& $Cli thermal-study results/thermal
& $Cli backprop results/backprop
& $Cli benchmark results/performance 400 20000 20261101
& $Cli sysinfo | Set-Content results/performance/system_info.json -Encoding ascii
python scripts/make_figures.py
