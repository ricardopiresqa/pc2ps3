param([switch]$Confirm)
$ErrorActionPreference = "Stop"
$cleanArgs = @()
if ($Confirm) { $cleanArgs += "--confirm" }
& python (Join-Path $PSScriptRoot "../tools/phase0.py") clean @cleanArgs @args
exit $LASTEXITCODE
