# SMAA lookup resources

Raw, headerless bytes extracted without modification from the previously vendored
`AreaTex.h` and `SearchTex.h` from iryoku/smaa. See [LICENSE.txt](LICENSE.txt).

| File | Layout | Bytes | SHA-256 |
| --- | --- | --- | --- |
| `area.bin` | 160 × 560, RG8, row pitch 320 | 179200 | `35065cef2a02cabcad711d6bf430239ae64e27d71c4e4fa06f29cce2c992f0d2` |
| `search.bin` | 64 × 16, R8, row pitch 64 | 1024 | `3694eae5e9d44b8ebb4415a13f8c7b94dc08a2fc86658434d771c4610fe5744d` |

CMake generates `generated/smaa.cpp` in the build directory and compiles it into
`orbital_assets`. The renderer accesses the embedded bytes through `assets/smaa.hpp`;
the binaries do not need to be installed or read at startup. Fixed-extent spans
enforce the expected byte counts at compile time. The `smaa` CTest compares the linked
data with these source binaries.

This uses C++20 and CMake only. The project's installed MSVC compiler rejects
`#embed` even in its latest language mode; the build-time conversion avoids requiring
a newer compiler or an additional scripting runtime.
