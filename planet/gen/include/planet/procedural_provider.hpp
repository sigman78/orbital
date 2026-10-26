#pragma once
#include "planet/crater_terrain.hpp"
#include "planet/tile.hpp"

namespace planet {

// Tiles sampled from the procedural crater terrain on demand: one layout for every level, the
// height plane quantized to the body's shell and the colour planes as the sampling writes them.
class ProceduralProvider final : public TileProvider {
public:
    explicit ProceduralProvider(const MinorPlanetTerrain& terrain) : terrain_(&terrain) {} // terrain outlives this
    static constexpr TileLayout tile_layout{
        .planes = {{.id = PlaneId::height, .format = TileFormat::r16_unorm, .side = tile_side},
                   {.id = PlaneId::albedo, .format = TileFormat::rgba8_unorm, .side = tile_colour_side},
                   {.id = PlaneId::slope, .format = TileFormat::rg16_unorm, .side = tile_colour_side}},
        .plane_count = 3,
    };
    const BodyShape& shape() const override { return MinorPlanetTerrain::shape; }
    const LevelTable& levels() const override { return MinorPlanetTerrain::levels; }
    const TileLayout& layout(unsigned) const override { return tile_layout; }
    bool produce(const TileRequest& request, TileOutput& out) override;

private:
    const MinorPlanetTerrain* terrain_;
};

} // namespace planet
