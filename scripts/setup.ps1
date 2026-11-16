$ErrorActionPreference = "Stop"
& python (Join-Path $PSScriptRoot "../tools/phase0.py") setup @args
exit $LASTEXITCODE
