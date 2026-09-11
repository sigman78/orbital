# Material assets

Run `tools/download-assets.ps1` to fetch or verify the source textures. The textures themselves are not tracked in git (see `.gitignore`); only `manifest.json` is. The script preserves upstream bytes and has no image-conversion dependency. SHA-256 hashes, source URLs, attribution, and per-map color-space metadata are recorded in `assets/materials/manifest.json`.

Solar System Scope's Earth day, cloud, and night maps use the original 8192x4096 downloads for close orbital views. The official `8k_jupiter.jpg` endpoint currently supplies a 4096x2048 image; that upstream file is preserved byte-for-byte. The Earth normal/specular and Moon maps remain at 2K. These maps are licensed CC BY 4.0 and are derived from spacecraft imagery and NASA Blue Marble/geodata. Credit: **Solar System Scope — https://www.solarsystemscope.com/textures/**. Poly Haven's 1K **Worn Rock Natural 01** maps are CC0; photography by Dimitrios Savva and processing by Rob Tuytel.

The runtime currently caps uploaded base levels at 4096 pixels while retaining complete filtered mip chains. Keeping the verified 8K source files avoids baking that runtime policy into the asset archive and preserves detail for future close-view quality settings.

Decode albedo, gas, moon, and night-emission maps as sRGB. Decode normal, specular, cloud-mask, and roughness maps as linear data. The Poly Haven normal is OpenGL-style +Y. The Solar System Scope normal orientation should be checked against the renderer's tangent basis before final sign-off.
