<#
.SYNOPSIS
    Rebuilds assets/materials from the upstream source textures.
.DESCRIPTION
    Downloads (or reuses from -SourceCache) the hash-verified upstream images, converts
    them to PNG no wider than -MaxWidth, writes them to -OutputDirectory and regenerates
    manifest.json with provenance, licensing and colour-space metadata. Height maps
    (NASA DEMs) are baked into normal+height PNGs by tools/bake-normal-map.py, which
    needs Python 3 with numpy and Pillow on PATH.

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
$havenJpg = 'https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/'
$havenPng = 'https://dl.polyhaven.org/file/ph-assets/Textures/png/2k/'
$havenWorn = 'Poly Haven: Worn Rock Natural 01, Dimitrios Savva and Rob Tuytel'
$havenFace = 'Poly Haven: Rock Face 03, Rob Tuytel'
$havenBoulder = 'Poly Haven: Rock Boulder Dry, Rob Tuytel'
$moonKit = 'https://svs.gsfc.nasa.gov/vis/a000000/a004700/a004720/'
$mola = 'https://pds-geosciences.wustl.edu/mgs/mgs-m-mola-5-megdr-l3-v1/mgsl_300x/meg016/'
$nasaMoon = 'NASA/GSFC Scientific Visualization Studio, CGI Moon Kit (LRO LROC and LOLA data)'
$nasaMars = 'NASA/JPL/GSFC, Mars Global Surveyor MOLA MEGDR (PDS Geosciences Node)'
$assets = @(
    @{ Name = 'earth_albedo';   Source = '8k_earth_daymap.jpg';   Url = $solar; Hash = '88AB060B6E7D241CFC590C69F528FAB2B3247B738D40124CB590999A6FE44ABC'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'sRGB' }
    @{ Name = 'earth_normal';   Source = '2k_earth_normal_map.tif'; Url = $solar; Hash = 'F518CE2646CA935DBC17E316041DE4FEA7A5DA0EC441E4EB22E711EABD843BA2'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'linear-data'; NormalConvention = 'source convention; verify Y orientation in shader' }
    @{ Name = 'earth_specular'; Source = '2k_earth_specular_map.tif'; Url = $solar; Hash = '6B90ECFCE248591A1ECC9A3E49ACCA1A7059B6828877E718302ED9A6B4471BD7'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'linear-data' }
    @{ Name = 'earth_clouds';   Source = '8k_earth_clouds.jpg';   Url = $solar; Hash = 'C792ECA228989D36EBB45D3EA6FF1198BE5E21A25D70D2FBCB2124FFD14BA7F5'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'linear-data' }
    @{ Name = 'earth_night';    Source = '8k_earth_nightmap.jpg'; Url = $solar; Hash = '9894E83A585A22C1C425E7CA4F987A9BA625BF08ECEE45D3C9DCACAE3C2AD5F7'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'sRGB-emissive' }
    # Stored at 2K: the 4K JPEG source carries visible block artifacts that the downscale suppresses; the
    # surface shader adds procedural band and storm detail on top.
    @{ Name = 'gas_albedo';     Source = '8k_jupiter.jpg';        Url = $solar; Hash = '0BD844BF20822C4E3E80882B077859833C0DAC44C7E4E1E0CD63D1B1B6D43085'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'sRGB'; MaxWidth = 2048 }
    @{ Name = 'moon_albedo';    Source = 'lroc_color_poles_4k.tif'; Url = $moonKit; Hash = '918649A7F8ED2F1329B2CD95BB0D25483BEFDCB60AE1A66DB681A637CC21344F'; License = 'Public-Domain-NASA'; Attribution = $nasaMoon; ColorSpace = 'sRGB' }
    # DEM-derived normal maps: red = east, green = south, height in alpha; slopes exaggerated 1.5x for readability.
    @{ Name = 'moon_normal';    Source = 'ldem_16_uint.tif';      Url = $moonKit; Hash = '45A2B32D56E81ED30DB07FEAD8ABC842B249B6511219D9CA2C53F81BC2DC5D62'; License = 'Public-Domain-NASA'; Attribution = $nasaMoon; ColorSpace = 'linear-data'; NormalConvention = 'DirectX (green = south), height in alpha'; Bake = @{ Format = 'ldem-uint16'; RadiusKm = 1737.4; Strength = 1.5 } }
    @{ Name = 'mars_albedo';    Source = '8k_mars.jpg';           Url = $solar; Hash = '4CC52149924ABC6AE507D63032F994E1D42A55CB82C09E002D1A567FF66C23EE'; License = 'CC-BY-4.0'; Attribution = 'Solar System Scope'; ColorSpace = 'sRGB' }
    @{ Name = 'mars_normal';    Source = 'megt90n000eb.img';      Url = $mola; Hash = 'D18D9B9AB8C5516D02E157DD2CDE0F1D0D160C21940E953BA22391269A545E7B'; License = 'Public-Domain-NASA'; Attribution = $nasaMars; ColorSpace = 'linear-data'; NormalConvention = 'DirectX (green = south), height in alpha'; Bake = @{ Format = 'pds-int16'; RadiusKm = 3396.2; Strength = 1.5 } }
    # Three CC0 rock sets at 2K for the belt: albedo, normal+height (packed by tools/pack-normal-height.py) and roughness.
    @{ Name = 'rock_albedo';    Source = 'worn_rock_natural_01_diff_2k.jpg';   Url = $havenJpg + 'worn_rock_natural_01/'; Hash = '084E528A8B63684CF6CD9C9C1D44CEF32A8F4C9E8404394A4A1A40C70CBF000D'; License = 'CC0-1.0'; Attribution = $havenWorn; ColorSpace = 'sRGB' }
    @{ Name = 'rock_normal';    Source = 'worn_rock_natural_01_nor_gl_2k.jpg'; Url = $havenJpg + 'worn_rock_natural_01/'; Hash = '51FB7B64EDA0D68BABD621C43A06D89300D789F27FBE63D917CD7B6AB93365CB'; License = 'CC0-1.0'; Attribution = $havenWorn; ColorSpace = 'linear-data'; NormalConvention = 'DirectX (green = +v), height in alpha'; Pack = @{ Source = 'worn_rock_natural_01_disp_2k.png'; Url = $havenPng + 'worn_rock_natural_01/'; Hash = '18F7556D90EA400C180547D74CADDFAF8DC6D3FD7ECE9061B5544058E2A5F200' } }
    @{ Name = 'rock_roughness'; Source = 'worn_rock_natural_01_rough_2k.jpg';  Url = $havenJpg + 'worn_rock_natural_01/'; Hash = '1AAC2CF72392F1207B5DBF880CBE0EF58D4C7EECD9DADC12640F031670969395'; License = 'CC0-1.0'; Attribution = $havenWorn; ColorSpace = 'linear-data' }
    @{ Name = 'rock_face_albedo';    Source = 'rock_face_03_diff_2k.jpg';   Url = $havenJpg + 'rock_face_03/'; Hash = '644EE79DF7FFBF5F6AFC330B64DD3F33794BAB8820915FE61FFE0C88ECFD7ECE'; License = 'CC0-1.0'; Attribution = $havenFace; ColorSpace = 'sRGB' }
    @{ Name = 'rock_face_normal';    Source = 'rock_face_03_nor_gl_2k.jpg'; Url = $havenJpg + 'rock_face_03/'; Hash = 'A498CEF4BA6191F6959B6D03FB835263C77271948264A289AF24497AB7111DEC'; License = 'CC0-1.0'; Attribution = $havenFace; ColorSpace = 'linear-data'; NormalConvention = 'DirectX (green = +v), height in alpha'; Pack = @{ Source = 'rock_face_03_disp_2k.png'; Url = $havenPng + 'rock_face_03/'; Hash = '3CB398FC7E958C4A1C05428DBDB124DA70AC5C62DAB0820C1FADA48065370D2B' } }
    @{ Name = 'rock_face_roughness'; Source = 'rock_face_03_rough_2k.jpg';  Url = $havenJpg + 'rock_face_03/'; Hash = '9CA3E9E196685F702328F24D66F039B72F5330FC35CF2A2C265CFA50A7916301'; License = 'CC0-1.0'; Attribution = $havenFace; ColorSpace = 'linear-data' }
    @{ Name = 'rock_boulder_albedo';    Source = 'rock_boulder_dry_diff_2k.jpg';   Url = $havenJpg + 'rock_boulder_dry/'; Hash = 'C66DE7F7B48EF1BA542C17AF780787CA6D43DA2160A46FF3544DD9FCDF65CA30'; License = 'CC0-1.0'; Attribution = $havenBoulder; ColorSpace = 'sRGB' }
    @{ Name = 'rock_boulder_normal';    Source = 'rock_boulder_dry_nor_gl_2k.jpg'; Url = $havenJpg + 'rock_boulder_dry/'; Hash = '72A3C96D563FC17AC36C568802E106222E6480512D98E07318E1F431CEA33DD6'; License = 'CC0-1.0'; Attribution = $havenBoulder; ColorSpace = 'linear-data'; NormalConvention = 'DirectX (green = +v), height in alpha'; Pack = @{ Source = 'rock_boulder_dry_disp_2k.png'; Url = $havenPng + 'rock_boulder_dry/'; Hash = 'E07B06123BDA2156F2A0FEF79B0EDB77280295135CB5063E7794998FF3E69A90' } }
    @{ Name = 'rock_boulder_roughness'; Source = 'rock_boulder_dry_rough_2k.jpg';  Url = $havenJpg + 'rock_boulder_dry/'; Hash = '63BB4105E6F60B5968DA785AFC292B94026A5AFA6F18388E98B00232E9E2C433'; License = 'CC0-1.0'; Attribution = $havenBoulder; ColorSpace = 'linear-data' }
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
        # Always re-encode through a 24-bit surface: a grayscale source would otherwise be
        # saved as a palette PNG, which the runtime decoder does not accept.
        $width = [Math]::Min($sourceWidth, $widthLimit)
        $height = [int][Math]::Round($sourceHeight * $width / $sourceWidth)
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
    $baked = $null
    if ($asset.Bake) {
        $bakeScript = Join-Path $PSScriptRoot 'bake-normal-map.py'
        $json = & python $bakeScript --source $sourcePath --format $asset.Bake.Format --output $outputPath `
            --radius-km $asset.Bake.RadiusKm --strength $asset.Bake.Strength --max-width $widthLimit
        if ($LASTEXITCODE -ne 0) { throw "bake-normal-map.py failed for $($asset.Name)" }
        $baked = $json | ConvertFrom-Json
        $dimensions = @($baked.source_width, $baked.source_height, $baked.width, $baked.height)
    } elseif ($asset.Pack) {
        $heightPath = Get-Source @{ Source = $asset.Pack.Source; Url = $asset.Pack.Url; Hash = $asset.Pack.Hash }
        $packScript = Join-Path $PSScriptRoot 'pack-normal-height.py'
        $json = & python $packScript --normal $sourcePath --height $heightPath --output $outputPath --max-width $widthLimit
        if ($LASTEXITCODE -ne 0) { throw "pack-normal-height.py failed for $($asset.Name)" }
        $packed = $json | ConvertFrom-Json
        $dimensions = @($packed.source_width, $packed.source_height, $packed.width, $packed.height)
    } else {
        $dimensions = Convert-Texture $sourcePath $outputPath $widthLimit
    }
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
    if ($asset.Pack) {
        $entry.height_source = $asset.Pack.Url + $asset.Pack.Source
        $entry.height_source_sha256 = $asset.Pack.Hash
    }
    if ($baked) {
        $entry.height_min_m = $baked.height_min_m
        $entry.height_max_m = $baked.height_max_m
        $entry.slope_strength = $baked.strength
    }
    $manifestEntries += [pscustomobject]$entry
}

$manifest = [ordered]@{
    version = 2
    note = 'PNG textures derived from the listed sources by tools/import-assets.ps1 (downscaled to at most 4096 px wide). Keep attribution when redistributing.'
    license_pages = [ordered]@{
        'Solar System Scope' = 'https://www.solarsystemscope.com/textures/'
        'Poly Haven' = 'https://polyhaven.com/license'
        'NASA CGI Moon Kit' = 'https://svs.gsfc.nasa.gov/4720'
        'NASA MOLA MEGDR' = 'https://pds-geosciences.wustl.edu/missions/mgs/megdr.html'
    }
    assets = $manifestEntries
}
$json = $manifest | ConvertTo-Json -Depth 4
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'manifest.json'), ($json -replace "`r`n", "`n") + "`n", [Text.UTF8Encoding]::new($false))
Write-Host "Wrote manifest.json"
