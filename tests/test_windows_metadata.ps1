[CmdletBinding()]
param([Parameter(Mandatory)][string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path -Parent $PSScriptRoot
cmake -S (Join-Path $PSScriptRoot 'windows_metadata') -B $BuildDirectory -G Ninja
if ($LASTEXITCODE) { throw 'Windows metadata fixture configure failed' }
cmake --build $BuildDirectory
if ($LASTEXITCODE) { throw 'Windows metadata fixture build failed' }
$adapters = @(Get-ChildItem -LiteralPath $BuildDirectory -File -Filter '*metadata_adapter.dll')
if ($adapters.Count -ne 1) { throw 'Expected one fixture DLL (MSVC or MinGW)' }
& (Join-Path $root 'tools/check_windows_metadata.ps1') -Binary @(
    (Join-Path $BuildDirectory 'metadata_app.exe'),
    $adapters[0].FullName) -Version '0.0.8.6'
if ($LASTEXITCODE) { throw 'Windows metadata fixture validation failed' }
# Exercise the independent adapter project's no-version fallback as well.
cmake -S (Join-Path $PSScriptRoot 'windows_metadata_no_version') -B (Join-Path $BuildDirectory 'no-version') -G Ninja
if ($LASTEXITCODE) { throw 'Adapter version fallback configure failed' }
cmake --build (Join-Path $BuildDirectory 'no-version')
if ($LASTEXITCODE) { throw 'Adapter version fallback build failed' }
& (Join-Path $root 'tools/check_windows_metadata.ps1') -Binary @(
    (Join-Path $BuildDirectory 'no-version/metadata_no_version.exe')) -Version (
        [regex]::Match((Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw),
            'project\(starfox_enhanced VERSION ([0-9.]+)').Groups[1].Value)
Write-Output 'Windows metadata fixture tests passed (application, DLL, duplicate registration and adapter version fallback).'
