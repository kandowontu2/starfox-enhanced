param([Parameter(Mandatory)][string]$Log,[Parameter(Mandatory)][string]$Output,
 [ValidateRange(0,1000)][int]$WarmupBatches=60)
$ErrorActionPreference='Stop'
$lines=Get-Content -LiteralPath $Log
$headers=@($lines | Where-Object {$_.StartsWith('model-host-cost-trace:')})
if($headers.Count -ne 1 -or $headers[0] -notmatch '^model-host-cost-trace: deferred=1 count=(\d+) dropped=0$') {
 throw 'Requires one complete, undropped serial-thread model trace'
}
$expected=[int]$Matches[1]
$cpuHeader=@($lines | Where-Object {$_.StartsWith('model-host-cpu-clock:')})
if($cpuHeader.Count -ne 1 -or $cpuHeader[0] -notmatch '^model-host-cpu-clock: provider=(win32-thread-kernel-user|posix-thread-cputime) unit=100ns count=(\d+) invalid=0$') {
 throw 'CPU clock missing, unsupported or invalid; do not interpret this as zero CPU work'
}
$provider=$Matches[1]
if([int]$Matches[2] -ne $expected){throw 'CPU/header record counts differ'}
$records=[Collections.Generic.List[object]]::new();$pending=$null
foreach($line in $lines) {
 if($line.StartsWith('model-host-cost-us')) {
  if($pending -or $line -notmatch '^model-host-cost-us shape=(\d+) vertices=(\d+) polygons=(\d+) pack=(\d+) prepare=(\d+) upload=(\d+) encode=(\d+) bytes=(\d+)$') {
   throw 'Malformed or unpaired wall record'
  }
  $pending=@{shape=[long]$Matches[1];vertices=[long]$Matches[2];polygons=[long]$Matches[3];bytes=[long]$Matches[8];wall=@{}}
  foreach($field in @(@('pack',4),@('prepare',5),@('upload',6),@('encode',7))){$pending.wall[$field[0]]=[long]$Matches[[int]$field[1]]}
 } elseif($line.StartsWith('model-host-cpu-100ns')) {
  if(!$pending -or $line -notmatch '^model-host-cpu-100ns shape=(\d+) vertices=(\d+) polygons=(\d+) pack=(\d+) prepare=(\d+) upload=(\d+) encode=(\d+) batch=(\d+) index=(\d+)$') {
   throw 'Malformed or unpaired CPU record'
  }
  foreach($field in @(@('shape',1),@('vertices',2),@('polygons',3))) {
   if($pending[$field[0]] -ne [long]$Matches[[int]$field[1]]){throw 'CPU/wall model identities differ'}
  }
  $pending.batch=[long]$Matches[8];$pending.index=[int]$Matches[9];$pending.cpu=@{}
  if(!$pending.batch){throw 'Missing actual scene batch annotation'}
  foreach($field in @(@('pack',4),@('prepare',5),@('upload',6),@('encode',7))){$pending.cpu[$field[0]]=[long]$Matches[[int]$field[1]]}
  $records.Add($pending);$pending=$null
 } elseif($line.StartsWith('model-host-cost-output:')) {throw 'Deferred diagnostic output failed'}
}
if($pending -or $records.Count -ne $expected){throw 'Incomplete CPU/wall trace'}
$measured=@($records | Where-Object {$_.batch -gt $WarmupBatches})
if(!$measured.Count){throw 'No post-warmup model records'}
$cpu=@{pack=0L;prepare=0L;upload=0L;encode=0L};$wall=@{pack=0L;prepare=0L;upload=0L;encode=0L};$bytes=0L
foreach($record in $measured) {
 $bytes+=$record.bytes
 foreach($field in 'pack','prepare','upload','encode'){$cpu[$field]+=$record.cpu[$field];$wall[$field]+=$record.wall[$field]}
}
$cpuTotal=($cpu.Values | Measure-Object -Sum).Sum
if(!$cpuTotal){throw 'Executing CPU time was never observed'}
$cpuUs=@{};foreach($field in $cpu.Keys){$cpuUs[$field]=$cpu[$field]/10.0}
$result=@{complete=$true;provider=$provider;log_sha256=(Get-FileHash -LiteralPath $Log).Hash;
 records=@($records);measured_records=$measured.Count;warmup_batches=$WarmupBatches;
 measured_cpu_100ns=$cpu;measured_cpu_us=$cpuUs;measured_wall_us=$wall;measured_upload_bytes=$bytes;
 scope='Ordinary model enqueue only, current executing thread; excludes driver/other worker threads and billboard path. CPU phase attribution can be OS-quantized; aggregate annotated draws. Clock calls add diagnostic overhead. No CPU cycles-to-time conversion or isolated FPS claim.'}
$result | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $Output
$result
