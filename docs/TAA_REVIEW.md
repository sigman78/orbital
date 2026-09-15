# TAA review — 2026-09-13

Reviewed revision: `c4bbb60`. This is a source and numerical review, not an implementation change. Numerical results below evaluate the current shader equations; they are not new GPU captures or a measured reproduction at 30 fps. The exact contribution of each mechanism to the reported scene still needs controlled visual tests.

Implementation update: stages 1 and 2 are implemented on `fix/taa-coordinates-occlusion`; see [TAA_COORDINATES.md](TAA_COORDINATES.md). Findings below describe the reviewed baseline, including defects subsequently corrected.

The implementation has useful components, but its sampling coordinates and treatment of partially covered pixels need correction before tuning filter constants. Replacing TAA wholesale is not yet justified.

## Current pipeline

Every frame applies an eight-position projection jitter, including when TAA is disabled. Opaque geometry and alpha-blended rock billboards render to HDR. Atmosphere then composites using opaque scene depth; belt dust and motion streaks follow. With TAA enabled, another billboard draw writes a binary mask and depth over entire billboard quads. TAA resolves the combined HDR image into alternating RGBA16F histories, storing linear depth in alpha. Bloom reads resolved HDR; a separate sun-visibility pass reads the modified scene depth. Tone mapping, lens effects and optional FXAA/SMAA follow.

The temporal resolve uses camera-only reprojection, a five-fetch Catmull–Rom approximation, 3×3 YCoCg variance clipping, and luminance-weighted blending. Current splat pixels bypass both clipping and depth rejection.

Relevant implementation: [frame construction](../src/render/renderer_frame_data.cpp), [pass ordering](../src/render/renderer_frame.cpp), [resolve](../shaders/aa/temporal.slang), [scene passes](../src/render/renderer_scene.cpp), [belt passes](../src/render/renderer_belt.cpp), [post passes](../src/render/renderer_post.cpp).

## Findings, in priority order

### 1. The output retains projection jitter — high priority, confirmed

`build_frame()` jitters the projection. The resolve loads current color at the raster pixel center, reconstructs that pixel's jittered ray, and projects it through the previous jittered projection. There is no reconstruction onto a stable output grid, and `sampleHDR()` presents history at unchanged UVs.

For a static scene, let `p` be a pixel coordinate and `j` the projection displacement in pixels. Current color represents `S(p − j_current)`. Reprojection reads history at `p + j_previous − j_current`, which represents that same displaced scene location. Blending therefore preserves the displacement; it does not remove it.

This directly explains a plausible whole-scene wobble at low frame rates. The sequence spans 0.8125 pixels horizontally and approximately 0.7778 vertically; its eight frames repeat every 267 ms at 30 fps. It is not literally a two-position sequence, although its horizontal signs mostly alternate. HUD/lens effects added afterward need not move with the scene.

Correct the coordinate contract throughout current-color reconstruction, history lookup, depth selection and clipping neighborhoods. Prefer storing history on a stable output grid. A final compensating sample alone would still leave repeated history resampling and current-filter issues. Do not simply remove previous jitter from the existing reprojection while leaving its other coordinates unchanged.

References: [ray construction](../shaders/scene/view.slang), [HDR sampling](../shaders/post/hdr.slang), [composite](../shaders/post/composite.slang).

### 2. Jitter is counted as motion, and “TAA off” still jitters — high priority, confirmed

`motionPixels = length((previousUV − uv) * size)` includes the jitter delta. Evaluating all eight transitions at a stationary camera gives 0.417–0.878 pixels of false motion. Ordinary history weights consequently vary from 0.811 to 0.931 instead of remaining at the intended 0.95. Splat weights also vary, although less strongly. This imposes a periodic change in responsiveness on otherwise static content.

The TAA switch only changes the resolve to a copy and skips the splat-mask draw; it does not stop projection jitter. Thus the existing off image is not a clean unjittered reference. Zero jitter when temporal AA is off, and use jitter-free scene motion for adaptive blending.

The sequence also has mean X = −0.0546875 pixels. Centering it is worthwhile after fixing the coordinate contract, but this small bias is not the main wobble mechanism.

### 3. Whole-quad depth contaminates sun occlusion — high priority, confirmed

The mask branch of [surface_rock.slang](../shaders/surface/surface_rock.slang) writes one everywhere in each quad, without using the billboard's actual Gaussian/disc alpha. The mask pass also writes depth. Consequently even nearly transparent quad edges become opaque occluders for [sun visibility](../shaders/post/sun_occlusion.slang). This happens only with TAA enabled.

The recent bloom prefilter and dense blur changes address spatial sampling defects. They cannot correct this depth/coverage mismatch. The newer 256-sample visibility integration smooths binary visibility spatially, but does not recover missing fractional coverage or accumulate visibility over time. Its result drives glare, ghosts and starburst after TAA, so their residual variation bypasses temporal accumulation entirely.

