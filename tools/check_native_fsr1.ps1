param([string]$BuildDirectory='build/current',
    [string]$OutputDirectory='tmp/native-fsr1-check')
$ErrorActionPreference='Stop'
$build=[IO.Path]::GetFullPath($BuildDirectory)
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a new evidence directory; existing results will not be overwritten'}
New-Item -ItemType Directory -Path $output | Out-Null
$names=@('starfox_pc.exe','starfox_calibrated_ray_composite_check.exe','starfox_displayxr_gpu_presenter_check.exe')
$hashes=@{}
foreach($name in $names){$hashes[$name]=(Get-FileHash -LiteralPath (Join-Path $build $name)).Hash}
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_GPU_DRIVER$|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$records=@()
try {
    foreach($backend in 'direct3d12','vulkan') {
        foreach($kind in 'component','owner') {
            $name="$backend-$kind"
            $binary=Join-Path $build $(if($kind -eq 'component'){'starfox_calibrated_ray_composite_check.exe'}else{'starfox_displayxr_gpu_presenter_check.exe'})
            $arguments=if($kind -eq 'component'){@($backend,'--fsr-only')}elseif($backend -eq 'vulkan'){@('--vulkan','--fsr-only')}else{@('--fsr-only')}
            $stdout=Join-Path $output "$name.log";$stderr=Join-Path $output "$name.err"
            $process=Start-Process -FilePath $binary -ArgumentList $arguments -WorkingDirectory $build -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput $stdout -RedirectStandardError $stderr
            $handle=$process.Handle
            while(!$process.WaitForExit(30000)){Write-Output "$name is still running (PID $($process.Id))"}
            $process.WaitForExit()
            if($process.ExitCode -ne 0){throw "$name failed with exit $($process.ExitCode); see $stderr and $stdout"}
            $log=Get-Content -LiteralPath $stdout -Raw
            $pattern=if($kind -eq 'component'){'Native FSR format \d+:'}else{'native FSR owner: 20 mode/AA/ray/effect combinations;'}
            $formats=([regex]::Matches($log,$pattern)).Count
            if($formats -ne 4){throw "$name did not complete all four negotiated colour formats"}
            $records+=@{name=$name;exit=$process.ExitCode;formats=$formats;log=$stdout}
            Write-Output "$name passed all four formats"
        }
    }
    foreach($name in $names){if((Get-FileHash -LiteralPath (Join-Path $build $name)).Hash -ne $hashes[$name]){throw 'A tested binary changed during verification'}}
    @{complete=$true;binaries=$hashes;checks=$records;scope='Real D3D12/Vulkan scalar-EASU/RCAS component and lower-resolution native eye owner; mocked XR compositor, not physical Leia or timing acceptance'} |
        ConvertTo-Json -Depth 5 | Out-File -LiteralPath (Join-Path $output 'summary.json') -Encoding utf8
} catch {
    @{complete=$false;binaries=$hashes;checks=$records;error=$_.Exception.Message} |
        ConvertTo-Json -Depth 5 | Out-File -LiteralPath (Join-Path $output 'summary.json') -Encoding utf8
    throw
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_GPU_DRIVER$|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $saved.GetEnumerator()){Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
