// planet-forge: bakes the minor planet's tile store (docs/PLANET_FORGE.md). The logic is the
// planet_cook library; this knows how the showcase turns its seed into the terrain's.
#include "planet/cook.hpp"
#include "scene/system.hpp"

namespace {

bool terrain_seed(void*, std::uint64_t showcase_seed, std::uint64_t& out) {
    const space::SystemDescription system = space::generate_system(showcase_seed);
    for (const space::BodyDescription& body : system.bodies)
        if (body.body_class == space::BodyClass::MinorPlanet) {
            out = body.material_seed;
            return true;
        }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    return planet::cook_main(argc, argv, {.terrain_seed = terrain_seed});
}
