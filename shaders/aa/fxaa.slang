#pragma once
#include "../lib/color.slang"

// FXAA over the tone-mapped image in image (the quality variant: edge
// end search plus sub-pixel blend), written to the final image in mode FXAA.
static const float FXAA_EDGE_MIN = .0312, FXAA_EDGE_RELATIVE = .125;
static const int FXAA_SEARCH_STEPS = 8;
static const float FXAA_SUBPIXEL = .5;
static const float FXAA_MIN_EDGE_TEXELS = 2.5; // shorter features are specks, not edges
static const float FXAA_SPECK_MAX_LUMA = .35;   // specks brighter than this are close-rock highlights

float fxaaLuma(Texture2D<float4> image, SamplerState sampler, float2 uv) { return luma601(image.SampleLevel(sampler, uv, 0.0).rgb); }

float4 fxaa(Texture2D<float4> image, SamplerState sampler, float2 uv, float2 texel)
{
    float4 centre = image.SampleLevel(sampler, uv, 0.0);
    float lumaM = luma601(centre.rgb);
    float lumaN = fxaaLuma(image, sampler, uv + float2(0, -texel.y)), lumaS = fxaaLuma(image, sampler, uv + float2(0, texel.y));
    float lumaW = fxaaLuma(image, sampler, uv + float2(-texel.x, 0)), lumaE = fxaaLuma(image, sampler, uv + float2(texel.x, 0));
    float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    float range = lumaMax - lumaMin;
    if (range < max(FXAA_EDGE_MIN, lumaMax * FXAA_EDGE_RELATIVE)) return centre;
    // A dim isolated speck, brighter than all four neighbours, is a distant
    // rock rather than an edge: it stays sharp, the temporal pass settles it.
    // Bright ones are highlights on close rocks, which still want the filter.
    float lumaNeighbourMax = max(max(lumaN, lumaS), max(lumaW, lumaE));
    if (lumaM > lumaNeighbourMax && lumaM < FXAA_SPECK_MAX_LUMA) return centre;
    float lumaNW = fxaaLuma(image, sampler, uv + float2(-texel.x, -texel.y)), lumaNE = fxaaLuma(image, sampler, uv + float2(texel.x, -texel.y));
    float lumaSW = fxaaLuma(image, sampler, uv + float2(-texel.x, texel.y)), lumaSE = fxaaLuma(image, sampler, uv + float2(texel.x, texel.y));
    float lumaNS = lumaN + lumaS, lumaWE = lumaW + lumaE;
    float lumaWCorners = lumaNW + lumaSW, lumaECorners = lumaNE + lumaSE;
    float lumaNCorners = lumaNW + lumaNE, lumaSCorners = lumaSW + lumaSE;
    float edgeHorizontal = abs(-2.0 * lumaW + lumaWCorners) + abs(-2.0 * lumaM + lumaNS) * 2.0 + abs(-2.0 * lumaE + lumaECorners);
    float edgeVertical = abs(-2.0 * lumaN + lumaNCorners) + abs(-2.0 * lumaM + lumaWE) * 2.0 + abs(-2.0 * lumaS + lumaSCorners);
    bool horizontal = edgeHorizontal >= edgeVertical;
    float luma1 = horizontal ? lumaN : lumaW, luma2 = horizontal ? lumaS : lumaE;
    float gradient1 = luma1 - lumaM, gradient2 = luma2 - lumaM;
    bool steepest1 = abs(gradient1) >= abs(gradient2);
    float gradientScaled = .25 * max(abs(gradient1), abs(gradient2));
    float stepLength = horizontal ? texel.y : texel.x;
    float lumaLocalAverage = steepest1 ? .5 * (luma1 + lumaM) : .5 * (luma2 + lumaM);
    if (steepest1) stepLength = -stepLength;
    float2 currentUV = uv;
    if (horizontal) currentUV.y += stepLength * .5; else currentUV.x += stepLength * .5;
    float2 offset = horizontal ? float2(texel.x, 0) : float2(0, texel.y);
    float2 uv1 = currentUV - offset, uv2 = currentUV + offset;
    float lumaEnd1 = fxaaLuma(image, sampler, uv1) - lumaLocalAverage, lumaEnd2 = fxaaLuma(image, sampler, uv2) - lumaLocalAverage;
    bool reached1 = abs(lumaEnd1) >= gradientScaled, reached2 = abs(lumaEnd2) >= gradientScaled;
    for (int i = 0; i < FXAA_SEARCH_STEPS && !(reached1 && reached2); ++i)
    {
        float stride = 1.0 + float(i) * .5;
        if (!reached1) { uv1 -= offset * stride; lumaEnd1 = fxaaLuma(image, sampler, uv1) - lumaLocalAverage; reached1 = abs(lumaEnd1) >= gradientScaled; }
        if (!reached2) { uv2 += offset * stride; lumaEnd2 = fxaaLuma(image, sampler, uv2) - lumaLocalAverage; reached2 = abs(lumaEnd2) >= gradientScaled; }
    }
    float distance1 = horizontal ? uv.x - uv1.x : uv.y - uv1.y;
    float distance2 = horizontal ? uv2.x - uv.x : uv2.y - uv.y;
    bool direction1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = distance1 + distance2;
    // The search ended within a couple of texels each way: a few-pixel rock, not an edge.
    if (edgeThickness < FXAA_MIN_EDGE_TEXELS * (horizontal ? texel.x : texel.y)) return centre;
    float pixelOffset = -distanceFinal / edgeThickness + .5;
    bool isLumaCentreSmaller = lumaM < lumaLocalAverage;
    bool correctVariation = ((direction1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCentreSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0;
    // Sub-pixel aliasing: blend toward the 3x3 average where the centre stands out.
    float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaNS + lumaWE) + lumaWCorners + lumaECorners);
    float subPixelOffset1 = saturate(abs(lumaAverage - lumaM) / range);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    finalOffset = max(finalOffset, subPixelOffset2 * subPixelOffset2 * FXAA_SUBPIXEL);
    float2 finalUV = uv;
    if (horizontal) finalUV.y += finalOffset * stepLength; else finalUV.x += finalOffset * stepLength;
    return image.SampleLevel(sampler, finalUV, 0.0);
}

