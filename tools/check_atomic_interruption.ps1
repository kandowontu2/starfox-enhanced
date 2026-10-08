param(
    [string]$Binary='build/current/starfox_runtime_input_tests.exe',
    [Parameter(Mandatory)][string]$Output
)
$ErrorActionPreference='Stop'
$binaryPath=(Resolve-Path -LiteralPath $Binary).Path
$binaryHash=(Get-FileHash -LiteralPath $binaryPath).Hash
if(Test-Path -LiteralPath $Output) {throw 'Choose a fresh interruption fixture directory'}
$root=(New-Item -ItemType Directory -Path $Output).FullName
$config=Join-Path $root 'pregame.cfg'
$fixture=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../tests/fixtures/gpu-recovery.cfg')).Path
Copy-Item -LiteralPath $fixture -Destination $config
$originalHash=(Get-FileHash -LiteralPath $config).Hash
$process=$null
try {
    $process=Start-Process -FilePath $binaryPath -WindowStyle Hidden -PassThru `
        -ArgumentList @('--atomic-write-interruption', ('"'+$root+'"'))
    $handle=$process.Handle
    $deadline=[DateTime]::UtcNow.AddSeconds(30)
    while(!(Test-Path -LiteralPath (Join-Path $root 'ready'))) {
        $process.Refresh()
        if($process.HasExited) {throw "Interruption helper exited early: $($process.ExitCode)"}
        if([DateTime]::UtcNow -ge $deadline) {throw 'No live temporary-write checkpoint'}
        Start-Sleep -Milliseconds 100
    }
    if((Get-FileHash -LiteralPath $config).Hash -ne $originalHash) {throw 'Live temporary write changed the old file'}
    # Exact PID created above; no unrelated build/game process is interrupted.
    Stop-Process -Id $process.Id
    $process.WaitForExit()
    $process.Dispose();$process=$null
    if((Get-FileHash -LiteralPath $config).Hash -ne $originalHash) {throw 'Forced close lost the previous settings'}
    $orphans=@(Get-ChildItem -LiteralPath $root -Filter 'pregame.cfg.sfe-tmp-*')
    if($orphans.Count -ne 1) {throw 'Interruption was not during a real temporary write'}
    $orphanHash=(Get-FileHash -LiteralPath $orphans[0].FullName).Hash
    $process=Start-Process -FilePath $binaryPath -WindowStyle Hidden -PassThru `
        -ArgumentList @('--atomic-recover-settings', ('"'+$root+'"'))
    $handle=$process.Handle
    if(!$process.WaitForExit(30000)) {throw 'Settings recovery helper did not finish'}
    if($process.ExitCode) {throw "Previous settings did not load/save after force close: $($process.ExitCode)"}
    $process.Dispose();$process=$null
    $text=Get-Content -LiteralPath $config -Raw
    foreach($entry in @('RENDERER_MODE 1','MUSIC_VOLUME 71','SFX_VOLUME 83')) {
        if($text -notmatch ('(?m)^'+[regex]::Escape($entry)+'\s*$')) {throw "Recovered settings differ: $entry"}
    }
    if((Get-FileHash -LiteralPath $orphans[0].FullName).Hash -ne $orphanHash) {throw 'Recovery overwrote an abandoned temporary'}
    if(@(Get-ChildItem -LiteralPath $root -Filter 'pregame.cfg.sfe-tmp-*').Count -ne 1) {throw 'Successful recovery leaked another temporary'}
    if((Get-FileHash -LiteralPath $binaryPath).Hash -ne $binaryHash) {throw 'Helper executable changed during the check'}
    @{sha256=$binaryHash;original_config=$originalHash;passed=$true;forced_close_during_write=$true;
        previous_file_preserved=$true;old_file_loadable=$true;unrelated_options_preserved=$true;
        abandoned_temporary_preserved=$true} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'results.json')
    "PASS atomic interruption: old settings intact, load/save succeeds, unrelated options and abandoned temporary preserved"
} finally {
    if($process) {
        $process.Refresh()
        if(!$process.HasExited) {Stop-Process -Id $process.Id; $process.WaitForExit()}
        $process.Dispose()
    }
}
