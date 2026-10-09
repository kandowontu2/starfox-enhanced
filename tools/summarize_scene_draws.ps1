param([Parameter(Mandatory)][string]$Log,
 [Parameter(Mandatory)][string]$Output,
 [ValidateRange(0,1000)][int]$WarmupFrames=60)
$ErrorActionPreference='Stop'
$lines=Get-Content -LiteralPath $Log
$frameLine=@($lines | Where-Object {$_ -match '^stereo-presentation-count: (\d+)$'})
if($frameLine.Count -ne 1){throw 'Requires one completed SBS profiling run'}
$frames=[int]($frameLine[0] -replace '^stereo-presentation-count: ','')
if($frames -le $WarmupFrames){throw 'No measured frames remain'}
$profiles=@();$profile=$null;$stream=$null
foreach($line in $lines) {
 if($line -match '^scene-draw-profile: batches=(\d+) owners=(\d+) oversized=(\d+)$') {
  if([int]$Matches[3]){throw 'Per-draw profile exceeded its resource/allocation bound'}
  $profile=@{Batches=[int]$Matches[1];ExpectedOwners=[int]$Matches[2];Streams=@()}
  if(!$profile.Batches -or $profile.Batches%$frames){throw 'Variable scene batch multiplicity: cannot infer warmup frames'}
  $profiles+=,$profile;$stream=$null
 } elseif($line -match '^scene-draw-gpu-timestamps: samples=(\d+) dropped=(\d+) cancelled=(\d+) errors=(\d+) .*boundary=bottom-bottom backend=(direct3d12|vulkan)$') {
  if(!$profile -or [int]$Matches[2] -or [int]$Matches[3] -or [int]$Matches[4]){throw 'Incomplete per-draw query owner'}
  $stream=@{Expected=[int]$Matches[1];Backend=$Matches[5];Samples=@()}
  if(!$stream.Expected){throw 'Empty per-draw query owner'}
  $profile.Streams+=,$stream
 } elseif($line -match '^scene-draw-gpu-time: serial=(\d+) draws=(\d+) ticks=\d+ us=([\d.e+]+)(?: prepare-us=([\d.e+]+)(?: clip-us=([\d.e+]+))? spans-us=([\d.e+]+) paint-us=([\d.e+]+))? batch=(\d+) index=(\d+) kind=([0-7])$') {
  if(!$stream){throw 'Per-draw sample without its owner'}
  $sample=@{Serial=[int]$Matches[1];Units=[int]$Matches[2];
   Us=[double]::Parse($Matches[3],[Globalization.CultureInfo]::InvariantCulture);
   Batch=[int]$Matches[8];Index=[int]$Matches[9];Kind=[int]$Matches[10]}
  if($Matches.ContainsKey(4)) {
   foreach($field in @(@('PrepareUs',4),@('SpansUs',6),@('PaintUs',7))) {
    $sample[$field[0]]=[double]::Parse($Matches[[int]$field[1]],[Globalization.CultureInfo]::InvariantCulture)
   }
   $sample.ClipUs=if($Matches.ContainsKey(5)){[double]::Parse($Matches[5],[Globalization.CultureInfo]::InvariantCulture)}else{0.0}
   $sample.SplitClip=$Matches.ContainsKey(5)
   if($sample.Kind -ne 0 -or [Math]::Abs($sample.Us-$sample.PrepareUs-$sample.ClipUs-$sample.SpansUs-$sample.PaintUs) -gt [Math]::Max(.1,$sample.Us*.00002)) {
    throw 'Invalid model phases or sum mismatch'
   }
  }
  $stream.Samples+=,$sample
 } elseif($line.StartsWith('scene-draw-gpu-timestamps:') -or $line.StartsWith('scene-draw-gpu-time:')) {
  throw 'Unmarked, unavailable or malformed per-draw GPU data'
 }
}
if(!$profiles.Count){throw 'Missing per-draw GPU profiles'}
$names=@('model','raster','grid','dust','particles','text','background','indexed-layer')
$totals=@{};$owners=@();$drawCount=0
foreach($owner in $profiles) {
 if($owner.Streams.Count -ne $owner.ExpectedOwners){throw 'Missing per-draw owner streams'}
 $warmupBatch=$WarmupFrames*[int]($owner.Batches/$frames)
 $indices=@{}
 foreach($query in $owner.Streams) {
  if($query.Samples.Count -ne $query.Expected){throw 'Per-draw query sample count mismatch'}
  $ordered=@($query.Samples | Sort-Object { [int]$_['Serial'] })
  $index=$ordered[0].Index
  if($indices.ContainsKey($index)){throw 'Duplicate per-draw owner index'}
  $indices[$index]=$true;$lastBatch=0
  for($i=0;$i -lt $ordered.Count;++$i) {
   $sample=$ordered[$i]
   if($sample.Serial -ne $i+1 -or $sample.Index -ne $index -or
      $sample.Batch -le $lastBatch -or $sample.Batch -gt $owner.Batches -or
      [double]::IsNaN($sample.Us) -or [double]::IsInfinity($sample.Us) -or $sample.Us -lt 0){throw 'Invalid/reused per-draw sample'}
   $lastBatch=$sample.Batch
   if($sample.Batch -le $warmupBatch){continue}
   $key="$($query.Backend)/$($names[$sample.Kind])"
   if(!$totals.ContainsKey($key)){$totals[$key]=@{Us=0.0;Count=0;Units=0L;PeakUs=0.0;Phased=0;SplitClip=0;PrepareUs=0.0;ClipUs=0.0;SpansUs=0.0;PaintUs=0.0}}
   $total=$totals[$key];$total.Us+=$sample.Us;++$total.Count
   $total.Units+=$sample.Units;$total.PeakUs=[Math]::Max($total.PeakUs,$sample.Us);++$drawCount
   if($sample.ContainsKey('PrepareUs')) {
    ++$total.Phased
    if($sample.SplitClip){++$total.SplitClip}
    foreach($field in 'PrepareUs','ClipUs','SpansUs','PaintUs'){$total[$field]+=$sample[$field]}
   }
  }
 }
 $owners+=@{Batches=$owner.Batches;DrawOwners=$owner.ExpectedOwners;WarmupBatches=$warmupBatch}
}
if(!$drawCount){throw 'No measured per-draw samples'}
$summary=@(foreach($key in ($totals.Keys | Sort-Object)) {
 $total=$totals[$key];$parts=$key.Split('/')
 @{Backend=$parts[0];Kind=$parts[1];MeanUsPerCompletePair=$total.Us/($frames-$WarmupFrames);
  MeanUsPerDraw=$total.Us/$total.Count;PeakUs=$total.PeakUs;Samples=$total.Count;
  MeanAuthoredUnits=$total.Units/[double]$total.Count;PhasedSamples=$total.Phased;
  SplitClipSamples=$total.SplitClip;PrepareUsPerPair=$total.PrepareUs/($frames-$WarmupFrames);
  ClipUsPerPair=$total.ClipUs/($frames-$WarmupFrames);
  SpansUsPerPair=$total.SpansUs/($frames-$WarmupFrames);
  PaintUsPerPair=$total.PaintUs/($frames-$WarmupFrames)}
})
$result=@{Log=(Resolve-Path -LiteralPath $Log).Path;LogSha256=(Get-FileHash -LiteralPath $Log).Hash;
 Frames=$frames;WarmupFrames=$WarmupFrames;DrawSamples=$drawCount;Owners=$owners;Kinds=$summary;
 Scope='Instrumented SDL draw intervals through their normal merge/MSAA/ray-input packing. Excludes scene preamble, empty-scene clearing, native hardware-ray queue, CPU encoding and presentation; not an FPS gain. Model units are authored faces, not expanded GPU face counts.'}
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Output -Encoding UTF8
$result
