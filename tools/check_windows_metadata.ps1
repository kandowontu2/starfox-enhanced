[CmdletBinding()]
param(
    [Parameter(Mandatory)][string[]]$Binary,
    [Parameter(Mandatory)][string]$Version
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
foreach ($item in $Binary) {
    $file = Get-Item -LiteralPath $item
    $info = $file.VersionInfo
    if ($info.ProductName -cne 'Star Fox Enhanced' -or
        $info.CompanyName -cne 'Star Fox Enhanced contributors' -or
        $info.FileVersion -cne $Version -or $info.ProductVersion -cne $Version -or
        $info.OriginalFilename -cne $file.Name -or !$info.FileDescription) {
        throw "Missing or inconsistent Windows file identity: $($file.Name)"
    }
    Write-Output "Verified Windows identity: $($file.Name), $($info.FileVersion)"
}
