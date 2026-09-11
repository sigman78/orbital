<#
.SYNOPSIS
    Rebuilds assets/materials from the upstream source textures.
.DESCRIPTION
    Downloads (or reuses from -SourceCache) the hash-verified upstream images, converts
    them to PNG no wider than -MaxWidth, writes them to -OutputDirectory and regenerates
    manifest.json with provenance, licensing and colour-space metadata.

    The committed PNGs are the canonical runtime assets; this script exists to document
    and reproduce how they were derived. Resampling uses GDI+ high-quality bicubic
    filtering, so regenerated files may differ by a few bits between Windows versions.
#>
param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\assets\materials'),
    [string]$SourceCache = (Join-Path $PSScriptRoot '..\.tools\asset-sources'),
    [int]$MaxWidth = 4096,
    [string[]]$Only = @()
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$solar = 'https://www.solarsystemscope.com/textures/download/'
$haven = 'https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/worn_rock_natural_01/'
$assets = @(
    @{ Name = 'earth_albedo';   Source = '8k_earth_daymap.jpg';   Url = $solar; Hash = '88AB060B6E7D241CFC590C69F528FAB2B3247B738D40124CB590999A6FE44ABC'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'sRGB' }
    @{ Name = 'earth_normal';   Source = '2k_earth_normal_map.tif'; Url = $solar; Hash = 'F518CE2646CA935DBC17E316041DE4FEA7A5DA0EC441E4EB22E711EABD843BA2'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'linear-data'; NormalConvention = 'source convention; verify Y orientation in shader' }
    @{ Name = 'earth_specular'; Source = '2k_earth_specular_map.tif'; Url = $solar; Hash = '6B90ECFCE248591A1ECC9A3E49ACCA1A7059B6828877E718302ED9A6B4471BD7'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'linear-data' }
    @{ Name = 'earth_clouds';   Source = '8k_earth_clouds.jpg';   Url = $solar; Hash = 'C792ECA228989D36EBB45D3EA6FF1198BE5E21A25D70D2FBCB2124FFD14BA7F5'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'linear-data' }
    @{ Name = 'earth_night';    Source = '8k_earth_nightmap.jpg'; Url = $solar; Hash = '9894E83A585A22C1C425E7CA4F987A9BA625BF08ECEE45D3C9DCACAE3C2AD5F7'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'sRGB-emissive' }
    # Stored at 2K: the 4K JPEG source carries visible block artifacts that the downscale suppresses; the
    # surface shader adds procedural band and storm detail on top.
    @{ Name = 'gas_albedo';     Source = '8k_jupiter.jpg';        Url = $solar; Hash = '0BD844BF20822C4E3E80882B077859833C0DAC44C7E4E1E0CD63D1B1B6D43085'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'sRGB'; MaxWidth = 2048 }
    @{ Name = 'moon_albedo';    Source = '2k_moon.jpg';           Url = $solar; Hash = '2764BA6535EA0481A062846EE033CC7A909DAE05B31A8FD13F3E98F3A7FD92BD'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'sRGB' }
    @{ Name = 'rock_albedo';    Source = 'worn_rock_natural_01_diff_1k.jpg';   Url = $haven; Hash = 'EF6C74732C61974C5DF7F0F7185E7D6A97308198BF10AEC7A0E95E953A617D9E'; License = 'CC0-1.0'; Attribution = 'Poly Haven: Worn Rock Natural 01, Dimitrios Savva and Rob Tuytel'; ColorSpace = 'sRGB' }
    @{ Name = 'rock_normal';    Source = 'worn_rock_natural_01_nor_gl_1k.jpg'; Url = $haven; Hash = '77A5D3F833AA9613581A5C6EF11422923C04B529F893A16D25704529BE7386DC'; License = 'CC0-1.0'; Attribution = 'Poly Haven: Worn Rock Natural 01, Dimitrios Savva and Rob Tuytel'; ColorSpace = 'linear-data'; NormalConvention = 'OpenGL +Y' }
    @{ Name = 'rock_roughness'; Source = 'worn_rock_natural_01_rough_1k.jpg';  Url = $haven; Hash = '6A96E1A9A86942D3A19FC332E1C0050390A34E7814CE20865A44C3916BD9B428'; License = 'CC0-1.0'; Attribution = 'Poly Haven: Worn Rock Natural 01, Dimitrios Savva and Rob Tuytel'; ColorSpace = 'linear-data' }
)

New-Item -ItemType Directory -Force -Path $OutputDirectory, $SourceCache | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$SourceCache = (Resolve-Path -LiteralPath $SourceCache).Path

