param(
    [string]$GameImage,
    [string]$Apk,
    [string]$Etc,
    [string]$Index,
    [string]$Snapshot
)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$pins = Get-Content (Join-Path $projectDir 'dependencies.json') -Raw | ConvertFrom-Json
Push-Location $projectDir
try {
    & docker run --rm --platform linux/amd64 -v "${projectDir}:/work" -w /work $pins.toolchain bash tests/build_bridge.sh
    if ($LASTEXITCODE -ne 0) { throw 'Host C/assembly checks failed' }
    & python tests/test_split_snapshot.py
    if ($LASTEXITCODE -ne 0) { throw 'Actor snapshot format checks failed' }
    & python tests/test_bridge.py
    if ($LASTEXITCODE -ne 0) { throw 'Bridge ABI checks failed; install tests/requirements.txt' }
    if ($GameImage) {
        $imagePath = (Resolve-Path -LiteralPath $GameImage).Path
        & docker run --rm --platform linux/amd64 -v "${projectDir}:/work" -v "${imagePath}:/inputs/game.s3e:ro" -w /work $pins.toolchain test-artifacts/load_image /inputs/game.s3e test-artifacts/game-image.bin test-artifacts/game-imports.txt
        if ($LASTEXITCODE -ne 0) { throw 'Game fixture load failed' }
        & python tests/test_aim_game.py
        if ($LASTEXITCODE -ne 0) { throw 'Game input checks failed' }
        & python tests/test_camera_game.py
        if ($LASTEXITCODE -ne 0) { throw 'Game camera checks failed' }
        & python tests/test_split_game.py
        if ($LASTEXITCODE -ne 0) { throw 'Split-screen anchor/hook checks failed' }
        & python tests/test_split_spawn_game.py
        if ($LASTEXITCODE -ne 0) { throw 'Split-screen spawn/name ownership checks failed' }
    }
    if ($Snapshot) {
        if (!$GameImage) { throw 'Actor sandbox checks need both -GameImage and -Snapshot' }
        $snapshotPath = (Resolve-Path -LiteralPath $Snapshot).Path
        & python tests/test_split_actor_snapshot.py $snapshotPath
        if ($LASTEXITCODE -ne 0) { throw 'Captured actor clone checks failed' }
    }
    if ($Apk -or $Etc -or $Index) {
        if (!$Apk -or !$Etc) { throw 'APK setup checks need both -Apk and -Etc' }
        $apkPath = (Resolve-Path -LiteralPath $Apk).Path
        $etcPath = (Resolve-Path -LiteralPath $Etc).Path
        $dockerArgs = @('run','--rm','--user','0','--platform','linux/amd64','-v',"${projectDir}:/work",'-v',"${apkPath}:/inputs/boz.apk:ro",'-v',"${etcPath}:/inputs/blackops_etc.dz:ro",'-w','/work')
        if ($Index) {
            $indexPath = (Resolve-Path -LiteralPath $Index).Path
            $dockerArgs += @('-v',"${indexPath}:/inputs/boz_files.idx:ro")
        }
        $dockerArgs += @($pins.toolchain,'bash','tests/build_setup.sh')
        & docker @dockerArgs
        if ($LASTEXITCODE -ne 0) { throw 'APK setup checks failed' }
    }
} finally { Pop-Location }
