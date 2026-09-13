# Vendored bc7enc

Unmodified `bc7enc.cpp` and `bc7enc.h` from Richard Geldreich's
https://github.com/richgel999/bc7enc_rdo at commit
`b9438627eef73a1157e84201b6fa6eb2ffd6d9f0`.

MIT or public domain; the full dual-license notice is retained in `bc7enc.cpp`.
Orbital uses only the block encoder, without the upstream RDO pipeline. The
optional `tools/bc7_compress.cpp` executable handles mip input and parallel block
encoding. The demo never links this library. Build via `texture_tools` with
`ORBITAL_BUILD_TEXTURE_TOOLS=ON`.
