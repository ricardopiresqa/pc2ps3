$ErrorActionPreference = "Stop"
& python (Join-Path $PSScriptRoot "../tools/phase0.py") package-ps3 @args
exit $LASTEXITCODE
