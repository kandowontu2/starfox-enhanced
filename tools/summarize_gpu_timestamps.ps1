param([Parameter(Mandatory)][string]$Log,
 [Parameter(Mandatory)][string]$Output,
 [ValidateRange(0,1000)][int]$WarmupFrames=60)
$ErrorActionPreference='Stop'
$lines=Get-Content -LiteralPath $Log
$frameLine=@($lines | Where-Object {$_ -match '^stereo-presentation-count: (\d+)$'})
if($frameLine.Count -ne 1){throw 'Requires one completed SBS profiling run'}
$frames=[int]($frameLine[0] -replace '^stereo-presentation-count: ','')
if($frames -le $WarmupFrames){throw 'No measured frames remain'}
$streams=@();$stream=$null
foreach($line in $lines) {
 if($line -match '^(scene|effects)-gpu-timestamps: samples=(\d+) dropped=(\d+) cancelled=(\d+) errors=(\d+) .*backend=(direct3d12|vulkan)$') {
  if([int]$Matches[3] -or [int]$Matches[4] -or [int]$Matches[5]){throw 'Incomplete GPU query stream'}
  if(!$line.Contains('boundary=bottom-bottom')){throw 'Missing bottom-of-pipe boundary marker; do not sum TOP/BOTTOM intervals as exclusive work'}
  $stream=@{Kind=$Matches[1];Expected=[int]$Matches[2];Backend=$Matches[6];Samples=@()}
  $streams+=,$stream
 } elseif($line -match '^(scene|effects)-gpu-time: serial=(\d+) draws=\d+ ticks=\d+ us=([\d.e+]+)(?: input-us=([\d.e+]+) effects-us=([\d.e+]+) output-us=([\d.e+]+))?$') {
  if(!$stream -or $Matches[1] -ne $stream.Kind){throw 'GPU sample without its owner stream'}
  $serial=[int]$Matches[2]
  $sample=@{Serial=$serial;Us=[double]::Parse($Matches[3],[Globalization.CultureInfo]::InvariantCulture)}
  if($stream.Kind -eq 'effects') {
   foreach($field in @(@('InputUs',4),@('EffectsUs',5),@('OutputUs',6))) {
    if(!$Matches.ContainsKey([int]$field[1])){throw 'Missing effect phase'}
    $sample[$field[0]]=[double]::Parse($Matches[[int]$field[1]],[Globalization.CultureInfo]::InvariantCulture)
   }
   if([Math]::Abs($sample.Us-$sample.InputUs-$sample.EffectsUs-$sample.OutputUs) -gt [Math]::Max(0.05,$sample.Us*0.00002)){throw 'GPU phase sum mismatch'}
  }
  $stream.Samples+=,$sample
 }
}
function Percentile($values,$fraction) {
 $sorted=@($values | Sort-Object); if(!$sorted.Count){throw 'Empty timing series'}
 return $sorted[[Math]::Min($sorted.Count-1,[int][Math]::Floor($sorted.Count*$fraction))]
}
$summary=@();$totals=@{SceneUs=0.0;EffectsUs=0.0;InputUs=0.0;OutputUs=0.0;EffectsWorkUs=0.0}
foreach($owner in $streams) {
 if($owner.Samples.Count -ne $owner.Expected -or $owner.Expected%$frames){throw 'Cannot map variable query multiplicity to the warmup frames'}
 # Windows PowerShell 5 does not resolve hashtable keys as Sort-Object
 # properties. Explicit numeric lookup preserves out-of-order fence retirement
 # without silently sorting arbitrarily or relaxing the contiguous-ID gate.
 $ordered=@($owner.Samples | Sort-Object { [int]$_['Serial'] })
 for($i=0;$i -lt $ordered.Count;++$i){if($ordered[$i].Serial -ne $i+1){
  throw "GPU query serial gap or reuse: $($owner.Kind), samples=$($owner.Expected), index=$i, actual=$($ordered[$i].Serial), expected=$($i+1)"
 }}
 $perFrame=[int]($owner.Expected/$frames)
 $measured=@($ordered | Select-Object -Skip ($WarmupFrames*$perFrame))
 $total=($measured.Us | Measure-Object -Sum).Sum
 $key=if($owner.Kind -eq 'scene'){'SceneUs'}else{'EffectsUs'}
 $totals[$key]+=$total/($frames-$WarmupFrames)
 $entry=@{Kind=$owner.Kind;Backend=$owner.Backend;Samples=$owner.Expected;QueriesPerFrame=$perFrame;
  MedianUs=(Percentile $measured.Us .5);P95Us=(Percentile $measured.Us .95);MeanUs=$total/$measured.Count}
 if($owner.Kind -eq 'effects') {
  foreach($field in @('InputUs','EffectsUs','OutputUs')) {
   $entry[$field]=($measured.$field | Measure-Object -Average).Average
   $target=if($field -eq 'EffectsUs'){'EffectsWorkUs'}else{$field}
   $totals[$target]+=($measured.$field | Measure-Object -Sum).Sum/($frames-$WarmupFrames)
  }
 }
 $summary+=,$entry
}
if(!@($streams | Where-Object {$_.Kind -eq 'scene'}).Count -or !@($streams | Where-Object {$_.Kind -eq 'effects'}).Count){throw 'Missing scene/effect GPU stream'}
$result=@{Log=(Resolve-Path -LiteralPath $Log).Path;LogSha256=(Get-FileHash -LiteralPath $Log).Hash;
 Frames=$frames;WarmupFrames=$WarmupFrames;GpuMeanUsPerCompletePair=$totals;Owners=$summary;
 Scope='SDL scene and effects commands only; excludes native DXR queue work, presentation, CPU encoding and queue-idle time. Instrumented timings are not an uninstrumented FPS gain.'}
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Output -Encoding UTF8
$result
