param(
    [Parameter(Mandatory=$true)][string]$Apk,
    [Parameter(Mandatory=$true)][string]$Data,
    [string]$Output = (Join-Path $PSScriptRoot '../dist/sdmc')
)
$ErrorActionPreference='Stop'
& python (Join-Path $PSScriptRoot 'prepare_sd.py') --apk $Apk --data $Data --output $Output
if($LASTEXITCODE -ne 0){throw 'Preparing the SD folder failed.'}
