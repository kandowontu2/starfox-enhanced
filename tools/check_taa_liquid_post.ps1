param(
    [string]$BuildDirectory='C:/Users/kando/Documents/ChatGPT/Starfox Enhanced/build/current',
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidateSet('direct3d12','vulkan')][string[]]$Backends=@('direct3d12','vulkan')
)
$ErrorActionPreference='Stop'
$build=[IO.Path]::GetFullPath($BuildDirectory);$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Use a fresh evidence directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$binary=Join-Path $build 'starfox_displayxr_gpu_presenter_check.exe'
$hash=(Get-FileHash -LiteralPath $binary).Hash;$saved=@{};$records=@()
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_GPU_DRIVER$|SDL_AUDIODRIVER$|SDL_ASSERT$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$env:DISABLE_VK_LAYER_reshade_1='1';$env:SDL_ASSERT='abort'
try {
    foreach($backend in $Backends) {
        $arguments=@('--taa-liquid-post-only');if($backend -eq 'vulkan'){$arguments=@('--vulkan')+$arguments}
        $stdout=Join-Path $output "$backend.log";$stderr=Join-Path $output "$backend.err"
        $process=Start-Process -FilePath $binary -ArgumentList $arguments -WorkingDirectory $build -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $handle=$process.Handle
        while(!$process.WaitForExit(30000)){Write-Output "$backend remains running on the same PID $($process.Id)"}
        $process.WaitForExit()
        if($process.ExitCode -ne 0){throw "$backend failed ($($process.ExitCode)); see $stdout and $stderr"}
        $log=Get-Content -LiteralPath $stdout -Raw
        $cases=([regex]::Matches($log,'TAA liquid post selection \d+ quality \d+: full image CPU order and authored plane oracle passed')).Count
        if($cases -ne 108 -or ([regex]::Matches($log,'TAA liquid post: 9 selections, 3 qualities')).Count -ne 4){
            throw "$backend omitted required four-format owner cases: $cases/108"
        }
        if($log -notmatch 'in-flight nonblocking cancellation|in-flight close' -or $log -notmatch 'Mocked dispatch, not physical display'){
            throw "$backend omitted queue/lifetime or final completion evidence"
        }
        if((Get-FileHash -LiteralPath $binary).Hash -ne $hash){throw 'Executable changed during verification'}
        $records+=@{backend=$backend;cases=$cases;exit=$process.ExitCode;log=$stdout}
        Write-Output "$backend passed $cases independent full-image liquid post cases"
    }
    @{complete=$true;binary=$hash;records=$records;
        scope='Real native TAA/RT water on both GPU APIs and four formats; CPU post-order with independent unstyled timelines and authored wet/dry centre-plane ownership, three qualities, moving/banked liquid/eyes, protected artwork, held/wait/rejected/queue-cancelled frames. Not an independent optical ray or depth-finish image oracle, true secondary motion, physical Leia or FPS claim. Other projects untouched'} |
        ConvertTo-Json -Depth 5 | Out-File -LiteralPath (Join-Path $output 'results.json') -Encoding utf8
} catch {
    @{complete=$false;binary=$hash;records=$records;error=$_.Exception.Message} |
        ConvertTo-Json -Depth 5 | Out-File -LiteralPath (Join-Path $output 'results.json') -Encoding utf8
    throw
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_GPU_DRIVER$|SDL_AUDIODRIVER$|SDL_ASSERT$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $saved.GetEnumerator()){Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
