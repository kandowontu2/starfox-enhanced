param([string]$BuildDirectory='build/current',
    [string]$OutputDirectory='tmp/native-edge-taa-check')
$ErrorActionPreference='Stop'
$build=[IO.Path]::GetFullPath($BuildDirectory)
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a new evidence directory; existing results will not be overwritten'}
New-Item -ItemType Directory -Path $output | Out-Null
$names=@('starfox_pc.exe','starfox_calibrated_ray_composite_check.exe','starfox_displayxr_gpu_presenter_check.exe')
$hashes=@{}
foreach($name in $names){$hashes[$name]=(Get-FileHash -LiteralPath (Join-Path $build $name)).Hash}
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_GPU_DRIVER$|SDL_AUDIODRIVER$|SDL_ASSERT$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$env:DISABLE_VK_LAYER_reshade_1='1';$env:SDL_ASSERT='abort'
$records=@()
try {
    foreach($kind in 'component','owner') {
        foreach($backend in 'direct3d12','vulkan') {
            $name="$backend-$kind"
            $binary=Join-Path $build $(if($kind -eq 'component'){'starfox_calibrated_ray_composite_check.exe'}else{'starfox_displayxr_gpu_presenter_check.exe'})
            $arguments=if($kind -eq 'component'){@($backend,'--temporal-edge-only')}elseif($backend -eq 'vulkan'){@('--vulkan','--taa-edge-only')}else{@('--taa-edge-only')}
            $stdout=Join-Path $output "$name.log";$stderr=Join-Path $output "$name.err"
            $process=Start-Process -FilePath $binary -ArgumentList $arguments -WorkingDirectory $build -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput $stdout -RedirectStandardError $stderr
            $handle=$process.Handle # Retain this SAME handle across observation timeouts.
            while(!$process.WaitForExit(30000)){Write-Output "$name remains running (PID $($process.Id))"}
            $process.WaitForExit()
            if($process.ExitCode -ne 0){throw "$name failed with exit $($process.ExitCode); see $stderr and $stdout"}
            $log=Get-Content -LiteralPath $stdout -Raw
            $pattern=if($kind -eq 'component'){'Edge TAA format \d+:'}else{'edge witness 1, intensity'}
            $cases=([regex]::Matches($log,$pattern)).Count
            $expected=if($kind -eq 'component'){4}else{108}
            if($cases -ne $expected){throw "$name completed $cases/$expected required format/style cases"}
            $records+=@{name=$name;exit=$process.ExitCode;cases=$cases;log=$stdout}
            Write-Output "$name passed $cases cases"
        }
    }
    foreach($name in $names){if((Get-FileHash -LiteralPath (Join-Path $build $name)).Hash -ne $hashes[$name]){throw 'A tested binary changed during verification'}}
    @{complete=$true;binaries=$hashes;checks=$records;scope='Real D3D12/Vulkan native TAA components and game owner, four formats; mocked XR transport, not physical Leia or SDK edge correspondence; no isolated speed measurement'} |
        ConvertTo-Json -Depth 5 | Out-File -LiteralPath (Join-Path $output 'summary.json') -Encoding utf8
} catch {
    @{complete=$false;binaries=$hashes;checks=$records;error=$_.Exception.Message} |
        ConvertTo-Json -Depth 5 | Out-File -LiteralPath (Join-Path $output 'summary.json') -Encoding utf8
    throw
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_GPU_DRIVER$|SDL_AUDIODRIVER$|SDL_ASSERT$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $saved.GetEnumerator()){Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
