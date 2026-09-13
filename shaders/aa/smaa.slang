#include "../scene/frame_bindings.slang"
#include "../lib/color.slang"

// SMAA 1x, Jimenez et al. 2012, ported from SMAA.hlsl (see third_party/smaa/LICENSE.txt):
// preset HIGH, luma edge detection, diagonal and corner patterns. Three
// fullscreen passes over the tone-mapped image in textures[TEX_LDR]: edges into
// textures[TEX_SMAA_EDGES] (mode 0), blending weights into textures[TEX_SMAA_WEIGHTS] (mode 1, through
// the area texture in [32] and the search texture in [33]), and the
// neighbourhood blend into the final image (mode 2). The vertex-side offsets
// of the reference are computed here per pixel; everything else is the
// reference algorithm with the offsets and texel-space maths written out.

static const uint MODE_EDGES = 0, MODE_WEIGHTS = 1, MODE_BLEND = 2;
static const float SMAA_THRESHOLD = .1;
static const int SMAA_MAX_SEARCH_STEPS = 16, SMAA_MAX_SEARCH_STEPS_DIAG = 8;
static const float SMAA_CORNER_ROUNDING_NORM = .25;
static const float SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR = 2.0;
static const float SMAA_AREATEX_MAX_DISTANCE = 16.0, SMAA_AREATEX_MAX_DISTANCE_DIAG = 20.0;
static const float2 SMAA_AREATEX_PIXEL_SIZE = 1.0 / float2(160.0, 560.0);
static const float SMAA_AREATEX_SUBTEX_SIZE = 1.0 / 7.0;
static const float2 SMAA_SEARCHTEX_SIZE = float2(66.0, 33.0), SMAA_SEARCHTEX_PACKED_SIZE = float2(64.0, 16.0);

// rt.xy is the texel size, rt.zw the resolution (SMAA_RT_METRICS).
static float4 rt;

float4 colourAt(float2 uv) { return textures[TEX_LDR].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0); }
float2 edgesAt(float2 uv) { return textures[TEX_SMAA_EDGES].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0).rg; }
float2 edgesAt(float2 uv, int2 offset) { return textures[TEX_SMAA_EDGES].SampleLevel(samplers[SAMPLER_CLAMP], uv + float2(offset) * rt.xy, 0.0).rg; } // texel offsets must be constants in SPIR-V; shift the coordinate instead
float2 areaAt(float2 uv) { return textures[TEX_SMAA_AREA].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0).rg; }
float searchAt(float2 uv) { return textures[TEX_SMAA_SEARCH].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0).r; }
float4 weightsAt(float2 uv) { return textures[TEX_SMAA_WEIGHTS].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0); }

// Luma of the gamma-encoded colour, as the edge detection expects; the image
// is stored sRGB, so samples come back linear.
float lumaAt(float2 uv)
{
    return luma709(pow(max(colourAt(uv).rgb, 0.0), 1.0 / 2.2));
}

// --- Pass 1: luma edge detection --------------------------------------------
float2 edgeDetection(float2 uv)
{
    float2 t = rt.xy;
    float L = lumaAt(uv);
    float Lleft = lumaAt(uv + t * float2(-1.0, 0.0)), Ltop = lumaAt(uv + t * float2(0.0, -1.0));
    float4 delta;
    delta.xy = abs(L - float2(Lleft, Ltop));
    float2 edges = step(SMAA_THRESHOLD, delta.xy);
    if (dot(edges, float2(1.0, 1.0)) == 0.0) discard;
    float Lright = lumaAt(uv + t * float2(1.0, 0.0)), Lbottom = lumaAt(uv + t * float2(0.0, 1.0));
    delta.zw = abs(L - float2(Lright, Lbottom));
    float2 maxDelta = max(delta.xy, delta.zw);
    float Lleftleft = lumaAt(uv + t * float2(-2.0, 0.0)), Ltoptop = lumaAt(uv + t * float2(0.0, -2.0));
    delta.zw = abs(float2(Lleft, Ltop) - float2(Lleftleft, Ltoptop));
    maxDelta = max(maxDelta, delta.zw);
    float finalDelta = max(maxDelta.x, maxDelta.y);
    // Local contrast adaptation.
    edges *= step(finalDelta, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta.xy);
    return edges;
}

