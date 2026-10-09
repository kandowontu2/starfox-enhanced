param(
    [Parameter(Mandatory=$true)][string]$LogPath,
    [ValidateRange(12,120)][int]$Frames=24
)
$ErrorActionPreference='Stop'
$taskLines=Get-Content -LiteralPath $LogPath
if(@($taskLines -match '^Native liquid ABBA timing completed:').Count -ne 1) {throw 'Native timing run did not complete exactly once'}
if($taskLines -match '^dxr-gpu-timing-error:') {throw 'Native timing retirement failed'}
$taskRows=@(foreach($taskLine in $taskLines) {
    if($taskLine -notmatch '^dxr-gpu-timing: ') {continue}
    $taskFields=@{}
    foreach($taskToken in $taskLine.Split(' ')) {
        $taskPair=$taskToken.Split('=',2)
        if($taskPair.Length -eq 2) {$taskFields[$taskPair[0]]=$taskPair[1]}
    }
    foreach($taskKey in @('producer','call','serial','width','height','material','triangles','rebuild','history','liquid_motion','as_ms','trace_ms','motion_ms','finish_ms','total_ms','diagnostic_readback_bytes')) {
        if(!$taskFields.ContainsKey($taskKey)) {throw "Missing native timing field: $taskKey"}
    }
    $taskCall=[int]$taskFields.call
    if($taskCall -le 0 -or $taskCall -gt 8*$Frames) {throw 'Native timing call exceeds the fixture sequence'}
    $taskKind=[int][math]::Floor(($taskCall-1)/(4*$Frames))
    $taskBlock=[int][math]::Floor((($taskCall-1)%(4*$Frames))/$Frames)
    $taskFrame=($taskCall-1)%$Frames
    $taskHistory=[int]($taskBlock -eq 1 -or $taskBlock -eq 2)
    if([int]$taskFields.material -ne 3*$taskKind -or [int]$taskFields.history -ne $taskHistory -or
        [int]$taskFields.liquid_motion -ne $taskHistory -or
        [int]$taskFields.triangles -ne 4 -or [int]$taskFields.diagnostic_readback_bytes -ne 40) {
        throw "Native timing input/layout/mode differs from ABBA fixture at call $taskCall"
    }
    $taskTimes=@{}
    foreach($taskKey in @('as_ms','trace_ms','motion_ms','finish_ms','total_ms')) {
        $taskValue=[double]::Parse($taskFields[$taskKey],[Globalization.CultureInfo]::InvariantCulture)
        if(![double]::IsFinite($taskValue) -or $taskValue -lt 0) {throw 'Nonfinite/negative native timing interval'}
        $taskTimes[$taskKey]=$taskValue
    }
    $taskSum=$taskTimes.as_ms+$taskTimes.trace_ms+$taskTimes.motion_ms+$taskTimes.finish_ms
    if([math]::Abs($taskSum-$taskTimes.total_ms) -gt .0001+$taskTimes.total_ms*.00001) {throw 'Native timing intervals do not reconstruct the total'}
    [pscustomobject]@{
        Producer=[int]$taskFields.producer;Call=$taskCall;Serial=[uint64]$taskFields.serial
        Width=[int]$taskFields.width;Height=[int]$taskFields.height;Material=[int]$taskFields.material
        Block=$taskBlock;Frame=$taskFrame;History=$taskHistory
        AS=$taskTimes.as_ms;Trace=$taskTimes.trace_ms;Motion=$taskTimes.motion_ms;Finish=$taskTimes.finish_ms;Total=$taskTimes.total_ms
    }
})
if($taskRows.Count -ne 16*$Frames) {throw 'Native timing receipt count differs from two complete eye producers'}
$taskProducers=@($taskRows | Group-Object Producer)
if($taskProducers.Count -ne 2) {throw 'Native timing requires exactly two independent eye producers'}
foreach($taskProducer in $taskProducers) {
    $taskOrdered=@($taskProducer.Group | Sort-Object Call)
    for($taskIndex=0;$taskIndex -lt 8*$Frames;++$taskIndex) {
        if($taskOrdered[$taskIndex].Call -ne $taskIndex+1 -or ($taskIndex -gt 0 -and $taskOrdered[$taskIndex].Serial -le $taskOrdered[$taskIndex-1].Serial)) {
            throw 'Native timing lost/repeated a completed query or producer fence'
        }
    }
}
$taskExtents=@($taskRows | Group-Object Width,Height)
if($taskExtents.Count -ne 1) {throw 'Native timing mixed eye extents'}
$taskWarm=@($taskRows | Where-Object {$_.Frame -ge 8})
$taskSummary=@($taskWarm | Group-Object Material,History | ForEach-Object {
    $taskGroup=$_.Group
    $taskIntervals=[ordered]@{}
    foreach($taskKey in @('AS','Trace','Motion','Finish','Total')) {
        $taskSamples=@($taskGroup.$taskKey | Sort-Object)
        $taskMiddle=[int]($taskSamples.Count/2)
        $taskIntervals[$taskKey]=[ordered]@{
            median_ms=($taskSamples[$taskMiddle-1]+$taskSamples[$taskMiddle])*.5
            p90_ms=$taskSamples[[int][math]::Ceiling($taskSamples.Count*.9)-1]
            maximum_ms=$taskSamples[-1]
        }
    }
    [ordered]@{material=$taskGroup[0].Material;history=$taskGroup[0].History;samples=$taskGroup.Count;intervals=$taskIntervals}
})
[ordered]@{
    log=(Resolve-Path -LiteralPath $LogPath).Path
    log_sha256=(Get-FileHash -LiteralPath $LogPath -Algorithm SHA256).Hash
    width=$taskRows[0].Width;height=$taskRows[0].Height
    frames_per_block_per_eye=$Frames;warmup_frames_per_block_per_eye=8
    completed_query_receipts=$taskRows.Count;warm_samples=$taskWarm.Count
    groups=$taskSummary
    scope='Loaded component-only same-input native producer ABBA. Four finite triangles; two eyes; GPU timestamps exclude CPU pipeline preparation, external geometry preparation/interop and the history resolver. Diagnostic synchronization is outside intervals. Other projects remain running. Not in-game or sustained FPS, physical device, peak memory, or whole-owner performance acceptance.'
} | ConvertTo-Json -Depth 7
