# Material assets

The runtime textures live in `assets/materials/` as PNG files and are tracked in git. Each map is stored at the resolution the renderer uploads (at most 4096 pixels wide); the renderer generates the full mip chain on the CPU at load time. `manifest.json` records, per file, the stored dimensions, colour space, license, attribution, the upstream source URL, its SHA-256 hash and its original dimensions.

`tools/import-assets.ps1` regenerates the whole set: it downloads (or reuses from `.tools/asset-sources/`) the hash-verified upstream JPEG/TIFF files, converts them to PNG with GDI+ high-quality bicubic downscaling where needed, and rewrites `manifest.json`. The committed PNGs are canonical; the script documents how they were derived and lets the set be rebuilt from the same sources.

## Sources and licensing

Solar System Scope's Earth day, cloud and night maps are 8192x4096 upstream and stored at 4096x2048. The official `8k_jupiter.jpg` endpoint supplies a 4096x2048 image. The Earth normal/specular and Moon maps are 2K and stored losslessly. These maps are licensed CC BY 4.0 and are derived from spacecraft imagery and NASA Blue Marble/geodata. Credit: **Solar System Scope — https://www.solarsystemscope.com/textures/**. Poly Haven's 1K **Worn Rock Natural 01** maps are CC0; photography by Dimitrios Savva and processing by Rob Tuytel. Keep the attribution with any redistribution.

## Decoding conventions

Decode albedo, gas, moon and night-emission maps as sRGB. Decode normal, specular, cloud-mask and roughness maps as linear data. The Poly Haven normal is OpenGL-style +Y. The Solar System Scope normal orientation should be checked against the renderer's tangent basis before final sign-off.

Loading is done by `src/assets/image.cpp` (vendored Wuffs PNG decoder; stb_image_write for screenshots) and `src/assets/materials.cpp` (sRGB-to-linear conversion, luminance-to-alpha masks, normal-map renormalisation and 2x2 box mip generation with horizontal wrap for equirectangular maps).
