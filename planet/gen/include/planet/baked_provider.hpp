#pragma once
#include "planet/tile.hpp"
#include "planet/tile_store.hpp"

namespace planet {

// Tiles read from a cooked store; a level the store lacks fails produce(), so a MixedProvider
// routes those to another source.
class BakedProvider final : public TileProvider {
public:
    explicit BakedProvider(const TileStore& store) : store_(&store) {} // open, and outliving this
    const BodyShape& shape() const override;
    const LevelTable& levels() const override;
    const TileLayout& layout(unsigned level) const override;
    bool produce(const TileRequest& request, TileOutput& out) override;

private:
    const TileStore* store_;
};

} // namespace planet
