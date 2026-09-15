#pragma once
// Rec. 709 luma for linear light; Rec. 601 for gamma-encoded samples.
float luma709(float3 c) { return dot(c, float3(.2126, .7152, .0722)); }

float luma601(float3 c) { return dot(c, float3(.299, .587, .114)); }

// Reversible luma/chroma coordinates for temporal neighbourhood clipping.
float3 toYCoCg(float3 c) { return float3(.25 * c.r + .5 * c.g + .25 * c.b, .5 * c.r - .5 * c.b, -.25 * c.r + .5 * c.g - .25 * c.b); }
float3 fromYCoCg(float3 c) { return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z); }

// Piecewise sRGB transfer, used to add display-code dither to linear HDR output.
// Rec. 709 to Rec. 2020 primaries, linear light; and the SMPTE ST 2084 (PQ)
// encoding of a luminance normalized to 10,000 nits, for HDR10 output.
static const float3x3 REC709_TO_REC2020 = float3x3(.6274, .3293, .0433, .0691, .9195, .0114, .0164, .0880, .8956);
float3 pqEncode(float3 normalized)
{
    float3 p = pow(max(normalized, 0.0), .1593017578);
    return pow((.8359375 + 18.8515625 * p) / (1.0 + 18.6875 * p), 78.84375);
}

float3 linearToSRGB(float3 color)
{
    return lerp(12.92 * color, 1.055 * pow(max(color, 0.0), 1.0 / 2.4) - .055, step(.0031308, color));
}

float3 sRGBToLinear(float3 encoded)
{
    return lerp(encoded / 12.92, pow((encoded + .055) / 1.055, 2.4), step(.04045, encoded));
}
