$ErrorActionPreference = "Stop"
& python (Join-Path $PSScriptRoot "../tools/phase0.py") test @args
exit $LASTEXITCODE
