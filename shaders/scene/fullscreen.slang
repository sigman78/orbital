struct FullscreenVaryings { float4 position : SV_Position; };
[shader("vertex")]
FullscreenVaryings vertexMain(uint vertexIndex : SV_VertexID)
{
    FullscreenVaryings output;
    float2 p = float2((vertexIndex << 1) & 2, vertexIndex & 2);
    output.position = float4(p * 2.0 - 1.0, 0.0, 1.0);
    return output;
}