Separate temporal surface metadata from physical occlusion. Preserve opaque scene depth and give sun visibility an explicit policy for fractional asteroid coverage. Measure bloom-only and lens-only outputs before deciding whether an additional short temporal visibility filter is needed; smoothing incorrect opaque quads would conceal the underlying error.

### 4. Splat reprojection drags atmosphere/background color at rock depth — high priority, confirmed mismatch; visual attribution pending

Atmosphere reads depth before splat quads write it. That is intentional: treating translucent quads as opaque would cut holes in atmosphere. However, TAA subsequently reprojects the entire already-composited pixel at the quad's depth, including the atmosphere and background visible through it. These contributions generally have different parallax. The current mask also exempts that mixed history from clipping and rejection.

This is a strong explanation for belt/atmosphere smearing, patches or apparent attachment during movement. It is not a simple missing jitter correction in the atmosphere ray: atmosphere and opaque geometry use the same jitter convention. Moving the full-quad depth pass ahead of atmosphere would create another incorrect result.

There is also a compositing limitation independent of TAA: billboards blend before atmosphere without writing depth, so foreground splats do not terminate its march as opaque rocks do. Correct treatment requires coverage-aware layering or a bounded reactive-history policy for mixed pixels. A separate temporally accumulated splat layer is an option, but it must preserve attenuation and occlusion ordering. The earlier unfiltered separate layer was removed because it twinkled; do not reintroduce that solution unchanged.

### 5. Contrast loss is a combination of filter choices — high priority, confirmed mechanisms

- Every jitter transition resamples history. Catmull–Rom reduces blur relative to bilinear but does not eliminate repeated reconstruction losses, and its negative lobes can ring. Clamping the filtered result to nonnegative values also changes its energy near high-contrast features.
- For a grayscale 3×3 neighborhood containing one value `B` and eight zeros, the current variance box's upper bound is only `0.5825 B`. Thus a legitimate isolated bright history sample can be clipped substantially. Splat pixels bypass this, but mesh pixels and pixels where the current mask disappears do not.
- Luminance weighting deliberately discounts bright samples. At nominal history weight 0.95, current luminance 10 against black history resolves to approximately 0.0476, versus 0.5 with linear blending. Reversing those inputs gives 6.333 rather than 9.5. These are isolated blend examples, not measured asteroid brightness: steady equal inputs remain unchanged. Intermittent coverage nevertheless receives a strong nonlinear bias.
- The mask is binary and current-frame-only. As a small footprint moves across pixel boundaries, its filtering policy changes abruptly between exempt and clipped/rejected history.
- Spatial AA adds another filter after TAA. Assess TAA with `--spatial 0` before attributing all contrast loss to it.

Update 2026-09-15: measured at rest and in deterministic forward flight (belt bookmark, 1920x1080, `--spatial 0`, TAA on against off). At rest the loss is nil: bright pixels (over 128) 0.999 of the TAA-off count, medians equal to the Sep 11 references. In the 1x flight TAA has 5.8 percent fewer bright pixels and 1 percent less light in sum; with the history weight in motion set to zero (the jittered current frame reconstructed alone) the deficit is still 3.6 percent, so most of it is the jittered reconstruction of sub-pixel rocks, not the blend. Including the current sample in the clip box and dropping the luminance weighting each changed nothing measurable, so neither the tightened motion box nor the weighting is where the light goes. The motion history weight is 0.6 (was 0.8) as the stopgap: bright pixels 0.950 of TAA off, the light TAA adds over eight codes 0.40 percent (was 0.46). What remains needs the supersampled reference above, not a blend constant.

Some reduction of a point's peak is legitimate antialiasing, since a subpixel rock should not retain the peak of its brightest aliased frame. Compare both integrated light and local contrast against a supersampled reference, not only against TAA-off peaks. Tune reconstruction and coverage-aware history confidence before adding sharpening.

### 6. History validation is weak at precisely the belt's edges — medium/high priority, confirmed

Update 2026-09-15: the splat exemption from clipping is gone (splat history is clipped to a loose box of three standard deviations) and the box tightens with motion for every pixel (1.5 standard deviations at rest, 0.75 from three pixels of motion per frame). This halves the light TAA adds beside fast rocks in forward flight (see DECISIONS.md). Depth-based validation of splats and the coverage-aware policy remain open.

Depth stored in history alpha is filtered with the same Catmull–Rom weights as color. Interpolating foreground depth with the sky sentinel, including negative lobes, produces values that need not belong to any surface. Those values control both the sky classification and the depth rejection test. RGBA16F can represent the 2000-unit sentinel; overflow is not the issue here.

Rejection runs only when both pixels classify as surfaces, and never for current splats. Allowing sky/surface transitions helped coverage convergence in previous experiments, but indiscriminately accepting them also admits disoccluded history. The 5% depth tolerance is additionally broad for nearby overlapping belt layers.