// --- Pass 2: blending weights -------------------------------------------------
// Two binary edge values decoded from one bilinear fetch at a quarter offset.
float2 decodeDiagBilinearAccess(float2 e)
{
    e.r = e.r * abs(5.0 * e.r - 5.0 * .75);
    return round(e);
}
float4 decodeDiagBilinearAccess(float4 e)
{
    e.rb = e.rb * abs(5.0 * e.rb - 5.0 * .75);
    return round(e);
}

float2 searchDiag1(float2 uv, float2 dir, out float2 e)
{
    float4 coord = float4(uv, -1.0, 1.0);
    float3 t = float3(rt.xy, 1.0);
    e = float2(0.0, 0.0);
    while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > .9)
    {
        coord.xyz = t * float3(dir, 1.0) + coord.xyz;
        e = edgesAt(coord.xy);
        coord.w = dot(e, float2(.5, .5));
    }
    return coord.zw;
}

float2 searchDiag2(float2 uv, float2 dir, out float2 e)
{
    float4 coord = float4(uv, -1.0, 1.0);
    coord.x += .25 * rt.x;
    float3 t = float3(rt.xy, 1.0);
    e = float2(0.0, 0.0);
    while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > .9)
    {
        coord.xyz = t * float3(dir, 1.0) + coord.xyz;
        e = decodeDiagBilinearAccess(edgesAt(coord.xy));
        coord.w = dot(e, float2(.5, .5));
    }
    return coord.zw;
}

float2 areaDiag(float2 dist, float2 e, float offset)
{
    float2 uv = float2(SMAA_AREATEX_MAX_DISTANCE_DIAG, SMAA_AREATEX_MAX_DISTANCE_DIAG) * e + dist;
    uv = SMAA_AREATEX_PIXEL_SIZE * uv + .5 * SMAA_AREATEX_PIXEL_SIZE;
    uv.x += .5; // diagonal areas sit in the second half of the texture
    uv.y += SMAA_AREATEX_SUBTEX_SIZE * offset;
    return areaAt(uv);
}

float2 calculateDiagWeights(float2 uv, float2 e, float4 subsampleIndices)
{
    float2 weights = float2(0.0, 0.0);
    float4 d;
    float2 end;
    if (e.r > 0.0)
    {
        d.xz = searchDiag1(uv, float2(-1.0, 1.0), end);
        d.x += float(end.y > .9);
    }
    else
        d.xz = float2(0.0, 0.0);
    d.yw = searchDiag1(uv, float2(1.0, -1.0), end);

    if (d.x + d.y > 2.0)
    {
        float4 coords = float4(-d.x + .25, d.x, d.y, -d.y - .25) * rt.xyxy + uv.xyxy;
        float4 c;
        c.xy = edgesAt(coords.xy, int2(-1, 0));
        c.zw = edgesAt(coords.zw, int2(1, 0));
        c.yxwz = decodeDiagBilinearAccess(c.xyzw);
        float2 cc = float2(2.0, 2.0) * c.xz + c.yw;
        // Drop the crossing edge where the line end was not found.
        cc = select(step(.9, d.zw) > .5, float2(0.0, 0.0), cc);
        weights += areaDiag(d.xy, cc, subsampleIndices.z);
    }

    d.xz = searchDiag2(uv, float2(-1.0, -1.0), end);
    if (edgesAt(uv, int2(1, 0)).r > 0.0)
    {
        d.yw = searchDiag2(uv, float2(1.0, 1.0), end);
        d.y += float(end.y > .9);
    }
    else
        d.yw = float2(0.0, 0.0);

    if (d.x + d.y > 2.0)
    {
        float4 coords = float4(-d.x, -d.x, d.y, d.y) * rt.xyxy + uv.xyxy;
        float4 c;
        c.x = edgesAt(coords.xy, int2(-1, 0)).g;
        c.y = edgesAt(coords.xy, int2(0, -1)).r;
        c.zw = edgesAt(coords.zw, int2(1, 0)).gr;
        float2 cc = float2(2.0, 2.0) * c.xz + c.yw;
        cc = select(step(.9, d.zw) > .5, float2(0.0, 0.0), cc);
        weights += areaDiag(d.xy, cc, subsampleIndices.w).gr;
    }
    return weights;
}

