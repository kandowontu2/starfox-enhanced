param(
    [Parameter(Mandatory=$true)][string]$Serial,
    [Parameter(Mandatory=$true)][string]$SdkRoot,
    [string]$Apk = 'platform/quest/build/outputs/apk/debug/quest-debug.apk',
    [switch]$Launch,
    [switch]$StopRunning
)
$ErrorActionPreference = 'Stop'
$questAdb = Join-Path (Resolve-Path $SdkRoot).Path 'platform-tools/adb.exe'
if (!(Test-Path -LiteralPath $questAdb)) { throw 'SDK platform-tools/adb.exe is missing' }
if ($Serial -notmatch '^[A-Za-z0-9._:-]+$') { throw 'Invalid device serial' }
$questApk = (Resolve-Path -LiteralPath $Apk).Path
if (!(Test-Path -LiteralPath $questApk -PathType Leaf)) { throw 'APK must be a file' }

function Invoke-QuestAdb([string[]]$Arguments) {
    # Windows PowerShell turns native stderr into ErrorRecords. Capture those
    # without throwing before we can check adb's actual exit status.
    $questOldPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $questOutput = & $questAdb -s $Serial @Arguments 2>&1
        $questExitCode = $LASTEXITCODE
    } finally { $ErrorActionPreference = $questOldPreference }
    if ($questExitCode -ne 0) { throw "ADB failed: $($questOutput -join "`n")" }
    return $questOutput
}

# Never pick the first attached device: phones and other headsets may coexist.
$questState = (Invoke-QuestAdb @('get-state') | Out-String).Trim()
if ($questState -ne 'device') { throw "Quest is not ready: $questState" }
$questModel = (Invoke-QuestAdb @('shell', 'getprop', 'ro.product.model') | Out-String).Trim()
if ($questModel -ne 'Quest 3') { throw "Expected Quest 3; selected device reports '$questModel'" }
# pidof returns status 1 for a stopped app. Do not treat that expected status as
# an ADB transport failure, or replace a live game and interrupt the user's run.
$questRunning = & $questAdb -s $Serial shell pidof com.starfox.enhanced.quest
$questPidStatus = $LASTEXITCODE
if ($questPidStatus -notin @(0,1)) { throw "Could not check running Quest app (ADB exit $questPidStatus)" }
if ($questPidStatus -eq 0 -or ($questRunning | Out-String).Trim()) {
    if (!$StopRunning) {
        throw 'The Quest game is running. Close it before deployment or use -StopRunning with user authorization; no installation or data changes were made.'
    }
    Write-Output 'Stopping the game for the authorized update. Unsaved session progress may be lost; saved data is retained.'
    Invoke-QuestAdb @('shell', 'am', 'force-stop', 'com.starfox.enhanced.quest') | Write-Output
    $questRunning = & $questAdb -s $Serial shell pidof com.starfox.enhanced.quest
    if ($LASTEXITCODE -ne 1 -or ($questRunning | Out-String).Trim()) {
        throw 'The Quest game did not stop cleanly; installation was not attempted.'
    }
}
Write-Output "Installing on Quest 3 ($Serial). Existing inputs and saves are retained."
# No uninstall, downgrade, data clear, or ROM/save replacement on failure.
$questInstall = Invoke-QuestAdb @('install', '-r', $questApk)
$questInstall | Write-Output
if (($questInstall -join "`n") -notmatch '(?m)^Success\s*$') {
    throw 'Package manager did not confirm a successful installation'
}
if ($Launch) {
    $questLaunch = Invoke-QuestAdb @('shell', 'am', 'start', '-W', '-a',
        'android.intent.action.MAIN', '-c', 'com.oculus.intent.category.VR', '-n',
        'com.starfox.enhanced.quest/.QuestActivity')
    $questLaunch | Write-Output
    if (($questLaunch -join "`n") -notmatch '(?m)^Status: ok\s*$') {
        throw 'Activity manager did not confirm startup'
    }
    if (($questLaunch -join "`n") -match '(?m)^Activity: (.+)\s*$') {
        $questLaunchedActivity = $Matches[1].Trim()
        if ($questLaunchedActivity -ne 'com.starfox.enhanced.quest/.QuestActivity' -and
            $questLaunchedActivity -ne 'com.starfox.enhanced.quest/com.starfox.enhanced.quest.QuestActivity') {
            throw "APK installed, but the headset opened '$questLaunchedActivity' instead. Resolve the headset prompt before testing."
        }
    }
    Write-Output 'Activity launch is not proof of working stereo rendering. Verify the view and controls inside the headset.'
}
