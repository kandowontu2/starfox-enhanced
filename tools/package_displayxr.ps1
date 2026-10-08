# Package untouched upstream installers. This script never executes them,
# installs a service, changes the OpenXR runtime, or writes registry settings.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$CacheDirectory = 'tmp/displayxr-package-inputs',
    [switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$manifestPath = Join-Path $PSScriptRoot 'package/displayxr-runtime.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne 1) { throw 'Unsupported DisplayXR package manifest' }
$output = [IO.Path]::GetFullPath($OutputDirectory)
$cache = [IO.Path]::GetFullPath($CacheDirectory)

function Assert-PinnedFile([string]$Path, $Entry, [bool]$Installer) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf) -or
        (Get-Item -LiteralPath $Path).Length -ne $Entry.bytes -or
        (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Entry.sha256) {
        throw "DisplayXR pinned file size/hash mismatch: $Path"
    }
    if ($Installer) {
        $signature = Get-AuthenticodeSignature -LiteralPath $Path
        if ($signature.Status -ne 'Valid' -or
            $signature.SignerCertificate.Subject -notmatch 'O="Leia, Inc\."') {
            throw "DisplayXR installer signature/publisher invalid: $Path"
        }
    }
}
function Get-PinnedFile([string]$Url, [string]$Name, $Entry, [bool]$Installer) {
    $path = Join-Path $cache $Name
    if (!(Test-Path -LiteralPath $path)) {
        New-Item -ItemType Directory -Force -Path $cache | Out-Null
        Invoke-WebRequest -Uri $Url -OutFile $path
    }
    # An interrupted or altered cached download is rejected, never packaged.
    Assert-PinnedFile $path $Entry $Installer
    return $path
}
function Package-PinnedFile([string]$Source, [string]$Destination, $Entry, [bool]$Installer) {
    if (!$VerifyOnly) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
    }
    Assert-PinnedFile $Destination $Entry $Installer
}
foreach ($entry in $manifest.installers) {
    $project = $manifest.projects.($entry.project)
    $destination = Join-Path $output "optional-runtimes/displayxr/$($entry.file)"
    if ($VerifyOnly) { Assert-PinnedFile $destination $entry $true; continue }
    $url = "https://github.com/$($project.repository)/releases/download/v$($project.version)/$($entry.file)"
    $source = Get-PinnedFile $url $entry.file $entry $true
    Package-PinnedFile $source $destination $entry $true
}
foreach ($entry in $manifest.notices) {
    $project = $manifest.projects.($entry.project)
    $destination = Join-Path $output "$($project.license_directory)/$($entry.source)"
    if ($VerifyOnly) { Assert-PinnedFile $destination $entry $false; continue }
    $url = "https://raw.githubusercontent.com/$($project.repository)/$($project.revision)/$($entry.source)"
    $source = Get-PinnedFile $url ($entry.project + '-' + ($entry.source -replace '/', '-')) $entry $false
    Package-PinnedFile $source $destination $entry $false
}
$instructions = Join-Path $output 'optional-runtimes/displayxr/README.txt'
$instructionsSource = Join-Path $PSScriptRoot 'package/DISPLAYXR-INSTALLERS.txt'
$packageManifest = Join-Path $output 'optional-runtimes/displayxr/manifest.json'
if (!$VerifyOnly) {
    Copy-Item -LiteralPath $instructionsSource -Destination $instructions -Force
    Copy-Item -LiteralPath $manifestPath -Destination $packageManifest -Force
}
foreach ($pair in @(@($instructionsSource,$instructions),@($manifestPath,$packageManifest))) {
    if (!(Test-Path -LiteralPath $pair[1] -PathType Leaf) -or
        (Get-FileHash -LiteralPath $pair[0]).Hash -ne (Get-FileHash -LiteralPath $pair[1]).Hash) {
        throw "DisplayXR package instructions/manifest missing or modified: $($pair[1])"
    }
}
Write-Output 'DisplayXR package verified: 2 untouched signed/hash-pinned optional installers, 14 upstream license/notice files; nothing executed or installed.'
