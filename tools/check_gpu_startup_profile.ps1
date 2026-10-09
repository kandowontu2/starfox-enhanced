param([string]$Binary='build/release/starfox_pc.exe',
    [string]$OutputDirectory='tmp/gpu-startup-profile-check',
    [ValidateRange(1,240)][int]$Frames=64,
    [switch]$LowPowerGpu,
    [switch]$CheckResponsiveness,
    [switch]$BlockingGpuPreparation,
    [ValidateRange(0,20000)][int]$RendererShaderDelayMs=0,
    [ValidateRange(0,20000)][int]$RendererPipelineDelayMs=0,
    [ValidateRange(100,5000)][int]$MaxUnresponsiveMs=5000,
    [switch]$CaptureUnresponsiveStack,
    [ValidateSet('saved-profile-plain','saved-profile-preview','standard-profile-preview','native-profile-preview')]
    [string[]]$ScenarioNames=@(),
    [switch]$WithoutDlssRuntime,
    [switch]$StartGame,
    [ValidateRange(0,480)][int]$PresentationFps=0,
    [switch]$PresentPacing,
    [ValidateRange(15,600)][int]$TimeoutSeconds=60,
    [ValidateSet('All','NoReflections','NoRays','NoBloom','Bare')][string]$Isolation='All')
