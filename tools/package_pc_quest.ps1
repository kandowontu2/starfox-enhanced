param([Parameter(Mandatory=$true)][string]$Archive)
$ErrorActionPreference='Stop'
$packageRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$archivePath=[IO.Path]::GetFullPath($Archive)
# Never replace an earlier handoff, or reuse a directory that a game launch
# may have populated with its extracted assets, settings or saves.
if(Test-Path -LiteralPath $archivePath) {throw "Archive already exists: $archivePath"}
foreach($source in @('build/current/starfox_pc.exe','build/current/starfox_asset_builder.exe',
    'platform/quest/build/outputs/apk/debug/quest-debug.apk','tools/package/PC-QUEST-START-HERE.txt')) {
    if(!(Test-Path -LiteralPath (Join-Path $packageRoot $source))) {throw "Missing package input: $source"}
}
$stage=Join-Path $packageRoot ("tmp/handoff-"+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $stage 'Windows'),(Join-Path $stage 'Quest') -Force | Out-Null
Push-Location $packageRoot
try {
    & cmake --install build/current --prefix (Join-Path $stage 'Windows')
    if($LASTEXITCODE -ne 0) {throw "Windows installation failed ($LASTEXITCODE)"}
    Copy-Item -LiteralPath 'platform/quest/build/outputs/apk/debug/quest-debug.apk' -Destination (Join-Path $stage 'Quest/Starfox-Enhanced-Quest.apk')
    Copy-Item -LiteralPath 'tools/package/PC-QUEST-START-HERE.txt' -Destination (Join-Path $stage 'START-HERE.txt')
    # Verify before creating the requested deliverable. Failed candidates are
    # retained in their unique workspace staging directory for inspection.
    $candidate="$stage.zip"
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($stage,$candidate)
    & (Join-Path $PSScriptRoot 'check_pc_quest_package.ps1') -Archive $candidate -VerifyCurrentBuild
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($archivePath)) -Force | Out-Null
    Move-Item -LiteralPath $candidate -Destination $archivePath
    Write-Output "Development handoff: $archivePath"
    Get-FileHash -LiteralPath $archivePath -Algorithm SHA256
} finally {Pop-Location}
