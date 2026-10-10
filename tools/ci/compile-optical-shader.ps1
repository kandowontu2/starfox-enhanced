param(
    [Parameter(Mandatory=$true)][ValidateSet('tiles','reduce','query','mapping','domains','frames','optical','optical_stream','optical_schedule','jets','jets_stream','roots_clear','witness','folds','guide','colour','compose','publish','publish_diagnostics','admission_clear','capture')][string]$Mode,
    [Parameter(Mandatory=$true)][ValidateSet('dxil','spirv')][string]$Format,
    [Parameter(Mandatory=$true)][string]$Compiler,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [switch]$PreflightOnly
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$ciShaderRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$ciShaderManifest=Join-Path $PSScriptRoot 'optical-shader-source.json'
$ciShaderManifestHash=(Get-FileHash -LiteralPath $ciShaderManifest -Algorithm SHA256).Hash
$ciShaderPins=Get-Content -LiteralPath $ciShaderManifest -Raw | ConvertFrom-Json
$ciShaderPlans=@{
    tiles=@('src/render/shaders/reflection_source_index.hlsl','feature_tiles_main')
    reduce=@('src/render/shaders/reflection_source_index.hlsl','feature_reduce_main')
    query=@('src/render/shaders/reflection_source_index.hlsl','feature_query_main')
    mapping=@('src/render/shaders/reflection_source_queries.hlsl','feature_mapping_main')
    domains=@('src/render/shaders/reflection_source_domains.hlsl','feature_domains_main')
    frames=@('src/render/shaders/reflection_source_frames.hlsl','feature_frames_main')
    optical=@('src/render/shaders/reflection_source_optical.hlsl','feature_optical_main')
    optical_stream=@('src/render/shaders/reflection_source_optical_stream.hlsl','feature_optical_stream_main')
    optical_schedule=@('src/render/shaders/reflection_source_optical_schedule.hlsl','feature_optical_schedule_main')
    jets=@('src/render/shaders/reflection_source_jets.hlsl','feature_jets_main')
    jets_stream=@('src/render/shaders/reflection_source_jets_stream.hlsl','feature_jets_stream_main')
    roots_clear=@('src/render/shaders/reflection_source_roots_clear.hlsl','feature_roots_clear_main')
    witness=@('src/render/shaders/reflection_source_witness.hlsl','feature_witness_main')
    folds=@('src/render/shaders/reflection_source_folds.hlsl','feature_folds_main')
    guide=@('src/render/shaders/reflection_source_guide.hlsl','feature_guide_main')
    colour=@('src/render/shaders/reflection_source_colour.hlsl','feature_colour_main')
    compose=@('src/render/shaders/reflection_source_compose.hlsl','feature_compose_main')
    publish=@('src/render/shaders/reflection_source_publish.hlsl','feature_publish_main')
    publish_diagnostics=@('src/render/shaders/reflection_source_publish_diagnostics.hlsl','feature_publish_diagnostics_main')
    admission_clear=@('src/render/shaders/reflection_source_admission_clear.hlsl','feature_admission_clear_main')
    capture=@('src/render/shaders/reflection_path_capture.hlsl','reflection_path_capture_main')
}
function Get-CiShaderCanonicalBytes([string]$Path) {
    $ciShaderText=[IO.File]::ReadAllText($Path,[Text.UTF8Encoding]::new($false,$true))
    if($ciShaderText.Contains([char]0)){throw 'Nontext source in shader closure'}
    return ,([Text.UTF8Encoding]::new($false,$true).GetBytes($ciShaderText.Replace("`r`n","`n")))
}
function Get-CiShaderDigest([byte[]]$Bytes) {
    return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}
function Resolve-CiShaderSource([string]$RelativePath) {
    $ciShaderFull=[IO.Path]::GetFullPath((Join-Path $ciShaderRoot $RelativePath))
    if(!$ciShaderFull.StartsWith($ciShaderRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
        throw 'Shader source escaped the source-only workspace'
    }
    return $ciShaderFull
}
function Assert-CiShaderInputs {
    if((Get-FileHash -LiteralPath $ciShaderManifest -Algorithm SHA256).Hash -ne $ciShaderManifestHash){
        throw 'Source manifest changed during compilation'
    }
    if($ciShaderPins.sources.Count -ne 38 -or $ciShaderPins.pipelines.Count -ne 21){throw 'Incomplete full source-only snapshot'}
    $ciShaderPaths=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach($ciShaderPin in $ciShaderPins.sources){
        if(!$ciShaderPaths.Add($ciShaderPin.path)){throw 'Repeated pinned source'}
        $ciShaderBytes=Get-CiShaderCanonicalBytes (Resolve-CiShaderSource $ciShaderPin.path)
        if($ciShaderBytes.Length -ne $ciShaderPin.canonical_bytes -or
            (Get-CiShaderDigest $ciShaderBytes) -ne $ciShaderPin.canonical_sha256){
            throw "Pinned source changed: $($ciShaderPin.path)"
        }
    }
    foreach($ciShaderPlanName in $ciShaderPlans.Keys){
        $ciShaderPlan=@($ciShaderPins.pipelines | Where-Object mode -eq $ciShaderPlanName)
        if($ciShaderPlan.Count -ne 1 -or $ciShaderPlan[0].source -ne $ciShaderPlans[$ciShaderPlanName][0] -or
            $ciShaderPlan[0].entry -ne $ciShaderPlans[$ciShaderPlanName][1]){throw 'Actual diagnostic/native/stream entry changed'}
        $ciShaderVisited=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $ciShaderPending=[Collections.Generic.Stack[string]]::new()
        $ciShaderPending.Push($ciShaderPlan[0].source)
        while($ciShaderPending.Count){
            $ciShaderPath=$ciShaderPending.Pop()
            if(!$ciShaderVisited.Add($ciShaderPath)){continue}
            if(!$ciShaderPaths.Contains($ciShaderPath)){throw "Unpinned compiler include: $ciShaderPath"}
            $ciShaderFull=Resolve-CiShaderSource $ciShaderPath
            $ciShaderText=[IO.File]::ReadAllText($ciShaderFull)
            foreach($ciShaderInclude in [regex]::Matches($ciShaderText,'(?m)^\s*#\s*include\s+([^\r\n]+)')){
                $ciShaderLiteral=[regex]::Match($ciShaderInclude.Groups[1].Value,'^"([^"]+)"\s*(?://.*)?$')
                if(!$ciShaderLiteral.Success){throw 'Dynamic or system include lacks a source pin'}
                $ciShaderIncluded=[IO.Path]::GetFullPath((Join-Path (Split-Path $ciShaderFull) $ciShaderLiteral.Groups[1].Value))
                $ciShaderRelative=[IO.Path]::GetRelativePath($ciShaderRoot,$ciShaderIncluded).Replace('\','/')
                [void](Resolve-CiShaderSource $ciShaderRelative)
                $ciShaderPending.Push($ciShaderRelative)
            }
        }
        $ciShaderActual=@($ciShaderVisited | Sort-Object)
        if(($ciShaderActual -join "`n") -ne (@($ciShaderPlan[0].closure | Sort-Object) -join "`n")){
            throw "Recursive actual closure changed: $ciShaderPlanName"
        }
    }
}
Assert-CiShaderInputs
if($PreflightOnly){
    Write-Output 'All38 exact canonical sources and all21 complete recursive shader closures verified; no compiler launched.'
    exit 0
}
$ciShaderCompiler=[IO.Path]::GetFullPath($Compiler)
if((Get-FileHash -LiteralPath $ciShaderCompiler).Hash -ne '980A3A4C6E5C88F5737DDE321E548860021D58FA2349790D4E572805CB298293' -or
    (Get-FileHash -LiteralPath (Join-Path (Split-Path $ciShaderCompiler) 'dxcompiler.dll')).Hash -ne
    '9A5100511E127C6A2FC78EDF984F95074A76D35B90C90C4D342430A5AE160E9B'){
    throw 'Compiler is not the exact already-tested DXC1.9.2607 binary pair'
}
$ciShaderOutput=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $ciShaderOutput){throw 'Existing output directory; refusing receipt/output replacement'}
[void](New-Item -ItemType Directory -Path $ciShaderOutput)
$ciShaderFlags=@('-T','cs_6_0','-E',$ciShaderPlans[$Mode][1],'-WX','-Gis','-O3')
if($Mode -eq 'tiles'){$ciShaderFlags+='-DINDEX_BUILD=1'}
elseif($Mode -eq 'reduce'){$ciShaderFlags+='-DINDEX_REDUCE=1'}
if($Mode -in @('optical','optical_stream','jets','jets_stream','witness','folds','guide')){
    $ciShaderFlags+='-DSTARFOX_OPTICAL_SCALAR_INLINE=1'
}
if($Mode -in @('jets','jets_stream') -or ($Format -eq 'dxil' -and $Mode -in @('optical','optical_stream'))){
    # Reuse shared scalar helper calls for the two memory-heavy DXIL optical stages.
    $ciShaderFlags+='-DSTARFOX_OPTICAL_SHARED_SCALAR=1'
}
if($Mode -in @('optical','optical_stream')){$ciShaderFlags+='-DSTARFOX_NATIVE_SOURCE_OPTICAL=1'}
if($Format -eq 'spirv'){
    $ciShaderFlags+=@('-spirv','-fspv-target-env=vulkan1.2')
    if($Mode -in @('jets','jets_stream')){
        $ciShaderFlags=@($ciShaderFlags | Where-Object {$_ -ne '-O3'})
        $ciShaderFlags+='-Oconfig=--inline-entry-points-exhaustive,--eliminate-dead-functions,--private-to-local,--scalar-replacement=100,--convert-local-access-chains,--eliminate-local-single-block,--eliminate-local-single-store,--ssa-rewrite,--ccp,--eliminate-dead-branches,--eliminate-dead-code-aggressive,--simplify-instructions,--eliminate-dead-code-aggressive,--compact-ids'
    }elseif($Mode -in @('optical','optical_stream')){
        $ciShaderFlags=@($ciShaderFlags | Where-Object {$_ -ne '-O3'})
        $ciShaderFlags+='-Oconfig=--inline-entry-points-exhaustive,--eliminate-dead-functions,--ccp,--eliminate-dead-branches,--eliminate-dead-code-aggressive,--simplify-instructions,--eliminate-dead-code-aggressive,--compact-ids'
    }elseif($Mode -eq 'witness'){
        # Bounded storage/SSA cleanup; preserve the explicit helper call boundaries.
        $ciShaderFlags=@($ciShaderFlags | Where-Object {$_ -ne '-O3'})
        $ciShaderFlags+='-Oconfig=--inline-entry-points-exhaustive,--eliminate-dead-functions,--private-to-local,--scalar-replacement=100,--convert-local-access-chains,--eliminate-local-single-block,--eliminate-local-single-store,--ssa-rewrite,--ccp,--eliminate-dead-branches,--eliminate-dead-code-aggressive,--simplify-instructions,--eliminate-dead-code-aggressive,--compact-ids'
    }elseif($Mode -eq 'folds'){$ciShaderFlags+='-DSTARFOX_FOLD_HELPER_NOINLINE=1'}
}
$ciShaderBinary=Join-Path $ciShaderOutput "$Mode.$Format"
$ciShaderReceipt=[ordered]@{
    mode=$Mode;format=$Format;source=$ciShaderPlans[$Mode][0];entry=$ciShaderPlans[$Mode][1]
    head_sha=$env:GITHUB_SHA;run_id=$env:GITHUB_RUN_ID;run_attempt=$env:GITHUB_RUN_ATTEMPT
    manifest_sha256=$ciShaderManifestHash.ToLowerInvariant();compiler_tag='v1.9.2607'
    compiler_sha256=(Get-FileHash -LiteralPath $ciShaderCompiler).Hash.ToLowerInvariant()
    compiler_library_sha256=(Get-FileHash -LiteralPath (Join-Path (Split-Path $ciShaderCompiler) 'dxcompiler.dll')).Hash.ToLowerInvariant()
    flags=$ciShaderFlags;source_count=38;closure_count=@($ciShaderPins.pipelines | Where-Object mode -eq $Mode)[0].closure.Count
    guards=@{prelaunch_physical_kib=6291456;prelaunch_virtual_kib=6291456;native_private_limit_bytes=2147483648;
        running_physical_floor_kib=1048576;running_virtual_floor_kib=1572864;elapsed_native_kill=$false}
    status='not_started';native_pid=$null;native_creation_utc=$null;launch_memory=$null;terminal_memory=$null
    native_exit_code=$null;resource_stopped=$false;peak_private_bytes=0;binary=$null;error=$null
    gpu_runtime_accepted=$false;production_adopted=$false
}
$ciShaderNative=$null;$ciShaderNativeStarted=$false;$ciShaderOutTask=$null;$ciShaderErrTask=$null;$ciShaderNativeStart=$null
try{
    $ciShaderDeadline=[DateTime]::UtcNow.AddMinutes(10)
    $ciShaderMemory=Get-CimInstance Win32_OperatingSystem
    while($ciShaderMemory.FreePhysicalMemory -lt 6291456 -or $ciShaderMemory.FreeVirtualMemory -lt 6291456){
        if([DateTime]::UtcNow -ge $ciShaderDeadline){
            $ciShaderReceipt.status='resource_deferred_before_launch'
            throw 'Original6GiB prelaunch memory gate not met; no native compiler launched'
        }
        Start-Sleep -Seconds 5
        Assert-CiShaderInputs
        $ciShaderMemory=Get-CimInstance Win32_OperatingSystem
    }
    $ciShaderReceipt.launch_memory=@{physical_kib=$ciShaderMemory.FreePhysicalMemory;virtual_kib=$ciShaderMemory.FreeVirtualMemory}
    $ciShaderProcessInfo=[Diagnostics.ProcessStartInfo]::new()
    $ciShaderProcessInfo.FileName=$ciShaderCompiler
    $ciShaderProcessInfo.WorkingDirectory=$ciShaderRoot
    $ciShaderProcessInfo.UseShellExecute=$false;$ciShaderProcessInfo.CreateNoWindow=$true
    $ciShaderProcessInfo.RedirectStandardOutput=$true;$ciShaderProcessInfo.RedirectStandardError=$true
    foreach($ciShaderFlag in ($ciShaderFlags+@('-Fo',$ciShaderBinary,(Resolve-CiShaderSource $ciShaderPlans[$Mode][0])))){
        $ciShaderProcessInfo.ArgumentList.Add($ciShaderFlag)
    }
    $ciShaderNative=[Diagnostics.Process]::new();$ciShaderNative.StartInfo=$ciShaderProcessInfo
    if(!$ciShaderNative.Start()){throw 'Compiler did not start'}
    $ciShaderNativeStarted=$true
    $ciShaderOutTask=$ciShaderNative.StandardOutput.ReadToEndAsync()
    $ciShaderErrTask=$ciShaderNative.StandardError.ReadToEndAsync()
    $ciShaderNativeStart=$ciShaderNative.StartTime
    $ciShaderReceipt.native_pid=$ciShaderNative.Id
    $ciShaderReceipt.native_creation_utc=$ciShaderNativeStart.ToUniversalTime().ToString('o')
    $ciShaderReceipt.status='running'
    Write-Output "Actual $Mode/$Format compiler PID=$($ciShaderNative.Id), creation=$($ciShaderReceipt.native_creation_utc), complete unchanged strict flags."
    while(!$ciShaderNative.HasExited){
        $ciShaderNative.Refresh();if($ciShaderNative.HasExited){break}
        $ciShaderReceipt.peak_private_bytes=[Math]::Max([long]$ciShaderReceipt.peak_private_bytes,$ciShaderNative.PrivateMemorySize64)
        $ciShaderMemory=Get-CimInstance Win32_OperatingSystem
        if($ciShaderReceipt.peak_private_bytes -gt 2147483648 -or $ciShaderMemory.FreePhysicalMemory -lt 1048576 -or
            $ciShaderMemory.FreeVirtualMemory -lt 1572864){
            $ciShaderReceipt.terminal_memory=@{physical_kib=$ciShaderMemory.FreePhysicalMemory;virtual_kib=$ciShaderMemory.FreeVirtualMemory}
            $ciShaderNative.Refresh()
            if(!$ciShaderNative.HasExited){
                $ciShaderIdentity=Get-Process -Id $ciShaderNative.Id -ErrorAction Stop
                if($ciShaderIdentity.StartTime -ne $ciShaderNativeStart -or $ciShaderIdentity.Path -ne $ciShaderCompiler){
                    throw 'Refusing a mismatched native compiler resource stop'
                }
                $ciShaderNative.Kill();$ciShaderReceipt.resource_stopped=$true
            }
            break
        }
        Start-Sleep -Seconds 1
    }
    $ciShaderNative.WaitForExit();$ciShaderReceipt.native_exit_code=$ciShaderNative.ExitCode
    Assert-CiShaderInputs
    if($ciShaderReceipt.resource_stopped){$ciShaderReceipt.status='resource_stopped';throw 'Original native compiler memory guard stopped this owned compiler'}
    if($ciShaderNative.ExitCode -ne 0){$ciShaderReceipt.status='compile_failed';throw "Compiler exited $($ciShaderNative.ExitCode)"}
    $ciShaderBinaryInfo=Get-Item -LiteralPath $ciShaderBinary
    if(!$ciShaderBinaryInfo.Length){throw 'Empty compiler output'}
    $ciShaderStream=[IO.File]::OpenRead($ciShaderBinary)
    try{
        $ciShaderMagic=[byte[]]::new(4)
        if($ciShaderStream.Read($ciShaderMagic,0,4) -ne 4){throw 'Truncated compiler output'}
        $ciShaderExpected=if($Format -eq 'dxil'){'44-58-42-43'}else{'03-02-23-07'}
        if([BitConverter]::ToString($ciShaderMagic) -ne $ciShaderExpected){throw 'Compiler output format signature differs'}
    }finally{$ciShaderStream.Dispose()}
    $ciShaderReceipt.binary=@{name=$ciShaderBinaryInfo.Name;bytes=$ciShaderBinaryInfo.Length;
        sha256=(Get-FileHash -LiteralPath $ciShaderBinary).Hash.ToLowerInvariant()}
    $ciShaderReceipt.status='compiled'
}catch{
    $ciShaderReceipt.error=$_.Exception.Message
    if($ciShaderReceipt.status -in @('not_started','running')){$ciShaderReceipt.status='observer_failed'}
    throw
}finally{
    if($ciShaderNativeStarted){
        if(!$ciShaderNative.HasExited){$ciShaderNative.WaitForExit()}
        $ciShaderReceipt.native_exit_code=$ciShaderNative.ExitCode
        if($ciShaderOutTask){[IO.File]::WriteAllText((Join-Path $ciShaderOutput 'compiler.stdout.log'),$ciShaderOutTask.GetAwaiter().GetResult())}
        if($ciShaderErrTask){[IO.File]::WriteAllText((Join-Path $ciShaderOutput 'compiler.stderr.log'),$ciShaderErrTask.GetAwaiter().GetResult())}
        $ciShaderNative.Dispose()
    }
    elseif($ciShaderNative){$ciShaderNative.Dispose()}
    [IO.File]::WriteAllText((Join-Path $ciShaderOutput 'receipt.json'),($ciShaderReceipt | ConvertTo-Json -Depth 10))
    Write-Output ($ciShaderReceipt | ConvertTo-Json -Depth 10)
    if(Test-Path -LiteralPath (Join-Path $ciShaderOutput 'compiler.stderr.log')){Get-Content -LiteralPath (Join-Path $ciShaderOutput 'compiler.stderr.log')}
}
