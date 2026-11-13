# inspect.ps1 - text-based inspection of a screenshot (useful for automated checks)
# Prints: size, region colour averages, an ASCII luminance map and lava-pixel counts.
param(
    [string]$Path = "screenshots/shot.png",
    [int]$Cols = 100,
    [int]$Rows = 48
)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Path))
Write-Output ("IMAGE {0}x{1}" -f $img.Width, $img.Height)

function Sample($x, $y) {
    $c = $img.GetPixel([int]$x, [int]$y)
    return @($c.R, $c.G, $c.B)
}

# region averages (3x3 grid)
Write-Output "REGION AVG (r,g,b) 3x3:"
for ($ry = 0; $ry -lt 3; $ry++) {
    $line = ""
    for ($rx = 0; $rx -lt 3; $rx++) {
        $sr=0; $sg=0; $sb=0; $n=0
        for ($y = [int]($ry*$img.Height/3); $y -lt [int](($ry+1)*$img.Height/3); $y += 7) {
            for ($x = [int]($rx*$img.Width/3); $x -lt [int](($rx+1)*$img.Width/3); $x += 7) {
                $c = $img.GetPixel($x,$y); $sr+=$c.R; $sg+=$c.G; $sb+=$c.B; $n++
            }
        }
        $line += ("({0,3},{1,3},{2,3})  " -f [int]($sr/$n), [int]($sg/$n), [int]($sb/$n))
    }
    Write-Output $line
}

# colour statistics
$lava=0; $sky=0; $smoke=0; $bright=0; $total=0
for ($y=0; $y -lt $img.Height; $y+=2) {
    for ($x=0; $x -lt $img.Width; $x+=2) {
        $c = $img.GetPixel($x,$y); $total++
        if ($c.R -gt 150 -and $c.G -gt 40 -and $c.B -lt 110 -and $c.R -gt ($c.B+60)) { $lava++ }
        if ($c.B -gt $c.R -and $c.B -gt 20 -and $c.R -lt 90) { $sky++ }
        if ([math]::Abs($c.R-$c.G) -lt 20 -and [math]::Abs($c.G-$c.B) -lt 20 -and $c.R -gt 40 -and $c.R -lt 150) { $smoke++ }
        if ($c.R -gt 180 -and $c.G -gt 160 -and $c.B -gt 120) { $bright++ }
    }
}
Write-Output ("PIXELS sampled={0} lava={1} ({2:P2}) sky={3} smoke-gray={4} bright={5}" -f $total,$lava,($lava/$total),$sky,$smoke,$bright)

# ASCII map: luminance; lava pixels as '*', bright as 'O'
$chars = " .:-=+*#%@"
Write-Output "ASCII MAP (luminance, lava='X', bright='O'):"
for ($r=0; $r -lt $Rows; $r++) {
    $line = ""
    for ($c=0; $c -lt $Cols; $c++) {
        $x0=[int]($c*$img.Width/$Cols); $x1=[int](($c+1)*$img.Width/$Cols)
        $y0=[int]($r*$img.Height/$Rows); $y1=[int](($r+1)*$img.Height/$Rows)
        $sr=0;$sg=0;$sb=0;$n=0;$lavaN=0;$maxL=0
        for ($y=$y0; $y -lt [math]::Min($y1,$img.Height); $y+=3) {
            for ($x=$x0; $x -lt [math]::Min($x1,$img.Width); $x+=3) {
                $c2=$img.GetPixel($x,$y); $sr+=$c2.R; $sg+=$c2.G; $sb+=$c2.B; $n++
                $l=(0.299*$c2.R+0.587*$c2.G+0.114*$c2.B)
                if ($l -gt $maxL) {$maxL=$l}
                if ($c2.R -gt 150 -and $c2.G -gt 30 -and $c2.B -lt 120 -and $c2.R -gt ($c2.B+60)) { $lavaN++ }
            }
        }
        $ar=$sr/$n; $ag=$sg/$n; $ab=$sb/$n
        $lum=(0.299*$ar+0.587*$ag+0.114*$ab)
        $idx=[int]([math]::Min(9,[math]::Max(0,($lum/255.0)*9)))
        $ch=$chars[$idx]
        if ($lavaN -gt ($n*0.12)) { $ch='X' }
        if ($ar -gt 180 -and $ag -gt 170 -and $ab -gt 140) { $ch='O' }
        $line += $ch
    }
    Write-Output $line
}
$img.Dispose()
