# probe.ps1 - print selected pixels and a coarse classified colour map.
# Usage: powershell -File tools/probe.ps1 screenshots/volcano_bench.bmp
param([string]$Path = "screenshots/volcano_bench.bmp")

Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Path))
Write-Output ("IMAGE {0}x{1}" -f $img.Width, $img.Height)

$points = @(
    @(600, 40,  "sky top centre"),
    @(300, 120, "sky upper left"),
    @(1030, 210, "moon centre"),
    @(1050, 230, "moon lower right"),
    @(600, 271, "crater lake"),
    @(600, 300, "crater front lip"),
    @(600, 450, "cone centre"),
    @(300, 600, "cone left flank"),
    @(1000, 600, "cone right flank"),
    @(600, 665, "cone foot"),
    @(100, 700, "foreground left"),
    @(1100, 700, "foreground right"),
    @(600, 740, "control bar"),
    @(200, 100, "title panel"),
    @(1000, 70, "stats panel")
)
foreach ($p in $points) {
    $c = $img.GetPixel($p[0], $p[1])
    Write-Output ("{0,-20} ({1,4},{2,4}) = ({3,3},{4,3},{5,3})" -f $p[2], $p[0], $p[1], $c.R, $c.G, $c.B)
}

# coarse classification map (60 x 38)
function Classify($c) {
    $r = $c.R; $g = $c.G; $b = $c.B
    if ($r -gt 200 -and $g -gt 150 -and $b -gt 120) { return 'O' }   # moon / white hot
    if ($r -gt 130 -and $g -gt 30 -and $b -lt 120 -and $r -gt $b + 55) { return 'L' } # lava
    if ($r -gt 70 -and $g -gt 55 -and $b -gt 45 -and $r -gt $b) { return 'r' } # lit rock
    if ($b -gt $r -and $b -gt 18) { return 's' }                    # blue sky
    if ($r -lt 20 -and $g -lt 20 -and $b -lt 26) { return '#' }     # panel / black
    return '.'
}

Write-Output "CLASS MAP  s=sky L=lava r=rock #=black O=bright"
$cols = 100; $rows = 46
for ($ry = 0; $ry -lt $rows; $ry++) {
    $line = ""
    for ($rx = 0; $rx -lt $cols; $rx++) {
        $x = [int](($rx + 0.5) * $img.Width / $cols)
        $y = [int](($ry + 0.5) * $img.Height / $rows)
        $line += Classify $img.GetPixel($x, $y)
    }
    Write-Output $line
}
$img.Dispose()
