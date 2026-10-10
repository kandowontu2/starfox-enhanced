param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('x64', 'x86', 'pcvr', 'uwp')]
    [string]$Target
)

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $env:CI_PROJECT_DIR
New-Item -ItemType Directory -Force release-out | Out-Null
$version = if ($env:CI_COMMIT_TAG) { $env:CI_COMMIT_TAG } `
    elseif ($env:CI_RELEASE_TAG) { $env:CI_RELEASE_TAG } else { 'v0.0.8' }
$version = $version -replace '^v', ''

function Assert-Exit([string]$Step) {
    if ($LASTEXITCODE -ne 0) { throw "$Step failed with exit code $LASTEXITCODE" }
}

function Find-Binary([string]$BuildDirectory, [string]$Name) {
    foreach ($candidate in @("$BuildDirectory/Release/$Name", "$BuildDirectory/$Name")) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw "$Name was not built in $BuildDirectory"
}

function Assert-Standalone([string]$Binary, [bool]$CheckVrDependencies = $false) {
    $objdump = Get-Command objdump.exe -ErrorAction SilentlyContinue
    if ($objdump) {
        $imports = (& $objdump.Source -p $Binary | Out-String)
        Assert-Exit "objdump $Binary"
    } else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) { throw 'No PE import inspection tool found' }
        $vsRoot = (& $vswhere -latest -products '*' -property installationPath | Select-Object -First 1)
        $dumpbin = Get-ChildItem -LiteralPath (Join-Path $vsRoot 'VC/Tools/MSVC') `
            -Filter dumpbin.exe -Recurse -File | Select-Object -First 1
        if (-not $dumpbin) { throw 'dumpbin.exe is missing from Visual Studio Build Tools' }
        $imports = (& $dumpbin.FullName /DEPENDENTS $Binary | Out-String)
        Assert-Exit "dumpbin $Binary"
    }
    if ($imports -match 'libgcc_s_|libstdc\+\+|libwinpthread') {
        throw "$Binary depends on an unpackaged MinGW runtime"
    }
    if ($CheckVrDependencies -and $imports -match 'SDL3\.dll|openxr_loader\.dll') {
        throw "$Binary depends on an unpackaged VR runtime DLL"
    }
}

