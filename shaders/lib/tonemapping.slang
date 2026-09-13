#pragma once
// --- Tone mapping curves, selected by Frame.quality.y ---------------------------
// All take linear scene radiance in sRGB primaries and return display-linear
// colour in [0, 1]. Matrix products are written out as row dot products so
// the memory layout flag for buffer matrices cannot change their meaning.

static const uint TONE_ACES = 0, TONE_AGX = 1, TONE_NEUTRAL = 2;

// ACES filmic: Stephen Hill's fit of the RRT and sRGB ODT (sRGB to AP1, the
// fitted curve, AP1 back to sRGB).
float3 acesFilmic(float3 c)
{
    float3 v = float3(dot(float3(0.59719, 0.35458, 0.04823), c), dot(float3(0.07600, 0.90834, 0.01566), c),
                      dot(float3(0.02840, 0.13383, 0.83777), c));
    float3 a = v * (v + 0.0245786) - 0.000090537;
    float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    v = a / b;
    return saturate(float3(dot(float3(1.60475, -0.53108, -0.07367), v), dot(float3(-0.10208, 1.10813, -0.00605), v),
                           dot(float3(-0.00327, -0.07276, 1.07602), v)));
}

// AgX (Troy Sobotka), the minimal form: inset to a wider gamut, log2
// encode over the working range, a sixth-order sigmoid, outset, decode.
float3 agx(float3 c)
{
    float3 v = float3(dot(float3(0.842479062253094, 0.0423282422610123, 0.0423756549057051), c),
                      dot(float3(0.0784335999999992, 0.878468636469772, 0.0784336), c),
                      dot(float3(0.0792237451477643, 0.0791661274605434, 0.879142973793104), c));
    const float minEv = -12.47393, maxEv = 4.026069;
    v = clamp(log2(max(v, 1e-10)), minEv, maxEv);
    v = (v - minEv) / (maxEv - minEv);
    float3 v2 = v * v, v4 = v2 * v2;
    v = 15.5 * v4 * v2 - 40.14 * v4 * v + 31.96 * v4 - 6.868 * v2 * v + 0.4298 * v2 + 0.1191 * v - 0.00232;
    v = float3(dot(float3(1.19687900512017, -0.0528968517574562, -0.0529716355144438), v),
               dot(float3(-0.0980208811401368, 1.15190312990417, -0.0980434501171241), v),
               dot(float3(-0.0990297440797205, -0.0989611768448433, 1.15107367264116), v));
    return pow(saturate(v), 2.2); // the sigmoid lands in encoded space; back to display linear
}

// Khronos PBR Neutral: hue-preserving, linear until 0.8, then a soft
// shoulder with a slight desaturation toward white. Its black offset (a
// flare subtraction that takes nearly the whole darkest channel below 0.08)
// is meant for well-exposed product shots; on a scene that is mostly dark
// sky it crushes dim neutrals threefold and inflates the tint of anything
// faint, so the panel scales it (post_more.z; 1 is the published curve).
float3 pbrNeutral(float3 c, float blackOffset)
{
    const float startCompression = 0.8 - 0.04, desaturation = 0.15;
    float x = min(c.r, min(c.g, c.b));
    float offset = (x < 0.08 ? x - 6.25 * x * x : 0.04) * blackOffset;
    c -= offset;
    float peak = max(c.r, max(c.g, c.b));
    if (peak < startCompression) return c;
    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    c *= newPeak / peak;
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return lerp(c, newPeak.xxx, g);
}

float3 toneMap(float3 c, uint curve, float blackOffset)
{
    if (curve == TONE_AGX) return agx(c);
    if (curve == TONE_NEUTRAL) return pbrNeutral(c, blackOffset);
    return acesFilmic(c);
}

