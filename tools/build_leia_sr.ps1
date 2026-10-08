param(
    [string]$BuildDirectory = 'tmp/leia-sr-msvc',
    [string]$OutputDirectory = 'build/current'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=[IO.Path]::GetFullPath((Join-Path $root $BuildDirectory))
$output=[IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
& cmake -S (Join-Path $root 'src/leia_sr') -B $build -G 'Visual Studio 17 2022' -A x64
if($LASTEXITCODE) {throw 'SR Platform adapter configure failed'}
& cmake --build $build --config Release --parallel 2
if($LASTEXITCODE) {throw 'SR Platform adapter build failed'}
& cmake --install $build --config Release --prefix $output
if($LASTEXITCODE) {throw 'SR Platform adapter install failed'}
Write-Output "Optional SR Platform adapter installed in $output (system SR runtime still required)."
