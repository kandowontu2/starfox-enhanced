# Scan only explicitly supplied artifacts. Do not execute their contents,
# restore quarantine, add exclusions, change protection settings or upload files.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string[]]$Path,
    [Parameter(Mandatory)][string]$Report
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$status = Get-MpComputerStatus
if (!$status.AMServiceEnabled -or !$status.AntivirusEnabled) {
    throw 'Defender is not available/enabled; artifacts have NOT passed antivirus validation.'
}
$platform = Join-Path $env:ProgramData 'Microsoft/Windows Defender/Platform'
$scanner = Get-ChildItem -LiteralPath $platform -Directory |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName 'MpCmdRun.exe' } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if (!$scanner) { $scanner = Join-Path $env:ProgramFiles 'Windows Defender/MpCmdRun.exe' }
if (!(Test-Path -LiteralPath $scanner -PathType Leaf)) { throw 'Defender scanner not found' }
$results = @()
$failed = $false
foreach ($item in $Path) {
    $file = Get-Item -LiteralPath $item
    if ($file.PSIsContainer) { throw 'Supply individual release files, not a workspace or directory' }
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    # Native Windows separators matter to MpCmdRun. DisableRemediation is a
    # documented diagnostic scan: archives are scanned, exclusions ignored,
    # detections reported, and the scan does not quarantine/delete the file.
    $output = @(& $scanner -Scan -ScanType 3 -File $file.FullName -DisableRemediation 2>&1)
    $scanExit = $LASTEXITCODE
    $results += [ordered]@{
        name = $file.Name
        bytes = $file.Length
        sha256 = $hash
        exit_code = $scanExit
        passed = ($scanExit -eq 0)
    }
    $output | Write-Output
    if ($scanExit -ne 0) { $failed = $true }
}
[ordered]@{
    schema = 1
    scanned_utc = (Get-Date).ToUniversalTime().ToString('o')
    defender_engine = $status.AMEngineVersion
    defender_signatures = $status.AntivirusSignatureVersion
    signature_updated_utc = $status.AntivirusSignatureLastUpdated.ToUniversalTime().ToString('o')
    scope = 'Explicit file scan only; not a SmartScreen/cloud reputation determination'
    passed = !$failed
    files = $results
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Report -Encoding utf8
if ($failed) { throw 'Defender detected a threat or returned a scan error; do not publish these artifacts.' }
Write-Output 'Defender scan passed for these exact file hashes and signature version only.'
