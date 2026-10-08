param([string]$Binary='build/current/starfox_pc.exe',
      [string]$OutputDirectory='tmp/sr-platform-fallback')
$ErrorActionPreference='Stop'
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $output) {throw 'Choose a new output directory; existing results will not be overwritten'}
New-Item -ItemType Directory -Path $output | Out-Null
$exe=[IO.Path]::GetFullPath($Binary)
$config=Join-Path (Split-Path $exe -Parent) 'pregame.cfg'
$before=if(Test-Path -LiteralPath $config){(Get-FileHash -LiteralPath $config).Hash}else{''}
$binaryHash=(Get-FileHash -LiteralPath $exe).Hash
$adapter=Join-Path (Split-Path $exe -Parent) 'starfox_leia_sr.dll'
$adapterHash=if(Test-Path -LiteralPath $adapter){(Get-FileHash -LiteralPath $adapter).Hash}else{''}
$cases=@()
foreach($experience in 'ORIGINAL','EX') {
    foreach($driver in 'direct3d12','vulkan') {
        $images=@()
        foreach($mode in 0,9) {
            $directory=Join-Path $output "$experience-$driver-$mode"
            $args=@{Binary=$exe;OutputDirectory=$directory;Experience=$experience;Stage='LEVEL1_1';
                GpuBackend=$driver;Stereo=$mode;Frames=8;CaptureInterval=1;PrerollTicks=1000;
                GroundEnabled=0;RayTracing=0;Reflections=0;RenderScale=1;HideFps=$true;
                FixedTemporalClock=$true}
            if($mode -eq 9) {$args.ExpectSrUnavailable=$true}
            & (Join-Path $PSScriptRoot 'capture_lava.ps1') @args
            $images+=,(Get-ChildItem -LiteralPath $directory -Filter '*.bmp' | Sort-Object Name)
        }
        if(!$images[0].Count -or $images[0].Count -ne $images[1].Count) {throw 'Missing fallback image sequence'}
        for($i=0;$i -lt $images[0].Count;++$i) {
            if((Get-FileHash -LiteralPath $images[0][$i].FullName).Hash -ne
               (Get-FileHash -LiteralPath $images[1][$i].FullName).Hash) {
                throw "SR unavailable fallback changed ordinary mono output: $experience $driver image $i"
            }
        }
        $cases+=@{experience=$experience;backend=$driver;exact_images=$images[0].Count}
    }
}
$after=if(Test-Path -LiteralPath $config){(Get-FileHash -LiteralPath $config).Hash}else{''}
if($before -ne $after){throw 'SR fallback launch overwrote saved preferences'}
if($binaryHash -ne (Get-FileHash -LiteralPath $exe).Hash){throw 'Executable changed during the fallback matrix'}
$adapterAfter=if(Test-Path -LiteralPath $adapter){(Get-FileHash -LiteralPath $adapter).Hash}else{''}
if($adapterHash -ne $adapterAfter){throw 'Adapter changed during the fallback matrix'}
@{complete=$true;binary_sha256=$binaryHash;adapter_sha256=$adapterHash;cases=$cases;preferences_unchanged=$true;
  physical_sr_validated=$false} | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $output 'results.json')
Write-Output 'PASS: unavailable SR Platform preserves exact mono images on D3D12/Vulkan in Original/EX; settings unchanged.'
