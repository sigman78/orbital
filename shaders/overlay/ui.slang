// Dear ImGui draw lists over the presented frame. Vertices are read through a
// pointer in the push constants (the renderer repacks ImDrawVert into 32-byte
// records); colours arrive sRGB-encoded and are decoded here because the
// swapchain is an sRGB target. The font atlas sits in a fixed texture slot.

#include "../scene_shared.h"
#include "../scene/bindings.slang"
#include "../lib/color.slang"

struct UiVertex {
    float4 position_uv; // pixel position, then atlas uv
    uint4 colour;       // x holds the packed ABGR bytes
};

struct UiRoot {
    float2 scale, translate; // pixels to clip space
    UiVertex* vertices;
    uint texture, display; // display: the output encoding, mode (0 sRGB, 1 scRGB, 2 HDR10) in the low byte, paper white in nits above
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
    float4 colour = input.colour * textures[TEX_UI_FONT].Sample(samplers[SAMPLER_CLAMP], input.uv);
    // An HDR swapchain does not encode for us: the panel is drawn at the paper white.
    uint mode = root.display & 0xff;
    if (mode == 0) return colour;
    float paperWhite = float(root.display >> 8);
    if (mode == 1) return float4(colour.rgb * paperWhite / 80.0, colour.a);
    return float4(pqEncode(mul(REC709_TO_REC2020, colour.rgb) * paperWhite / 10000.0), colour.a);
}
