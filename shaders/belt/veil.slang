#pragma once
#include "../scene/frame_records.slang"
#include "belt.slang"

// The twinkle veil: a fine glittering texture over the whole belt, standing for the
// multitude of distant rocks. Points on three planes through the slab, in cells that
// orbit with their band (world-anchored, so the veil drifts and parallaxes), each with
// a facet that turns at its own rate and flashes when it lines up with the half vector
// between the sun and the camera, so the twinkles are sparse, sunward, and die in the
// planets' shadows and the belt's own. Art on top of the rocks, not energy accounting:
// the panel sets density, brightness, sharpness, rate, drift and tint. Both marches take
// it at their crossing of each plane, the near one at half resolution and the far
// composite at full, each with its own pixel scale.

static const int VEIL_LAYERS = 3;
static const float VEIL_HEIGHTS[VEIL_LAYERS] = { -1.0, 0.0, 1.0 }; // in half thicknesses
static const float VEIL_CELL = .05;            // base cell, world units; octaves double it by the pixel footprint
static const float VEIL_MAX_FOOTPRINT = 8.0;   // plane units a pixel may span before the veil gives up (grazing)
static const float VEIL_SIGMA = .75;           // a point's footprint, pixels of the pass
static const float VEIL_DENSITY = 3.0;         // points per unit plane area at ring density 1 and the slider at 1x
static const float VEIL_FLASH = 8.0;           // a flash's peak, relative to the sun's irradiance, at the slider's 1x
static const float VEIL_GLOW = .08;            // a point's diffuse glow between flashes, likewise
static const float VEIL_EXPONENT = 120.0;      // the glint lobe at sharpness 1: about six degrees to half power
static const float VEIL_HANDOVER = .5;         // far-tier weight at which the far composite takes the veil from the near march
static const float VEIL_RATE = 3.0;            // a point's pulse, radians per second, at rate 1: a flash every two seconds
static const float BELT_SPIN_RATE = .0012, BELT_SHEAR = .35; // mirrors renderer_belt.cpp: the bands' orbital rates

struct VeilParams
{
    bool enabled;
    float density, brightness, sharpness, rate, drift;
    float3 tint;
};

VeilParams veilParams(Frame* frame)
{
    VeilParams v;
    v.enabled = frame->veil.x > 0.0;
    v.density = frame->veil.x;
    v.brightness = frame->veil.y;
    v.sharpness = frame->veil.z;
    v.rate = frame->veil.w;
    v.tint = frame->veil_tint.xyz;
    v.drift = frame->veil_tint.w;
    return v;
}

uint veilMix(uint x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x; }
uint veilHash(int2 cell, int layer, int octave)
{
    return veilMix(uint(cell.x) * 73856093u ^ uint(cell.y) * 19349663u ^ uint(layer * 64 + octave + 32) * 40503u);
}
float veilUnit(uint bits) { return float(bits & 0xffffffu) / 16777216.0; }

// A random unit vector from 32 bits.
float3 veilDirection(uint r)
{
    float z = float(r & 0xffffu) / 32768.0 - 1.0;
    float phi = float(r >> 16) / 65536.0 * 6.283185307;
    float s = sqrt(max(1.0 - z * z, 0.0));
    return float3(s * cos(phi), s * sin(phi), z);
}

// The belt plane's crossings of the ray for the veil's layers, in ray units; negative when missed.
void veilCrossings(BeltParams belt, float3 rd, out float crossings[VEIL_LAYERS])
{
    float hn = dot(-belt.centre, belt.normal), dn = dot(rd, belt.normal);
    for (int k = 0; k < VEIL_LAYERS; ++k)
        crossings[k] = abs(dn) < 1e-5 ? -1.0 : (VEIL_HEIGHTS[k] * belt.thickness * .5 - hn) / dn;
}

