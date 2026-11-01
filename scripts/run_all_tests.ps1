param([string]$Build = "build")
# Runs every automated check and saves the outputs as evidence (evidence/test_results/).
$ErrorActionPreference = "Continue"
$out = "evidence/test_results"
New-Item -ItemType Directory -Force $out | Out-Null
ctest --test-dir $Build --output-on-failure 2>&1 | Tee-Object "$out/ctest.txt" | Select-Object -Last 3
python tests/api_smoke_test.py "$Build/Launch.exe" 2>&1 | Tee-Object "$out/api_smoke_test.txt" | Select-Object -Last 1
python tests/e2e_walkthrough.py "$Build/Launch.exe" 2>&1 | Tee-Object "$out/e2e_walkthrough.txt" | Select-Object -Last 1
python tests/contrast_check.py 2>&1 | Tee-Object "$out/contrast_check.txt" | Select-Object -Last 1