Fetch depth separately with an edge-aware/point-sampled policy. Distinguish expected subpixel coverage changes from actual disocclusion using trustworthy surface metadata, and bound splat history acceptance rather than exempting it completely. Depth-neighborhood selection should be evaluated at silhouettes; a center-only depth is not always representative of filtered color.

### 7. Reprojection accounts only for camera motion — medium priority, confirmed

The previous position is reconstructed by adding camera translation. There are no previous object transforms or object motion vectors in the resolve. Orbiting bodies, shearing/tumbling rocks and animated shading therefore do not reproject exactly. The comments claiming exact splat reprojection are only approximately true for stationary objects and cannot justify accepting arbitrary history.

Atmosphere, dust and additive motion streaks also lack their own temporal depth/motion representation. In particular, streaks depth-test without depth writes, so their composite is not reprojected at each mote's own depth despite the older statement in `DECISIONS.md`.

Prioritize the static-camera defects first. Then evaluate object velocity and reactive masks against actual scene motion; these solve different problems and neither alone fixes multilayer transparency.

### 8. History lifecycle and time response need explicit policy — medium priority, confirmed

Resize invalidates history. Large translations (>10 units) and turns (forward dot <0.7) reject it for the frame. Smaller discontinuous camera changes, temporal-mode changes and abrupt scene/shading changes have no explicit reset. Off frames still populate history, so enabling TAA can reuse an image produced with different depth semantics.

Weights are frame-based. A nominal 0.95 history weight has a 13.51-frame half-life: 450 ms at 30 fps versus 225 ms at 60 fps, before clipping and luminance weighting. Actual ordinary weights are lower because of the false-motion bug. Separate sample accumulation from real-time responsiveness: blindly making every weight time-based would also change sample convergence. Add explicit invalidation for discontinuities and validate responsiveness across frame rates.

### 9. Other contributors to isolate — medium priority

Half-resolution [belt dust](../shaders/belt/dust.slang) changes its march noise using time and projection jitter, then relies on this same mixed-color TAA to settle. Its samples represent full-resolution even-pixel centers, while the upsample uses the conventional half-resolution texel-center mapping: this is a half-full-resolution-pixel phase discrepancy worth testing independently near silhouettes. It is not established as the reported atmosphere artifact.

Automatic exposure reads a new meter every 16 frames (533 ms at 30 fps), though its adaptation runs smoothly each frame. Animated grain, procedural shading, LOD transitions and periodically rebaked far-belt maps can also vary independently of TAA. Freeze or isolate these for diagnosis. Prior belt capture work found small run-to-run differences even with an unchanged binary; repeat baselines before assigning tiny numerical differences to a change.

## What is sound

HDR accumulation before bloom and tone mapping is a reasonable ordering. Camera-relative reconstruction avoids large world-coordinate cancellation; sky reprojection correctly uses a direction rather than camera translation. YCoCg clipping and sharpened history reconstruction are established building blocks, although their current acceptance policy needs work. History ping-pong, resize invalidation and first-frame bypass are present. No obvious reversed history-buffer binding was found; the alternating appearance is better explained by the coordinate analysis than by a buffer swap error.

## Recommended implementation sequence and validation

1. Establish stable output/history coordinates, disable jitter with TAA off, remove jitter from motion magnitude and explicitly reset history on mode/camera discontinuities. Validate a frozen high-contrast edge and planet limb over complete jitter cycles at 30/60/120 fps. Measure position as well as frame differences.
2. Separate scene occlusion from splat temporal metadata. Validate fractional splats crossing the solar disc with TAA on/off; split bloom and lens contribution captures. Keep the existing bloom sampling regression.
3. Replace unconditional splat acceptance with a coverage-aware policy; fetch validation depth independently. Test belt against black sky and against the giant's atmosphere, with static camera, translation, rotation and animated rocks separately. Include entering/leaving footprints and crossing silhouettes.
4. Tune contrast and history response against a supersampled reference. Measure point peak, 5×5 integrated energy, neighborhood contrast, temporal variance and ghost length. Compare TAA alone, SMAA alone and both, at startup, odd-size and fullscreen resolutions.
5. Add object motion or separate layer accumulation where the remaining evidence justifies it. Include history resets, resize and large/small camera cuts in regression captures.

The first stage can likely fit the current resolve and targets, although better current reconstruction changes texture-fetch cost. Separating metadata/layers can add bandwidth, targets and draws. No new runtime cost was measured in this review; profile each stage rather than predicting a net frame-time result. CPU CTest cannot establish visual correctness for these defects and was not rerun for this documentation-only review.

## External technique cross-checks

[Playdead's reference temporal resolve](https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader) explicitly distinguishes unjittering color, neighborhoods and reprojection, and uses a velocity input. This supports treating coordinate conventions as a coherent contract rather than patching a single UV expression.

[Emilio López's implementation discussion](https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/) covers jitter handling, motion, disocclusion and reconstruction tradeoffs. It provides a useful primary implementation comparison; the specific diagnoses and numerical examples above are derived from this repository, not measurements borrowed from that article.
