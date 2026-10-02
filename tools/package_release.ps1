param([string]$Output = (Join-Path $PSScriptRoot '../release'))
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$nro = Join-Path $projectDir 'launcher/codboz.nro'
if (!(Test-Path -LiteralPath $nro)) { throw 'Build first: ./build.ps1 -Launcher' }
New-Item -ItemType Directory -Force $Output | Out-Null
# Explicit file list prevents local game assets from entering a release.
Copy-Item -LiteralPath $nro -Destination (Join-Path $Output 'codboz.nro') -Force
Copy-Item -LiteralPath (Join-Path $projectDir 'README.md') -Destination (Join-Path $Output 'README.md') -Force
Get-FileHash -LiteralPath (Join-Path $Output 'codboz.nro') -Algorithm SHA256