// How much length the last search step adds (0, 1 or 2) given the
// bilinearly fetched edge pair.
float searchLength(float2 e, float offset)
{
    float2 scale = SMAA_SEARCHTEX_SIZE * float2(.5, -1.0);
    float2 bias = SMAA_SEARCHTEX_SIZE * float2(offset, 1.0);
    scale += float2(-1.0, 1.0);
    bias += float2(.5, -.5);
    scale *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;
    bias *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;
    return searchAt(scale * e + bias);
}

float searchXLeft(float2 uv, float end)
{
    float2 e = float2(0.0, 1.0);
    while (uv.x > end && e.g > .8281 && e.r == 0.0)
    {
        e = edgesAt(uv);
        uv = -float2(2.0, 0.0) * rt.xy + uv;
    }
    float offset = -(255.0 / 127.0) * searchLength(e, 0.0) + 3.25;
    return rt.x * offset + uv.x;
}

float searchXRight(float2 uv, float end)
{
    float2 e = float2(0.0, 1.0);
    while (uv.x < end && e.g > .8281 && e.r == 0.0)
    {
        e = edgesAt(uv);
        uv = float2(2.0, 0.0) * rt.xy + uv;
    }
    float offset = -(255.0 / 127.0) * searchLength(e, .5) + 3.25;
    return -rt.x * offset + uv.x;
}

float searchYUp(float2 uv, float end)
{
    float2 e = float2(1.0, 0.0);
    while (uv.y > end && e.r > .8281 && e.g == 0.0)
    {
        e = edgesAt(uv);
        uv = -float2(0.0, 2.0) * rt.xy + uv;
    }
    float offset = -(255.0 / 127.0) * searchLength(e.gr, 0.0) + 3.25;
    return rt.y * offset + uv.y;
}

float searchYDown(float2 uv, float end)
{
    float2 e = float2(1.0, 0.0);
    while (uv.y < end && e.r > .8281 && e.g == 0.0)
    {
        e = edgesAt(uv);
        uv = float2(0.0, 2.0) * rt.xy + uv;
    }
    float offset = -(255.0 / 127.0) * searchLength(e.gr, .5) + 3.25;
    return -rt.y * offset + uv.y;
}

float2 area(float2 dist, float e1, float e2, float offset)
{
    float2 uv = float2(SMAA_AREATEX_MAX_DISTANCE, SMAA_AREATEX_MAX_DISTANCE) * round(4.0 * float2(e1, e2)) + dist;
    uv = SMAA_AREATEX_PIXEL_SIZE * uv + .5 * SMAA_AREATEX_PIXEL_SIZE;
    uv.y = SMAA_AREATEX_SUBTEX_SIZE * offset + uv.y;
    return areaAt(uv);
}

void detectHorizontalCornerPattern(inout float2 weights, float4 uv, float2 d)
{
    float2 leftRight = step(d.xy, d.yx);
    float2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;
    rounding /= leftRight.x + leftRight.y;
    float2 factor = float2(1.0, 1.0);
    factor.x -= rounding.x * edgesAt(uv.xy, int2(0, 1)).r;
    factor.x -= rounding.y * edgesAt(uv.zw, int2(1, 1)).r;
    factor.y -= rounding.x * edgesAt(uv.xy, int2(0, -2)).r;
    factor.y -= rounding.y * edgesAt(uv.zw, int2(1, -2)).r;
    weights *= saturate(factor);
}

void detectVerticalCornerPattern(inout float2 weights, float4 uv, float2 d)
{
    float2 leftRight = step(d.xy, d.yx);
    float2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;
    rounding /= leftRight.x + leftRight.y;
    float2 factor = float2(1.0, 1.0);
    factor.x -= rounding.x * edgesAt(uv.xy, int2(1, 0)).g;
    factor.x -= rounding.y * edgesAt(uv.zw, int2(1, 1)).g;
    factor.y -= rounding.x * edgesAt(uv.xy, int2(-2, 0)).g;
    factor.y -= rounding.y * edgesAt(uv.zw, int2(-2, 1)).g;
    weights *= saturate(factor);
}

