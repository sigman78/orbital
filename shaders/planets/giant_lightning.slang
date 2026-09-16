#pragma once
#include "../lib/math.slang"
#include "../lib/noise.slang"

// --- Night-side lightning -----------------------------------------------------------
//
// Galileo imaged 26 storms on the night side, up to 1500 km across, with the
// flashes themselves bell-shaped spots of 45 to 80 km half-width, concentrated
// near 50 degrees in both hemispheres (more in the north) in regions of
// cyclonic shear; Juno measured peak rates of four flashes a second and none
// near the equator. Here storm cells are picked by hash on a grid over the
// map and live for a couple of minutes each; every storm flashes at random
// moments, each flash a fast rise and a 120 ms decay, a small hot core with a
// wider glow of cloud lit from below. Where the core is smaller than a pixel
// it widens to one and dims to keep its energy, so a flash stays a spark for
// the bloom from any distance.

static const float2 STORM_CELLS = float2(512.0, 256.0);
static const float STORM_EPOCH = 150.0;       // seconds a set of storms lasts
static const float STORM_DENSITY = 4e-4;      // fraction of cells that hold a storm
static const float FLASH_SLOT = .125;         // seconds; at most one flash per storm per slot
static const float FLASH_DECAY = .12;         // seconds
static const float FLASH_CORE = 60.0 / 71492.0 / (2.0 * PI);   // sigma of the core in texture units at the equator
static const float FLASH_GLOW = 500.0 / 71492.0 / (2.0 * PI);  // sigma of the lit cloud around it

float hashCell(float2 cell, float salt) { return hash2(cell * 1.37 + salt * float2(17.13, 5.71)); }

// Where lightning happens: the 50 degree bands, the north more, some over the
// poles, nothing at the equator; and the shear regions the flow map marks.
float stormWeight(float lat, float shear)
{
    float bands = exp(-pow((abs(lat) - 50.0) / 11.0, 2.0)) * (lat > 0.0 ? 1.0 : .7);
    float polar = smoothstep(60.0, 75.0, abs(lat)) * .5;
    return saturate(bands + polar) * (.4 + .6 * shear);
}

float3 giantLightning(float rate, float brightness, float2 uv, float t, float shear, float2 footprint)
{
    if (rate <= 0.0 || brightness <= 0.0) return float3(0.0, 0.0, 0.0);
    float lat = (.5 - uv.y) * 180.0;
    float weight = stormWeight(lat, shear);
    if (weight < .01) return float3(0.0, 0.0, 0.0);
    float epoch = floor(t / STORM_EPOCH);
    float epochPhase = frac(t / STORM_EPOCH);
    float life = smoothstep(0.0, .07, epochPhase) * (1.0 - smoothstep(.93, 1.0, epochPhase));
    // Flash probability per storm per slot that gives the panel's rate over the planet's storms.
    float storms = STORM_CELLS.x * STORM_CELLS.y * STORM_DENSITY * .45; // .45: the mean of the weight over the map
    float chance = rate * FLASH_SLOT / storms;
    float cosLat = max(cos(radians(lat)), .05);
    float2 cellSize = 1.0 / STORM_CELLS;
    float2 base = floor(uv * STORM_CELLS);
    float total = 0.0;
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
    {
        float2 cell = base + float2(dx, dy);
        cell.x -= STORM_CELLS.x * floor(cell.x / STORM_CELLS.x); // wrap in longitude
        if (hashCell(cell, epoch) > STORM_DENSITY * weight / .45) continue;
        float2 centre = (cell + float2(hashCell(cell, epoch + .3), hashCell(cell, epoch + .6))) * cellSize;
        float2 d = uv - centre;
        d.x -= round(d.x);
        d.x *= cosLat; // texture units to equator-equivalent, so a flash is round
        float r2 = dot(d, d);
        // The current slot and the previous, whose flash may still be decaying.
        float slot = floor(t / FLASH_SLOT);
        for (int k = 0; k < 2; ++k)
        {
            float s = slot - float(k);
            if (hashCell(cell, s * .01 + 3.0) > chance) continue;
            float start = (s + hashCell(cell, s * .01 + 7.0)) * FLASH_SLOT;
            float age = t - start;
            if (age < 0.0) continue;
            float envelope = exp(-age / FLASH_DECAY) * (1.0 - exp(-age / .012));
            // Some flashes strike twice.
            envelope += hashCell(cell, s * .01 + 9.0) < .4 ? .6 * exp(-max(age - .07, 0.0) / FLASH_DECAY) * step(.07, age) : 0.0;
            float strength = .5 + hashCell(cell, s * .01 + 11.0);
            float sigma = max(FLASH_CORE, max(footprint.x, footprint.y) * .7);
            float core = exp(-r2 / (2.0 * sigma * sigma)) * (FLASH_CORE * FLASH_CORE) / (sigma * sigma);
            float glow = exp(-r2 / (2.0 * FLASH_GLOW * FLASH_GLOW)) * .012;
            total += (core + glow) * envelope * strength;
        }
    }
    return total * life * brightness * 40.0 * float3(.85, .9, 1.0);
}

