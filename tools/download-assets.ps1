param([string]$OutputDirectory = (Join-Path $PSScriptRoot '..\assets\materials'))
$ErrorActionPreference = 'Stop'
$assets = @(
    @{ Name='earth_albedo.jpg'; Url='https://www.solarsystemscope.com/textures/download/8k_earth_daymap.jpg'; Hash='88AB060B6E7D241CFC590C69F528FAB2B3247B738D40124CB590999A6FE44ABC' },
    @{ Name='earth_normal.tif'; Url='https://www.solarsystemscope.com/textures/download/2k_earth_normal_map.tif'; Hash='F518CE2646CA935DBC17E316041DE4FEA7A5DA0EC441E4EB22E711EABD843BA2' },
    @{ Name='earth_specular.tif'; Url='https://www.solarsystemscope.com/textures/download/2k_earth_specular_map.tif'; Hash='6B90ECFCE248591A1ECC9A3E49ACCA1A7059B6828877E718302ED9A6B4471BD7' },
    @{ Name='earth_clouds.jpg'; Url='https://www.solarsystemscope.com/textures/download/8k_earth_clouds.jpg'; Hash='C792ECA228989D36EBB45D3EA6FF1198BE5E21A25D70D2FBCB2124FFD14BA7F5' },
    @{ Name='earth_night.jpg'; Url='https://www.solarsystemscope.com/textures/download/8k_earth_nightmap.jpg'; Hash='9894E83A585A22C1C425E7CA4F987A9BA625BF08ECEE45D3C9DCACAE3C2AD5F7' },
    @{ Name='gas_albedo.jpg'; Url='https://www.solarsystemscope.com/textures/download/8k_jupiter.jpg'; Hash='0BD844BF20822C4E3E80882B077859833C0DAC44C7E4E1E0CD63D1B1B6D43085' },
    @{ Name='moon_albedo.jpg'; Url='https://www.solarsystemscope.com/textures/download/2k_moon.jpg'; Hash='2764BA6535EA0481A062846EE033CC7A909DAE05B31A8FD13F3E98F3A7FD92BD' },
    @{ Name='rock_albedo.jpg'; Url='https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/worn_rock_natural_01/worn_rock_natural_01_diff_1k.jpg'; Hash='EF6C74732C61974C5DF7F0F7185E7D6A97308198BF10AEC7A0E95E953A617D9E' },
    @{ Name='rock_normal.jpg'; Url='https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/worn_rock_natural_01/worn_rock_natural_01_nor_gl_1k.jpg'; Hash='77A5D3F833AA9613581A5C6EF11422923C04B529F893A16D25704529BE7386DC' },
    @{ Name='rock_roughness.jpg'; Url='https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/worn_rock_natural_01/worn_rock_natural_01_rough_1k.jpg'; Hash='6A96E1A9A86942D3A19FC332E1C0050390A34E7814CE20865A44C3916BD9B428' }
)
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
foreach ($asset in $assets) {
    $destination = Join-Path $OutputDirectory $asset.Name
    if ((Test-Path -LiteralPath $destination) -and (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -eq $asset.Hash) {
        Write-Host "Verified $($asset.Name)"
        continue
    }
    $temporary = Join-Path ([IO.Path]::GetTempPath()) ("space-demo-" + [guid]::NewGuid().ToString('N'))
    try {
        Invoke-WebRequest -Uri $asset.Url -OutFile $temporary -Headers @{'User-Agent'='space-demo/1.0'}
        $actual = (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash
        if ($actual -ne $asset.Hash) { throw "Hash mismatch for $($asset.Name): expected $($asset.Hash), got $actual" }
        Move-Item -LiteralPath $temporary -Destination $destination -Force
        Write-Host "Downloaded $($asset.Name)"
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}