switch ($Target) {
    'x64' {
        cmake -S . -B build/gitlab-windows-x64 -G 'Visual Studio 17 2022' -A x64 `
            -DSTARFOX_BUILD_TESTS=OFF -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
        Assert-Exit 'Windows x64 configure'
        cmake --build build/gitlab-windows-x64 --config Release --parallel 2 `
            --target starfox_pc starfox_asset_builder
        Assert-Exit 'Windows x64 build'
        $runtimeBinary = Find-Binary build/gitlab-windows-x64 starfox_pc.exe
        $builderBinary = Find-Binary build/gitlab-windows-x64 starfox_asset_builder.exe
        Assert-Standalone $runtimeBinary
        Assert-Standalone $builderBinary
        & $builderBinary
        if ($LASTEXITCODE -ne 2) { throw 'Asset builder did not print its usage' }

        $archive = Join-Path $env:TEMP 'streamline-sdk-v2.14.1.zip'
        Invoke-WebRequest 'https://github.com/NVIDIA-RTX/Streamline/releases/download/v2.14.1/streamline-sdk-v2.14.1.zip' `
            -OutFile $archive
        if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne `
            '92C4D954631A1710DA86CA3FA8D5034F2B9503838C95FC4AE977AE149319781B') {
            throw 'Streamline SDK archive checksum mismatch'
        }
        $expanded = Join-Path $env:TEMP 'starfox-streamline-sdk'
        Expand-Archive -LiteralPath $archive -DestinationPath $expanded -Force
        $headers = @(Get-ChildItem -LiteralPath $expanded -Recurse -Filter sl_security.h)
        if ($headers.Count -ne 1) { throw 'Unexpected Streamline SDK layout' }
        $sdk = $headers[0].Directory.Parent.FullName
        cmake -S tools/streamline_probe -B build/gitlab-dlss -G 'Visual Studio 17 2022' `
            -A x64 "-DSTREAMLINE_SDK=$sdk"
        Assert-Exit 'DLSS adapter configure'
        cmake --build build/gitlab-dlss --config Release --target starfox_dlss_native
        Assert-Exit 'DLSS adapter build'
        $adapter = Find-Binary build/gitlab-dlss starfox_dlss_native.dll

        $runtime = "StarFoxEnhanced-$version-windows-x64"
        cmake --install build/gitlab-windows-x64 --config Release --prefix $runtime
        Assert-Exit 'Windows x64 install'
        & ./tools/package_dlss.ps1 -SdkDirectory $sdk -Adapter $adapter -Destination $runtime
        if (-not (Test-Path -LiteralPath "$runtime/starfox_pc.exe")) {
            throw 'Windows x64 runtime is missing from the package'
        }
        Compress-Archive -Path "$runtime/*" -DestinationPath "release-out/$runtime.zip"
        $builder = "StarFoxAssetBuilder-$version-windows-x64"
        New-Item -ItemType Directory -Force $builder | Out-Null
        Copy-Item -LiteralPath $builderBinary -Destination "$builder/starfox_asset_builder.exe"
        Copy-Item platform/mobile/ASSET_BUILDER.md "$builder/README.md"
        Copy-Item THIRD_PARTY_NOTICES.md "$builder/"
        Compress-Archive -Path "$builder/*" -DestinationPath "release-out/$builder.zip"
    }
    'x86' {
        choco install ninja --yes --no-progress
        Assert-Exit 'Ninja installation'
        $archive = Join-Path $env:TEMP 'llvm-mingw-20260826.zip'
        $directory = Join-Path $env:TEMP 'llvm-mingw-20260826'
        Invoke-WebRequest `
            'https://github.com/mstorsjo/llvm-mingw/releases/download/20260826/llvm-mingw-20260826-ucrt-x86_64.zip' `
            -OutFile $archive
        Expand-Archive -LiteralPath $archive -DestinationPath $directory -Force
        $root = (Get-ChildItem -LiteralPath $directory -Directory | Select-Object -First 1).FullName
        if (-not $root) { throw 'LLVM-MinGW archive layout is empty' }
        & ./tools/build_windows_x86.ps1 -LlvmMingwRoot $root `
            -BuildDirectory build/gitlab-windows-x86 `
            -InstallDirectory dist/StarFoxEnhanced-windows-x86
        $binary = 'dist/StarFoxEnhanced-windows-x86/starfox_pc.exe'
        if (-not (Test-Path -LiteralPath $binary)) { throw 'x86 runtime was not installed' }
        $readobj = Join-Path $root 'bin/llvm-readobj.exe'
        $headers = (& $readobj --file-headers $binary | Out-String)
        Assert-Exit 'x86 PE inspection'
        if ($headers -notmatch 'IMAGE_FILE_MACHINE_I386') {
            throw 'Packaged runtime is not a 32-bit x86 PE image'
        }
        $package = "StarFoxEnhanced-$version-windows-x86"
        Move-Item dist/StarFoxEnhanced-windows-x86 $package
        Compress-Archive -Path "$package/*" -DestinationPath "release-out/$package.zip"
    }
    'pcvr' {
        cmake -S . -B build/gitlab-pcvr -G 'Visual Studio 17 2022' -A x64 `
            -DSTARFOX_BUILD_VR=ON -DSTARFOX_BUILD_TESTS=OFF `
            -DBUILD_TESTING=OFF -DSTARFOX_PACKAGE_MSU1_MUSIC=OFF
        Assert-Exit 'PCVR configure'
        cmake --build build/gitlab-pcvr --config Release --parallel 2 `
            --target starfox_pcvr starfox_asset_builder
        Assert-Exit 'PCVR build'
        $player = Find-Binary build/gitlab-pcvr starfox_pcvr.exe
        $builder = Find-Binary build/gitlab-pcvr starfox_asset_builder.exe
        & $builder
        if ($LASTEXITCODE -ne 2) { throw 'PCVR asset builder did not print usage' }
        & $player --help
        Assert-Exit 'PCVR startup check'
        Assert-Standalone $player $true
        cmake --install build/gitlab-pcvr --config Release --prefix pcvr-package --component pcvr
        Assert-Exit 'PCVR install'
        foreach ($file in @('starfox_pcvr.exe', 'starfox_asset_builder.exe',
                'BUILD-ASSETS.bat', 'LAUNCH-PCVR.bat', 'START-HERE.txt')) {
            if (-not (Test-Path -LiteralPath (Join-Path pcvr-package $file))) {
                throw "PCVR package is missing $file"
            }
        }
        foreach ($private in @('Starfox-Assets.BIN', 'SF.SFC', 'SFES.SFC',
                'Starfox-MSU1.PAK')) {
            if (Test-Path -LiteralPath (Join-Path pcvr-package $private)) {
                throw "PCVR package unexpectedly contains private asset $private"
            }
        }
        Copy-Item THIRD_PARTY_NOTICES.md,CREDITS.md -Destination pcvr-package
        Compress-Archive -Path pcvr-package/* `
            -DestinationPath "release-out/StarFoxEnhanced-$version-windows-pcvr.zip"
    }
    'uwp' {
        & ./tools/build_xbox_uwp.ps1 -BuildDirectory build/gitlab-xbox-uwp-x64 `
            -InstallDirectory dist/StarFoxEnhanced-xbox-uwp-x64
        $root = 'dist/StarFoxEnhanced-xbox-uwp-x64'
        $appx = Join-Path $root "StarFoxEnhanced-$version-xbox-uwp-x64.appx"
        if (-not (Test-Path -LiteralPath $appx -PathType Leaf)) { throw 'Xbox UWP package is missing' }
        $all = @(Get-ChildItem -LiteralPath $root -File |
            Where-Object { $_.Extension -in '.appx', '.msix', '.appxbundle', '.msixbundle' })
        if ($all.Count -ne 1 -or $all[0].Name -ne (Split-Path -Leaf $appx)) {
            throw 'Xbox UWP output contains stale or unexpected packages'
        }
        $certificate = Join-Path $root 'StarFoxEnhanced-UWP-Development.cer'
        if (-not (Test-Path -LiteralPath $certificate)) { throw 'UWP certificate is missing' }
        $dependencies = @(Get-ChildItem -LiteralPath "$root/Dependencies/x64" -File `
            -Filter '*.appx' -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^Microsoft\.VCLibs(?:\.140\.00.*|\.x64\.14\.00)\.appx$' })
        if ($dependencies.Count -ne 1) { throw 'UWP VCLibs dependency is missing or ambiguous' }
        $releaseRoot = 'build/gitlab-xbox-release'
        New-Item -ItemType Directory -Force "$releaseRoot/Dependencies/x64" | Out-Null
        Copy-Item -LiteralPath $appx -Destination $releaseRoot
        Copy-Item -LiteralPath $certificate -Destination $releaseRoot
        Copy-Item -LiteralPath "$root/README-XBOX-UWP.md" -Destination $releaseRoot
        Copy-Item -LiteralPath $dependencies[0].FullName -Destination "$releaseRoot/Dependencies/x64"
        Compress-Archive -Path "$releaseRoot/*" `
            -DestinationPath "release-out/StarFoxEnhanced-$version-xbox-uwp-x64.zip"
    }
}

if (-not @(Get-ChildItem -LiteralPath release-out -File).Count) {
    throw "No $Target release package was produced"
}
