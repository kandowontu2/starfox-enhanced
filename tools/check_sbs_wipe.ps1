param([string]$OutputDirectory='tmp/sbs-wipe-regression',
    [ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL',
    [ValidateSet(1,2)][int]$Mode=2)
$ErrorActionPreference='Stop'
& "$PSScriptRoot/check_sbs_presentation.ps1" -OutputDirectory $OutputDirectory -Experience $Experience -Mode $Mode -Frames 480 -Sequence -ScrambleWipe -PresentationFps 240
Add-Type -AssemblyName System.Drawing
$closed=$false; $previousTop=112; $previousBottom=111; $fullFrame=0; $changes=0
$eyeWidth=if($Mode -eq 2){400}else{200}
for($frame=1;$frame -le 480;++$frame) {
    $path=[IO.Path]::GetFullPath((Join-Path $OutputDirectory "presentation.bmp.frame-$frame.bmp"))
    $bmp=[System.Drawing.Bitmap]::new($path)
    $bounds=@()
    try {
        foreach($offset in @(0,$eyeWidth)) {
            $top=224; $bottom=-1
            for($y=0;$y -lt 224;++$y) {
                foreach($fraction in @(.025,.25,.5,.75)) {
                    $c=$bmp.GetPixel(($offset+[int]($eyeWidth*$fraction)),$y)
                    if(($c.R+$c.G+$c.B) -gt 0) {$top=[Math]::Min($top,$y); $bottom=$y; break}
                }
            }
            $bounds+=,@($top,$bottom)
        }
    } finally {$bmp.Dispose()}
    if($bounds[0][0] -ne $bounds[1][0] -or $bounds[0][1] -ne $bounds[1][1]) {throw "Eye bounds differ at frame $frame"}
    $top=$bounds[0][0]; $bottom=$bounds[0][1]
    if($bottom -lt 0) {$closed=$true; continue}
    # The injected source command starts after the first presentation.
    if(!$closed -or $fullFrame) {continue}
    if($top -gt $previousTop -or $bottom -lt $previousBottom) {throw "Opening reversed at frame $frame"}
    if(($previousTop-$top) -gt 2 -or ($bottom-$previousBottom) -gt 2) {throw "Opening jumped at frame $frame"}
    if($top -ne $previousTop -or $bottom -ne $previousBottom) {++$changes}
    $previousTop=$top; $previousBottom=$bottom
    if($top -eq 0 -and $bottom -eq 223) {$fullFrame=$frame}
}
if(!$closed -or !$fullFrame) {throw 'Complete opening was not observed'}
Write-Output "PASS $Experience SBS $Mode complete wipe: $changes sampled edge positions, fully open at frame $fullFrame; no edge step exceeds 2 pixels."
