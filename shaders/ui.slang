// Dear ImGui draw lists over the presented frame. Vertices are read through a
// pointer in the push constants (the renderer repacks ImDrawVert into 32-byte
// records); colours arrive sRGB-encoded and are decoded here because the
// swapchain is an sRGB target. The font atlas sits in a fixed texture slot.

#include "scene_shared.h"
#include "scene/bindings.slang"

struct UiVertex {
    float4 position_uv; // pixel position, then atlas uv
    uint4 colour;       // x holds the packed ABGR bytes
};

struct UiRoot {
    float2 scale, translate; // pixels to clip space
    UiVertex* vertices;
    uint texture, unused;
};

[[vk::push_constant]] ConstantBuffer<UiRoot> root;

struct UiVaryings {
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : UV;
    [[vk::location(1)]] float4 colour : COLOUR;
};

[shader("vertex")]
UiVaryings vertexMain(uint vertexIndex : SV_VertexID)
{
    UiVertex v = root.vertices[vertexIndex];
    uint c = v.colour.x;
    float4 colour = float4(float(c & 0xff), float((c >> 8) & 0xff), float((c >> 16) & 0xff), float((c >> 24) & 0xff)) / 255.0;
    colour.rgb = pow(colour.rgb, 2.2);
    UiVaryings output;
    output.position = float4(v.position_uv.xy * root.scale + root.translate, 0.0, 1.0);
    output.uv = v.position_uv.zw;
    output.colour = colour;
    return output;
}

[shader("fragment")]
float4 fragmentMain(UiVaryings input) : SV_Target0
{
    return input.colour * textures[TEX_UI_FONT].Sample(samplers[SAMPLER_CLAMP], input.uv);
}
