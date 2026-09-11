param([switch]$Rebuild)
$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$source = Join-Path $workspace '.tools\Vulkan-ValidationLayers-src'
$build = Join-Path $workspace '.tools\Vulkan-ValidationLayers-build'
$install = Join-Path $workspace '.tools\Vulkan-ValidationLayers'
$revision = 'f4874eee15c78d7bdb2b7e60659d539f14741500'
if (-not (Test-Path -LiteralPath $source)) {
    & git clone --branch vulkan-sdk-1.4.357.0 --depth 1 https://github.com/KhronosGroup/Vulkan-ValidationLayers.git $source
    if ($LASTEXITCODE -ne 0) { throw 'Validation Layers clone failed' }
}
if ((& git -C $source rev-parse HEAD) -ne $revision) { throw 'Unexpected Vulkan-ValidationLayers revision' }
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
$configure = 'call "' + $vcvars + '" x64 && cmake -S "' + $source + '" -B "' + $build + '" -G Ninja -DUPDATE_DEPS=ON -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="' + $install + '"'
$compile = 'call "' + $vcvars + '" x64 && cmake --build "' + $build + '" --target install -j 8'
& cmd.exe /d /s /c $configure
if ($LASTEXITCODE -ne 0) { throw 'Validation Layers configuration failed' }
& cmd.exe /d /s /c $compile
if ($LASTEXITCODE -ne 0) { throw 'Validation Layers build failed' }
$validator = Get-ChildItem (Join-Path $source 'external') -Filter spirv-val.exe -Recurse | Where-Object FullName -like '*install\bin\spirv-val.exe' | Select-Object -First 1
if ($validator) { Copy-Item -LiteralPath $validator.FullName -Destination (Join-Path $install 'bin\spirv-val.exe') -Force }
Write-Host "Set VK_LAYER_PATH=$install\bin for an isolated validation run."
