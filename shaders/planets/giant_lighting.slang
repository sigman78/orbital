#pragma once
#include "../surface/lighting.slang"
#include "giant_lightning.slang"
#include "../lib/scattering.slang"

// A gas giant is lit by scattering, not a surface. The cloud deck itself is
// close to Lambert (Jupiter's continuum Minnaert exponent is 0.9 to 1.0),
// wrapped a few degrees past the terminator; over it lies a thin haze whose
// view path grows as 1/cos toward the limb. The haze is forward scattering,
// so at full phase it veils the deck with little light of its own and the
// limb darkens, at crescent phases it lights up into a rim, and being high
// it stays sunlit a little past the terminator, carrying the glow across.
float3 shadeGiant(Frame* frame, Material m, float3 N, float3 geometricN, float3 L, float3 V, float2 uv, float shear)
{
    float4 haze = frame->giant_more;
    float wrap = haze.y;
    float lit = saturate((dot(N, L) + wrap) / (1.0 + wrap));
    float hazeWrap = wrap * 2.0 + .05;
    float hazeLit = saturate((dot(geometricN, L) + hazeWrap) / (1.0 + hazeWrap));
    float mu = max(dot(geometricN, V), .02);
    float veil = 1.0 - exp(-haze.x / mu);
    float phase = 4.0 * PI * henyeyGreenstein(-dot(L, V), .5); // relative to isotropic
    float3 hazeColour = float3(.55, .62, .74);
    float3 deck = m.albedo / PI * lit * (1.0 - veil);
    float3 scattered = hazeColour / PI * veil * hazeLit * phase;
    // The starlight fill follows the panel's ambient fill, as every surface's does.
    float3 colour = (deck + scattered) * sunlight(frame) * m.visibility + m.albedo * float3(.003, .004, .008) * frame->quality.w;
    // Lightning shows where the sun does not.
    float dark = 1.0 - smoothstep(-.12, .02, dot(geometricN, L));
    float2 footprint = max(abs(ddx(uv)), abs(ddy(uv))); // before the branch: derivatives need uniform flow
    if (dark > 0.0)
    {
        colour += giantLightning(frame->giant_night, uv, frame->camera_time.w, shear, footprint) * dark * (m.albedo * 1.5 + .25);
    }
    return colour;
}

