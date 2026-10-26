#include "planet/mixed_provider.hpp"

#include "core/panic.hpp"

namespace planet {

MixedProvider::MixedProvider(TileProvider& coarse, TileProvider& fine, unsigned split_level)
    : coarse_(&coarse), fine_(&fine), split_(split_level) {
    ORBITAL_ASSERT(coarse.shape().heights == fine.shape().heights);
    const LevelTable& a = coarse.levels();
    const LevelTable& b = fine.levels();
    for (unsigned i = 0; i < LevelTable::count; i++)
        ORBITAL_ASSERT(a.error[i] == b.error[i] && a.relief[i] == b.relief[i]);
}

} // namespace planet
