param([string]$Output)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$version = (Get-Content -LiteralPath (Join-Path $projectDir 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid release VERSION' }
if (!$Output) { $Output = Join-Path $projectDir "release/$version" }
$nro = Join-Path $projectDir 'launcher/codboz.nro'
if (!(Test-Path -LiteralPath $nro)) { throw 'Build first: ./build.ps1 -Launcher' }
$bytes = [System.IO.File]::ReadAllBytes($nro)
if ($bytes.Length -lt 0x80 -or [System.Text.Encoding]::ASCII.GetString($bytes, 0x10, 4) -ne 'NRO0') {
    throw 'Launcher is not a valid NRO'
}
$assetStart = [long][System.BitConverter]::ToUInt32($bytes, 0x18)
if ($assetStart -gt $bytes.Length - 56 -or [System.Text.Encoding]::ASCII.GetString($bytes, $assetStart, 4) -ne 'ASET') {
    throw 'Launcher is missing its asset metadata'
}
$nacpOffset = [System.BitConverter]::ToUInt64($bytes, $assetStart + 24)
$nacpSize = [System.BitConverter]::ToUInt64($bytes, $assetStart + 32)
if ($nacpOffset -gt $bytes.Length - $assetStart -or $nacpSize -lt 0x3070 -or
    $nacpSize -gt $bytes.Length - $assetStart - $nacpOffset) {
    throw 'Launcher metadata is incomplete'
}
$embeddedVersion = [System.Text.Encoding]::UTF8.GetString($bytes, $assetStart + $nacpOffset + 0x3060, 16).TrimEnd([char]0)
if ($embeddedVersion -ne $version) {
    throw "Launcher version $embeddedVersion does not match VERSION $version. Rebuild with ./build.ps1 -Launcher"
}
New-Item -ItemType Directory -Force $Output | Out-Null
# Explicit file list prevents local game assets from entering a release.
Copy-Item -LiteralPath $nro -Destination (Join-Path $Output 'codboz.nro') -Force
Copy-Item -LiteralPath (Join-Path $projectDir 'README.md') -Destination (Join-Path $Output 'README.md') -Force
$packagedNro = Join-Path $Output 'codboz.nro'
$nroHash = (Get-FileHash -LiteralPath $packagedNro -Algorithm SHA256).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText((Join-Path $Output 'SHA256SUMS.txt'), "$nroHash  codboz.nro`n", [System.Text.UTF8Encoding]::new($false))
$archive = Join-Path $Output "BOZ-NX-$version.zip"
$files = @($packagedNro, (Join-Path $Output 'README.md'), (Join-Path $Output 'SHA256SUMS.txt'))
Compress-Archive -LiteralPath $files -DestinationPath $archive -Force
Get-FileHash -LiteralPath $packagedNro, $archive -Algorithm SHA256
