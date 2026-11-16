$ErrorActionPreference = "Stop"
& python (Join-Path $PSScriptRoot "../tools/phase0.py") build-ps3 @args
exit $LASTEXITCODE
