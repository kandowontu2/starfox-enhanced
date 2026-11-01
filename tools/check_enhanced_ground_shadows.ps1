param(
    [string]$Executable='build/current/starfox_pc.exe',
    [string]$OutputDirectory='build/validation/active',
    [ValidateSet(1,2,4)][int[]]$RenderScales=@(1,2),
    [ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL',
    [ValidateRange(0,1)][int]$RayTracing=1,
    [switch]$HostCartridge
)
# Compare the actual fallback and resident presentation paths with identical
# source geometry and CPU-reference ray masks. No ROMs or settings are copied.
$ErrorActionPreference='Stop'
$root=(Resolve-Path '.').Path
$exe=(Resolve-Path $Executable).Path
$output=[IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
$scratch=[IO.Path]::GetFullPath((Join-Path $root 'build/validation'))
if(!$output.StartsWith($scratch+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'OutputDirectory must be inside build/validation.'
}
$bytes=(Get-ChildItem -LiteralPath $scratch -File -Recurse -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum
$estimate=($RenderScales | ForEach-Object {400*224*$_*$_*20} | Measure-Object -Sum).Sum
if($bytes+$estimate -gt 1GB -or (Get-PSDrive C).Free-$estimate -lt 20GB) {
    throw 'Validation storage budget would be exceeded.'
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
if(!('EnhancedGroundShadowCompare' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class EnhancedGroundShadowCompare {
    public static int[] Compare(string first,string second) {
        var a=File.ReadAllBytes(first);var b=File.ReadAllBytes(second);
        int width=BitConverter.ToInt32(a,18),height=BitConverter.ToInt32(a,22);
        int channels=BitConverter.ToUInt16(a,28)/8;
        if(width!=BitConverter.ToInt32(b,18) || height!=BitConverter.ToInt32(b,22)
            || channels!=BitConverter.ToUInt16(b,28)/8 || (channels!=3 && channels!=4))
            throw new Exception("Capture dimensions/format differ");
        int ao=BitConverter.ToInt32(a,10),bo=BitConverter.ToInt32(b,10);
        int stride=(width*channels+3)&~3;
        int changed=0,failed=0,maximum=0;
        for(int y=0;y<Math.Abs(height);++y) for(int x=0;x<width;++x) {
            int p=ao+y*stride+x*channels,q=bo+y*stride+x*channels,delta=0;
            for(int c=0;c<3;++c) delta=Math.Max(delta,Math.Abs(a[p+c]-b[q+c]));
            if(delta!=0) ++changed;
            if(delta>1 || (channels==4 && a[p+3]!=b[q+3])) ++failed;
            maximum=Math.Max(maximum,delta);
        }
        return new[]{changed,failed,maximum};
    }
}
'@
}
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
$configHashes=@{}
foreach($config in @((Join-Path $root 'pregame.cfg'),(Join-Path (Split-Path $exe) 'pregame.cfg'))) {
    if(Test-Path -LiteralPath $config) {$configHashes[$config]=(Get-FileHash -LiteralPath $config).Hash}
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES='1'
        STARFOX_TEST_WINDOW_SIZE='800x450';STARFOX_TEST_DISPLAY_MODE='16_9'
        STARFOX_TEST_PRESENTATION_FPS='120';STARFOX_TEST_SOURCE_FRAME='1'
        STARFOX_TEST_EXPERIENCE=$Experience;STARFOX_TEST_GOD_MODE='1'
        STARFOX_TEST_PREROLL_TICKS='500';STARFOX_TEST_PREROLL_AUDIO='1'
        STARFOX_TEST_RAY_TRACING="$RayTracing";STARFOX_TEST_SOFTWARE_SHADOWS="$RayTracing"
        STARFOX_TEST_SHADOW_REFERENCE="$RayTracing";STARFOX_TRACE_GPU='1'
        STARFOX_TEST_EFFECT='0';STARFOX_TEST_WORLD_EFFECT='0';STARFOX_TEST_MANIPULATION='0'
        STARFOX_TEST_MATERIAL='0';STARFOX_TEST_MODEL_SMOOTHING='0';STARFOX_TEST_SEPARATED_MODELS='0'
        STARFOX_TEST_WORLD_DISTORTION='0';STARFOX_TEST_MODEL_FX='0';STARFOX_TEST_WORLD_FX='0'
        STARFOX_TEST_GLOBAL_ENHANCEMENTS='0';STARFOX_TEST_PHOSPHOR_PERSISTENCE='0'
        STARFOX_TEST_ADAPTIVE_EXPOSURE='0';STARFOX_TEST_REFLECTIVE_SURFACES='0'
        STARFOX_TEST_BLOOM='0';STARFOX_TEST_CHROMATIC_ABERRATION='0';STARFOX_TEST_HDR_EFFECT='0'
        STARFOX_TEST_RTX_LIGHTING='0';STARFOX_TEST_ANTI_ALIASING='OFF';STARFOX_TEST_2D_FILTER='OFF'
        STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_DLSS45_SELECTION='0';STARFOX_TEST_FSR1_SELECTION='0'
        STARFOX_TEST_STEREO_OUTPUT='0';STARFOX_TEST_MSU1='0';STARFOX_TEST_VSYNC='0'
        STARFOX_TEST_ENVIRONMENT_1='0';STARFOX_TEST_ENVIRONMENT_2='0'
        STARFOX_TEST_ENVIRONMENT_4='0';STARFOX_TEST_ENVIRONMENT_5='0'
    }
    foreach($key in $settings.Keys) {Set-Item "Env:$key" $settings[$key]}
    if($HostCartridge) {$env:STARFOX_TEST_HOST_CARTRIDGE='1'}
    foreach($scale in $RenderScales) {
        $env:STARFOX_TEST_RENDER_SCALE="$scale"
        foreach($enhanced in @(0,1)) {
            $env:STARFOX_TEST_ENVIRONMENT_0="$enhanced"
            $env:STARFOX_TEST_ENVIRONMENT_3="$enhanced"
            $captures=@{}
            foreach($renderer in @('SOFTWARE','GPU_FAST')) {
                $env:STARFOX_TEST_RENDERER=$renderer
                $stem="ground-shadow-$Experience-$scale-$enhanced-$renderer"
                $captures[$renderer]=Join-Path $output "$stem.bmp"
                $env:STARFOX_CAPTURE_PATH=$captures[$renderer]
                $log=Join-Path $output "$stem.log"
                $process=Start-Process -FilePath $exe -ArgumentList LEVEL1_1 -WindowStyle Hidden -PassThru -RedirectStandardError $log
                $handle=$process.Handle
                if(!$process.WaitForExit(30000)) {throw "Owned capture is still running: PID $($process.Id); inspect before retrying."}
                $process.Refresh()
                if($process.ExitCode -ne 0) {throw "Capture failed ($($process.ExitCode)): $log"}
                if($RayTracing -and !(Select-String -LiteralPath $log -Pattern 'shadow-receivers: ground=[1-9][0-9]* model=' -Quiet)) {
                    throw "Fixture did not exercise ground shadows: $log"
                }
                if($renderer -eq 'GPU_FAST' -and !(Select-String -LiteralPath $log -SimpleMatch 'native-pipeline: resident raster -> composition -> effects -> presentation' -Quiet)) {
                    throw "Fixture did not exercise resident GPU presentation: $log"
                }
            }
            $result=[EnhancedGroundShadowCompare]::Compare($captures.SOFTWARE,$captures.GPU_FAST)
            if($result[1]) {throw "Enhanced=$enhanced scale=${scale}: $($result[1]) pixels differ by more than 1 (max=$($result[2]))."}
            "PASS $Experience scale=$scale enhanced=${enhanced}: RT=$RayTracing CPU/resident GPU environment agrees (max channel delta=$($result[2]))."
        }
        $native=Join-Path $output "ground-shadow-$Experience-$scale-0-SOFTWARE.bmp"
        $enhanced=Join-Path $output "ground-shadow-$Experience-$scale-1-SOFTWARE.bmp"
        if([EnhancedGroundShadowCompare]::Compare($native,$enhanced)[0] -lt 1000*$scale*$scale) {
            throw 'Fixture did not visibly replace the environment.'
        }
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys) {Set-Item "Env:$key" $saved[$key]}
    foreach($config in $configHashes.Keys) {
        if(!(Test-Path -LiteralPath $config) -or (Get-FileHash -LiteralPath $config).Hash -ne $configHashes[$config]) {
            throw "Test changed settings: $config"
        }
    }
}
