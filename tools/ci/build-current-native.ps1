param(
    [Parameter(Mandatory=$true)][string]$Dxc,
    [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$ciRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$ciResult=[IO.Path]::GetFullPath($Output)
$ciBuild=Join-Path $env:RUNNER_TEMP 'current-native-build'
$ciPins=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'native-source-manifest.json') -Raw|ConvertFrom-Json
$ciCompiler='C:/Strawberry/c/bin/g++.exe'
if(Test-Path -LiteralPath $ciResult){throw 'Preserve prior native build evidence'}
[void](New-Item -ItemType Directory -Path $ciResult)
$ciReceipt=[ordered]@{commit=$env:GITHUB_SHA;run_id=$env:GITHUB_RUN_ID;run_attempt=$env:GITHUB_RUN_ATTEMPT;
    target='starfox_displayxr_gpu_presenter_check';sources=$ciPins.sources.Count;operations=@();status='preflight';
    guards=@{single_worker=$true;launch_physical_kib=6291456;launch_virtual_kib=2097152;
        live_physical_floor_kib=6291456;live_virtual_floor_kib=2097152;owned_private_cap=2147483648;
        elapsed_native_kill=$false};
    full_fixture_flag='--source-motion-owner-only';gpu_executed=$false;gpu_accepted=$false;
    private_assets_present=$false;player_built=$false;release_published=$false;error=$null;outputs=@()}
function Save-CiReceipt {
    $ciReceipt|ConvertTo-Json -Depth 12|Out-File -LiteralPath (Join-Path $ciResult 'build-receipt.json') -Encoding utf8
}
function Assert-CiSources {
    foreach($ciPin in $ciPins.sources){
        $ciPath=[IO.Path]::GetFullPath((Join-Path $ciRoot $ciPin.path))
        if(!$ciPath.StartsWith($ciRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {throw 'Public source escaped checkout'}
        $ciText=[Text.Encoding]::Latin1.GetString([IO.File]::ReadAllBytes($ciPath)).Replace("`r`n","`n")
        $ciBytes=[Text.Encoding]::Latin1.GetBytes($ciText)
        if($ciBytes.Length -ne $ciPin.bytes -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($ciBytes)).ToLowerInvariant() -cne $ciPin.sha256){throw ('Public source changed: '+$ciPin.path)}
    }
    if(@(Get-ChildItem -LiteralPath $ciRoot -Recurse -File | Where-Object {$_.Extension -match '^\.(sfc|smc|bin|rom|jks|keystore|pfx|p12|ipa|apk|exe|dll)$' -and !$_.FullName.Contains('\.git\')}).Count){throw 'Unexpected private/binary input in source-only checkout'}
}
function Invoke-CiOwned([string]$Name,[string]$Program,[string[]]$Arguments){
    $Program=[IO.Path]::GetFullPath($Program)
    Assert-CiSources
    $ciMemory=Get-CimInstance Win32_OperatingSystem
    if($ciMemory.FreePhysicalMemory -lt 6291456 -or $ciMemory.FreeVirtualMemory -lt 2097152){throw 'Original compiler launch headroom unavailable; no process launched'}
    $ciInfo=[Diagnostics.ProcessStartInfo]::new($Program)
    $ciInfo.WorkingDirectory=$ciRoot;$ciInfo.UseShellExecute=$false;$ciInfo.CreateNoWindow=$true
    $ciInfo.RedirectStandardOutput=$true;$ciInfo.RedirectStandardError=$true
    foreach($ciArg in $Arguments){[void]$ciInfo.ArgumentList.Add($ciArg)}
    $ciNative=[Diagnostics.Process]::Start($ciInfo)
    $ciBirth=$ciNative.StartTime.ToUniversalTime()
    $ciOut=$ciNative.StandardOutput.ReadToEndAsync();$ciErr=$ciNative.StandardError.ReadToEndAsync()
    $ciRecord=[ordered]@{name=$Name;program=$Program;arguments=$Arguments;pid=$ciNative.Id;birth_utc=$ciBirth.ToString('o');
        exit_code=$null;peak_private_bytes=0L;resource_stop=$null;completed=$false;cap_workers=@()}
    try {
        while(!$ciNative.WaitForExit(1000)){
            $ciMemory=Get-CimInstance Win32_OperatingSystem
            $ciNative.Refresh();if($ciNative.HasExited){break}
            $ciLiveRoot=Get-Process -Id $ciNative.Id
            if($ciLiveRoot.StartTime.ToUniversalTime() -ne $ciBirth -or $ciLiveRoot.Path -ine $Program){throw 'Owned compiler identity changed'}
            $ciPrivate=[long]$ciLiveRoot.PrivateMemorySize64
            $ciChildren=@(Get-CimInstance Win32_Process)
            $ciParents=[Collections.Generic.HashSet[int]]::new();[void]$ciParents.Add($ciNative.Id)
            $ciNew=$true
            while($ciNew){
                $ciNew=$false
                foreach($ciChild in $ciChildren){
                    if($ciParents.Contains([int]$ciChild.ParentProcessId) -and !$ciParents.Contains([int]$ciChild.ProcessId)){
                        $ciLive=Get-Process -Id $ciChild.ProcessId -ErrorAction SilentlyContinue
                        if(!$ciLive){continue}
                        if($ciLive.StartTime.ToUniversalTime() -lt $ciBirth){throw 'Older descendant in owned compiler graph'}
                        $ciPrivate+=[long]$ciLive.PrivateMemorySize64
                        [void]$ciParents.Add([int]$ciChild.ProcessId);$ciNew=$true
                    }
                }
            }
            $ciRecord.peak_private_bytes=[Math]::Max($ciRecord.peak_private_bytes,$ciPrivate)
            if($ciPrivate -gt 2147483648){
                $ciRecord.resource_stop='Original owned compiler private cap above 2 GiB'
                $ciRecord.cap_workers=@($ciChildren|Where-Object {$ciParents.Contains([int]$_.ProcessId)}|ForEach-Object {
                    $ciObserved=Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue
                    if($ciObserved){[ordered]@{pid=$ciObserved.Id;name=$ciObserved.ProcessName;private_bytes=$ciObserved.PrivateMemorySize64;
                        source_names=@([regex]::Matches($_.CommandLine,'[A-Za-z0-9_]+\.(?:cpp|c|hlsl)\b')|ForEach-Object {$_.Value}|Sort-Object -Unique)}}
                })
            }
            elseif($ciMemory.FreePhysicalMemory -lt 6291456 -or $ciMemory.FreeVirtualMemory -lt 2097152){$ciRecord.resource_stop='Original live compiler RAM/commit floor'}
            if($ciRecord.resource_stop){$ciNative.Kill($true);$ciNative.WaitForExit();break}
        }
    } catch {
        $ciRecord.resource_stop='Owned compiler observation failed closed: '+$_.Exception.Message
        $ciNative.Refresh();if(!$ciNative.HasExited){$ciNative.Kill($true);$ciNative.WaitForExit()}
    } finally {
        $ciNative.Refresh()
        if(!$ciNative.HasExited){$ciNative.Kill($true);$ciNative.WaitForExit()}
        $ciRecord.exit_code=$ciNative.ExitCode;$ciRecord.completed=$true
        $ciOut.GetAwaiter().GetResult()|Out-File -LiteralPath (Join-Path $ciResult ($Name+'.stdout.log')) -Encoding utf8
        $ciErr.GetAwaiter().GetResult()|Out-File -LiteralPath (Join-Path $ciResult ($Name+'.stderr.log')) -Encoding utf8
        $ciReceipt.operations+=,$ciRecord;Save-CiReceipt
        $ciNative.Dispose()
    }
    if($ciRecord.resource_stop -or $ciRecord.exit_code -ne 0){throw ('Native '+$Name+' did not pass; preserve its actual terminal result')}
    Assert-CiSources
}
try {
    Assert-CiSources
    if((Get-FileHash -LiteralPath $Dxc).Hash -ne '980A3A4C6E5C88F5737DDE321E548860021D58FA2349790D4E572805CB298293' -or
       (Get-FileHash -LiteralPath (Join-Path (Split-Path $Dxc) 'dxcompiler.dll')).Hash -ne '9A5100511E127C6A2FC78EDF984F95074A76D35B90C90C4D342430A5AE160E9B'){throw 'DXC identity mismatch'}
    $ciReceipt.compiler_sha256=(Get-FileHash -LiteralPath $ciCompiler).Hash
    $ciReceipt.compiler_version=@(& $ciCompiler --version)
    if($LASTEXITCODE -ne 0 -or ($ciReceipt.compiler_version -join "`n") -notmatch 'g\+\+.*\b1[3-9]\.'){throw 'Need GCC 13 or newer; do not change compiler family silently'}
    $ciCmake=(Get-Command cmake.exe).Source
    $ciConfig=@('-S',$ciRoot,'-B',$ciBuild,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
        '-DCMAKE_C_COMPILER=C:/Strawberry/c/bin/gcc.exe',('-DCMAKE_CXX_COMPILER='+$ciCompiler),
        '-DCMAKE_CXX_FLAGS=--param=ggc-min-expand=10 --param=ggc-min-heapsize=16384',
        ('-DCMAKE_PROJECT_INCLUDE='+$ciRoot.Replace('\','/')+'/tools/ci/external-dxr-payloads.cmake'),
        '-DSTARFOX_BUILD_RUNTIME=ON','-DSTARFOX_BUILD_TOOLS=ON','-DSTARFOX_BUILD_TESTS=ON',
        '-DSTARFOX_BUILD_VR=OFF','-DSTARFOX_ENABLE_DISPLAYXR=ON','-DSTARFOX_ENABLE_LEIASR=OFF',
        '-DSTARFOX_EMBED_RUNTIME_ASSETS=OFF','-DSTARFOX_PACKAGE_MSU1_MUSIC=OFF','-DSTARFOX_DLSS_RUNTIME_DIRECTORY=',
        '-DSTARFOX_ROM_FILE=source-only-no-rom.sfc','-DSTARFOX_EX_ROM_FILE=source-only-no-ex-rom.sfc',
        '-DSTARFOX_RETAIL_V12_FILE=source-only-no-retail.sfc',('-DSTARFOX_DXC='+[IO.Path]::GetFullPath($Dxc)))
    $ciReceipt.embedding_candidate='All 22 original DXR shader arrays: byte-exact COFF data objects, original fixed-size declarations and original DXC commands'
    Invoke-CiOwned 'configure' $ciCmake $ciConfig
    $ciCommands=Get-Content -LiteralPath (Join-Path $ciBuild 'compile_commands.json') -Raw|ConvertFrom-Json
    $ciFixture=@($ciCommands|Where-Object {$_.file.Replace('\','/').EndsWith('/tests/displayxr_runtime_tests.cpp') -and $_.command -match 'starfox_displayxr_gpu_presenter_check\.dir'})
    if($ciFixture.Count -ne 1 -or $ciFixture[0].command -notmatch 'STARFOX_DISPLAYXR_GPU_FIXTURE=1' -or
       $ciFixture[0].command -notmatch 'STARFOX_CURVED_OWNER_TRACE_AVAILABLE=1' -or $ciFixture[0].command -notmatch '-O3\b') {throw 'Correct full moving fixture/macros/Release optimization missing'}
    $ciReceipt.full_fixture_compile_command=$ciFixture[0].command;Save-CiReceipt
    Invoke-CiOwned 'build' $ciCmake @('--build',$ciBuild,'--target','starfox_displayxr_gpu_presenter_check','--parallel','1')
    $ciExe=Join-Path $ciBuild 'starfox_displayxr_gpu_presenter_check.exe'
    if(!(Test-Path -LiteralPath $ciExe) -or (Test-Path -LiteralPath (Join-Path $ciBuild 'starfox_pc.exe'))){throw 'Missing diagnostic or unexpected player build'}
    $ciFiles=@($ciExe)
    # The target's original link is dynamic MinGW. Include only its GCC runtime,
    # not a player, proprietary graphics runtime, ROM, or fetched source tree.
    foreach($ciDll in @('libstdc++-6.dll','libgcc_s_seh-1.dll','libwinpthread-1.dll')){
        $ciRuntime=Join-Path (Split-Path $ciCompiler) $ciDll
        if(Test-Path -LiteralPath $ciRuntime){$ciFiles+=,$ciRuntime}
    }
    $ciTotal=[long](($ciFiles|ForEach-Object {(Get-Item -LiteralPath $_).Length}|Measure-Object -Sum).Sum)
    if($ciTotal -gt 188743680){throw 'Selected diagnostic exceeds the bounded local artifact allowance'}
    foreach($ciFile in $ciFiles){
        Copy-Item -LiteralPath $ciFile -Destination $ciResult
        $ciReceipt.outputs+=,[ordered]@{name=[IO.Path]::GetFileName($ciFile);bytes=(Get-Item -LiteralPath $ciFile).Length;sha256=(Get-FileHash -LiteralPath $ciFile).Hash}
    }
    $ciPayloads=@(Get-ChildItem -LiteralPath (Join-Path $ciBuild 'external-dxr-payloads') -Filter '*.json')
    if($ciPayloads.Count -ne 22){throw 'Complete byte-exact payload receipts missing'}
    $ciReceipt.payloads=@($ciPayloads|ForEach-Object {Get-Content -LiteralPath $_.FullName -Raw|ConvertFrom-Json})
    if(@($ciReceipt.payloads|Where-Object {!$_.shader_bytes_unchanged -or !$_.original_array_extent_retained}).Count){throw 'Payload identity or ABI changed'}
    $ciReceipt.status='fresh_full_native_link_passed_not_gpu_executed'
} catch {
    $ciReceipt.status='failed_or_resource_deferred';$ciReceipt.error=$_.Exception.Message;Save-CiReceipt;throw
} finally {Save-CiReceipt}
