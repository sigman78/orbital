<#
.SYNOPSIS
    Portability check: builds the CPU libraries and every test with GCC (MinGW) and runs them.
.DESCRIPTION
    Mirrors the Linux CI job locally. The demo executable needs the Win32 platform backend and a
    Vulkan loader, so it is skipped; everything under src/core, src/scene and src/assets plus the
    tests must compile and pass on a non-MSVC compiler. The C++ runtime is linked statically so the
    test executables do not depend on whichever libstdc++ DLL happens to be first on PATH.
.PARAMETER BuildDirectory
    Where to configure and build; defaults to build/gcc.
#>
param([string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build\gcc'))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
foreach ($tool in 'g++', 'gcc', 'cmake', 'ninja') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool was not found on PATH." }
}
& cmake -S $root -B $BuildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc `
    -DORBITAL_BUILD_DEMO=OFF -DBUILD_TESTING=ON '-DCMAKE_EXE_LINKER_FLAGS=-static'
if ($LASTEXITCODE -ne 0) { throw 'GCC configure failed' }
& cmake --build $BuildDirectory
if ($LASTEXITCODE -ne 0) { throw 'GCC build failed' }
& ctest --test-dir $BuildDirectory --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'GCC tests failed' }
Write-Host 'GCC portability check passed.'
