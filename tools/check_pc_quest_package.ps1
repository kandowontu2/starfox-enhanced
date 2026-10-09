param(
    [Parameter(Mandatory=$true)][string]$Archive,
    [switch]$VerifyCurrentBuild
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
# This check is intentionally for the combined PC/Quest development handoff,
# not a generic platform archive. New runtime dependencies require review.
$expected=@(
    'START-HERE.txt', 'Quest/Starfox-Enhanced-Quest.apk',
    'Windows/starfox_pc.exe', 'Windows/starfox_asset_builder.exe',
    'Windows/README.md', 'Windows/CREDITS.md',
    'Windows/THIRD_PARTY_NOTICES.md', 'Windows/LICENSE-XBRZ.txt',
    'Windows/licenses/fonts/README.md', 'Windows/licenses/fonts/misaki.txt'
)
$sources=@{
    'Quest/Starfox-Enhanced-Quest.apk'='platform/quest/build/outputs/apk/debug/quest-debug.apk'
    'Windows/starfox_pc.exe'='build/current/starfox_pc.exe'
    'Windows/starfox_asset_builder.exe'='build/current/starfox_asset_builder.exe'
}
$zip=[IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $Archive).Path)
try {
    $seen=@{}
    foreach($entry in $zip.Entries) {
        $name=$entry.FullName.Replace('\','/')
        if($name.EndsWith('/')) {continue}
        if($name -cnotin $expected) {throw "Unexpected package file: $name"}
        if($seen.ContainsKey($name)) {throw "Duplicate package file: $name"}
        if($entry.Length -le 0) {throw "Empty package file: $name"}
        $seen[$name]=$true
        # Reading every entry also verifies it is decompressible, rather than
        # trusting only the archive's central-directory filenames and lengths.
        $stream=$entry.Open()
        $sha=[Security.Cryptography.SHA256]::Create()
        try {$hash=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}
        finally {$sha.Dispose();$stream.Dispose()}
        if($VerifyCurrentBuild -and $sources.ContainsKey($name)) {
            $current=(Get-FileHash -LiteralPath $sources[$name] -Algorithm SHA256).Hash
            if($current -ne $hash) {throw "Package does not contain the current build: $name"}
        }
    }
    foreach($name in $expected) {if(!$seen.ContainsKey($name)) {throw "Missing package file: $name"}}
    Write-Output "Verified $($seen.Count) handoff files; no ROM/BIN, saves/settings, add-ons or development audits."
    if($VerifyCurrentBuild) {Write-Output 'APK and both executables match the current build byte-for-byte.'}
} finally {$zip.Dispose()}
