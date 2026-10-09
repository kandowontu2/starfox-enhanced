param(
    [string]$Executable='build/vr-dev/starfox_vr_scene_check.exe',
    [string]$Outdir='tmp/vr-water-height-regression'
)
$ErrorActionPreference='Stop'
foreach($variant in @('ORIGINAL','EX')) {
    $rom=if($variant -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC'} else {'upstream-ultrastarfox/SF.SFC'}
    $symbols=if($variant -eq 'EX') {'assets/symbols/starfox-ex.txt'} else {'upstream-ultrastarfox/SYMBOLS.TXT'}
    foreach($representation in @('native','scaled')) {
        $destination=Join-Path $Outdir "$variant-$representation"
        $stage='--live-stage=LEVEL2_3:water'
        if($representation -eq 'scaled') {$stage+='@height-equivalent'}
        $output=& $Executable $destination $rom $symbols $stage 2>&1
        if($LASTEXITCODE -ne 0) {throw "Water capture failed: $variant/$representation`n$output"}
    }
    foreach($eye in @('left','right')) {
        $native=Join-Path $Outdir "$variant-native/live-scene-$eye.bmp"
        $scaled=Join-Path $Outdir "$variant-scaled/live-scene-$eye.bmp"
        if((Get-FileHash -LiteralPath $native).Hash -ne (Get-FileHash -LiteralPath $scaled).Hash) {
            throw "Water world-height texture projection differs: $variant/$eye"
        }
        Write-Output "PASS $variant/${eye}: native and model-scaled water planes render identically"
    }
}
Write-Output 'GPU capture comparison only; no headset acceptance implied.'
