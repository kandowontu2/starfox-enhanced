param(
    [Parameter(Mandatory=$true)][string]$JdkRoot,
    [Parameter(Mandatory=$true)][string]$SdkRoot,
    [string]$BuildRoot
)
$ErrorActionPreference='Stop'
$questSource=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$questJdk=(Resolve-Path $JdkRoot).Path
$questSdk=(Resolve-Path $SdkRoot).Path
if(!(Test-Path (Join-Path $questJdk 'bin/javac.exe'))){throw 'JdkRoot must contain a full JDK (17 or newer), not a JRE'}
$questAar=Join-Path $questSource 'platform/android/app/libs/SDL3-3.4.14.aar'
# Use the same pinned official archive and digest as the Unix build helper.
# A clean Windows checkout must not depend on a previous Android/Unix build.
if(!(Test-Path $questAar)) {
    $questArchive=Join-Path $questSource 'build/downloads/SDL3-devel-3.4.14-android.zip'
    New-Item -ItemType Directory -Force -Path (Split-Path $questArchive),(Split-Path $questAar) | Out-Null
    $questDigest='e41691e75433b2a0a75685781bed2160fe4a85f75f3803f7f43d1811e212e3ef'
    if(!(Test-Path $questArchive) -or (Get-FileHash $questArchive -Algorithm SHA256).Hash -ne $questDigest) {
        Invoke-WebRequest -Uri 'https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-devel-3.4.14-android.zip' -OutFile $questArchive
    }
    if((Get-FileHash $questArchive -Algorithm SHA256).Hash -ne $questDigest) {throw 'SDL archive checksum mismatch'}
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $questZip=[IO.Compression.ZipFile]::OpenRead($questArchive)
    try {
        $questEntry=$questZip.GetEntry('SDL3-3.4.14.aar')
        if(!$questEntry){throw 'Pinned SDL archive has no expected AAR'}
        [IO.Compression.ZipFileExtensions]::ExtractToFile($questEntry,$questAar,$true)
    } finally {$questZip.Dispose()}
}
# Set the toolchain only for this process and restore the caller's environment.
$questOldJava=$env:JAVA_HOME
$questOldSdk=$env:ANDROID_HOME
$questOldSdkRoot=$env:ANDROID_SDK_ROOT
$questOldBuildRoot=$env:STARFOX_QUEST_BUILD_ROOT
try {
    $env:JAVA_HOME=$questJdk
    $env:ANDROID_HOME=$questSdk
    $env:ANDROID_SDK_ROOT=$questSdk
    if($BuildRoot){$env:STARFOX_QUEST_BUILD_ROOT=[IO.Path]::GetFullPath($BuildRoot)}
    & (Join-Path $questSource 'platform/android/gradlew.bat') -p (Join-Path $questSource 'platform/android') --no-daemon ':quest:assembleDebug'
    if($LASTEXITCODE -ne 0){throw "Quest build failed ($LASTEXITCODE)"}
    $questApk=Join-Path $questSource 'platform/quest/build/outputs/apk/debug/quest-debug.apk'
    if(!(Test-Path $questApk)){throw 'Gradle returned success without the Quest APK'}
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $questPayload=[IO.Compression.ZipFile]::OpenRead($questApk)
    try {
        foreach($questEntry in @('lib/arm64-v8a/libstarfox_quest.so','lib/arm64-v8a/libSDL3.so','AndroidManifest.xml')) {
            $questItem=$questPayload.GetEntry($questEntry)
            if(!$questItem -or $questItem.Length -eq 0){throw "Quest APK is missing $questEntry"}
        }
        if($questPayload.GetEntry('lib/arm64-v8a/libmain.so')){throw 'Quest APK unexpectedly contains the flat game entry library'}
        foreach($questEntry in $questPayload.Entries) {
            if($questEntry.FullName -match '^lib/[^/]+/lib(android|log)\.so$') {
                throw "Quest APK contains an NDK platform link stub: $($questEntry.FullName)"
            }
        }
    } finally {$questPayload.Dispose()}
    Write-Output "Development APK: $questApk"
    Get-FileHash -LiteralPath $questApk -Algorithm SHA256 | Format-List
    Write-Output 'Building an APK does not verify headset rendering or presentation parity.'
} finally {
    $env:JAVA_HOME=$questOldJava
    $env:ANDROID_HOME=$questOldSdk
    $env:ANDROID_SDK_ROOT=$questOldSdkRoot
    $env:STARFOX_QUEST_BUILD_ROOT=$questOldBuildRoot
}
