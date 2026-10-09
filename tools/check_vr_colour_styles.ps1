param([string]$OutputDirectory='tmp/vr-colour-style-matrix',
    [int[]]$Styles=@(1,4,8,9,10,11,13,14,15,16))
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class VrStylePixels {
    public static void Blend(string a, string b, string middle, bool zero) {
        var off=File.ReadAllBytes(a);var full=File.ReadAllBytes(b);var half=File.ReadAllBytes(middle);
        {
            int width=BitConverter.ToInt32(off,18),height=Math.Abs(BitConverter.ToInt32(off,22));
            int bits=BitConverter.ToInt16(off,28),offset=BitConverter.ToInt32(off,10);
            if((bits!=24 && bits!=32) || BitConverter.ToInt32(off,30)!=0) throw new Exception("Unsupported BMP");
            if(off.Length!=full.Length || off.Length!=half.Length) throw new Exception("Style dimensions changed");
            for(int i=0;i<offset;i++) if(off[i]!=full[i] || off[i]!=half[i]) throw new Exception("BMP header changed");
            int stride=((width*bits+31)/32)*4;
            int changed=0;
            for(int y=0;y<height;y++) for(int x=0;x<width;x++) {
                for(int c=0;c<3;c++) {
                    int at=offset+y*stride+x*(bits/8)+c;
                    if(off[at]!=full[at]) changed++;
                    double expected=zero?off[at]:(off[at]+full[at])*.5;
                    if(Math.Abs(half[at]-expected)>(zero?0:1.1))
                        throw new Exception("Style intensity mismatch at "+x+","+y+" channel "+c);
                }
            }
            if(changed<100) throw new Exception("Style did not change enough visible pixels");
        }
    }
}
'@
$root=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $root) {throw 'Use a new proof directory to preserve earlier results'}
New-Item -ItemType Directory -Path $root | Out-Null
function Capture([string]$name,[string]$flags) {
    $directory=Join-Path $root $name
    & build/vr-dev/starfox_vr_scene_check.exe $directory tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt "--live-stage=LEVEL5_1:2200@vr-world@background-only$flags@from-entry" |
        Select-String 'passed; no headset' | ForEach-Object { Write-Host $_.Line }
    if($LASTEXITCODE -ne 0) {throw "Style capture failed: $name"}
    return (Join-Path $directory 'live-scene-left.bmp')
}
# Capture writes progress to the host stream; returned value is only the image.
$baseline=Capture 'off' ''
foreach($style in $Styles) {
    $full=Capture "style-$style-full" "@style-$style"
    $half=Capture "style-$style-half" "@style-$style@intensity-50"
    $zero=Capture "style-$style-zero" "@style-$style@intensity-0"
    [VrStylePixels]::Blend($baseline,$full,$half,$false)
    [VrStylePixels]::Blend($baseline,$full,$zero,$true)
    Write-Host "Style ${style}: zero is exact OFF; half matches midpoint; full changes pixels."
}
