#pragma once
// Equirectangular sampling with seam-correct derivatives; the caller supplies its sampler.
float4 sampleSphere(Texture2D<float4> tex, SamplerState sampler, float2 uv)
{
    float2 dx = ddx(uv), dy = ddy(uv);
    dx.x -= round(dx.x); dy.x -= round(dy.x);
    return tex.SampleGrad(sampler, uv, dx, dy);
}

float4 sampleSphereBias(Texture2D<float4> tex, SamplerState sampler, float2 uv, float bias)
{
    float2 dx = ddx(uv), dy = ddy(uv);
    dx.x -= round(dx.x); dy.x -= round(dy.x);
    float widen = exp2(bias);
    return tex.SampleGrad(sampler, uv, dx * widen, dy * widen);
}

float magnification(float2 uv, float2 texels)
{
    float2 d = max(abs(ddx(uv)), abs(ddy(uv))) * texels;
    float lod = log2(max(max(d.x, d.y), 1e-6));
    return saturate(-lod);
}
