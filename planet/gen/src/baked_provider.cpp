#include "planet/baked_provider.hpp"

namespace planet {

const BodyShape& BakedProvider::shape() const {
    return store_->header().shape;
}

const LevelTable& BakedProvider::levels() const {
    return store_->header().levels;
}

const TileLayout& BakedProvider::layout(unsigned level) const {
    static constexpr TileLayout none{};
    return level < store::max_levels ? store_->header().layouts[level] : none;
}

bool BakedProvider::produce(const TileRequest& request, TileOutput& out) {
    return store_->read(request.key, out);
}

} // namespace planet
