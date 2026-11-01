param(
  [string]$CandidateId = "CandidateID",
  [string]$OutDir = "dist"
)
# Builds, tests and packages <CandidateId>_Workbench.zip. Packaging only:
# no backend feature lives here. Requires CMake, Ninja, MinGW-w64 GCC and git.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (git status --porcelain) { throw "Commit or stash changes first: the package is built from git HEAD." }

cmake --preset release
cmake --build --preset release
ctest --preset release
if ($LASTEXITCODE -ne 0) { throw "Tests failed; not packaging." }

$name = "${CandidateId}_Workbench"
$stage = Join-Path $OutDir $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

# Source tree exactly as committed (relative paths, no build output, no local files).
git archive --format=tar HEAD | tar -x -C $stage

# Packaged application: Launch.exe at the root, UI and CLI under app/.
Copy-Item build/Launch.exe $stage
New-Item -ItemType Directory -Force "$stage/app" | Out-Null
Copy-Item src/web "$stage/app/web" -Recurse
Copy-Item build/uew_cli.exe "$stage/app"

# Version-control evidence.
New-Item -ItemType Directory -Force "$stage/evidence/git" | Out-Null
git bundle create "$stage/evidence/git/repository.bundle" --all
git log --date=iso --stat --pretty=format:"commit %h%nDate:   %ad%n%n    %s%n%n%w(0,4,4)%b" |
  Set-Content "$stage/evidence/git/commit_log.txt" -Encoding utf8

# Proof that Launch.exe needs only DLLs that ship with Windows 10/11.
objdump -p build/Launch.exe | Select-String "DLL Name" |
  Set-Content "$stage/evidence/launch_exe_dll_dependencies.txt" -Encoding ascii

$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip }
tar -a -c -f $zip -C $OutDir $name
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash
"$hash  $name.zip" | Set-Content (Join-Path $OutDir "$name.zip.sha256") -Encoding ascii
"Packaged $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 2)) MB), SHA-256 $hash"
