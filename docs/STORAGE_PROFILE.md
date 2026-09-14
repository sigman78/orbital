# Storage and scene evaluation measurements

Stage 6 decision: defer contiguous mip storage and a validated immutable scene/evaluation API. The measured benefit of these representation changes is small for the installed workload. Runtime code and cache format remain unchanged.

## Reproduction and scope

```powershell
./tools/build.ps1 -Target storage_profile
./build/release/storage_profile.exe build/release/assets/materials > storage-profile.csv
```

The optional target is excluded from normal builds. Its global allocation replacements exist only in the probe executable. It measures single-threaded C++ `new` requests, including aligned allocation, with a checked 192-byte control and a zero-live-allocation check after each case. Direct `malloc`, allocator headers, resident memory, GPU storage and pre-existing allocations are excluded. Allocation instrumentation also adds overhead to timings.

Three rounds run against warm local files. The cache cases use the 27-entry material catalog, including the three galaxy layers, with 320 mips and 207,396,272 bytes of payload (197.79 MiB). Inputs are preloaded outside the measured scope. Parse/load cases retain all texture results to model the upload batch, but execute serially: the renderer's actual concurrent loading and staging peak are not measured. PNG cases include decoding, conversion, mip generation and `texture_from_images` for representative color, mask and normal maps.

[Metadata and input hashes](performance/storage-profile.json), [MSVC runs](performance/storage-profile-msvc.csv), and [GCC runs](performance/storage-profile-gcc.csv) retain the evidence. Both optimized compiler builds ran successfully; no rendering code changed, so GPU validation and image comparisons were not repeated for this measurement-only chunk.

## MSVC results

Times are medians of three runs. Memory is peak live requested bytes within each scope, expressed in MiB except where noted.

| Case | Work | Time | Allocations | Peak requested memory |
| --- | --- | ---: | ---: | ---: |
| Scene evaluation | 100,000 calls, six bodies | 45.59 ms | 100,000 | 288 bytes |
| Scene validation | 100,000 calls | 22.90 ms | 0 | 0 |
| Cache checksum only | 27 textures | 180.98 ms | 0 | 0 |
| Current cache parsing, retained | 27 textures | 227.01 ms | 508 | 197.81 MiB |
| Optimistic contiguous-copy baseline | 27 textures | 222.38 ms | 28 | 197.79 MiB |
| Actual cache loading, retained | 27 textures | 501.45 ms | 650 | 224.26 MiB |
| Earth albedo PNG preparation | One map | 63.22 ms | 18 | 63.15 MiB |
| Earth cloud PNG preparation | One map | 77.36 ms | 18 | 65.67 MiB |
| Earth normal PNG preparation | One map | 11.86 ms | 17 | 14.49 MiB |

Scene evaluation costs about 0.456 microseconds per call. Even at 160 frames/second, the measured entire evaluator uses about 0.073 ms per second. A reusable output vector would eliminate its 288-byte allocation, but neither that nor removing repeated validation warrants a new lifetime/immutability contract for this workload. Keep validation at current boundaries.

The contiguous-copy baseline hashes each payload and copies it into one allocation, omitting header validation and mip metadata entirely. It is deliberately optimistic, not a valid replacement reader. Compared with current parsing it saves 480 allocations, but only 16,337 bytes of peak requested memory and 4.63 ms across the entire batch. The fixed case order and optimistic baseline prevent treating this as a measured production speedup. Most startup data is pixel/block payload; replacing separate buffers with a newly allocated slab still copies that payload. PNG preparation already moves its per-mip buffers into texture data.

## What could merit a later change

Transferring ownership of the cache file buffer into a texture, with validated mip offsets, could avoid copying approximately 198 MiB in this sample. That is a different change from merely consolidating allocations. The current actual-load scope peaks about 26.47 MiB above the retained payload, but that difference also includes source-file verification and other temporary allocations; it is not a promised saving from ownership transfer.

Revisit this if startup latency or memory becomes a constraint. Measure a production-equivalent ownership-transfer prototype, including the renderer's concurrent workers, validation, decoded-image fallback and GPU staging. Preserve corruption checks and add move/lifetime tests before changing storage. A checksum-format change also needs compatibility planning; do not drop checksum validation to make the timing smaller.
