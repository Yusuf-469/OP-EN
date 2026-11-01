param(
  [string]$CandidateId = "CandidateID",
  [string]$OutDir = "dist"
)
# Builds, tests and packages <CandidateId>_Workbench.zip. Packaging only:
# no backend feature lives here. Requires CMake, Ninja, MinGW-w64 GCC and git.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# Native tools do not raise PowerShell errors, so check every exit code.
function Invoke-Checked([string]$what, [scriptblock]$command) {
  & $command
  if ($LASTEXITCODE -ne 0) { throw "$what failed (exit code $LASTEXITCODE)." }
}

if (git status --porcelain) { throw "Commit or stash changes first: the package is built from git HEAD." }

Invoke-Checked "Configure" { cmake --preset release }
Invoke-Checked "Build" { cmake --build --preset release }
Invoke-Checked "Tests" { ctest --preset release }

$name = "${CandidateId}_Workbench"
$stage = Join-Path $OutDir $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

# Source tree exactly as committed (relative paths, no build output, no local files).
# Written to a file: PowerShell 5.1 pipes native output as text and would corrupt a tar stream.
$tarball = Join-Path $OutDir "source.tar"
Invoke-Checked "git archive" { git archive --format=tar --output=$tarball HEAD }
Invoke-Checked "Extract source" { tar -x -f $tarball -C $stage }
Remove-Item $tarball

# Packaged application: Launch.exe at the root, UI and CLI under app/.
Copy-Item build/Launch.exe $stage
New-Item -ItemType Directory -Force "$stage/app" | Out-Null
Copy-Item src/web "$stage/app/web" -Recurse
Copy-Item build/uew_cli.exe "$stage/app"

# Version-control evidence.
New-Item -ItemType Directory -Force "$stage/evidence/git" | Out-Null
Invoke-Checked "git bundle" { git bundle create "$stage/evidence/git/repository.bundle" --all }
git log --date=iso --stat --pretty=format:"commit %h%nDate:   %ad%n%n    %s%n%n%w(0,4,4)%b" |
  Set-Content "$stage/evidence/git/commit_log.txt" -Encoding utf8

# Proof that Launch.exe needs only DLLs that ship with Windows 10/11.
objdump -p build/Launch.exe | Select-String "DLL Name" |
  Set-Content "$stage/evidence/launch_exe_dll_dependencies.txt" -Encoding ascii

$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip }
Invoke-Checked "Zip" { tar -a -c -f $zip -C $OutDir $name }
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash
"$hash  $name.zip" | Set-Content (Join-Path $OutDir "$name.zip.sha256") -Encoding ascii
"Packaged $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 2)) MB), SHA-256 $hash"
