# GPU scopes and ownership helpers

The renderer uses the optional `NoGraphicsAPIUtility::commands` target. The core GPU API stays unchanged.

- `RenderPassScope` consumes a rendering description immediately and ends the pass at scope exit. It cannot be copied or moved. Barriers remain outside the scope; the destructor adds no synchronization.
- `SubmissionTimeline` owns the semaphore and advances completion values on explicit submit/present calls. Waiting remains explicit, including the blocking `submit_and_wait` convenience. Its destructor never submits or waits.
- `UniqueGpuHeap` and `UniquePso` are move-only owners with borrowed accessors. Reset, move replacement and destruction release immediately, so GPU use must already be complete. Existing `GpuImage` ownership follows the same policy.
- Persistent heaps form a resettable buffer group; pipelines have an owning vector and borrowed pass-family aliases. Shutdown waits idle, clears all resource groups, resets the timeline and destroys the device last. Temporary upload/readback heaps are released after their explicit completion waits.
- `gpu_sync.hpp` names recurring source/destination stage-access pairs. These are global dependencies, not tracked texture state. Unusual combined-stage and compute barriers stay explicit.

The upload flush is a named function receiving the command pointer, staging offset and submission timeline. It retains the transfer-write-to-fragment-read dependency, submits and waits, then clears the command pointer and offset before staging reuse. It does not flush mapped host caches or silently submit at scope exit.

## Verification

- Final release build and all 15 CTest suites pass, including core/synchronization validation and window lifecycle checks.
- The separate GPU-assisted suite passes its negative missing-barrier control, synchronized probe, Earth, belt TAA on/off, camera stop, tour, UI and fullscreen cases. Evidence: `.scratch/gpu-scopes/validation-gpu/`.
- CPU doubles check early-return pass closure, move construction/replacement, self-move, repeated reset, timeline sequencing and absence of implicit submit/wait. A real-device probe clears an image inside an early-return scope, verifies its pixels through readback, and replaces heap owners after completion.
- All 13 MinGW CPU suites pass. The wrapper-specific CPU test is excluded on MinGW because NoGraphicsAPI's headers explicitly reject that platform; it runs in the MSVC suite and is enabled on supported non-MinGW configurations.
- A source comparison expands named dependencies back to raw barrier calls and confirms identical per-file arguments/order. All 33 runtime shader binaries are unchanged.
- Six paired 1600x900 captures cover Earth, belt, tour, sun-through-belt, high quality and stopping the camera. Mean absolute RGBA differences range from 0.000012 to 0.000347 display codes; maxima are 1, 16, 1, 3, 10 and 5. These are near-identical, not bit-identical results. Commands, logs and metrics: `.scratch/gpu-scopes/captures/`.
- Three-run belt GPU medians are 5.225 to 5.278 ms windowed and 14.441 to 14.528 ms fullscreen. No tracked metric crosses the combined 5% / 0.2 ms regression threshold against either the preceding executable or packed-culling baseline. This is not a zero-overhead proof. Reports: [before](performance/gpu-scopes-before.md), [after](performance/gpu-scopes-after.md), with adjacent JSON metadata. The before executable was saved from `79ccaad`; both reports were recorded while this refactor was uncommitted.
