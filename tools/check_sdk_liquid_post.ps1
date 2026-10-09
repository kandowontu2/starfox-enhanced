param(
    [string]$BuildDirectory='build/current',
    [Parameter(Mandatory=$true)][string]$SdkDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$build=[IO.Path]::GetFullPath($BuildDirectory)
$sdk=[IO.Path]::GetFullPath($SdkDirectory)
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Use a fresh evidence directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$binary=Join-Path $build 'starfox_displayxr_gpu_presenter_check.exe'
$hash=(Get-FileHash -LiteralPath $binary).Hash
$runtimeHashes=@{}
foreach($name in 'starfox_dlss_native.dll','sl.interposer.dll','sl.common.dll','sl.dlss.dll','nvngx_dlss.dll') {
    $runtimeHashes[$name]=(Get-FileHash -LiteralPath (Join-Path $sdk $name)).Hash
}
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_GPU_DRIVER$|SDL_AUDIODRIVER$|SDL_ASSERT$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$env:DISABLE_VK_LAYER_reshade_1='1';$env:SDL_ASSERT='abort'
try {
    $stdout=Join-Path $output 'sdk.log';$stderr=Join-Path $output 'sdk.err'
    $arguments=@('--dlss-sdk',('"'+$sdk+'"'),'--dlss-liquid-post-only')
    $process=Start-Process -FilePath $binary -ArgumentList $arguments -WorkingDirectory $build -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $handle=$process.Handle
    while(!$process.WaitForExit(30000)){Write-Output "SDK liquid post remains running on the same PID $($process.Id)"}
    $process.WaitForExit()
    if($process.ExitCode -ne 0){throw "SDK liquid post failed ($($process.ExitCode)); see $stdout and $stderr"}
    $log=Get-Content -LiteralPath $stdout -Raw
    $cases=([regex]::Matches($log,'SDK liquid post model [01] selection \d+ quality \d+: full image CPU order and authored plane oracle passed')).Count
    $summaries=([regex]::Matches($log,'SDK liquid post model [01]: 9 selections, 3 qualities')).Count
    if($cases -ne 216 -or $summaries -ne 8){throw "Missing K/M/four-format/centre-SSAA-MSAA cases: $cases/216, $summaries/8"}
    if($log -notmatch 'in-flight nonblocking cancellation' -or $log -notmatch 'Mocked dispatch, not physical display' `
        -or $log -notmatch 'Real native SDK lifecycle: \d+ evaluation attempts') {
        throw 'Missing actual SDK, queue/lifetime or terminal completion evidence'
    }
    if((Get-FileHash -LiteralPath $binary).Hash -ne $hash){throw 'Executable changed during verification'}
    foreach($entry in $runtimeHashes.GetEnumerator()) {
        if((Get-FileHash -LiteralPath (Join-Path $sdk $entry.Key)).Hash -ne $entry.Value){throw "SDK module changed: $($entry.Key)"}
    }
    @{complete=$true;binary=$hash;runtime=$runtimeHashes;cases=$cases;summaries=$summaries;exit=$process.ExitCode;
      scope='Actual native K/M SDK allocation/evaluation/acceptance on D3D12 and four formats, with an equal-size identity copy after the opaque filter. Full-image CPU post order and independent authored wet/dry centre ownership; centre/3x SSAA/4-sample MSAA, moving/banked liquid/eyes, protected ink, held/wait/rejected/queue-cancelled frames. Not private neural-filter quality, an independent optical/depth-finishing oracle, true secondary motion, physical Leia or performance acceptance. Other projects untouched'} |
        ConvertTo-Json -Depth 5 | Out-File -LiteralPath (Join-Path $output 'results.json') -Encoding utf8
    Write-Output "PASS: $cases actual SDK liquid post cases, K/M, four formats and three source-AA layouts."
} catch {
    @{complete=$false;binary=$hash;runtime=$runtimeHashes;error=$_.Exception.Message} |
        ConvertTo-Json -Depth 5 | Out-File -LiteralPath (Join-Path $output 'results.json') -Encoding utf8
    throw
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_GPU_DRIVER$|SDL_AUDIODRIVER$|SDL_ASSERT$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $saved.GetEnumerator()){Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