function Get-Source($asset) {
    $path = Join-Path $SourceCache $asset.Source
    if ((Test-Path -LiteralPath $path) -and (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -eq $asset.Hash) {
        return $path
    }
    $temporary = Join-Path ([IO.Path]::GetTempPath()) ("space-demo-" + [guid]::NewGuid().ToString('N'))
    try {
        Invoke-WebRequest -Uri ($asset.Url + $asset.Source) -OutFile $temporary -Headers @{ 'User-Agent' = 'space-demo/1.0' }
        $actual = (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash
        if ($actual -ne $asset.Hash) { throw "Hash mismatch for $($asset.Source): expected $($asset.Hash), got $actual" }
        Move-Item -LiteralPath $temporary -Destination $path -Force
        Write-Host "Downloaded $($asset.Source)"
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
    return $path
}

function Convert-Texture($sourcePath, $outputPath, $widthLimit) {
    $source = [System.Drawing.Image]::FromFile($sourcePath)
    try {
        $sourceWidth = $source.Width
        $sourceHeight = $source.Height
        if ($sourceWidth -le $widthLimit) {
            # Lossless re-encode; keeps the source pixel format (e.g. 8-bit grayscale).
            $source.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
            return @($sourceWidth, $sourceHeight, $sourceWidth, $sourceHeight)
        }
        $width = $widthLimit
        $height = [int][Math]::Round($sourceHeight * $widthLimit / $sourceWidth)
        $target = New-Object System.Drawing.Bitmap $width, $height, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
        $graphics = [System.Drawing.Graphics]::FromImage($target)
        $attributes = New-Object System.Drawing.Imaging.ImageAttributes
        try {
            # TileFlipXY avoids the transparent-edge fringe GDI+ otherwise blends in at
            # borders; it also keeps equirectangular maps seam-safe at the wrap column.
            $attributes.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
            $graphics.DrawImage($source, (New-Object System.Drawing.Rectangle 0, 0, $width, $height),
                0, 0, $sourceWidth, $sourceHeight, [System.Drawing.GraphicsUnit]::Pixel, $attributes)
            $target.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally {
            $attributes.Dispose(); $graphics.Dispose(); $target.Dispose()
        }
        return @($sourceWidth, $sourceHeight, $width, $height)
    } finally {
        $source.Dispose()
    }
}

$manifestEntries = @()
$existingManifest = $null
$manifestPath = Join-Path $OutputDirectory 'manifest.json'
if ($Only.Count -gt 0 -and (Test-Path -LiteralPath $manifestPath)) {
    $existingManifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
}
foreach ($asset in $assets) {
    if ($Only.Count -gt 0 -and $Only -notcontains $asset.Name) {
        $manifestEntries += ($existingManifest.assets | Where-Object { $_.file -eq ($asset.Name + '.png') })
        continue
    }
    $sourcePath = Get-Source $asset
    $outputPath = Join-Path $OutputDirectory ($asset.Name + '.png')
    $widthLimit = if ($asset.MaxWidth) { $asset.MaxWidth } else { $MaxWidth }
    $dimensions = Convert-Texture $sourcePath $outputPath $widthLimit
    Write-Host ("{0,-20} {1}x{2} -> {3}x{4} ({5:N1} MB)" -f ($asset.Name + '.png'), $dimensions[0], $dimensions[1],
        $dimensions[2], $dimensions[3], ((Get-Item -LiteralPath $outputPath).Length / 1MB))
    $entry = [ordered]@{
        file = $asset.Name + '.png'
        width = $dimensions[2]
        height = $dimensions[3]
        color_space = $asset.ColorSpace
        license = $asset.License
        attribution = $asset.Attribution
        source = $asset.Url + $asset.Source
        source_sha256 = $asset.Hash
        source_width = $dimensions[0]
        source_height = $dimensions[1]
    }
    if ($asset.NormalConvention) { $entry.normal_convention = $asset.NormalConvention }
    $manifestEntries += [pscustomobject]$entry
}

$manifest = [ordered]@{
    version = 2
    note = 'PNG textures derived from the listed sources by tools/import-assets.ps1 (downscaled to at most 4096 px wide). Keep attribution when redistributing.'
    license_pages = [ordered]@{
        'Solar System Scope' = 'https://www.solarsystemscope.com/textures/'
        'Poly Haven' = 'https://polyhaven.com/license'
    }
    assets = $manifestEntries
}
$json = $manifest | ConvertTo-Json -Depth 4
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'manifest.json'), ($json -replace "`r`n", "`n") + "`n", [Text.UTF8Encoding]::new($false))
Write-Host "Wrote manifest.json"
