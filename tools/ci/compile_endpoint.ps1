param([Parameter(Mandatory=$true)][string]$Compiler,[ValidateSet('dxil','spirv')][string]$Format)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskPins=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'endpoint-source.json') -Raw|ConvertFrom-Json
. (Join-Path $PSScriptRoot 'owned-compiler-resources.ps1')
function Assert-Inputs {
    foreach($pin in $taskPins.sources){
        $path=[IO.Path]::GetFullPath((Join-Path $taskRoot $pin.path))
        if(!$path.StartsWith($taskRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Source escaped isolated checkout'}
        $text=[IO.File]::ReadAllText($path).Replace("`r`n","`n")
        $bytes=[Text.UTF8Encoding]::new($false).GetBytes($text)
        if($bytes.Length -ne $pin.bytes -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant() -cne $pin.sha256){throw ('Changed source: '+$pin.path)}
    }
}
Assert-Inputs
if((Get-FileHash -LiteralPath $Compiler).Hash -cne '980A3A4C6E5C88F5737DDE321E548860021D58FA2349790D4E572805CB298293' -or
    (Get-FileHash -LiteralPath (Join-Path (Split-Path $Compiler) 'dxcompiler.dll')).Hash -cne '9A5100511E127C6A2FC78EDF984F95074A76D35B90C90C4D342430A5AE160E9B'){throw 'Pinned compiler identity changed'}
$result=Join-Path $env:RUNNER_TEMP ('endpoint-'+$Format);[void](New-Item -ItemType Directory -Path $result)
$flags=@('-T','cs_6_0','-E','checked_endpoints_main','-WX','-Gis','-O3')
if($Format -eq 'spirv'){$flags+=@('-spirv','-fspv-target-env=vulkan1.2')}
$binary=Join-Path $result ('endpoints.'+$Format)
$memory=Get-CimInstance Win32_OperatingSystem
if($memory.FreePhysicalMemory -lt 6291456 -or $memory.FreeVirtualMemory -lt 6291456){throw 'Original launch headroom unavailable'}
$info=[Diagnostics.ProcessStartInfo]::new($Compiler);$info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.WorkingDirectory=$taskRoot
$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
foreach($arg in ($flags+@('-Fo',$binary,'tools/shaders/check_endpoints.hlsl'))){[void]$info.ArgumentList.Add($arg)}
$native=[Diagnostics.Process]::Start($info);$null=$native.Handle;$birth=$native.StartTime.ToUniversalTime()
$stdout=$native.StandardOutput.ReadToEndAsync();$stderr=$native.StandardError.ReadToEndAsync();$peak=0L;$stop=$null
try {
    while(!$native.WaitForExit(500)) {
        $sample=Read-CiOwnedCompilerResources $native $birth $Compiler;if($sample.Completed){break}
        $peak=[Math]::Max($peak,$sample.Private);$memory=Get-CimInstance Win32_OperatingSystem
        if($sample.Private -gt 2147483648){$stop='Original 2 GiB private compiler cap'}
        elseif($memory.FreePhysicalMemory -lt 1048576 -or $memory.FreeVirtualMemory -lt 1572864){$stop='Original live global memory floor'}
        if($stop){$native.Kill($true);$native.WaitForExit();break}
    }
}catch{$stop='Owned observation failed closed: '+$_.Exception.Message;$native.Refresh();if(!$native.HasExited){$native.Kill($true);$native.WaitForExit()}}
$native.Refresh();if(!$native.HasExited){throw 'Actual compiler still live'}
$stdout.GetAwaiter().GetResult()|Out-File -LiteralPath (Join-Path $result 'stdout.log') -Encoding utf8
$stderr.GetAwaiter().GetResult()|Out-File -LiteralPath (Join-Path $result 'stderr.log') -Encoding utf8
Assert-Inputs
[ordered]@{commit=$env:GITHUB_SHA;run=$env:GITHUB_RUN_ID;pid=$native.Id;birth_utc=$birth.ToString('o');
    flags=$flags;exit_code=$native.ExitCode;resource_stop=$stop;peak_private_bytes=$peak;
    binary_sha256=if(Test-Path -LiteralPath $binary){(Get-FileHash -LiteralPath $binary).Hash}else{$null};
    source_manifest_sha256=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'endpoint-source.json')).Hash;
    native_gpu_accepted=$false;production_adopted=$false}|ConvertTo-Json -Depth 5|
    Out-File -LiteralPath (Join-Path $result 'receipt.json') -Encoding utf8
$exitCode=$native.ExitCode;$native.Dispose();if($exitCode -ne 0 -or $stop){throw 'Actual compile did not pass'}
