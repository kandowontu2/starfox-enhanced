param([string]$Binary='build/release/starfox_pc.exe',
    [string]$OutputDirectory='tmp/sparse-scene-timing',
    [ValidateSet('direct3d12','vulkan')][string]$GpuBackend='vulkan',
    [ValidateRange(120,600)][int]$Frames=480,
    [ValidateRange(1,5)][int]$Runs=3,
    [ValidateSet('SparseModel','Dedither','OccupiedTiles')][string]$Comparison='SparseModel',
    [string[]]$CaseNames=@('plain1','plain4','blur2','lava2'))
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output){throw 'Choose a new directory; timing results will not be overwritten'}
New-Item -ItemType Directory -Path $output | Out-Null
$binaryPath=[IO.Path]::GetFullPath($Binary)
$binaryHash=(Get-FileHash -LiteralPath $binaryPath).Hash
$preferences=Join-Path ([IO.Path]::GetDirectoryName($binaryPath)) 'pregame.cfg'
$before=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
$cases=@{
    plain1=@{Experiences=@('ORIGINAL');Levels=@('LEVEL1_1');RenderScale=1;PrerollTicks=1000}
    plain4=@{Experiences=@('ORIGINAL');Levels=@('LEVEL1_1');RenderScale=4;PrerollTicks=1000}
    blur2=@{Experiences=@('ORIGINAL');Levels=@('LEVEL1_1');RenderScale=2;PrerollTicks=1000;MotionBlurQuality=2}
    lava2=@{Experiences=@('EX');Levels=@('LEVEL6_6');RenderScale=2;PrerollTicks=120;
        EnhancedGround=$true;GroundMaterial=9;EnhancedSky=$true;MotionBlurQuality=2}
}
$results=@()
foreach($caseName in $CaseNames){
    if(!$cases.ContainsKey($caseName)){throw "Unknown timing case: $caseName"}
    for($run=1;$run -le $Runs;++$run){
        # Interleave paths and reverse their order; change only the selected
        # optimization. Shared uploads, direct eyes and motion stay on in both.
        $paths=switch($Comparison){'SparseModel'{@('copy','sparse')};'Dedither'{@('generic','slim')};'OccupiedTiles'{@('dense','occupied')}}
        $order=if($run%2){$paths}else{@($paths[1],$paths[0])}
        foreach($path in $order){
            $name="$caseName-$path-$run"
            $directory=Join-Path $output $name
            $driverLog=Join-Path $output "$name-driver.log"
            $settings=@{Executable=$binaryPath;OutputDirectory=$directory;Frames=$Frames;Fps=60;
                GpuDriver=$GpuBackend;Renderers=@('GPU');Stereo=2;GodMode=$true;
                CopyModelBackgrounds=($path -eq 'copy');GenericDedither=($path -eq 'generic');FullTileDispatch=($path -eq 'dense');OccupiedTiles=($path -eq 'occupied')}
            foreach($entry in $cases[$caseName].GetEnumerator()){$settings[$entry.Key]=$entry.Value}
            & (Join-Path $PSScriptRoot 'benchmark_native_defaults.ps1') @settings *> $driverLog
            if(!$?){throw "Timing run failed: $driverLog"}
            $metric=Select-String -LiteralPath $driverLog -Pattern '^frame-work-distribution-us median=(\d+) p95=(\d+) '
            if(@($metric).Count -ne 1){throw "Missing/ambiguous frame-work metrics: $driverLog"}
            $results+=@{case=$caseName;path=$path;run=$run;median_us=[int64]$metric.Matches[0].Groups[1].Value;
                p95_us=[int64]$metric.Matches[0].Groups[2].Value;frames=$Frames;log=$driverLog}
            Write-Output "$name median=$($results[-1].median_us) us p95=$($results[-1].p95_us) us"
        }
    }
}
$after=if(Test-Path -LiteralPath $preferences){(Get-FileHash -LiteralPath $preferences).Hash}else{''}
if($before -ne $after){throw 'Saved preferences changed during timing'}
if((Get-FileHash -LiteralPath $binaryPath).Hash -ne $binaryHash){throw 'Executable changed during timing'}
@{sha256=$binaryHash;backend=$GpuBackend;comparison=$Comparison;warmup_frames=60;
    preferences_unchanged=$true;results=$results} | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'Quiet timing completed: full stereo pairs verified; no per-pass trace or capture readback.'
