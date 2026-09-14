param([switch]$Rebuild, [ValidateRange(1,64)][int]$Jobs = 8)
$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$source = Join-Path $workspace '.tools\Vulkan-ValidationLayers-src'
$build = Join-Path $workspace '.tools\Vulkan-ValidationLayers-build'
$install = Join-Path $workspace '.tools\Vulkan-ValidationLayers'
$pins = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'validation-dependencies.json') -Raw | ConvertFrom-Json
$revision = $pins.revision
if (-not (Test-Path -LiteralPath $source)) {
    & git clone --branch $pins.tag --depth 1 https://github.com/KhronosGroup/Vulkan-ValidationLayers.git $source
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
$env:CMAKE_BUILD_PARALLEL_LEVEL = [string]$Jobs
$configure = 'call "' + $vcvars + '" x64 && cmake -S "' + $source + '" -B "' + $build + '" -G Ninja -DUPDATE_DEPS=ON -DBUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="' + $install + '"'
$compile = 'call "' + $vcvars + '" x64 && cmake --build "' + $build + '" --target install -j ' + $Jobs + $(if ($Rebuild) { ' --clean-first' } else { '' })
& cmd.exe /d /s /c $configure
if ($LASTEXITCODE -ne 0) { throw 'Validation Layers configuration failed' }
foreach ($dependency in $pins.dependencies.PSObject.Properties) {
    $checkout = Join-Path $source ('external/Release/64/' + $dependency.Name)
    if ((& git -C $checkout rev-parse HEAD) -ne $dependency.Value) {
        throw "Unexpected validation dependency revision: $($dependency.Name)"
    }
}
& cmd.exe /d /s /c $compile
if ($LASTEXITCODE -ne 0) { throw 'Validation Layers build failed' }
$validator = Get-ChildItem (Join-Path $source 'external') -Filter spirv-val.exe -Recurse | Where-Object FullName -like '*install\bin\spirv-val.exe' | Select-Object -First 1
if ($validator) { Copy-Item -LiteralPath $validator.FullName -Destination (Join-Path $install 'bin\spirv-val.exe') -Force }
$dll = Join-Path $install 'bin/VkLayer_khronos_validation.dll'
$manifest = @{
    revision = $revision
    dependencies = $pins.dependencies
    dll_sha256 = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $install 'manifest.json') -Encoding utf8
Copy-Item -LiteralPath (Join-Path $source 'LICENSE.txt') -Destination (Join-Path $install 'LICENSE.txt') -Force
Write-Host "Set VK_LAYER_PATH=$install\bin for an isolated validation run."
