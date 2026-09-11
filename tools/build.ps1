param([ValidateSet('release','debug')][string]$Preset = 'release')
$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vcvars = $env:ORBITAL_VCVARS
if (-not $vcvars) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($installation) { $vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvarsall.bat' }
    }
}
if (-not $vcvars -or -not (Test-Path -LiteralPath $vcvars)) {
    throw 'MSVC environment not found. Set ORBITAL_VCVARS to the absolute vcvarsall.bat path.'
}
$command = 'call "' + $vcvars + '" x64 && cd /d "' + $workspace + '" && cmake --preset ' + $Preset + ' && cmake --build --preset ' + $Preset
& cmd.exe /d /s /c $command
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
