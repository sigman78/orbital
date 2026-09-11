param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [string]$Capture = "captures/window.bmp",
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = 'Stop'
$native = @"
using System;
using System.Runtime.InteropServices;
public static class SmokeWin32 {
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool repaint);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  public const int SW_RESTORE=9, WM_KEYDOWN=0x100, WM_KEYUP=0x101;
}
"@
Add-Type -TypeDefinition $native

$capturePath = [IO.Path]::GetFullPath($Capture)
$captureDir = Split-Path -Parent $capturePath
New-Item -ItemType Directory -Force -Path $captureDir | Out-Null
$proc = Start-Process -FilePath $Executable -ArgumentList "--duration 15 --width 960 --height 540 --capture `"$capturePath`"" -PassThru -WindowStyle Hidden
try {
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$hwnd = [IntPtr]::Zero
$callback = [SmokeWin32+EnumProc]{ param($h,$p); $id=0; $name=New-Object Text.StringBuilder 256; [SmokeWin32]::GetWindowThreadProcessId($h,[ref]$id)|Out-Null; [SmokeWin32]::GetClassName($h,$name,256)|Out-Null; if($id -eq $proc.Id -and $name.ToString() -eq 'OrbitalWindow'){$script:found=$h; return $false}; return $true }
$script:found=[IntPtr]::Zero
while ($script:found -eq [IntPtr]::Zero -and -not $proc.HasExited -and [DateTime]::UtcNow -lt $deadline) {
    [SmokeWin32]::EnumWindows($callback,[IntPtr]::Zero)|Out-Null
    if ($script:found -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 100 }
}
if ($proc.HasExited) { throw "Application exited before creating a window (exit $($proc.ExitCode))." }
$hwnd=$script:found
if($hwnd -eq [IntPtr]::Zero){throw 'No application window found'}
[SmokeWin32]::ShowWindow($hwnd, [SmokeWin32]::SW_RESTORE) | Out-Null
[SmokeWin32]::ShowWindow($hwnd, 6) | Out-Null; Start-Sleep -Milliseconds 300
[SmokeWin32]::ShowWindow($hwnd, [SmokeWin32]::SW_RESTORE) | Out-Null
[SmokeWin32]::MoveWindow($hwnd, 40, 40, 960, 540, $true) | Out-Null
foreach ($size in @(@(960,540), @(1280,720), @(800,600))) {
    [SmokeWin32]::MoveWindow($hwnd, 40, 40, $size[0], $size[1], $true) | Out-Null
    Start-Sleep -Milliseconds 300
}
foreach ($key in @(0x31,0x32,0x33,0x34,0x35,0x54,0x46,0x20,0x71)) {
    [SmokeWin32]::PostMessage($hwnd, [SmokeWin32]::WM_KEYDOWN, [IntPtr]$key, [IntPtr]::Zero) | Out-Null
    [SmokeWin32]::PostMessage($hwnd, [SmokeWin32]::WM_KEYUP, [IntPtr]$key, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 300
}
if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) { $proc.Kill(); throw "Application did not exit within timeout." }
if ($proc.ExitCode -ne 0) { throw "Application exited with code $($proc.ExitCode)." }
if (-not (Test-Path -LiteralPath $capturePath)) { throw "Expected capture was not written: $capturePath" }
if ((Get-Item -LiteralPath $capturePath).Length -le 54) { throw "Capture is empty or not a BMP: $capturePath" }
Write-Output "Window smoke passed: exit=$($proc.ExitCode), capture=$capturePath"
} finally { if ($proc -and -not $proc.HasExited) { $proc.Kill(); $proc.WaitForExit() } }
