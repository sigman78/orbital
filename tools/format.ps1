<#
.SYNOPSIS
    Formats (or checks) the project's C++ sources with clang-format.
.PARAMETER Check
    Report files that are not formatted and exit non-zero instead of rewriting them.
#>
param([switch]$Check)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $clangFormat) { throw 'clang-format was not found on PATH. Install LLVM 17 or newer.' }

$files = Get-ChildItem -Path (Join-Path $root 'src'), (Join-Path $root 'tests') -Recurse -Include *.cpp, *.hpp, *.h |
    Select-Object -ExpandProperty FullName
if (-not $files) { throw 'No source files found.' }

$arguments = @('--style=file')
if ($Check) { $arguments += '--dry-run', '--Werror' } else { $arguments += '-i' }
& $clangFormat.Source @arguments @files
if ($LASTEXITCODE -ne 0) {
    if ($Check) { throw 'Formatting check failed. Run tools/format.ps1 to fix.' }
    throw "clang-format exited with code $LASTEXITCODE"
}
if ($Check) { Write-Host "Formatting check passed ($($files.Count) files)." }
else { Write-Host "Formatted $($files.Count) files." }
