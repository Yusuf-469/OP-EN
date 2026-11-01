param([string]$Cli = "build/uew_cli.exe", [string]$Dir = "results/size_test")
# Parses generated files of increasing size and records parse time (docs/file_limits.md).
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $Dir | Out-Null
$log = @()
foreach ($n in 1000, 100000, 500000) {
  & $Cli make-large-stl "$Dir/t$n.stl" $n | Out-Null
  $log += "STL $n triangles: " + (& $Cli parse-stl "$Dir/t$n.stl")
}
foreach ($n in 10000, 1000000, 2000000) {
  & $Cli make-large-csv "$Dir/n$n.csv" $n | Out-Null
  $log += "CSV $n nodes: " + (& $Cli parse-csv "$Dir/n$n.csv")
}
& $Cli make-large-csv "$Dir/over.csv" 2000001 | Out-Null
$log += "CSV 2000001 nodes (over limit, must be rejected): " + (cmd /c "`"$Cli`" parse-csv $Dir/over.csv 2>&1")
$log | Set-Content results/file_size_limits.txt -Encoding ascii
$log
Remove-Item "$Dir" -Recurse
