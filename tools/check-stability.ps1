param(
    [string]$Executable = "build/release/orbital.exe",
    [string]$OutputDirectory = "captures/stability",
    [int]$TimeoutSeconds = 45
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($OutputDirectory); New-Item -ItemType Directory -Force $root | Out-Null
$paths = @((Join-Path $root 'fixed-a.png'), (Join-Path $root 'fixed-b.png'))
$procs = @()
try {
  for ($i=0; $i -lt 2; $i++) {
    $frames = if ($i -eq 0) { 120 } else { 124 }
    $args = "--bookmark 4 --time 0 --no-hud --frames $frames --width 1280 --height 720 --capture `"$($paths[$i])`""
    $p = Start-Process -FilePath $Executable -ArgumentList $args -PassThru -WindowStyle Hidden; $procs += $p
    if (-not $p.WaitForExit($TimeoutSeconds * 1000)) { throw 'stability process timed out' }
    if ($p.ExitCode -ne 0) { throw "stability process exit $($p.ExitCode)" }
  }
  Add-Type -AssemblyName System.Drawing
  $a = [Drawing.Bitmap]::new($paths[0]); $b = [Drawing.Bitmap]::new($paths[1])
  if ($a.Width -ne $b.Width -or $a.Height -ne $b.Height) { throw 'capture dimensions differ' }
  [double]$sum=0; [int]$max=0; [int]$changed=0; [int]$total=$a.Width*$a.Height
  for ($y=0; $y -lt $a.Height; $y++) { for ($x=0; $x -lt $a.Width; $x++) {
    $pa=$a.GetPixel($x,$y); $pb=$b.GetPixel($x,$y)
    $d=[Math]::Max([Math]::Max([Math]::Abs($pa.R-$pb.R),[Math]::Abs($pa.G-$pb.G)),[Math]::Abs($pa.B-$pb.B))
    $sum += $d; if ($d -gt $max) {$max=$d}; if ($d -gt 1) {$changed++}
  }}
  $a.Dispose(); $b.Dispose(); $mean=$sum/$total; $pct=100.0*$changed/$total
  Write-Output ("mean_abs_delta={0:N4} max_delta={1} percent_changed={2:N4}" -f $mean,$max,$pct)
} finally {
  foreach ($p in $procs) { if ($p -and -not $p.HasExited) { $p.Kill(); $p.WaitForExit() } }
}
