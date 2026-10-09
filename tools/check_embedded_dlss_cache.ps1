param([Parameter(Mandatory)][string]$PackageDirectory,
    [string]$Binary='build/release/starfox_pc.exe',
    [string]$OutputDirectory='tmp/embedded-dlss-cache-check')
$ErrorActionPreference='Stop'
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
if((Split-Path $package -Leaf) -ne 'dlss') {throw 'Use the verified staged dlss directory embedded in this executable'}
& (Join-Path $PSScriptRoot 'verify_dlss_package.ps1') -Installation (Split-Path $package)
$files=@('starfox_dlss_native.dll','sl.interposer.dll','sl.common.dll','sl.dlss.dll','nvngx_dlss.dll',
    'Streamline-LICENSE.txt','Streamline-THIRD-PARTY.md','nvngx_dlss.license.txt','DLSS-RUNTIME.txt','runtime-manifest.json')
$digests=($files | ForEach-Object {(Get-FileHash -LiteralPath (Join-Path $package $_)).Hash.ToLowerInvariant()}) -join ''
$packageId=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($digests))).ToLowerInvariant()
$cache=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) "StarFoxEnhanced/dlss/$packageId"
# Cache tests operate only on the exact application-managed version matching
# this verified source package, with no recursive delete or user DLL replacement.
if(!(Test-Path -LiteralPath $cache -PathType Container)) {throw 'Run check_embedded_dlss first to prepare this versioned cache'}
foreach($name in $files) {
    $path=Join-Path $cache $name
    if((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint -or
        (Get-FileHash -LiteralPath $path).Hash -ne (Get-FileHash -LiteralPath (Join-Path $package $name)).Hash) {
        throw "Cache does not match the embedded package: $name"
    }
}
$notice=Join-Path $cache 'DLSS-RUNTIME.txt'
$original=[IO.File]::ReadAllBytes($notice)
try {
    [IO.File]::WriteAllBytes($notice,[byte[]]@(0,1,2,3))
    & (Join-Path $PSScriptRoot 'capture_lava.ps1') -Binary $Binary `
        -OutputDirectory (Join-Path $OutputDirectory 'repair') -Experience ORIGINAL -Stage LEVEL1_1 `
        -Frames 8 -GroundEnabled 0 -Sky 0 -RayTracing 0 -Reflections 0 -Dlss 3 -RenderScale 2
    if((Get-FileHash -LiteralPath $notice).Hash -ne (Get-FileHash -LiteralPath (Join-Path $package 'DLSS-RUNTIME.txt')).Hash) {
        throw 'Changed cache file was not repaired to the exact embedded bytes'
    }
} finally { [IO.File]::WriteAllBytes($notice,$original) }
$extra=Join-Path $cache ('check-unrecognized-'+[guid]::NewGuid()+'.DLL')
try {
    [IO.File]::WriteAllBytes($extra,[byte[]]@(0))
    $output=Join-Path $OutputDirectory 'unexpected-dll'
    & (Join-Path $PSScriptRoot 'capture_lava.ps1') -Binary $Binary `
        -OutputDirectory $output -Experience ORIGINAL -Stage LEVEL1_1 `
        -Frames 8 -GroundEnabled 0 -Sky 0 -RayTracing 0 -Reflections 0 -Dlss 0 -RenderScale 2
    $log=Get-Content -LiteralPath (Join-Path $output 'runtime.log') -Raw
    if($log -notmatch 'dlss-lifecycle: unavailable: Cannot prepare embedded DLSS runtime cache' -or
        $log -match 'dlss-lifecycle: initialized before SDL') {
        throw 'Unexpected uppercase DLL was not rejected before loading the optional runtime'
    }
} finally { [IO.File]::Delete($extra) }
Write-Output 'Embedded DLSS cache: changed bytes repaired; unexpected uppercase DLL refused while ordinary GPU rendering remained usable.'
