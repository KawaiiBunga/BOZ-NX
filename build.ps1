param([switch]$Launcher, [string[]]$MakeArgs = @(), [string]$DevkitPro = $env:DEVKITPRO)
$ErrorActionPreference = 'Stop'
$projectDir = $PSScriptRoot
$nxPrefix = Join-Path $projectDir 'deps/libnx32/prefix'
if (!(Test-Path "$nxPrefix/lib/libnx.a")) { throw 'Run tools/setup.ps1 first.' }
$pins = Get-Content (Join-Path $projectDir 'dependencies.json') -Raw | ConvertFrom-Json
$toolchain = $pins.toolchain
$dockerArgs = @('run','--rm','--platform','linux/amd64','-v',"${projectDir}:/work",'-w','/work')
foreach ($part in @('include/switch','include/switch.h','lib/libnx.a','lib/libnxd.a')) {
    $dockerArgs += @('-v',"${nxPrefix}/${part}:/opt/devkitpro/libnx32/${part}:ro")
}
$dockerArgs += @($toolchain,'bash','-lc','exec make -j4 "$@"','make') + $MakeArgs
& docker @dockerArgs
if ($LASTEXITCODE -ne 0) { throw 'Native payload build failed.' }
if ($Launcher) {
    if (!$DevkitPro -or (!$PSBoundParameters.ContainsKey('DevkitPro') -and !(Test-Path -LiteralPath $DevkitPro))) {
        $DevkitPro = 'C:/devkitPro'
    }
    $bash = Join-Path $DevkitPro 'msys2/usr/bin/bash.exe'
    if (!(Test-Path $bash)) { throw 'Set DEVKITPRO or pass -DevkitPro with your devkitPro installation path.' }
    $env:DEVKITPRO = $DevkitPro.Replace('\','/')
    # Both runtime payload-copy rules remove the same stale NRO. Serializing
    # this small launcher build avoids a Windows file-removal race.
    # The shared GNU Make rules use absolute paths as word lists. A temporary
    # drive alias avoids spaces without copying the checkout or its outputs.
    $launcherRoot = $projectDir.Replace('\','/')
    $mappedDrive = $null
    $launcherBuildPath = [System.IO.Path]::GetFullPath((Join-Path $projectDir 'launcher/build'))
    $rootMarker = Join-Path $launcherBuildPath '.host-root'
    try {
        if ($projectDir -match '\s') {
            $usedDrives = [System.IO.Directory]::GetLogicalDrives()
            $freeDrive = 90..68 | ForEach-Object { [string][char]$_ + ':' } |
                Where-Object { ($usedDrives -notcontains ($_ + '\')) -and !(Test-Path -LiteralPath ($_ + '\')) } |
                Select-Object -First 1
            if (!$freeDrive) { throw 'Launcher build needs a free drive letter for a temporary path alias.' }
            & subst.exe $freeDrive $projectDir
            if ($LASTEXITCODE -ne 0) { throw 'Could not create the temporary launcher path alias.' }
            $mappedDrive = $freeDrive
            $launcherRoot = $freeDrive
        }
        # Copied/moved objects can retain dependencies on old absolute paths.
        # A different free alias or SDK location also requires fresh objects.
        $expectedRoot = "$projectDir`n$launcherRoot`n$env:DEVKITPRO"
        if ((Test-Path -LiteralPath $launcherBuildPath) -and
            (!(Test-Path -LiteralPath $rootMarker) -or (Get-Content -LiteralPath $rootMarker -Raw) -ne $expectedRoot)) {
            $workspacePrefix = [System.IO.Path]::GetFullPath($projectDir).TrimEnd('\') + '\'
            if (!$launcherBuildPath.StartsWith($workspacePrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
                ((Get-Item -LiteralPath $launcherBuildPath).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
                throw 'Generated launcher build directory must be inside the checkout and must not be a directory link.'
            }
            Remove-Item -LiteralPath $launcherBuildPath -Recurse -Force
        }
        & $bash -lc 'cd "$1/launcher" && exec make -j1' bash $launcherRoot
        if ($LASTEXITCODE -ne 0) { throw 'Launcher build failed.' }
        [System.IO.File]::WriteAllText($rootMarker, $expectedRoot)
    } finally {
        if ($mappedDrive) { & subst.exe $mappedDrive /D }
    }
}
