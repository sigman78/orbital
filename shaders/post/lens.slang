#pragma once
#include "../lib/math.slang"

// The lens flare stack: the sun's image reflected between the lens elements,
// drawn as a set of flare elements along the flare axis, the line through the
// image centre and the sun. Geometry is in image-height units (half the frame
// height is 1, q = p * aspect) so circles stay circular on wide frames. The sun
// sits at s = sun * aspect and every element is centred at s * offset: offset 1
// is the sun itself, 0 the image centre, negative the far side of the centre.
//
// Down the stack from the sun (offsets after the names):
//   glare          the light's own glow, at full resolution in the composite
//   starburst      aperture blades through the sun, full resolution
//   mini crescent  a small dispersed arc at .48 facing the sun, red inside
//   main ring      a large neutral ring about .45, two lobes on the axis with
//                  the far lobe brighter: the main lens
//   streak         a thin line along the axis (full resolution) carrying two
//                  spindle knots, white near the centre and blue at .48
//   ghosts         soft coloured discs from .5 to -.8, each rim brighter than
//                  its body and the body brighter than its core
//   big crescent   a wide dispersed arc on the far side, its arc turned away from the sun
//
// Major elements (ring and crescents) show with the sun in frame and fade as
// it leaves; the minor ones (ghosts) are lost in the glare until the sun nears
// the edge and stay while it is just outside; the streak peaks at the edge.
// Not a traced lens prescription: an arrangement fitted to reference footage.

// Visibility of the major, minor and streak groups against the sun's distance
// from the frame edge (1 at the edge, in frame units). The ghosts are lost in
// the glare with the sun well inside the frame: a floor there left their flat
// discs over dark surfaces. The far cutoffs end before the sun projection's
// off-screen reach in project_sun.
float3 flareGroupWeights(float2 sun)
{
    float edge = max(abs(sun.x), abs(sun.y));
    float major = 1.0 - smoothstep(1.0, 1.35, edge);
    float minor = smoothstep(.5, 1.05, edge) * (1.0 - smoothstep(1.7, 2.1, edge));
    float streak = smoothstep(.55, .95, edge) * (1.0 - smoothstep(1.15, 1.45, edge));
    return float3(major, minor, streak);
}

// A soft aperture image over its radius: the body fades over its outer fifth,
// the core is dimmed by `hollow` and a rim of `rim` sits just inside the edge.
float ghostDisc(float r, float rim, float hollow)
{
    float body = 1.0 - smoothstep(.8, 1.0, r);
    float core = 1.0 - hollow * (1.0 - smoothstep(0.0, .8, r));
    float edge = exp(-pow((r - .85) / .1, 2.0));
    return body * (core + rim * edge);
}

// A dispersed arc: three radial Gaussians offset per channel, red inside and
// blue outside, masked to the side of the circle that faces `axis`. Part of
// the light stays neutral, as the references' pastel bands do.
float3 dispersedArc(float2 d, float2 axis, float radius, float width, float dispersion, float sharpness)
{
    float r = length(d);
    float facing = saturate(dot(d / max(r, .001), axis));
    float3 distance = (r - radius - float3(-dispersion, 0.0, dispersion)) / width;
    float3 spectrum = exp(-.5 * distance * distance);
    float neutral = (spectrum.r + spectrum.g + spectrum.b) / 3.0;
    return lerp(spectrum, float3(neutral), .35) * pow(facing, sharpness);
}

// A spindle knot on the axis: a bright core tapering to points along it.
float spindle(float along, float perp, float extent, float width)
{
    float taper = 1.0 - saturate(abs(along) / extent);
    float across = exp(-pow(perp / (width * taper + .008), 2.0));
    return across * taper * taper;
}

// The coloured ghosts, from the reference footage: offset along the axis,
// radius, tint, and shape as rim, hollow and intensity.
static const int GHOST_COUNT = 9;
static const float GHOST_OFFSET[GHOST_COUNT] = { .50, .41, .20, .105, -.175, -.23, -.33, -.45, -.81 };
static const float GHOST_RADIUS[GHOST_COUNT] = { .04, .05, .20, .035, .12, .11, .11, .045, .25 };
static const float3 GHOST_TINT[GHOST_COUNT] = {
    float3(1.0, .75, .55), float3(1.0, .8, .6), float3(1.0, .55, .8), float3(1.0, .8, .4), float3(.45, .6, 1.0),
    float3(.5, 1.0, .6), float3(1.0, .9, .45), float3(1.0, .5, .6), float3(1.0, .6, .45) };
