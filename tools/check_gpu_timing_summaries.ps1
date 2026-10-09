param([Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a fresh fixture directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$assertions=0
function Check($ok,$message){if(!$ok){throw $message};$script:assertions++}
function Fixture($name,$lines){
 $path=Join-Path $output "$name.log"
 [IO.File]::WriteAllLines($path,[string[]]$lines)
 return $path
}
function Summarize($name,$path,$draws=$false){
 $tool=if($draws){'summarize_scene_draws.ps1'}else{'summarize_gpu_timestamps.ps1'}
 $json=Join-Path $output "$name.json"
 & (Join-Path $PSScriptRoot $tool) -Log $path -Output $json -WarmupFrames 1 | Out-Null
 return Get-Content -LiteralPath $json -Raw | ConvertFrom-Json
}
function Reject($name,$lines,$draws=$false){
 $path=Fixture $name $lines
 $rejected=$false
 try{Summarize $name $path $draws | Out-Null}catch{$rejected=$true}
 Check $rejected "Invalid fixture was accepted: $name"
}
$header='samples=4 dropped=0 cancelled=0 errors=0 period-ns=1 valid-bits=64 boundary=bottom-bottom backend=direct3d12'
$log=@('stereo-presentation-count: 4',"scene-gpu-timestamps: $header")
# Fence slots can retire out of order. Contiguous IDs, not log order, define
# warmup. Hashtable Sort-Object properties differed between PowerShell 5/7.
foreach($serial in 3,1,4,2){$log+="scene-gpu-time: serial=$serial draws=1 ticks=1 us=$($serial*10)"}
$log+="effects-gpu-timestamps: $header"
foreach($serial in 2,4,1,3){$log+="effects-gpu-time: serial=$serial draws=0 ticks=1 us=$($serial*6) input-us=$serial effects-us=$($serial*2) output-us=$($serial*3)"}
$path=Fixture 'out-of-order' $log
$result=Summarize 'out-of-order' $path
Check ($result.GpuMeanUsPerCompletePair.SceneUs -eq 30) 'Scene warmup did not follow serials'
Check ($result.GpuMeanUsPerCompletePair.EffectsUs -eq 18) 'Effects warmup did not follow serials'
Check ($result.GpuMeanUsPerCompletePair.InputUs -eq 3 -and $result.GpuMeanUsPerCompletePair.EffectsWorkUs -eq 6 -and $result.GpuMeanUsPerCompletePair.OutputUs -eq 9) 'Exclusive effect phases changed'
Reject 'duplicate-serial' @($log | ForEach-Object {$_ -replace 'scene-gpu-time: serial=2 ','scene-gpu-time: serial=3 '})
Reject 'missing-sample' @($log | Where-Object {$_ -notmatch '^scene-gpu-time: serial=2 '})
Reject 'cancelled' @($log | ForEach-Object {$_ -replace 'cancelled=0','cancelled=1'})
Reject 'phase-mismatch' @($log | ForEach-Object {$_ -replace 'effects-gpu-time: serial=2 draws=0 ticks=1 us=12','effects-gpu-time: serial=2 draws=0 ticks=1 us=13'})
foreach($split in $false,$true){
 $name=if($split){'four-model-phases'}else{'three-model-phases'}
 $drawLog=@('stereo-presentation-count: 4','scene-draw-profile: batches=4 owners=2 oversized=0',"scene-draw-gpu-timestamps: $header")
 foreach($serial in 3,1,4,2){
  $phases=if($split){"prepare-us=$serial clip-us=$($serial*2) spans-us=$($serial*3) paint-us=$($serial*4)"}else{"prepare-us=$($serial*3) spans-us=$($serial*3) paint-us=$($serial*4)"}
  $drawLog+="scene-draw-gpu-time: serial=$serial draws=3 ticks=1 us=$($serial*10) $phases batch=$serial index=0 kind=0"
 }
 $drawLog+="scene-draw-gpu-timestamps: $header"
 foreach($serial in 4,2,1,3){$drawLog+="scene-draw-gpu-time: serial=$serial draws=225 ticks=1 us=$($serial*5) batch=$serial index=1 kind=2"}
 $path=Fixture $name $drawLog
 $result=Summarize $name $path $true
 $model=@($result.Kinds | Where-Object {$_.Kind -eq 'model'})[0]
 $grid=@($result.Kinds | Where-Object {$_.Kind -eq 'grid'})[0]
 Check ($model.MeanUsPerCompletePair -eq 30 -and $grid.MeanUsPerCompletePair -eq 15) 'Draw warmup did not follow serials'
 Check ($model.PrepareUsPerPair -eq $(if($split){3}else{9}) -and $model.ClipUsPerPair -eq $(if($split){6}else{0}) -and $model.SpansUsPerPair -eq 9 -and $model.PaintUsPerPair -eq 12) 'Draw phases changed'
 Check ($model.SplitClipSamples -eq $(if($split){3}else{0})) 'Split clipping coverage count changed'
 Reject "$name-duplicate" @($drawLog | ForEach-Object {$_ -replace 'serial=2 draws=3','serial=3 draws=3'}) $true
 Reject "$name-reused-batch" @($drawLog | ForEach-Object {$_ -replace 'batch=2 index=0','batch=1 index=0'}) $true
 Reject "$name-phase-mismatch" @($drawLog | ForEach-Object {$_ -replace 'us=20 prepare-us=','us=21 prepare-us='}) $true
}
@{assertions=$assertions;host_version=$PSVersionTable.PSVersion.ToString();scope='Synthetic numerical/ownership parser fixtures, not GPU performance evidence'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output "PASS: $assertions timing summary assertions on PowerShell $($PSVersionTable.PSVersion)"
