#pragma once
#include "planet/tile.hpp"

namespace planet {

// Coarse levels from one source, finer ones from another: baked tiles up to split_level, the
// procedural terrain beyond. Shape and levels come from the fine source, the one that is the
// terrain's truth; the coarse source's must agree, which the constructor asserts.
class MixedProvider final : public TileProvider {
public:
    MixedProvider(TileProvider& coarse, TileProvider& fine, unsigned split_level); // levels <= split_level go to coarse
    const BodyShape& shape() const override { return fine_->shape(); }
    const LevelTable& levels() const override { return fine_->levels(); }
    const TileLayout& layout(unsigned level) const override { return source(level).layout(level); }
    bool produce(const TileRequest& request, TileOutput& out) override {
        return source(request.key.level).produce(request, out);
    }
    unsigned split_level() const { return split_; }

private:
    TileProvider& source(unsigned level) const { return level <= split_ ? *coarse_ : *fine_; }
    TileProvider* coarse_;
    TileProvider* fine_;
    unsigned split_;
};

} // namespace planet