static const float3 GHOST_SHAPE[GHOST_COUNT] = {
    float3(.2, .1, .005), float3(.2, .1, .006), float3(.7, .7, .012), float3(.3, .2, .010), float3(.7, .3, .012),
    float3(.4, .3, .006), float3(.8, .4, .012), float3(.5, .3, .008), float3(.4, .6, .010) };

// The soft part of the stack, drawn at a fraction of the frame: the main ring,
// both crescents, the ghosts and the streak's spindles. strengths: main ring,
// big crescent, mini crescent, ghosts; tune: ghost spread (scales the offsets),
// ghost size (scales the radii), streak.
float3 lensStack(float2 p, float2 sun, float vis, float2 aspect, float4 strengths, float3 tune)
{
    float3 weights = flareGroupWeights(sun) * vis;
    if (all(weights <= 0.0)) return float3(0.0);
    float2 q = p * aspect, s = sun * aspect;
    float offAxis = length(s);
    // Regularized direction keeps the axial limit continuous through the centre.
    float2 axis = s / max(offAxis, .001);
    float oblique = smoothstep(.05, .5, offAxis);
    float3 color = float3(0.0);
    float major = weights.x;
    if (major > 0.0 && strengths.x > 0.0) {
        float2 d = q - s * .45;
        float r = length(d);
        float facing = dot(d / max(r, .001), axis);
        float radius = .5 + .1 * offAxis;
        float ring = exp(-.5 * pow((r - radius) / .06, 2.0));
        // Two lobes on the axis, the far one brighter; uniform with the sun on axis.
        float lobes = lerp(1.0, (.12 + .88 * facing * facing) * (1.0 + .35 * saturate(-facing)), oblique);
        // A dim interior keeps the ring from reading as a hard doughnut.
        float veil = exp(-.5 * pow(r / (radius * .8), 2.0));
        // Fainter on axis, where the glare already fills the frame.
        color += float3(1.0, .96, .9) * (.014 * ring * lobes + .002 * veil) * lerp(.5, 1.0, oblique) * strengths.x * major;
    }
    if (major > 0.0 && strengths.z > 0.0) {
        float radius = .2 + .09 * offAxis;
        color += dispersedArc(q - s * .48, axis, radius, .028, .03, 3.0) * float3(1.0, .95, .85)
                 * .025 * oblique * strengths.z * major;
    }
    if (major > 0.0 && strengths.y > 0.0) {
        // Centred past the sun's mirror point with its arc turned away from the
        // sun, so the arc lies near the far edge of the frame; the band scales
        // with the arc.
        float radius = .25 + .42 * offAxis;
        color += dispersedArc(q + s * .6, -axis, radius, .12 * radius, .14 * radius, 2.0) * float3(1.0, .95, .9)
                 * .014 * oblique * strengths.y * major;
    }
    if (weights.y > 0.0 && strengths.w > 0.0) {
        for (int i = 0; i < GHOST_COUNT; ++i) {
            float radius = GHOST_RADIUS[i] * tune.y;
            float r = length(q - s * (GHOST_OFFSET[i] * tune.x)) / radius;
            if (r < 1.0)
                color += GHOST_TINT[i] * ghostDisc(r, GHOST_SHAPE[i].x, GHOST_SHAPE[i].y) * GHOST_SHAPE[i].z
                         * strengths.w * weights.y;
        }
    }
    if (weights.z > 0.0 && tune.z > 0.0) {
        float along = dot(q, axis), perp = dot(q, float2(-axis.y, axis.x));
        color += float3(1.0, .97, .92) * spindle(along - .08 * offAxis, perp, .2, .035) * .08 * tune.z * weights.z;
        color += float3(.75, .85, 1.0) * spindle(along - .48 * offAxis, perp, .12, .025) * .025 * tune.z * weights.z;
    }
    return color;
}

// The thin streak along the flare axis, at full resolution: a bright core with
// a soft skirt, strongest around the white knot near the centre.
float3 lensStreak(float2 p, float2 sun, float vis, float2 aspect, float strength)
{
    float weight = flareGroupWeights(sun).z * vis * strength;
    if (weight <= 0.0) return float3(0.0);
    float2 q = p * aspect, s = sun * aspect;
    float offAxis = length(s);
    float2 axis = s / max(offAxis, .001);
    float along = dot(q, axis), perp = dot(q, float2(-axis.y, axis.x));
    float core = exp(-pow(perp / .006, 2.0));
    float skirt = exp(-pow(perp / .025, 2.0)) * .35;
    float profile = .2 + .8 * exp(-pow((along - .08 * offAxis) / .7, 2.0));
    return float3(1.0, .97, .92) * (core + skirt) * profile * .014 * weight;
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