// The veil's points around this pixel's crossing of one layer, as radiance on the pixel.
// pixelScale: full-resolution pixels per pixel of the calling pass. irradiance: the sun's,
// shadowed, at the crossing.
float3 veilLayer(Frame* frame, BeltParams belt, ViewParams view, VeilParams veil, float3 p, int layer, float3 rd, float3 light,
                 float irradiance, float pixelScale)
{
    float3 e1 = normalize(cross(belt.normal, float3(0.0, 0.0, 1.0))), e2 = cross(e1, belt.normal);
    float3 q = p - belt.centre;
    float2 plane = float2(dot(q, e1), dot(q, e2));
    float r = length(plane);
    float t = (r - belt.inner) / (belt.outer - belt.inner);
    float ring = beltRingDensity(t);
    if (ring < 1e-4) return float3(0.0, 0.0, 0.0);
    float scaleHeight = belt.thickness * .5 * lerp(.6, 1.4, saturate(t));
    float h = dot(q, belt.normal);
    float weightSum = 0.0;
    for (int j = 0; j < VEIL_LAYERS; ++j)
    {
        float hj = VEIL_HEIGHTS[j] * belt.thickness * .5;
        weightSum += exp(-hj * hj / (2.0 * scaleHeight * scaleHeight));
    }
    float layerWeight = exp(-h * h / (2.0 * scaleHeight * scaleHeight)) / weightSum;
    float density = veil.density * VEIL_DENSITY * ring * layerWeight;
    // The band's co-rotating frame, the drift multiplied.
    int band = clamp(int(saturate(t) * float(ORBITAL_BELT_BANDS)), 0, ORBITAL_BELT_BANDS - 1);
    float bandRadius = belt.inner + (float(band) + .5) / float(ORBITAL_BELT_BANDS) * (belt.outer - belt.inner);
    float angle = BELT_SPIN_RATE * pow((belt.inner + belt.outer) * .5 / bandRadius, BELT_SHEAR) * veil.drift * view.time;
    float ca = cos(angle), sa = sin(angle);
    float2 rotated = float2(plane.x * ca + plane.y * sa, plane.y * ca - plane.x * sa);
    float3 wx = e1 * ca + e2 * sa, wy = e2 * ca - e1 * sa;
    // Screen offset per unit of plane offset, in this pass's pixels, with the perspective term.
    float z = max(dot(p, view.forward), .1);
    float pixelsPerUnit = view.size.y / (2.0 * view.tanHalfFov) / z / pixelScale;
    float2 perp = float2(dot(p, view.right), dot(p, view.up)) / z;
    float2 sx = (float2(dot(wx, view.right), dot(wx, view.up)) - perp * dot(wx, view.forward)) * pixelsPerUnit;
    float2 sy = (float2(dot(wy, view.right), dot(wy, view.up)) - perp * dot(wy, view.forward)) * pixelsPerUnit;
    float a = dot(sx, sx), c = dot(sy, sy), b = dot(sx, sy);
    float eigenMin = .5 * (a + c) - sqrt(.25 * (a - c) * (a - c) + b * b);
    if (eigenMin < 1e-8) return float3(0.0, 0.0, 0.0);
    float footprint = min(rsqrt(eigenMin), VEIL_MAX_FOOTPRINT);
    // The glint: a point whose facet faces near the half vector flashes in pulses at its
    // own rate and phase, the others only glow.
    float3 halfVector = normalize(light - rd);
    float facing = 1.0 - .15 / veil.sharpness; // the facet's alignment a flash needs
    float pulseWidth = 24.0;
    float reach = (2.5 * VEIL_SIGMA) * (2.5 * VEIL_SIGMA);
    // One octave, cells three to six footprints wide, the four around the crossing: they cover
    // a point's reach, and the twinkling hides the pattern's switch of scale as the camera moves.
    int octave = int(ceil(log2(3.0 * footprint / VEIL_CELL)));
    float cellSize = VEIL_CELL * exp2(float(octave));
    float mu = min(density * cellSize * cellSize, 1.0); // sparse: at most one point per cell
    if (mu < 1e-4) return float3(0.0, 0.0, 0.0);
    int2 base = int2(floor(rotated / cellSize - .5));
    float3 sum = float3(0.0, 0.0, 0.0);
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i)
        {
            int2 cell = base + int2(i, j);
            float2 nearest = clamp(rotated / cellSize, float2(cell), float2(cell) + 1.0) * cellSize - rotated;
            float2 nearestScreen = sx * nearest.x + sy * nearest.y;
            if (dot(nearestScreen, nearestScreen) > reach) continue;
            uint hsh = veilHash(cell, layer, octave);
            if (veilUnit(hsh) >= mu) continue;
            uint r0 = veilMix(hsh ^ 0x68bc21ebu), r1 = veilMix(r0 + 0x2545f491u), r2 = veilMix(r1 ^ 0x9e3779b9u);
            float2 delta = (float2(cell) + float2(r0 & 0xffffu, r0 >> 16) / 65536.0) * cellSize - rotated;
            float2 offset = sx * delta.x + sy * delta.y;
            float kernel = exp(-dot(offset, offset) / (2.0 * VEIL_SIGMA * VEIL_SIGMA));
            float3 facet = veilDirection(r1);
            float align = smoothstep(facing, facing + .05, dot(facet, halfVector));
            float glow = VEIL_GLOW * saturate(dot(facet, light));
            float glint = 0.0;
            if (align > 0.0)
            {
                float speed = VEIL_RATE * veil.rate * (.5 + float(r2 & 0xffffu) / 65536.0), phase = float(r2 >> 16) / 65536.0 * 6.283185307;
                glint = align * pow(.5 + .5 * sin(speed * view.time + phase), pulseWidth);
            }
            sum += kernel * (VEIL_FLASH * glint + glow);
        }
    return sum * (veil.brightness * irradiance) * veil.tint;
}
