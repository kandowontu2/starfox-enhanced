param(
    [string]$OutputDirectory='tmp/vr-orbital-scope-check',
    [string]$Executable='build/vr-dev/starfox_vr_scene_check.exe',
    [string]$OriginalRom='upstream-ultrastarfox/SF.SFC',
    [string]$OriginalSymbols='assets/symbols/ultrastarfox.txt',
    [string]$ExRom='tmp/runtime-inputs/starfox-ex/SFES.SFC',
    [string]$ExSymbols='assets/symbols/starfox-ex.txt'
)
$ErrorActionPreference='Stop'
foreach($path in $Executable,$OriginalRom,$OriginalSymbols,$ExRom,$ExSymbols) {
    if(!(Test-Path -LiteralPath $path -PathType Leaf)) {throw "Missing input: $path"}
}
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $root) {throw 'Choose a new output directory to preserve previous evidence'}
New-Item -ItemType Directory -Path $root | Out-Null
$cases=@(
    @('original-sector-x',$OriginalRom,$OriginalSymbols,'LEVEL2_2:400@vr-world@background-only@front'),
    @('ex-entry',$ExRom,$ExSymbols,'LEVEL5_1:60@vr-world@background-only@from-entry@front'),
    @('ex-boss',$ExRom,$ExSymbols,'LEVEL5_1:2500@vr-world@background-only@from-entry@front'),
    @('ex-menu-21',$ExRom,$ExSymbols,'TITLEMAP:menu-choice-21@vr-world@background-only@front'),
    @('ex-menu-25',$ExRom,$ExSymbols,'TITLEMAP:menu-choice-25@vr-world@background-only@front'),
    @('ex-menu-35',$ExRom,$ExSymbols,'TITLEMAP:menu-choice-35@vr-world@background-only@front')
)
foreach($case in $cases) {
    $directory=Join-Path $root $case[0]
    & $Executable $directory $case[1] $case[2] "--live-stage=$($case[3])" |
        Select-String 'Stage capture|Orbital|passed; no headset' | ForEach-Object {Write-Host $_.Line}
    if($LASTEXITCODE -ne 0) {throw "Orbital fixture failed: $($case[0])"}
    foreach($eye in 'left','right') {
        $file=Join-Path $directory "live-scene-$eye.bmp"
        if(!(Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).Length -lt 54) {
            throw "Missing eye capture: $file"
        }
    }
}
Write-Host 'Six orbital fixtures passed scope assertions; inspect eye images. No headset acceptance implied.'
