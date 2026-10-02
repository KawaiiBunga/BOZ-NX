$ErrorActionPreference='Stop'
$projectDir=Split-Path $PSScriptRoot -Parent
$pins=Get-Content (Join-Path $projectDir 'dependencies.json') -Raw | ConvertFrom-Json
$depsDir=Join-Path $projectDir 'deps'
New-Item -ItemType Directory -Force $depsDir | Out-Null
$nxDir=Join-Path $depsDir 'libnx32'
if(!(Test-Path "$nxDir/.git")) {
    & git clone https://github.com/aks796/libnx32.git $nxDir
    if($LASTEXITCODE -ne 0){throw 'libnx32 clone failed'}
}
if ((& git -C $nxDir rev-parse HEAD) -ne $pins.libnx32) {
    & git -C $nxDir fetch origin $pins.libnx32
    if($LASTEXITCODE -ne 0){throw 'libnx32 revision fetch failed'}
    & git -C $nxDir checkout --detach $pins.libnx32
    if($LASTEXITCODE -ne 0){throw 'libnx32 revision checkout failed'}
}
$image=$pins.toolchain
& docker run --rm --platform linux/amd64 -v "${nxDir}:/work" -w /work $image bash -lc 'bash build_libnx32.sh'
if($LASTEXITCODE -ne 0){throw 'libnx32 build failed'}
$mesaZip=Join-Path $depsDir 'mesa32.zip'
if(!(Test-Path $mesaZip)) {
    Invoke-WebRequest $pins.mesa32.url -OutFile $mesaZip
}
if((Get-FileHash -LiteralPath $mesaZip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.mesa32.sha256) {throw 'mesa32 archive checksum mismatch'}
$mesaDir=Join-Path $depsDir 'mesa32-download'
if(!(Test-Path "$mesaDir/mesa32")){Expand-Archive -LiteralPath $mesaZip -DestinationPath $mesaDir}
New-Item -ItemType Directory -Force (Join-Path $projectDir 'portlibs32') | Out-Null
Copy-Item "$mesaDir/mesa32/*" (Join-Path $projectDir 'portlibs32') -Recurse -Force
& git -C $projectDir submodule update --init runtime
if($LASTEXITCODE -ne 0){throw 'android32 runtime checkout failed'}
if ((& git -C (Join-Path $projectDir 'runtime') rev-parse HEAD) -ne $pins.android32) {
    throw 'The runtime submodule does not match dependencies.json; use the committed submodule revision.'
}
