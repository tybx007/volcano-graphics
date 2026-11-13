# capture.ps1 - save a PNG screenshot of a window (by process name or title)
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\capture.ps1 -ProcessName stage1 -Out screenshots\shot.png
#   powershell -ExecutionPolicy Bypass -File tools\capture.ps1 -Title "VolcanoX" -Out screenshots\shot.png
param(
    [string]$Title = "",
    [string]$ProcessName = "volcano",
    [string]$Out = "screenshot.png"
)

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class WinCap {
    [DllImport("user32.dll")] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

$h = [IntPtr]::Zero
$p = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if ($p) { $h = $p.MainWindowHandle }
if ($h -eq [IntPtr]::Zero -and $Title -ne "") { $h = [WinCap]::FindWindow($null, $Title) }
if ($h -eq [IntPtr]::Zero) {
    Write-Error "No window found (process '$ProcessName', title '$Title')"
    exit 1
}

[WinCap]::ShowWindow($h, 9) | Out-Null
[WinCap]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 500

$rect = New-Object WinCap+RECT
[WinCap]::GetWindowRect($h, [ref]$rect) | Out-Null
$w = $rect.Right - $rect.Left
$hgt = $rect.Bottom - $rect.Top

$bmp = New-Object System.Drawing.Bitmap($w, $hgt)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bmp.Size)
$dir = Split-Path -Parent $Out
if ($dir -and !(Test-Path $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null }
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()
Write-Output "Saved $Out ($w x $hgt)"