float4 blendingWeights(float2 uv)
{
    float2 pixcoord = uv * rt.zw;
    // Search offsets (see @PSEUDO_GATHER4 in the reference) and search ends.
    float4 offset0 = rt.xyxy * float4(-.25, -.125, 1.25, -.125) + uv.xyxy;
    float4 offset1 = rt.xyxy * float4(-.125, -.25, -.125, 1.25) + uv.xyxy;
    float4 offset2 = rt.xxyy * float4(-2.0, 2.0, -2.0, 2.0) * float(SMAA_MAX_SEARCH_STEPS) + float4(offset0.xz, offset1.yw);
    const float4 subsampleIndices = float4(0.0, 0.0, 0.0, 0.0);

    float4 weights = float4(0.0, 0.0, 0.0, 0.0);
    float2 e = edgesAt(uv);

    if (e.g > 0.0) // edge at north
    {
        // Diagonals have both north and west edges; they take priority.
        weights.rg = calculateDiagWeights(uv, e, subsampleIndices);
        if (weights.r == -weights.g)
        {
            float2 d;
            float3 coords;
            coords.x = searchXLeft(offset0.xy, offset2.x);
            coords.y = offset1.y;
            d.x = coords.x;
            float e1 = edgesAt(coords.xy).r;
            coords.z = searchXRight(offset0.zw, offset2.y);
            d.y = coords.z;
            d = abs(round(rt.zz * d - pixcoord.xx));
            float2 sqrtD = sqrt(d);
            float e2 = edgesAt(coords.zy, int2(1, 0)).r;
            weights.rg = area(sqrtD, e1, e2, subsampleIndices.y);
            coords.y = uv.y;
            detectHorizontalCornerPattern(weights.rg, coords.xyzy, d);
        }
        else
            e.r = 0.0; // a diagonal was found: skip the vertical search
    }

    if (e.r > 0.0) // edge at west
    {
        float2 d;
        float3 coords;
        coords.y = searchYUp(offset1.xy, offset2.z);
        coords.x = offset0.x;
        d.x = coords.y;
        float e1 = edgesAt(coords.xy).g;
        coords.z = searchYDown(offset1.zw, offset2.w);
        d.y = coords.z;
        d = abs(round(rt.ww * d - pixcoord.yy));
        float2 sqrtD = sqrt(d);
        float e2 = edgesAt(coords.xz, int2(0, 1)).g;
        weights.ba = area(sqrtD, e1, e2, subsampleIndices.x);
        coords.x = uv.x;
        detectVerticalCornerPattern(weights.ba, coords.xyxz, d);
    }
    return weights;
}

// --- Pass 3: neighbourhood blending -------------------------------------------
float4 neighbourhoodBlending(float2 uv)
{
    float4 offset = rt.xyxy * float4(1.0, 0.0, 0.0, 1.0) + uv.xyxy;
    float4 a;
    a.x = weightsAt(offset.xy).a; // right
    a.y = weightsAt(offset.zw).g; // top
    a.wz = weightsAt(uv).xz;      // bottom, left
    if (dot(a, float4(1.0, 1.0, 1.0, 1.0)) < 1e-5) return colourAt(uv);
    bool h = max(a.x, a.z) > max(a.y, a.w);
    float4 blendingOffset = h ? float4(a.x, 0.0, a.z, 0.0) : float4(0.0, a.y, 0.0, a.w);
    float2 blendingWeight = h ? a.xz : a.yw;
    blendingWeight /= dot(blendingWeight, float2(1.0, 1.0));
    float4 blendingCoord = blendingOffset * float4(rt.xy, -rt.xy) + uv.xyxy;
    // Bilinear filtering mixes the pixel with its chosen neighbour.
    return blendingWeight.x * colourAt(blendingCoord.xy) + blendingWeight.y * colourAt(blendingCoord.zw);
}

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    float2 size = root.frame.options.xy;
    rt = float4(1.0 / size, size);
    float2 uv = position.xy / size;
    if (root.mode == MODE_EDGES) return float4(edgeDetection(uv), 0.0, 1.0);
    if (root.mode == MODE_WEIGHTS) return blendingWeights(uv);
    return neighbourhoodBlending(uv);
}
