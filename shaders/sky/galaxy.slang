#include "../scene/frame_bindings.slang"
#include "../scene/view.slang"
#include "../lib/math.slang"
#include "../lib/color.slang"

// The Milky Way as a cloud of Gaussian splats on the sphere, fitted to ESO's
// panorama by tools/bake-splats.py and summed here per pixel at a quarter of
// the frame (the sum is smooth at a degree and more; the background reads it
// bilinearly). Each splat has a centre, two widths along a tangent frame (in
// angle, so it is right anywhere on the sphere) and a signed colour, the
// negative ones the dust lanes. Nothing is sampled from a map, so there is no
// texel lattice at any zoom and no star residue. Two records per splat:
// centre.xyz, sigma1 | axis1.xyz, sigma2 | rgb, cutoff | unused. The bake
// buckets the splats by the cells of a longitude/latitude grid they reach
// into (the table after the records: cells_lon, cells_lat, per cell an offset
// and count, then the index list), so a pixel sums only its cell's list and
// the cost follows the splats in view rather than the cloud. Output: the
// radiance and, in w, a dust depth: how much the dark splats take from the
// bright ones, which the background redistributes into filaments.

static const float MILKY_WAY_SCALE = .1; // HDR radiance of the fit's unit (the bulge) at the panel's 1
// Equatorial J2000 (standard xyz: x toward the vernal equinox, z toward the celestial pole)
// to galactic, IAU 1958 pole at J2000; the sky frame stores y as the pole, so the rows swap.
static const float3x3 EQUATORIAL_TO_GALACTIC = float3x3(-.054876, -.873437, -.483835,
                                                        .494109, -.444830, .746982,
                                                        -.867666, -.198076, .455984);

[shader("fragment")]
float4 fragmentMain(float4 position : SV_Position) : SV_Target0
{
    // This target is the frame over a divisor: the frame pixel under this one's centre.
    float2 pixel = position.xy * root.frame.options.xy / floor(root.frame.options.xy / root.frame.galaxy_more.z);
    float3 rd = rayDirection(root.frame, pixel);
    float3 g = mul(EQUATORIAL_TO_GALACTIC, float3(rd.x, rd.z, rd.y));
    float3 light = float3(0.0, 0.0, 0.0), dark = float3(0.0, 0.0, 0.0);
    // The cell table follows the records; the pass points root.instances at it.
    uint* table = (uint*)(uint64_t(root.instances));
    uint cellsLon = table[0], cellsLat = table[1];
    float longitude = atan2(g.y, g.x), latitude = asin(clamp(g.z, -1.0, 1.0));
    uint i = min(uint((.5 - longitude / (2.0 * PI)) * float(cellsLon)), cellsLon - 1); // longitude increases to the left
    uint j = min(uint((.5 - latitude / PI) * float(cellsLat)), cellsLat - 1);
    uint cell = 2 + 2 * (j * cellsLon + i);
    uint first = 2 + 2 * cellsLon * cellsLat + table[cell], last = first + table[cell + 1];
    for (uint n = first; n < last; ++n)
    {
        uint k = table[n];
        if (k >= root.base) continue; // past the panel's count (the records are ordered by energy)
        Vertex shape = root.vertices[2 * k], tint = root.vertices[2 * k + 1];
        float3 centre = shape.position.xyz;
        float cosAngle = dot(g, centre);
        if (cosAngle < tint.position.w) continue; // beyond the splat's reach
        float sinAngle = sqrt(max(1.0 - cosAngle * cosAngle, 1e-12));
        // Azimuthal equidistant coordinates: the tangent-plane direction scaled to the angle.
        float scale = sinAngle > 1e-4 ? atan2(sinAngle, cosAngle) / sinAngle : 1.0;
        float3 axis1 = shape.normal.xyz, axis2 = cross(centre, axis1);
        float u = dot(g, axis1) * scale / shape.position.w, v = dot(g, axis2) * scale / shape.normal.w;
        float3 c = tint.position.xyz * exp(-.5 * (u * u + v * v));
        light += max(c, 0.0);
        dark += max(-c, 0.0);
    }
    float depth = 2.0 * saturate(luma709(dark) / (luma709(light) + 1e-3)); // a lane's share as an optical depth, kept mild
    // Band contrast: an exponent on the fit's radiance about its unit (the bulge), so the halo
    // can stay faint while the core comes up, the photographic look of the references.
    float3 radiance = pow(max(light - dark, 0.0), root.frame.galaxy_more.w);
    return float4(radiance * MILKY_WAY_SCALE, depth);
}
