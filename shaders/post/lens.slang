#pragma once
#include "../lib/math.slang"

// Lens ghosts: a few soft, faintly tinted ellipses spaced along the axis from
// the sun through the frame centre, stretched across that axis, plus a very
// faint anamorphic streak. Everything scales with the sun's visibility.
// Ghost images down the lens axis: the sun's image reflected between lens
// elements lands as a few dim, soft-edged copies of the aperture (the blade
// polygon, or a circle with no blades), lightly tinted by the coatings and a
// touch brighter at the rim than in the body, as spherical aberration leaves
// them. Low contrast: they read as a veil over the picture, not as shapes.
float3 lensGhosts(float2 p, float2 sun, float vis, float2 aspect, int blades)
{
    float3 color = float3(0.0, 0.0, 0.0);
    const float offsets[4] = { .22, .55, .95, 1.40 };
    const float sizes[4] = { .06, .11, .08, .15 };
    const float3 tints[4] = { float3(.75, .85, 1.0), float3(1.0, .90, .75), float3(.80, 1.0, .90), float3(1.0, .80, .70) };
    for (int i = 0; i < 4; ++i)
    {
        float2 d = (p + sun * offsets[i]) * aspect / sizes[i];
        // Signed distance to the aperture: the largest projection onto the blade normals.
        float r = length(d);
        if (blades >= 3)
        {
            r = 0.0;
            for (int k = 0; k < 12; ++k)
            {
                if (k >= blades) break;
                float angle = float(k) * 2.0 * PI / float(blades) + .35;
                r = max(r, dot(d, float2(cos(angle), sin(angle))));
            }
        }
        float body = 1.0 - smoothstep(.7, 1.0, r);             // soft edge over the outer third
        float rim = exp(-pow((r - .85) / .12, 2.0)) * .5;       // the brighter rim
        color += tints[i] * vis * .008 * (body + rim);
    }
    return color;
}

// Aperture starburst: blades through the sun with chromatic dispersion along
// each streak, blue near the core and warm toward the tips.
float3 starburst(float2 p, float2 sun, float vis, float2 aspect, int blades, float strength)
{
    float3 color = float3(0.0, 0.0, 0.0);
    float2 d = (p - sun) * aspect;
    for (int k = 0; k < 12; ++k)
    {
        if (k >= blades) break;
        float angle = float(k) * PI / float(blades) + .35;
        float2 dir = float2(cos(angle), sin(angle));
        float along = abs(dot(d, dir)), perp = dot(d, float2(-dir.y, dir.x));
        float width = .0025 + .018 * along;
        float streak = exp(-(perp * perp) / (width * width)) * exp(-along * 2.6);
        float3 tint = lerp(float3(.75, .85, 1.0), float3(1.0, .62, .38), saturate(along * 1.6));
        color += tint * streak;
    }
    return color * vis * .012 * strength;
}

