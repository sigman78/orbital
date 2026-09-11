#include "common.slang"

float3 sampleHDR(float2 uv) { return textures[16].Sample(samplers[0], uv).rgb; }
float3 aces(float3 x) { return clamp((x * (2.51 * x + .03)) / (x * (2.43 * x + .59) + .14), 0.0, 1.0); }

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float2 resolution = root.frame.options.xy;
    if (root.mode == 4) {
        float2 uv = position.xy / 16.0;
        float3 c = sampleHDR(uv);
        float lum = dot(c, float3(.2126, .7152, .0722));
        return float4(log(max(lum, .0001)), lum, 0.0, 1.0);
    }
    if (root.mode == 3)
        return textures[6].Sample(samplers[0], position.xy / resolution);
    if (root.mode == 1 || root.mode == 2) {
        float2 uv = position.xy / max(floor(resolution * .25), float2(1.0, 1.0));
        float3 sum = float3(0.0, 0.0, 0.0);
        for (int i = -4; i <= 4; ++i) {
            float2 delta = (root.mode == 1 ? float2(float(i), 0.0) : float2(0.0, float(i))) * 4.0 / resolution;
            float3 c = root.mode == 1 ? sampleHDR(uv + delta) : textures[1].Sample(samplers[0], uv + delta).rgb;
            if (root.mode == 1) c = max(c - float3(.85, .85, .85), float3(0.0, 0.0, 0.0));
            sum += c * exp(-float(i * i) / 8.0);
        }
        return float4(sum / 4.898, 1.0);
    }
    float2 uv = position.xy / resolution;
    float3 color = sampleHDR(uv) + textures[2].Sample(samplers[0], uv).rgb * .24;
    float2 p = uv * 2.0 - 1.0;
    float2 sun = root.frame.screen_sun.xy;
    float vis = root.frame.screen_sun.z;
    float2 aspect = float2(resolution.x / resolution.y, 1.0);
    float dist = length((p - sun) * aspect);
    color += float3(1.0, .6, .3) * vis * .014 / (dist * dist + .012);
    for (int i = 0; i < 4; ++i) {
        float2 center = -sun * (.25 + float(i) * .35);
        float r = length((p - center) * aspect);
        float radius = .035 + float(i) * .018;
        color += float3(.17, .23, .31) * vis * .1 * exp(-pow((r - radius) * 90.0, 2.0));
    }
    color = aces(color * root.frame.forward_exposure.w);
    color *= 1.0 - .17 * pow(length(p * .65), 2.0);
    // Dither in display-code space; linear-space 1/255 noise is far too bright in black space.
    float dither = (hash(float3(position.xy, 0.0)) - .5) / 255.0;
    if (root.frame.options.w > .5) {
        float2 hp = (position.xy - float2(32.0, 26.0)) / float2(1024.0, 256.0);
        if (all(hp >= float2(0.0, 0.0)) && all(hp < float2(1.0, 1.0))) {
            float a = textures[14].Sample(samplers[0], hp).a;
            color = lerp(color, float3(.68, .75, .81), a * .8);
        }
    }
    float3 encoded=lerp(12.92*color,1.055*pow(max(color,0.0),1.0/2.4)-.055,step(.0031308,color));
    encoded=clamp(encoded+dither,0.0,1.0);
    float3 decoded=lerp(encoded/12.92,pow((encoded+.055)/1.055,2.4),step(.04045,encoded));
    return float4(decoded,1.0);
}
