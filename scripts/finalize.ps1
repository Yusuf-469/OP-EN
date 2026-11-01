param([string]$CandidateId = "RA2311026010469")
# One command after any change: commit first, then this repackages the ZIP, records its SHA-256 and size in
# docs/report/submission.json (untracked) and rebuilds the < 1 MB submission PDF in dist/.
$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $PSScriptRoot)
& powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package_windows.ps1 -CandidateId $CandidateId
if ($LASTEXITCODE -ne 0) { throw "Packaging failed." }
$zip = "dist/${CandidateId}_Workbench.zip"
$cfgPath = "docs/report/submission.json"
$cfg = if (Test-Path $cfgPath) { Get-Content $cfgPath -Raw | ConvertFrom-Json } else { [pscustomobject]@{} }
$cfg | Add-Member -Force candidate_id $CandidateId
$cfg | Add-Member -Force zip_sha256 (Get-FileHash $zip -Algorithm SHA256).Hash
$cfg | Add-Member -Force zip_size ("({0:N1} MB)" -f ((Get-Item $zip).Length / 1MB))
[IO.File]::WriteAllText((Join-Path $PWD $cfgPath), ($cfg | ConvertTo-Json))
python scripts/build_submission_pdf.py
if ($LASTEXITCODE -ne 0) { throw "PDF build failed." }
