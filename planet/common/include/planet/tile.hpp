#pragma once
#include "planet/body.hpp"
#include "planet/patch.hpp"
#include "planet/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace planet {

// Texel formats a tile plane may use, stored as the GPU reads them; the host maps them to its API.
enum class TileFormat : unsigned char { r16_unorm, rg16_unorm, rgba8_unorm };
constexpr unsigned texel_bytes(TileFormat format) {
    return format == TileFormat::r16_unorm ? 2 : 4;
}

// What a plane holds, so the shaders and the cook agree on which array it fills.
enum class PlaneId : unsigned char { height, albedo, slope };

struct PlaneLayout {
    PlaneId id = PlaneId::height;
    TileFormat format = TileFormat::r16_unorm;
    unsigned side = 0; // texels per edge
    constexpr unsigned bytes() const { return side * side * texel_bytes(format); }
};

// The planes a level's tiles carry and where each lies in a tile's bytes: every plane starts on
// a four-byte boundary so a copy of any plane is aligned to its texel.
struct TileLayout {
    static constexpr unsigned max_planes = 4;
    PlaneLayout planes[max_planes];
    unsigned plane_count = 0;
    constexpr unsigned offset(unsigned plane) const {
        unsigned at = 0;
        for (unsigned i = 0; i < plane; i++)
            at = (at + planes[i].bytes() + 3) & ~3u;
        return at;
    }
    constexpr unsigned bytes() const { return offset(plane_count); }
};

// One tile asked of a provider. `detail` is the source's own quality knob (the micro-relief
// strength for the procedural terrain); a store ignores it.
struct TileRequest {
    PatchKey key;
    unsigned slot = 0;
    std::uint32_t stamp = 0;
    float detail = 1;
};

// Where a provider writes: one span per plane of the level's layout, and the heights it measured.
struct TileOutput {
    std::span<std::byte> planes[TileLayout::max_planes];
    Range<float> heights{};
};

// A source of tiles: procedural, baked or a mix. produce() runs on a worker thread, must be
// reentrant, fills every plane of layout(request.key.level) and returns false when it cannot.
class TileProvider {
public:
    virtual ~TileProvider() = default;
    virtual const BodyShape& shape() const = 0;
    virtual const LevelTable& levels() const = 0;
    virtual const TileLayout& layout(unsigned level) const = 0;
    virtual bool produce(const TileRequest& request, TileOutput& out) = 0;
};

} // namespace planet
