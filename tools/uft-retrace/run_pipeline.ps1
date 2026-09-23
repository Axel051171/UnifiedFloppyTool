param(
    [Parameter(Mandatory=$true)][string]$InputPath,
    [Parameter(Mandatory=$true)][string]$WorkDirectory
)

$ErrorActionPreference = "Stop"
py -3 -m uft_retrace inventory $InputPath -o $WorkDirectory
py -3 -m uft_retrace analyze $WorkDirectory
py -3 -m uft_retrace report $WorkDirectory -o (Join-Path $WorkDirectory "REPORT.md")
Write-Host "Fertig:" (Join-Path $WorkDirectory "REPORT.md")