$ErrorActionPreference='Stop'
$exe=[IO.Path]::GetFullPath($Binary)
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output) {throw 'Choose a new output directory'}
$preferences=Join-Path ([IO.Path]::GetDirectoryName($exe)) 'pregame.cfg'
if(!(Test-Path -LiteralPath $preferences)) {throw 'This diagnostic requires the existing saved enhancement profile'}
$preferencesHash=(Get-FileHash -LiteralPath $preferences).Hash
$savedNeural=(Get-Content -LiteralPath $preferences) -match '^DLSS(?:45)?_MODE [1-4]$'
New-Item -ItemType Directory -Path $output | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$results=@()
if(-not ('StarfoxStartupWindowProbe' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Diagnostics;
using System.ComponentModel;
public static class StarfoxStartupWindowProbe {
    [StructLayout(LayoutKind.Sequential)] private struct Security {
        public int size; public IntPtr descriptor; public int inherit;
    }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] private struct Startup {
        public int size; public string reserved, desktop, title;
        public uint x,y,width,height,columns,rows,fill,flags;
        public ushort show,reservedSize; public IntPtr reservedData,input,output,error;
    }
    [StructLayout(LayoutKind.Sequential)] private struct Child {
        public IntPtr process,thread; public uint processId,threadId;
    }
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] private static extern IntPtr CreateFile(
        string name,uint access,uint share,ref Security security,uint creation,uint flags,IntPtr template);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] private static extern bool CreateProcess(
        string application,StringBuilder command,IntPtr processSecurity,IntPtr threadSecurity,
        bool inherit,uint flags,IntPtr environment,string directory,ref Startup startup,out Child child);
    [DllImport("kernel32.dll")] private static extern bool CloseHandle(IntPtr handle);
    public static Process Start(string exe,string output,string error) {
        // Direct inherited FILE handles, not PowerShell's redirected pipes.
        // SendMessageTimeout can otherwise stall the parent's pipe-draining
        // callbacks, block the child in WriteFile, and invent a GPU/UI stall.
        var security=new Security {size=Marshal.SizeOf(typeof(Security)),inherit=1};
        IntPtr input=IntPtr.Zero,stdout=IntPtr.Zero,stderr=IntPtr.Zero;
        Child child=new Child();
        try {
            input=CreateFile("NUL",0x80000000,3,ref security,3,0,IntPtr.Zero);
            stdout=CreateFile(output,0x40000000,1,ref security,2,0,IntPtr.Zero);
            stderr=CreateFile(error,0x40000000,1,ref security,2,0,IntPtr.Zero);
            if(input==new IntPtr(-1) || stdout==new IntPtr(-1) || stderr==new IntPtr(-1))
                throw new Win32Exception(Marshal.GetLastWin32Error());
            var startup=new Startup {size=Marshal.SizeOf(typeof(Startup)),flags=0x101,
                show=0,input=input,output=stdout,error=stderr};
            if(!CreateProcess(exe,new StringBuilder("\""+exe+"\""),IntPtr.Zero,IntPtr.Zero,
                true,0x08000000,IntPtr.Zero,null,ref startup,out child))
                throw new Win32Exception(Marshal.GetLastWin32Error());
            var process=Process.GetProcessById((int)child.processId);
            var retained=process.Handle; // Retain exit status after process exit.
            return process;
        } finally {
            foreach(var handle in new[] {input,stdout,stderr,child.process,child.thread})
                if(handle!=IntPtr.Zero && handle!=new IntPtr(-1)) CloseHandle(handle);
        }
    }
    private delegate bool EnumCallback(IntPtr window, IntPtr value);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumCallback callback, IntPtr value);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
    [DllImport("user32.dll", SetLastError=true)] private static extern IntPtr SendMessageTimeout(
        IntPtr window, uint message, UIntPtr wparam, IntPtr lparam, uint flags, uint timeout, out UIntPtr result);
    public static IntPtr Find(uint process) {
        IntPtr found=IntPtr.Zero;
        EnumWindows((window, unused) => {
            uint owner; GetWindowThreadProcessId(window,out owner);
            if(owner!=process) return true;
            var name=new StringBuilder(128); GetClassName(window,name,name.Capacity);
            if(name.ToString()!="SDL_app") return true;
            found=window; return false;
        },IntPtr.Zero);
        return found;
    }
    public static bool Responds(IntPtr window) {
        UIntPtr result;
        // WM_NULL is read-only. SMTO_BLOCK | SMTO_ABORTIFHUNG, bounded to 500ms.
        return SendMessageTimeout(window,0,UIntPtr.Zero,IntPtr.Zero,3,500,out result)!=IntPtr.Zero;
    }
}
'@
}
try {
    $scenarios=@(
        @{name='saved-profile-plain';preview=$false;model='saved'},
        @{name='saved-profile-preview';preview=$true;model='saved'},
        @{name='standard-profile-preview';preview=$true;model='standard'},
        @{name='native-profile-preview';preview=$true;model='off'})
    if($Isolation -ne 'All') {$scenarios=@(@{name="isolation-$Isolation";preview=$true;model='saved'})}
    elseif($ScenarioNames.Count) {$scenarios=@($scenarios | Where-Object {$_.name -in $ScenarioNames})}
    if($StartGame) {
        if($Isolation -ne 'All' -or $ScenarioNames.Count -and 'saved-profile-plain' -notin $ScenarioNames) {
            throw 'Start Game tests the real saved plain setup; use saved-profile-plain without isolation'
        }
        $scenarios=@(@{name='saved-profile-start-game';preview=$false;model='saved'})
    }
    foreach($scenario in $scenarios) {
        $directory=Join-Path $output $scenario.name
        New-Item -ItemType Directory -Path $directory | Out-Null
        # No enhancement overrides: exercise the owner's saved upscale, rays,
        # bloom, scenery, exposure and scene options together, not a clean preset.
        $settings=@{
            SDL_AUDIODRIVER='dummy';SDL_GPU_DRIVER='direct3d12';
            STARFOX_TEST_FRAMES="$Frames";STARFOX_TEST_HIDDEN='1';
            STARFOX_TEST_RENDERER='GPU';STARFOX_TEST_EXPERIENCE='ORIGINAL';
            STARFOX_TEST_UNPACED='1';STARFOX_TEST_VSYNC='0';
            STARFOX_TRACE_GPU='1';STARFOX_TRACE_GPU_RAYS='1';STARFOX_TRACE_PLAIN_UI='1';
            STARFOX_CAPTURE_PATH=(Join-Path $directory 'frame.bmp')
        }
        if($LowPowerGpu) {$settings.STARFOX_TEST_LOW_POWER_GPU='1'}
        if($PresentationFps) {$settings.STARFOX_TEST_PRESENTATION_FPS=[string]$PresentationFps}
        if($PresentPacing) {
            $settings.Remove('STARFOX_TEST_UNPACED')
            $settings.STARFOX_TEST_PRESENT_PACING='1'
        }
        if($StartGame) {
            # Start from the real setup, not a direct-stage shortcut. Fixed
            # raster progress keeps 60/120 tests reproducible without dropping
            # actual presentation pacing or changing source game speed.
            $settings.STARFOX_TEST_PRESSES='24:0x1000'
            $settings.STARFOX_TEST_PRESS_FRAMES='3'
            $settings.STARFOX_TEST_FIXED_RASTER='1'
            $settings.STARFOX_TEST_ENDING='1'
            $settings.STARFOX_TRACE_FPS='1'
            $settings.STARFOX_CAPTURE_DIR=Join-Path $directory 'transition'
            $settings.STARFOX_CAPTURE_START=[string]($Frames-1)
            $settings.STARFOX_CAPTURE_INTERVAL=[string]$Frames
        }
        if($BlockingGpuPreparation) {$settings.STARFOX_TEST_BLOCKING_GPU_PREPARE='1'}
        if($RendererShaderDelayMs) {$settings.STARFOX_TEST_RENDERER_SHADER_DELAY_MS=[string]$RendererShaderDelayMs}
        if($RendererPipelineDelayMs) {$settings.STARFOX_TEST_RENDERER_PIPELINE_DELAY_MS=[string]$RendererPipelineDelayMs}
        if($WithoutDlssRuntime) {
            $settings.STARFOX_DLSS_ADAPTER=Join-Path $directory 'intentionally-absent-adapter.dll'
            $settings.STARFOX_DLSS_BINARIES=$directory
        }
        if($Isolation -in 'NoReflections','NoRays','Bare') {$settings.STARFOX_TEST_REFLECTIVE_SURFACES='0'}
        if($Isolation -in 'NoRays','Bare') {$settings.STARFOX_TEST_RAY_TRACING='0'}
        if($Isolation -in 'NoBloom','Bare') {$settings.STARFOX_TEST_BLOOM='0';$settings.STARFOX_TEST_BLOOM_2D='0'}
        if($Isolation -eq 'Bare') {
            $settings.STARFOX_TEST_RENDER_SCALE='1';$settings.STARFOX_TEST_SCENE_ENHANCEMENTS='0'
            $settings.STARFOX_TEST_PARTICLE_ENHANCEMENTS='0';$settings.STARFOX_TEST_ADAPTIVE_EXPOSURE='0'
            $settings.STARFOX_TEST_CAMERA_RESPONSE='0';$settings.STARFOX_TEST_ENVIRONMENT_3='0'
        }
        if($scenario.preview) {
            $settings.STARFOX_TEST_MENU_PREVIEW='1'
            $settings.STARFOX_TEST_PREROLL_TICKS='1000'
            $settings.STARFOX_CAPTURE_LOADING_PATH=Join-Path $directory 'rendering.bmp'
        }
        if($scenario.model -ne 'saved') {
            $settings.STARFOX_TEST_DLSS_SELECTION=$(if($scenario.model -eq 'standard'){'1'}else{'0'})
            $settings.STARFOX_TEST_DLSS45_SELECTION='0'
        }
        $process=$null
        $log=Join-Path $directory 'runtime.log'
        $timer=[Diagnostics.Stopwatch]::StartNew()
        $responses=0;$timeouts=0;$maxResponse=0;$maxStall=0;$stallStarted=-1
        try {
            foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
            $process=[StarfoxStartupWindowProbe]::Start($exe,(Join-Path $directory 'sdk.log'),$log)
            $handle=$process.Handle
            $stackCaptured=$false
            if($CheckResponsiveness) {
                while(!$process.HasExited -and $timer.ElapsedMilliseconds -lt $TimeoutSeconds*1000) {
                    $windowHandle=[StarfoxStartupWindowProbe]::Find([uint32]$process.Id)
                    if($windowHandle -ne [IntPtr]::Zero) {
                        $probe=[Diagnostics.Stopwatch]::StartNew()
                        $responded=[StarfoxStartupWindowProbe]::Responds($windowHandle)
                        $probe.Stop();$maxResponse=[Math]::Max($maxResponse,$probe.ElapsedMilliseconds)
                        if($responded) {
                            ++$responses
                            if($stallStarted -ge 0) {$maxStall=[Math]::Max($maxStall,$timer.ElapsedMilliseconds-$stallStarted);$stallStarted=-1}
                        } elseif(!$process.HasExited) {
                            ++$timeouts
                            # A plain-menu bootstrap can stall before any model
                            # raster log exists. Capture that owner thread too;
                            # this opt-in diagnostic run is not a clean timing
                            # acceptance run because debugger work adds latency.
                            if($CaptureUnresponsiveStack -and !$stackCaptured) {
                                $stackCaptured=$true
                                & 'C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe' -pv -p $process.Id `
                                    -c '~0 kb; qd' *> (Join-Path $directory 'unresponsive-stack.log')
                            }
                            if($stallStarted -lt 0) {$stallStarted=$timer.ElapsedMilliseconds-$probe.ElapsedMilliseconds}
                            $maxStall=[Math]::Max($maxStall,$timer.ElapsedMilliseconds-$stallStarted)
                            if($maxStall -ge $MaxUnresponsiveMs) {throw "SDL window unresponsive for $maxStall ms during $($scenario.name): $log"}
                        }
                    }
                    if(!$process.HasExited) {Start-Sleep -Milliseconds 25}
                }
                if($responses -eq 0) {throw "No responsive SDL window was sampled in $($scenario.name)"}
            }
            $exitWait=if($CheckResponsiveness){1000}else{$TimeoutSeconds*1000}
            if(!$process.HasExited -and !$process.WaitForExit($exitWait)) {throw "GPU startup profile timed out: $($scenario.name) (see $log)"}
            $process.WaitForExit()
            if($process.ExitCode -ne 0) {throw "GPU startup profile failed ($($process.ExitCode)): $log"}
            $timer.Stop()
            if(!(Test-Path -LiteralPath (Join-Path $directory 'frame.bmp'))) {throw 'No completed frame capture'}
            $text=Get-Content -LiteralPath $log -Raw
            $preparation=[regex]::Match($text,'gpu-preparation: jobs=(\d+) event-pumps=(\d+)')
            if(!$preparation.Success) {throw 'GPU preparation diagnostic missing'}
            $preparationJobs=[int]$preparation.Groups[1].Value
            $preparationPumps=[int]$preparation.Groups[2].Value
            $presenter=[regex]::Matches($text,'gpu-renderer-prepare: end presenter-shaders success=1 ms=(\d+)')
            $presenterPipelines=[regex]::Matches($text,'gpu-renderer-prepare: end presenter-pipeline success=1 ms=(\d+)')
            if($RendererPipelineDelayMs) {
                if(!$presenterPipelines.Count -or [int]$presenterPipelines[0].Groups[1].Value -lt $RendererPipelineDelayMs) {
                    throw 'Requested long presenter-pipeline preparation did not execute'
                }
                if(!$BlockingGpuPreparation -and ($preparationJobs -lt 1 -or $preparationPumps -lt 10)) {
                    throw 'Presenter-pipeline preparation did not service owner events'
                }
            }
            if($RendererShaderDelayMs) {
                if(!$presenter.Count -or [int]$presenter[0].Groups[1].Value -lt $RendererShaderDelayMs) {
                    throw 'Requested long presenter-shader preparation did not execute'
                }
                if(!$BlockingGpuPreparation -and ($preparationJobs -lt 1 -or $preparationPumps -lt 10)) {
                    throw 'Presenter-shader preparation did not keep servicing owner events'
                }
            }
            if($text -notmatch 'mode=GPU actual=gpu driver=direct3d12') {throw 'Requested GPU did not start on D3D12'}
            $sdkShutdown=$text.LastIndexOf('dlss-lifecycle: shutdown before renderer destruction')
            if($sdkShutdown -ge 0 -and ($text.LastIndexOf('gpu-lifecycle: rendering resources released') -lt 0 -or
                $text.LastIndexOf('gpu-lifecycle: rendering resources released') -gt $sdkShutdown)) {
                throw 'DLSS shut down before app-owned GPU resources were released'
            }
            if($StartGame) {
                $plainFrames=[regex]::Matches($text,'plain-menu:').Count
                if($plainFrames -lt 24 -or $plainFrames -ge $Frames -or
                    $text -notmatch 'scripted-input: frame=\d+ pressed=4096' -or
                    $text -notmatch 'ending-frame \d+ flow=3 ' -or $text -notmatch 'native-pipeline:') {
                    throw 'Start Game did not leave the real setup and render the intro'
                }
                if($PresentationFps -and $text -notmatch "fps-matrix [^\r\n]+requested=$PresentationFps ") {
                    throw 'Start Game did not retain the requested presentation rate'
                }
            } elseif($scenario.preview) {
                if(!(Test-Path -LiteralPath (Join-Path $directory 'rendering.bmp'))) {throw 'Preview loading indicator missing'}
                if($text -notmatch 'native-pipeline:') {throw 'Saved preview profile did not render a scene'}
                if(!$BlockingGpuPreparation -and ($preparationJobs -lt 1 -or $preparationPumps -lt $preparationJobs)) {
                    throw 'Preview did not use joined responsive shader preparation'
                }
                $expectNeural=$scenario.model -eq 'standard' -or ($scenario.model -eq 'saved' -and $savedNeural)
                if($expectNeural -and $text -match 'dlss-lifecycle: actual game GPU bound' -and $text -notmatch 'dlss-gameplay: evaluated') {
                    throw 'Supported selected DLSS never reconstructed the preview'
                }
            } else {
                if([regex]::Matches($text,'plain-menu:').Count -ne $Frames) {throw 'Saved profile bypassed the plain menu'}
                if($text -match 'native-pipeline:|dlss-preview:|viewport configured|DXR rendering pipeline initialized') {
                    throw 'Saved enhancements rendered with Preview OFF'
                }
                # Availability probes may prepare an adapter, but a plain menu
                # must not compile any game/effect shader or ray pipeline.
                if($text -match 'gpu-prepare-resource:|DXR rendering pipeline initialized') {
                    throw 'Shader preparation ran with Preview OFF'
                }
            }
            $results+=@{name=$scenario.name;elapsed_ms=$timer.ElapsedMilliseconds;
                adapter=([regex]::Match($text,'test-gpu-adapter: ([^\r\n]+)').Groups[1].Value);
                dlss_evaluations=[regex]::Matches($text,'dlss-gameplay: evaluated').Count;
                menu_uploads=[regex]::Matches($text,'plain-ui-texture: uploaded').Count;
                menu_retained=[regex]::Matches($text,'plain-ui-texture: retained').Count;
                responsiveness_checked=[bool]$CheckResponsiveness;responsive_samples=$responses;
                preparation_jobs=$preparationJobs;preparation_event_pumps=$preparationPumps;
                presenter_shader_preparations=$presenter.Count;
                presenter_pipeline_preparations=$presenterPipelines.Count;
                shader_resources=[regex]::Matches($text,'gpu-prepare-resource: begin').Count;
                response_timeouts=$timeouts;max_response_ms=$maxResponse;max_unresponsive_streak_ms=$maxStall;
                frame_sha256=(Get-FileHash -LiteralPath (Join-Path $directory 'frame.bmp')).Hash}
        } catch {
            # Retain failed-probe metadata even if no frame completed.
            $failureText=if(Test-Path -LiteralPath $log) {Get-Content -LiteralPath $log -Raw}else{''}
            $lastStage=[regex]::Matches($failureText,'gpu-startup: (?:begin|end) [^\r\n]+')
            @{executable=$exe;sha256=(Get-FileHash -LiteralPath $exe).Hash;
                scenario=$scenario.name;error=$_.Exception.Message;
                elapsed_ms=$timer.ElapsedMilliseconds;responsive_samples=$responses;
                response_timeouts=$timeouts;max_unresponsive_streak_ms=$maxStall;
                max_unresponsive_limit_ms=$MaxUnresponsiveMs;
                start_game=[bool]$StartGame;presentation_fps=$PresentationFps;
                present_pacing=[bool]$PresentPacing;timeout_seconds=$TimeoutSeconds;
                renderer_shader_delay_ms=$RendererShaderDelayMs;
                renderer_pipeline_delay_ms=$RendererPipelineDelayMs;
                blocking_gpu_preparation=[bool]$BlockingGpuPreparation;
                last_startup_stage=$(if($lastStage.Count){$lastStage[$lastStage.Count-1].Value}else{''});
                runtime_log=$log;preferences_sha256=$preferencesHash;
                preferences_unchanged=((Get-FileHash -LiteralPath $preferences).Hash -eq $preferencesHash)} |
                ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $directory 'failure.json')
            throw
        } finally {
            if($process -and !$process.HasExited) {
                # The finite fixture can exit between HasExited and Stop-Process.
                # Do not replace the actual test failure with a missing-PID error.
                try {Stop-Process -Id $process.Id -ErrorAction Stop}
                catch {if(!$process.WaitForExit(1000)) {throw}}
                $process.WaitForExit()
            }
            foreach($key in $settings.Keys) {Remove-Item -LiteralPath "Env:$key" -ErrorAction SilentlyContinue}
        }
    }
    if((Get-FileHash -LiteralPath $preferences).Hash -ne $preferencesHash) {throw 'Saved preferences changed'}
    @{executable=$exe;sha256=(Get-FileHash -LiteralPath $exe).Hash;
        low_power_gpu=[bool]$LowPowerGpu;
        responsiveness_checked=[bool]$CheckResponsiveness;
        blocking_gpu_preparation=[bool]$BlockingGpuPreparation;
        renderer_shader_delay_ms=$RendererShaderDelayMs;
        renderer_pipeline_delay_ms=$RendererPipelineDelayMs;
        start_game=[bool]$StartGame;presentation_fps=$PresentationFps;
        present_pacing=[bool]$PresentPacing;timeout_seconds=$TimeoutSeconds;
        max_unresponsive_limit_ms=$MaxUnresponsiveMs;
        diagnostic_stack_capture=[bool]$CaptureUnresponsiveStack;
        without_dlss_runtime=[bool]$WithoutDlssRuntime;
        isolation=$Isolation;
        preferences_sha256=$preferencesHash;preferences_unchanged=$true;results=$results} |
        ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'results.json')
    $results | ForEach-Object {[pscustomobject]$_} | Format-Table name,elapsed_ms,dlss_evaluations,menu_uploads,menu_retained -AutoSize
    Write-Output "PASS: GPU startup profile ($Isolation); selected transitions completed; preferences unchanged."
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} |
        ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
