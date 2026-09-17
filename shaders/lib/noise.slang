#pragma once
// Deterministic scalar noise; no frame data or bound resources.
float hash(float3 p)
{
    p = frac(p * .1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float noise(float3 p)
{
    float3 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y),
                lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+float3(1,1,1)),f.x),f.y),f.z);
}

float fbm(float3 p)
{
    float n = 0, a = .5;
    for (int i = 0; i < 4; i++)
    {
        n += a * noise(p);
        p = p * 2.03 + float3(2.1, 1.7, 4.3);
        a *= .5;
    }
    return n;
}

float3 hashGradient(float3 p)
{
    return float3(hash(p), hash(p + 31.7), hash(p + 67.3)) * 2.0 - 1.0; // unnormalised: the range is what the fade needs
}

// Gradient noise in [-1, 1] with quintic fade; smooth at the lattice points.
float gradientNoise(float3 p)
{
    float3 i = floor(p), f = frac(p);
    float3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float c000 = dot(hashGradient(i), f), c100 = dot(hashGradient(i + float3(1, 0, 0)), f - float3(1, 0, 0));
    float c010 = dot(hashGradient(i + float3(0, 1, 0)), f - float3(0, 1, 0)), c110 = dot(hashGradient(i + float3(1, 1, 0)), f - float3(1, 1, 0));
    float c001 = dot(hashGradient(i + float3(0, 0, 1)), f - float3(0, 0, 1)), c101 = dot(hashGradient(i + float3(1, 0, 1)), f - float3(1, 0, 1));
    float c011 = dot(hashGradient(i + float3(0, 1, 1)), f - float3(0, 1, 1)), c111 = dot(hashGradient(i + float3(1, 1, 1)), f - float3(1, 1, 1));
    return lerp(lerp(lerp(c000, c100, u.x), lerp(c010, c110, u.x), u.y), lerp(lerp(c001, c101, u.x), lerp(c011, c111, u.x), u.y), u.z);
}

float hash2(float2 p)
{
    float3 q = frac(float3(p.xyx) * .1031);
    q += dot(q, q.yzx + 33.33);
    return frac((q.x + q.y) * q.z);
}

float noise2(float2 p)
{
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(hash2(i), hash2(i + float2(1, 0)), f.x), lerp(hash2(i + float2(0, 1)), hash2(i + float2(1, 1)), f.x), f.y);
}

float fbm2(float2 p, int octaves)
{
    float n = 0.0, a = .5;
    for (int i = 0; i < octaves; ++i)
    {
        n += a * noise2(p);
        p = p * 2.03 + float2(1.7, 9.2);
        a *= .5;
    }
    return n;
}

// Interleaved gradient noise: a fine, regular screen pattern with no visible
// lumps, the dither of the marches. Rotated by the golden ratio each frame it
// covers [0, 1) evenly over a history's length, so the temporal pass settles it
// faster than a hash; without the temporal pass it stands still, fine grain
// instead of a pattern that shimmers.
float interleavedGradientNoise(float2 pixel)
{
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

float marchJitter(float2 pixel, float frame, bool temporal)
{
    float pattern = interleavedGradientNoise(pixel);
    return temporal ? frac(pattern + frame * 0.61803398875) : pattern;
}
