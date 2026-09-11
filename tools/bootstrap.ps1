param([switch]$Force)
$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$toolRoot = Join-Path $workspace '.tools'
New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null

function Install-Archive($Name, $Url, $Sha256, $Destination, $StripDirectory) {
    if ((Test-Path -LiteralPath $Destination) -and -not $Force) {
        Write-Host "$Name already present at $Destination"
        return
    }
    $archive = Join-Path ([IO.Path]::GetTempPath()) ("space-demo-" + [guid]::NewGuid().ToString('N') + '.zip')
    $extract = Join-Path ([IO.Path]::GetTempPath()) ("space-demo-" + [guid]::NewGuid().ToString('N'))
    try {
        Invoke-WebRequest -Uri $Url -OutFile $archive -Headers @{'User-Agent'='space-demo-bootstrap/1.0'}
        $actual = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
        if ($actual -ne $Sha256) { throw "$Name archive hash mismatch: expected $Sha256, got $actual" }
        Expand-Archive -LiteralPath $archive -DestinationPath $extract
        $source = $extract
        if ($StripDirectory) { $source = (Get-ChildItem -LiteralPath $extract -Directory | Select-Object -First 1).FullName }
        New-Item -ItemType Directory -Force -Path $Destination | Out-Null
        Copy-Item -Path (Join-Path $source '*') -Destination $Destination -Recurse -Force
        Write-Host "Installed $Name"
    } finally {
        if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
        if (Test-Path -LiteralPath $extract) {
            $resolvedExtract = (Resolve-Path -LiteralPath $extract).Path
            $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
            if (-not $resolvedExtract.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -or
                (Split-Path -Leaf $resolvedExtract) -notmatch '^space-demo-[a-f0-9]{32}$') {
                throw "Refusing to remove unverified extraction directory: $resolvedExtract"
            }
            Remove-Item -LiteralPath $resolvedExtract -Recurse -Force
        }
    }
}

Install-Archive 'Vulkan-Headers 1.4.357' `
    'https://github.com/KhronosGroup/Vulkan-Headers/archive/e3b1eec08173d6b825cd3ac88c885a63b621504a.zip' `
    'FDAB38AB0077B491F387D8F0C53301C0005D6152F21973C32ED2C8FAD00F625C' `
    (Join-Path $toolRoot 'Vulkan-Headers') $true
Install-Archive 'glslang 16.5.0' `
    'https://github.com/KhronosGroup/glslang/releases/download/16.5.0/glslang-16.5.0-windows-x86_64-release.zip' `
    '06B71298B750268C127F2EE7AE0EF7525E2068120C6C8A3A08B2F58CA6F325CE' `
    (Join-Path $toolRoot 'glslang') $false
Install-Archive 'Slang 2026.14.1' `
    'https://github.com/shader-slang/slang/releases/download/v2026.14.1/slang-2026.14.1-windows-x86_64.zip' `
    '5ED0A59D650A0AF0ACA45D5DB4E083B3D8FB5CEA05748747DD95DFBE9C580658' `
    (Join-Path $toolRoot 'slang') $false

& (Join-Path $toolRoot 'glslang\bin\glslang.exe') --version | Select-Object -First 1
& (Join-Path $toolRoot 'slang\bin\slangc.exe') -version
Write-Host 'A Vulkan loader/import library is not redistributed. Install the Vulkan SDK or set Vulkan_LIBRARY during CMake configuration.'
