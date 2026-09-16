#include "../scene/frame_bindings.slang"
#include "../scene/frame_records.slang"

// The atmosphere pass drawn over the body's projected shell only: one triangle
// covering the screen rectangle of the shell's bounding box, so the pixels the
// shell cannot reach never run the pass (its early-out still cost the ray setup
// and the occluder loop at every pixel of the frame, three times a frame at
// fullscreen). The box is the projection of the eight corners of the cube
// around the shell, a pixel of margin added; with the camera inside or beside
// the shell (a corner behind the near plane) the triangle covers the frame.

static const float SHELL_MARGIN = 1.02; // over the largest shell (1.018 radii), the cube's half-extent

struct FullscreenVaryings { float4 position : SV_Position; };

[shader("vertex")]
FullscreenVaryings vertexMain(uint vertexIndex : SV_VertexID)
{
    Frame* frame = root.frame;
    ViewParams view = viewParams(frame);
    Instance body = root.instances[root.base];
    float3 centre = body.center_radius.xyz;
    float half = body.center_radius.w * SHELL_MARGIN;
    float2 lower = float2(1.0, 1.0), upper = float2(-1.0, -1.0);
    bool clipped = false;
    for (int corner = 0; corner < 8; ++corner) {
        float3 offset = float3((corner & 1) != 0 ? half : -half, (corner & 2) != 0 ? half : -half,
                               (corner & 4) != 0 ? half : -half);
        float4 clip = mul(view.viewProjection, float4(centre + offset, 1.0));
        if (clip.w <= 1e-4) { clipped = true; break; }
        float2 ndc = clip.xy / clip.w;
        lower = min(lower, ndc);
        upper = max(upper, ndc);
    }
    float2 margin = 2.0 / view.size; // one pixel
    if (clipped) {
        lower = float2(-1.0, -1.0);
        upper = float2(1.0, 1.0);
    } else {
        lower = max(lower - margin, float2(-1.0, -1.0));
        upper = min(upper + margin, float2(1.0, 1.0));
    }
    // A triangle whose legs are twice the box: vertex 0 at the lower corner,
    // 1 across, 2 up; the box is its lower-left quadrant, the rest is clipped.
    float2 size = max(upper - lower, float2(0.0, 0.0));
    float2 p = lower + float2(vertexIndex == 1 ? 2.0 * size.x : 0.0, vertexIndex == 2 ? 2.0 * size.y : 0.0);
    FullscreenVaryings output;
    output.position = float4(p, 1.0, 1.0); // at the far plane, as the full-screen triangle
    return output;
}
