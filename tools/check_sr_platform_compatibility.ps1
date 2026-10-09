param([Parameter(Mandatory)][string]$Binary,
    [Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$binaryPath=[IO.Path]::GetFullPath($Binary)
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output) {throw 'Use a fresh output directory'}
New-Item -ItemType Directory -Path $output | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$results=@()
try {
    foreach($experience in 'ORIGINAL','EX') {
        foreach($choice in 'auto','direct3d12','vulkan','mono') {
            $directory=Join-Path $output "$experience-$choice"
            $driver=if($choice -eq 'auto'){''}elseif($choice -eq 'mono'){'direct3d12'}else{$choice}
            $sr=$choice -ne 'mono'
            & "$PSScriptRoot/capture_lava.ps1" -Binary $binaryPath -OutputDirectory $directory `
                -GpuBackend $driver -Stereo $(if($sr){9}else{0}) -ExpectSrUnavailable:$sr `
                -Experience $experience -Stage LEVEL1_1 -Frames 8 -CaptureFirst 8 -CaptureInterval 8 `
                -PrerollTicks 120 -RenderScale 1 -GroundEnabled 0 -RayTracing 0 -Reflections 0 -MenuPreview
            $log=Get-Content -LiteralPath (Join-Path $directory 'runtime.log') -Raw
            $expected=if($choice -eq 'vulkan'){'vulkan'}else{'direct3d12'}
            $created=[regex]::Matches($log,'renderer-picker: requested=AUTO mode=GPU actual=gpu driver=(\w+)')
            if($created.Count -ne 1 -or $created[0].Groups[1].Value -ne $expected) {
                throw "Incorrect/repeated startup backend: $experience-$choice"
            }
            if($sr) {
                $reason=if($choice -eq 'vulkan'){'D3D12 is required;'}else{'Cannot load SimulatedRealityDirectX.dll'}
                if([regex]::Matches($log,'SR Platform unavailable:').Count -ne 1 -or $log -notmatch [regex]::Escape($reason)) {
                    throw "Missing/repeated exact unavailable reason: $experience-$choice"
                }
            }
            $results+=@{experience=$experience;choice=$choice;driver=$expected;renderer_creations=$created.Count;
                image_sha256=(Get-FileHash -LiteralPath (Join-Path $directory 'lava.bmp')).Hash}
        }
        $scene=@($results | Where-Object {$_.experience -eq $experience})
        $mono=($scene | Where-Object {$_.choice -eq 'mono'}).image_sha256
        foreach($sr in $scene | Where-Object {$_.choice -in 'auto','direct3d12'}) {
            if($sr.image_sha256 -ne $mono) {throw "Unavailable SR changed the mono image: $experience-$($sr.choice)"}
        }
        $directory=Join-Path $output "$experience-plain-auto"
        New-Item -ItemType Directory -Path $directory | Out-Null
        $settings=@{
            DISABLE_VK_LAYER_reshade_1='1';SDL_AUDIODRIVER='dummy';
            STARFOX_TEST_FRAMES='40';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_UNPACED='1';STARFOX_TEST_VSYNC='0';
            STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_EXPERIENCE=$experience;STARFOX_TEST_RENDERER='GPU';
            STARFOX_TEST_STEREO_OUTPUT='9';STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_DLSS45_SELECTION='0';
            STARFOX_TEST_RENDER_SCALE='6';STARFOX_TEST_RAY_TRACING='1';STARFOX_TEST_REFLECTIVE_SURFACES='3';
            STARFOX_TRACE_PLAIN_UI='1';STARFOX_TRACE_GPU='1';STARFOX_TRACE_GPU_RAYS='1';STARFOX_TRACE_SCENE_FX='1'
        }
        try {
            foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
            $logPath=Join-Path $directory 'runtime.log'
            $process=Start-Process -FilePath $binaryPath -WindowStyle Hidden -PassThru -RedirectStandardError $logPath
            $handle=$process.Handle
            while(!$process.WaitForExit(30000)) {Write-Output "Plain SR menu check running: PID $($process.Id), $logPath"}
            if($process.ExitCode) {throw "Plain SR menu check failed: $logPath"}
            $log=Get-Content -LiteralPath $logPath -Raw
            if([regex]::Matches($log,'plain-menu: frame=\d+ scale=1 effects=0 scene=0').Count -ne 40 -or
                $log -match 'SR Platform unavailable:|SR Platform weaver initialized|viewport configured|native-pipeline:|ray-water:|DXR rendering pipeline initialized') {
                throw "Preview OFF performed native/scene work: $experience"
            }
            if([regex]::Matches($log,'renderer-picker: requested=AUTO mode=GPU actual=gpu driver=direct3d12').Count -ne 1) {
                throw "SR Preview OFF recreated/used incorrect backend: $experience"
            }
            $results+=@{experience=$experience;choice='plain-auto';frames=40;scene_or_sdk_work=$false}
        } finally {
            foreach($key in $settings.Keys) {Remove-Item -LiteralPath "Env:$key" -ErrorAction SilentlyContinue}
        }
    }
    $startupLog=Join-Path ([IO.Path]::GetDirectoryName($binaryPath)) 'startup.log'
    if(!(Select-String -LiteralPath $startupLog -SimpleMatch 'SR Platform unavailable: Cannot load SimulatedRealityDirectX.dll' -Quiet)) {
        throw 'Unavailable SDK reason was not persisted in startup.log'
    }
    $results | ForEach-Object {[pscustomobject]$_} | Format-Table -AutoSize
    Write-Output 'PASS: Auto D3D12, explicit backends, exact mono fallback, persisted reasons, Preview-OFF isolation. No physical SR panel claim.'
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$|DISABLE_VK_LAYER_reshade_1$)'} |
        ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
