#pragma once
// Equirectangular sampling with seam-correct derivatives; the caller supplies its sampler.
// The *Grad forms take a footprint from sphereFootprint, for use inside control flow
// that diverges per fragment, where ddx/ddy are undefined.
void sphereFootprint(float2 uv, out float2 dx, out float2 dy) { dx = ddx(uv); dy = ddy(uv); }

float4 sampleSphereGrad(Texture2D<float4> tex, SamplerState sampler, float2 uv, float2 dx, float2 dy)
{
    dx.x -= round(dx.x); dy.x -= round(dy.x);
    return tex.SampleGrad(sampler, uv, dx, dy);
}

float4 sampleSphere(Texture2D<float4> tex, SamplerState sampler, float2 uv)
{
    return sampleSphereGrad(tex, sampler, uv, ddx(uv), ddy(uv));
}

float4 sampleSphereBias(Texture2D<float4> tex, SamplerState sampler, float2 uv, float bias)
{
    float2 dx = ddx(uv), dy = ddy(uv);
    dx.x -= round(dx.x); dy.x -= round(dy.x);
    float widen = exp2(bias);
    return tex.SampleGrad(sampler, uv, dx * widen, dy * widen);
}

float magnificationGrad(float2 dx, float2 dy, float2 texels)
{
    float2 d = max(abs(dx), abs(dy)) * texels;
    float lod = log2(max(max(d.x, d.y), 1e-6));
    return saturate(-lod);
}

float magnification(float2 uv, float2 texels) { return magnificationGrad(ddx(uv), ddy(uv), texels); }
