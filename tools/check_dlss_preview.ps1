param([string]$Binary='build/release/starfox_pc.exe',
    [string]$OutputDirectory='tmp/dlss-preview-regression',
    [string]$Python='python',
    [ValidateRange(32,120)][int]$Frames=48,
    [ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL',
    [ValidateRange(1,4)][int]$RenderScale=1,
    [ValidateRange(0,1)][int]$EnhancedSky=1,
    [ValidateRange(0,63)][byte]$CameraResponse=0)
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$common=@{Binary=$Binary;Experience=$Experience;Stage='LEVEL1_1';Frames=$Frames;PrerollTicks=1000;
    GroundEnabled=0;Sky=$EnhancedSky;RayTracing=0;Reflections=0;RenderScale=$RenderScale;
    CameraResponse=$CameraResponse;MenuPreview=$true;GpuBackend='direct3d12'}
& (Join-Path $PSScriptRoot 'capture_lava.ps1') @common -OutputDirectory (Join-Path $output 'native')
$measure=Join-Path $PSScriptRoot 'check_dlss_sequence.py'
# Outside the menu panel/model silhouettes: all background artwork must be
# exactly stationary, not merely successfully submitted to the NVIDIA SDK.
# EX's frozen Corneria preview places a tall pillar on the left. Do not
# accidentally measure reconstructed model pixels as native sky artwork.
$skyBottom=if($Experience -eq 'EX'){'0.18'}else{'0.35'}
$region=@('0','0.03','0.20',$skyBottom)
function Read-Metric([string]$directory) {
    $json=& $Python $measure (Join-Path $directory 'lava.bmp') --first 17 --last $Frames --step 4 --region @region
    if($LASTEXITCODE -ne 0){throw 'Frame sequence analysis failed'}
    ($json | ConvertFrom-Json)[0]
}
$native=Read-Metric (Join-Path $output 'native')
if($native.max_change_0_255 -ne 0){throw 'Reference preview is moving; static-jitter test is invalid'}
$results=@()
foreach($model in 'standard','4.5') {foreach($mode in 1,2,3,4) {
    $directory=Join-Path $output "$model-mode-$mode"
    $selection=if($model -eq 'standard'){@{Dlss=$mode}}else{@{Dlss45=$mode}}
    & (Join-Path $PSScriptRoot 'capture_lava.ps1') @common @selection -OutputDirectory $directory -CheckDlssStationary
    $log=Get-Content -LiteralPath (Join-Path $directory 'runtime.log') -Raw
    if(([regex]::Matches($log,'dlss-gameplay: evaluated frame=')).Count -lt 32 -or
        ($Frames -gt 32 -and $log -notmatch 'dlss-preview: reused reconstructed frame=') -or
        $log -match 'dlss-gameplay: failed|dlss-fallback: unjittered scene replay' -or
        $log -notmatch 'dlss-model-audit: covered=[1-9][0-9]* valid_motion=[1-9][0-9]*') {
        throw "DLSS $model mode $mode declined reconstruction or failed stationary correspondence"
    }
    $result=Read-Metric $directory
    if($result.max_change_0_255 -ne 0){throw "DLSS $model mode $mode still moves stationary background artwork"}
    $reference=Join-Path $output 'native/lava.bmp.frame-17.bmp'
    $comparison=Join-Path $directory 'lava.bmp.frame-17.bmp'
    # Check sky and the native shield HUD, using the sequence checker's BMP
    # decoder. There are no optional image-library dependencies.
    & $Python -c 'import sys; from pathlib import Path; sys.path.insert(0,sys.argv[1]); from check_dlss_sequence import bmp; a,b=map(bmp,map(Path,sys.argv[2:4])); h,w=len(a),len(a[0]); assert (len(b),len(b[0]))==(h,w); regions=[(0,.03,.20,float(sys.argv[4])),(.02,.83,.19,.93)]; assert all(a[y][x]==b[y][x] for l,t,r,d in regions for y in range(int(t*h),int(d*h)) for x in range(int(l*w),int(r*w))), "Native sky/HUD artwork is blurred"' $PSScriptRoot $reference $comparison $skyBottom
    if($LASTEXITCODE -ne 0){throw "DLSS $model mode $mode softened native sky/HUD artwork"}
    $results+=$result
    Write-Output "DLSS $model mode ${mode}: stationary native sky and shield HUD exact; model/terrain correspondence valid."
}}
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'stability.json')
Write-Output 'DLSS preview regression passed; this is not a blanket image-quality or performance certification.'
