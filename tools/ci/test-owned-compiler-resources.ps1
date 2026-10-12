param([Parameter(Mandatory=$true)][string]$Receipt)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'owned-compiler-resources.ps1')
if(Test-Path -LiteralPath $Receipt){throw 'Preserve previous observer controls'}
$ciChecks=[Collections.Generic.List[string]]::new()
$ciEpoch=[datetime]::Parse('2026-10-12T08:00:00Z').ToUniversalTime()
function New-CiProcess([int]$Id,[int]$Parent,[long]$Seconds){
    [pscustomobject]@{ProcessId=$Id;ParentProcessId=$Parent;CreationDate=$ciEpoch.AddSeconds($Seconds)}
}
function Assert-CiIds($Tree,[int[]]$Expected,[string]$Name){
    if(@(Compare-Object @($Tree.Members.ProcessId|Sort-Object) @($Expected|Sort-Object)).Count){throw ('Identity control failed: '+$Name)}
    $ciChecks.Add($Name)
}
$ciRoot=New-CiProcess 10 0 0
$ciChild=New-CiProcess 20 10 1
$ciGrandchild=New-CiProcess 30 20 2
Assert-CiIds (Select-CiOwnedSnapshot @($ciRoot,$ciChild,$ciGrandchild,(New-CiProcess 40 99 1)) 10 $ciEpoch) @(10,20,30) 'actual ancestry, unrelated process excluded'
Assert-CiIds (Select-CiOwnedSnapshot @($ciRoot,(New-CiProcess 40 10 -1),(New-CiProcess 50 40 0)) 10 $ciEpoch) @(10) 'older PID-recycled pseudo-child and descendants excluded'
Assert-CiIds (Select-CiOwnedSnapshot @($ciRoot,$ciChild,(New-CiProcess 40 20 0)) 10 $ciEpoch) @(10,20) 'pseudo-child older than immediate selected parent excluded'
Assert-CiIds (Select-CiOwnedSnapshot @($ciRoot,(New-CiProcess 20 10 0)) 10 $ciEpoch) @(10,20) 'equal-resolution creation timestamp retained'
foreach($ciNegative in @(
    @{name='root birth mismatch rejects';snapshot=@($ciRoot);birth=$ciEpoch.AddSeconds(1)},
    @{name='missing root rejects';snapshot=@($ciChild);birth=$ciEpoch},
    @{name='duplicate PID rejects';snapshot=@($ciRoot,$ciRoot);birth=$ciEpoch})){
    $ciRejected=$false
    try {$null=Select-CiOwnedSnapshot $ciNegative.snapshot 10 $ciNegative.birth}catch{$ciRejected=$true}
    if(!$ciRejected){throw ('Observer accepted '+$ciNegative.name)}
    $ciChecks.Add($ciNegative.name)
}
$ciNative=$null
try {
    $ciProgram=[IO.Path]::GetFullPath((Get-Process -Id $PID).Path)
    $ciInfo=[Diagnostics.ProcessStartInfo]::new($ciProgram)
    $ciInfo.UseShellExecute=$false;$ciInfo.CreateNoWindow=$true
    foreach($ciArg in @('-NoProfile','-Command','Start-Sleep -Seconds 8')){[void]$ciInfo.ArgumentList.Add($ciArg)}
    $ciNative=[Diagnostics.Process]::Start($ciInfo)
    $ciBirth=$ciNative.StartTime.ToUniversalTime()
    $ciMeasured=Read-CiOwnedCompilerResources $ciNative $ciBirth $ciProgram
    if($ciMeasured.Completed -or $ciMeasured.Private -le 0 -or @($ciMeasured.Workers|Where-Object pid -eq $ciNative.Id).Count -ne 1){throw 'Actual handle/birth/image sample did not retain live owner'}
    $ciChecks.Add('actual Windows handle, birth, image and private-memory sample')
    if(!$ciNative.WaitForExit(12000)){throw 'Authored observation control did not retire normally'}
    if($ciNative.ExitCode -ne 0){throw 'Actual observer control process failed'}
    if(!(Read-CiOwnedCompilerResources $ciNative $ciBirth $ciProgram).Completed){throw 'Retired actual owner remained live'}
    $ciChecks.Add('actual terminal owner remains terminal')
} finally {
    if($ciNative){$ciNative.Refresh();if(!$ciNative.HasExited){$ciNative.Kill($true);$ciNative.WaitForExit()};$ciNative.Dispose()}
}
[ordered]@{passed=$true;checks=$ciChecks.ToArray();helper_sha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'owned-compiler-resources.ps1')).Hash;
    private_cap_changed=$false;launch_or_live_floor_changed=$false;gpu_executed=$false}|
    ConvertTo-Json -Depth 6|Out-File -LiteralPath $Receipt -Encoding utf8
Write-Output ('PASS: '+$ciChecks.Count+' owned-compiler identity controls; original resource guards unchanged')
