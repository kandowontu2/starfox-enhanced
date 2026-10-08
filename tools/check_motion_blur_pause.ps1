param([string]$Log, [switch]$SelfTest, [ValidateRange(1,2)][int]$EyeCount=1)
$ErrorActionPreference='Stop'

function Test-MotionPauseHistory([string[]]$Lines, [int]$ExpectedEyes=1) {
    $paused=$false; $blur=$null; $fallback=$false
    $blurCount=0
    $particleHistory=$null; $particleEyes=0; $sceneMatches=$null
    $phase=0; $pauseCount=0; $resumedCount=0; $lastSerial=0
    foreach($line in $Lines) {
        if($line -match 'scene-motion: points=\d+ matched=(\d+) paused=([01])') {
            $sceneMatches=[int]$Matches[1]
            if($Matches[2] -eq '1' -and $sceneMatches -ne 0) {throw 'Paused particles retained motion correspondence'}
        }
        if($line -match 'particle-shutter-live: count=\d+ history=([01])') {
            $value=[int]$Matches[1]
            if($null -ne $particleHistory -and $particleHistory -ne $value) {throw 'Particle eyes disagree about history'}
            $particleHistory=$value; ++$particleEyes
        }
        if($line -match 'temporal-paused-native:') {$paused=$true}
        if($line -match 'motion-blur-live: unavailable|motion-blur-live: applied=0') {
            throw 'The requested blur path failed or declined a frame'
        }
        if($line -match 'motion-blur-live: applied=1 history=([01])') {
            $history=[int]$Matches[1]
            if($null -ne $blur -and $blur -ne $history) {throw 'Eyes disagree about blur history'}
            $blur=$history; ++$blurCount
        }
        if($line -match 'unjittered scene replay') {$fallback=$true}
        if($line -notmatch 'temporal-inputs: serial=(\d+) previous=(\d+) depth=\d+ motion=([01])') {continue}
        $serial=[long]$Matches[1]; $previous=[int]$Matches[2]; $motion=[int]$Matches[3]
        if($lastSerial -and $serial -ne $lastSerial+1) {throw 'Missing frame records'}
        $lastSerial=$serial
        if($blurCount -ne 0 -and $blurCount -ne $ExpectedEyes) {throw 'Incomplete eye pair'}
        if($particleEyes -ne 0 -and ($particleEyes -ne $ExpectedEyes -or $particleHistory -ne $blur)) {
            throw 'Incomplete or inconsistent particle exposure pair'
        }
        if($paused) {
            if($phase -eq 0) {throw 'No working blur history before pause'}
            if($phase -ge 3) {throw 'Unexpected second pause in this fixture'}
            if($null -eq $blur -or $blur -ne 0) {throw 'Paused frame did not bypass exposure'}
            ++$pauseCount; $phase=2
        } elseif($phase -eq 2) {
            if($pauseCount -lt 2) {throw 'Pause was too short to validate'}
            if($previous -ne 0 -or $motion -ne 0 -or $blur -eq 1) {throw 'Resume reused pre-pause motion'}
            if($null -ne $sceneMatches -and $sceneMatches -ne 0) {throw 'Resume reused pre-pause particle correspondence'}
            if($null -eq $blur -and !$fallback) {throw 'Resume has no identity or fallback evidence'}
            $phase=3
        } elseif($phase -eq 3) {
            if($blur -eq 1 -and $motion -eq 1 -and $previous -gt 0) {++$resumedCount}
        } elseif($blur -eq 1) {$phase=1}
        $paused=$false; $blur=$null; $fallback=$false; $blurCount=0
        $particleHistory=$null; $particleEyes=0; $sceneMatches=$null
    }
    if($phase -ne 3 -or $resumedCount -lt 2) {throw 'No sustained motion blur after resume'}
    [pscustomobject]@{PausedFrames=$pauseCount; ResumedBlurFrames=$resumedCount}
}

if($SelfTest) {
    $sample=@(
        'motion-blur-live: applied=1 history=1','temporal-inputs: serial=1 previous=2 depth=1 motion=1',
        'temporal-paused-native: frame=1','motion-blur-live: applied=1 history=0','temporal-inputs: serial=2 previous=0 depth=1 motion=0',
        'temporal-paused-native: frame=2','motion-blur-live: applied=1 history=0','temporal-inputs: serial=3 previous=2 depth=1 motion=1',
        'dlss-fallback: unjittered scene replay','temporal-inputs: serial=4 previous=0 depth=1 motion=0',
        'motion-blur-live: applied=1 history=1','temporal-inputs: serial=5 previous=2 depth=1 motion=1',
        'motion-blur-live: applied=1 history=1','temporal-inputs: serial=6 previous=2 depth=1 motion=1')
    $null=Test-MotionPauseHistory $sample
    foreach($bad in 0..3) {
        $candidate=$sample.Clone()
        switch($bad) {
            0 {$candidate[3]='motion-blur-live: applied=1 history=1'}
            1 {$candidate[9]='temporal-inputs: serial=4 previous=2 depth=1 motion=1'}
            2 {$candidate=$candidate[0..9]}
            3 {$candidate[13]='temporal-inputs: serial=7 previous=2 depth=1 motion=1'}
        }
        $rejected=$false
        try {$null=Test-MotionPauseHistory $candidate} catch {$rejected=$true}
        if(!$rejected) {throw "Negative pause fixture $bad was accepted"}
    }
    foreach($injected in @(
        'scene-motion: points=3 matched=2 paused=1',
        'particle-shutter-live: count=3 history=0',
        "particle-shutter-live: count=3 history=1`nparticle-shutter-live: count=3 history=1")) {
        $candidate=@($injected -split "`n")+$sample
        $rejected=$false
        try {$null=Test-MotionPauseHistory $candidate} catch {$rejected=$true}
        if(!$rejected) {throw 'Invalid particle pause fixture was accepted'}
    }
    'Pause verifier positive and seven negative fixtures passed'
}
if($Log) {Test-MotionPauseHistory (Get-Content -LiteralPath $Log) $EyeCount}
elseif(!$SelfTest) {throw 'Provide -Log runtime.log or -SelfTest'}
