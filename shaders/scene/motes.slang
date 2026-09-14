#include "frame_bindings.slang"
#include "../lib/projection.slang"
#include "../lib/noise.slang"

// Motion cue: dust motes fixed in space around the camera, each drawn as a
// soft streak from where it was last frame to where it is now, both seen
// through this frame's view, so the streak is the mote's motion from the
// camera's translation alone: it points along the movement, grows with speed,
// and vanishes when the camera rests or only turns; the parallax is real,
// since the motes are world-fixed points in a lattice that repeats around the
// camera. Depth tested, so bodies occlude them. Drawn after temporal resolve
// into reused HDR storage, then added before bloom/tone mapping; streaks never
// enter history because their transient extent has no valid scene motion/depth.

static const uint MOTE_CELLS = 4;          // cells per axis around the camera
static const uint MOTES_PER_CELL = 16;
static const float STREAK_WIDTH = 1.2;      // half width in pixels
static const float STREAK_PAD = 1.5;        // pixels added at each end
static const float3 MOTE_COLOUR = float3(.7, .8, 1.0);

struct MoteVaryings {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 local : LOCAL;                  // along the streak 0..1, across -1..1
    [[vk::location(1)]] nointerpolation float3 tint : TINT;    // radiance at the streak's core
};

[shader("vertex")]
MoteVaryings vertexMain(uint vertexIndex : SV_VertexID, uint instanceId : SV_InstanceID)
{
    MoteVaryings o;
    o.position = float4(2.0, 2.0, 2.0, 1.0); // off screen unless the mote qualifies
    o.local = float2(0.0, 0.0);
    o.tint = float3(0.0, 0.0, 0.0);
    float4 lattice = root.frame->camera_lattice; // the camera's offset within its cell, and the cell size
    float4 cellIndex = root.frame->camera_cell;  // the camera's cell, and the panel's intensity
    if (cellIndex.w <= 0.0 || root.frame->previous_camera_delta.w < .5) return o;

    uint cell = instanceId / MOTES_PER_CELL, k = instanceId % MOTES_PER_CELL;
    int3 offset = int3(cell % MOTE_CELLS, (cell / MOTE_CELLS) % MOTE_CELLS, cell / (MOTE_CELLS * MOTE_CELLS))
                  - int(MOTE_CELLS / 2);
    float3 absoluteCell = cellIndex.xyz + float3(offset);
    float3 h = float3(hash(absoluteCell * 1.7 + float(k) * 13.1), hash(absoluteCell * 2.3 + float(k) * 7.7 + 5.0),
                      hash(absoluteCell * 3.1 + float(k) * 3.3 + 9.0));
    float3 relative = (float3(offset) + h) * lattice.w - lattice.xyz; // camera-relative, as the scene is
    float3 previous = relative + root.frame->previous_camera_delta.xyz;
    float4 now = mul(root.frame->view_projection, float4(relative, 1.0));
    float4 before = mul(root.frame->view_projection, float4(previous, 1.0)); // this frame's view: translation only
    if (now.w <= NEAR_Z || before.w <= NEAR_Z) return o;

    float2 resolution = root.frame->options.xy;
    float2 p1 = (now.xy / now.w * .5 + .5) * resolution, p0 = (before.xy / before.w * .5 + .5) * resolution;
    float2 d = p1 - p0;
    float length_ = length(d);
    float2 along = length_ > 1e-3 ? d / length_ : float2(1.0, 0.0);
    float2 across = float2(-along.y, along.x);
    // Nothing at rest, a dot at a crawl, a streak in flight; long streaks spread their light.
    float show = smoothstep(2.0, 12.0, length_);
    float distance = length(relative), range = float(MOTE_CELLS / 2) * lattice.w;
    float reach = smoothstep(.3, 1.0, distance) * (1.0 - smoothstep(range * .6, range, distance));
    float energy = cellIndex.w * show * reach * .35 / sqrt(max(length_ / 6.0, 1.0));
    if (energy <= 0.0) return o;

    const float2 corners[6] = {float2(0, -1), float2(1, -1), float2(1, 1), float2(0, -1), float2(1, 1), float2(0, 1)};
    float2 c = corners[vertexIndex];
    float2 px = p0 + along * (c.x * (length_ + 2.0 * STREAK_PAD) - STREAK_PAD) + across * c.y * STREAK_WIDTH;
    float2 ndc = px / resolution * 2.0 - 1.0;
    o.position = float4(ndc, now.z / now.w, 1.0);
    o.local = c;
    o.tint = MOTE_COLOUR * energy;
    return o;
}

[shader("fragment")]
float4 fragmentMain(MoteVaryings i) : SV_Target0
{
    float across = 1.0 - i.local.y * i.local.y;
    float ends = smoothstep(0.0, .2, i.local.x) * smoothstep(1.0, .8, i.local.x);
    return float4(i.tint * across * across * ends, 0.0);
}
