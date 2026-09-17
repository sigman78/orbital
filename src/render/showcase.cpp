#include "render/showcase.hpp"
#include <cmath>

namespace space::render {

std::optional<Showcase> Showcase::resolve(const SystemDescription& system, std::string& error) {
    error.clear();
    const auto invalid = [&](const std::string& reason) -> std::optional<Showcase> {
        error = reason;
        return std::nullopt;
    };
    const auto errors = validate_system(system);
    if (!errors.empty())
        return invalid(errors.front());
    if (system.belts.size() != 1)
        return invalid("showcase requires exactly one belt");
    if (system.belts.front().thickness <= 0)
        return invalid("showcase belt thickness must be positive");

    Showcase result;
    result.count_ = unsigned(system.bodies.size());
    std::array<unsigned, 4> counts{};
    for (unsigned i = 0; i < result.count_; ++i) {
        const auto& body = system.bodies[i];
        result.ids_[i] = body.id;
        switch (body.body_class) {
        case BodyClass::Terrestrial:
            ++counts[0];
            result.earth_ = i;
            break;
        case BodyClass::GasGiant:
            ++counts[1];
            result.giant_ = i;
            break;
        case BodyClass::Desert:
            ++counts[2];
            result.desert_ = i;
            break;
        case BodyClass::RockyMoon: ++counts[3]; break;
        case BodyClass::Moonlet: break;
        case BodyClass::Planetoid:
            if (result.planetoid_ < max_body_count)
                return invalid("showcase allows one planetoid");
            result.planetoid_ = i;
            break;
        default: return invalid("showcase has an unsupported body class");
        }
    }
    constexpr const char* names[] = {"terrestrial", "gas giant", "desert", "rocky moon"};
    for (unsigned role = 0; role < counts.size(); ++role)
        if (counts[role] != 1)
            return invalid(std::string("showcase requires exactly one ") + names[role] + " body");
    const auto parent_id = system.belts.front().parent_id;
    if (parent_id != result.ids_[result.giant_])
        return invalid("showcase belt parent must be the gas giant ID");
    result.belt_parent_ = result.giant_;
    return result;
}

bool Showcase::order_states(std::span<const BodyState> input, std::span<BodyState> output) const {
    if (input.size() != count_ || output.size() != count_)
        return false;
    std::array<bool, max_body_count> seen{};
    for (const auto& state : input) {
        unsigned index = 0;
        while (index < count_ && ids_[index] != state.id)
            ++index;
        if (index == count_ || seen[index] || !is_finite(state.position) || !std::isfinite(state.rotation_angle) ||
            !std::isfinite(state.radius) || state.radius <= 0)
            return false;
        seen[index] = true;
        output[index] = state;
    }
    return true;
}

} // namespace space::render
