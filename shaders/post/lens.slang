#pragma once
#include "../lib/math.slang"

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

// Artistic approximation of two internal lens reflections, not a traced lens
// prescription. All distances use image-height units so circles stay circular
// on wide frames. Separate from lateral aberration of the scene itself.
float3 lensHalo(float2 p, float2 sun, float vis, float2 aspect, float2 strengths)
{
    float edge = max(abs(sun.x), abs(sun.y));
    // Finish fading before project_sun's off-screen cutoff, including when the
    // sun is just outside the viewport and no depth samples are available.
    float visibility = vis * (1.0 - smoothstep(1.0, 1.3, edge));
    if (visibility <= 0.0 || all(strengths <= 0.0)) return float3(0.0);

    float2 source = sun * aspect;
    float offAxis = length(source);
    // Regularized direction makes the axial limit continuous, without a sudden
    // rotation or division by zero when the sun passes through the image centre.
    float2 axis = source / max(offAxis, .001);
    float oblique = smoothstep(.05, .65, offAxis);
    float3 color = float3(0.0);
    if (strengths.x > 0.0) {
        float2 d = p * aspect - source * .55;
        float radius = .62 + .08 * saturate(offAxis);
        float r = length(d);
        float facing = dot(d / max(r, .001), axis);
        float width = .075 + .035 * (.5 + .5 * facing) * oblique;
        float ring = exp(-.5 * pow((r - radius) / width, 2.0));
        float veil = exp(-.5 * pow(r / (radius * .8), 2.0));
        // Broad shoulders and a dim interior prevent a hard doughnut silhouette.
        color += float3(1.0, .94, .83) * (.022 * ring * (1.0 - .45 * facing * oblique)
                                         + .003 * veil) * strengths.x;
    }
    if (strengths.y > 0.0) {
        float2 d = p * aspect - source * .28;
        float r = length(d);
        // Brighten the arc on the side away from the sun.
        float facing = saturate(-dot(d / max(r, .001), axis));
        float arc = pow(facing, 4.0) * oblique;
        float radius = .36 + .09 * saturate(offAxis);
        // Overlapping RGB profiles approximate dispersion: blue inside, red
        // outside, mixed hues between. No hue wheel painted along the arc.
        float dispersion = .026 * (.5 + .5 * oblique);
        float3 distance = (r - radius - float3(dispersion, 0.0, -dispersion)) / .024;
        float3 spectrum = exp(-.5 * distance * distance);
        color += spectrum * float3(1.0, .92, .82) * arc * .055 * strengths.y;
    }
    return color * visibility;
}

