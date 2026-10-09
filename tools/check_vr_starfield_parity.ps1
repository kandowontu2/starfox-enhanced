param(
    [string]$Executable = 'build/vr-dev/starfox_vr_scene_check.exe',
    [string]$OutputDirectory = 'tmp/vr-starfield-regression',
    [ValidateSet('Original','EX','Both')][string]$Cartridge = 'Both',
    [ValidateSet('Starfield','ConnectedGrid','ConnectedGridBinned')][string]$Suite = 'Starfield'
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path $PSScriptRoot -Parent
function Resolve-RepositoryPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repository $Path))
}
$diagnostic = Resolve-RepositoryPath $Executable
if (-not (Test-Path -LiteralPath $diagnostic -PathType Leaf)) { throw "Missing diagnostic: $diagnostic" }
# A fresh run directory prevents old captures from making a failed run pass.
$runDirectory = Join-Path (Resolve-RepositoryPath $OutputDirectory) ([Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
$cartridges = @(
    @{ Name='Original'; Rom='upstream-ultrastarfox/SF.sfc'; Symbols='upstream-ultrastarfox/SYMBOLS.TXT' },
    @{ Name='EX'; Rom='tmp/runtime-inputs/starfox-ex/SFES.SFC'; Symbols='assets/symbols/starfox-ex.txt' }
)
$cases = @(
    @{ Name='basic'; CPU='--dust'; GPU='--dust-gpu' },
    @{ Name='colours'; CPU='--dust-colours'; GPU='--dust-gpu-colours' },
    @{ Name='motion'; CPU='--dust-motion'; GPU='--dust-gpu-motion' },
    @{ Name='wrapped'; CPU='--dust-wrapped'; GPU='--dust-gpu-wrapped' },
    @{ Name='maximum'; CPU='--dust-511'; GPU='--dust-gpu-511' },
    @{ Name='composite'; CPU='--live-space-cpu-dust'; GPU='--live-space' }
)
if ($Suite -eq 'ConnectedGrid') {
    $cases = @(
        @{ Name='connected'; CPU='--connected-grid-reference'; GPU='--connected-grid' },
        @{ Name='connected-rotated'; CPU='--connected-grid-reference-rotated'; GPU='--connected-grid-rotated' }
    )
}
if ($Suite -eq 'ConnectedGridBinned') {
    $cases = @(
        @{ Name='binned'; CPU='--connected-grid-texture'; GPU='--connected-grid-binned' },
        @{ Name='binned-rotated'; CPU='--connected-grid-texture-rotated'; GPU='--connected-grid-binned-rotated' }
    )
}
$results = [Collections.Generic.List[object]]::new()
foreach ($game in $cartridges) {
    if ($Cartridge -ne 'Both' -and $Cartridge -ne $game.Name) { continue }
    $rom = Resolve-RepositoryPath $game.Rom
    $symbols = Resolve-RepositoryPath $game.Symbols
    foreach ($path in @($rom,$symbols)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing input: $path" }
    }
    foreach ($case in $cases) {
        $pair = Join-Path $runDirectory "$($game.Name)-$($case.Name)"
        New-Item -ItemType Directory -Path $pair | Out-Null
        foreach ($backend in @('CPU','GPU')) {
            $capture = Join-Path $pair $backend
            & $diagnostic $capture $rom $symbols $case[$backend] 2>&1 |
                Tee-Object -FilePath (Join-Path $pair "$backend.log") | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "$($game.Name) $($case.Name) $backend failed; see $pair" }
        }
        foreach ($eye in @('left','right')) {
            $filename = "live-scene-$eye.bmp"
            $cpu = (Get-FileHash -LiteralPath (Join-Path (Join-Path $pair 'CPU') $filename)).Hash
            $gpu = (Get-FileHash -LiteralPath (Join-Path (Join-Path $pair 'GPU') $filename)).Hash
            if ($cpu -ne $gpu) { throw "$($game.Name) $($case.Name) $eye mismatch; see $pair" }
            $results.Add([pscustomobject]@{ Cartridge=$game.Name; Case=$case.Name; Eye=$eye; SHA256=$gpu })
        }
        Write-Output "$($game.Name) $($case.Name): both eyes exact"
    }
}
[pscustomobject]@{
    Scope="Offscreen Vulkan CPU/GPU $Suite comparison; not headset or complete-game parity."
    ExecutableSHA256=(Get-FileHash -LiteralPath $diagnostic).Hash
    Results=$results.ToArray()
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runDirectory 'results.json')
Write-Output "Passed $($results.Count) eye comparisons. Evidence: $runDirectory"
