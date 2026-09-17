#include "render/showcase.hpp"
#include <algorithm>
#include <cassert>
#include <limits>

using namespace space;
using render::Showcase;

int main() {
    auto system = generate_system(showcase_seed);
    std::string error;
    const auto binding = Showcase::resolve(system, error);
    assert(binding && error.empty());
    assert(system.bodies[binding->earth()].body_class == BodyClass::Terrestrial);
    assert(system.bodies[binding->desert()].body_class == BodyClass::Desert);
    assert(binding->has_minor_planet() && system.bodies[binding->minor_planet()].body_class == BodyClass::MinorPlanet);
    assert(system.bodies[binding->belt_parent()].id == system.belts.front().parent_id);
    const auto original = evaluate_system(system, 1234);
    auto shuffled = original;
    std::reverse(shuffled.begin(), shuffled.end());
    BodyStates ordered(original.size());
    assert(binding->order_states(shuffled, ordered));
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        assert(ordered[i].id == original[i].id && ordered[i].position == original[i].position);
        assert(ordered[i].rotation_angle == original[i].rotation_angle && ordered[i].radius == original[i].radius);
    }
    shuffled[0] = shuffled[1];
    assert(!binding->order_states(shuffled, ordered));
    shuffled = original;
    shuffled[0].id = 0;
    assert(!binding->order_states(shuffled, ordered));
    shuffled = original;
    shuffled[0].position.x = std::numeric_limits<double>::quiet_NaN();
    assert(!binding->order_states(shuffled, ordered));
    shuffled = original;
    shuffled.pop_back();
    assert(!binding->order_states(shuffled, ordered));
    assert(!binding->order_states(original, {}));

    // Description order may change independently of evaluated state order.
    std::reverse(system.bodies.begin(), system.bodies.end());
    const auto reordered = Showcase::resolve(system, error);
    assert(reordered && reordered->earth() != binding->earth());
    assert(system.bodies[reordered->giant()].id == system.belts.front().parent_id);
    assert(reordered->order_states(original, ordered));
    for (std::size_t i = 0; i < ordered.size(); ++i)
        assert(ordered[i].id == system.bodies[i].id);

    const auto unsupported = [&](const SystemDescription& candidate) {
        assert(validate_system(candidate).empty()); // renderer policy must stay out of generic validation
        assert(!Showcase::resolve(candidate, error) && !error.empty());
    };
    for (auto role : {BodyClass::Terrestrial, BodyClass::GasGiant, BodyClass::Desert, BodyClass::RockyMoon}) {
        auto missing = system;
        for (auto& body : missing.bodies)
            if (body.body_class == role)
                body.body_class = BodyClass::Moonlet;
        unsupported(missing);
        auto duplicate = system;
        duplicate.bodies.front().body_class = role; // reversed default starts with a moonlet
        unsupported(duplicate);
    }
    auto wrong_parent = system;
    wrong_parent.belts.front().parent_id = system.bodies[reordered->earth()].id;
    unsupported(wrong_parent);
    wrong_parent.belts.front().parent_id = 0;
    unsupported(wrong_parent);
    auto multiple = system;
    multiple.belts.push_back(multiple.belts.front());
    multiple.belts.back().id += 1;
    unsupported(multiple);
    auto no_belt = system;
    no_belt.belts.clear();
    unsupported(no_belt);
    auto flat = system;
    flat.belts.front().thickness = 0; // legal generically; renderer LOD divides by thickness
    unsupported(flat);
    auto invalid = system;
    invalid.bodies.front().radius = -1;
    assert(!Showcase::resolve(invalid, error));
}
