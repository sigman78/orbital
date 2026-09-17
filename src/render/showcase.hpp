#pragma once
#include "scene/system.hpp"
#include <array>
#include <optional>

namespace space::render {

// Renderer policy, separate from generic scene validity. Indices refer to the
// immutable description; IDs bind incoming frame states to those same slots.
class Showcase {
public:
    constexpr Showcase() = default; // empty until resolved
    static std::optional<Showcase> resolve(const SystemDescription& system, std::string& error);
    constexpr unsigned body_count() const { return count_; }
    constexpr unsigned earth() const { return earth_; }
    constexpr unsigned giant() const { return giant_; }
    constexpr unsigned desert() const { return desert_; }
    constexpr unsigned belt_parent() const { return belt_parent_; }
    // The planetoid is optional: none, or one.
    constexpr bool has_planetoid() const { return planetoid_ < count_; }
    constexpr unsigned planetoid() const { return planetoid_; }

    // Accept any order, exactly one finite state per bound ID. Output must have
    // body_count entries and must not overlap input. Failure leaves output unspecified.
    bool order_states(std::span<const BodyState> input, std::span<BodyState> output) const;

private:
    std::array<std::uint64_t, max_body_count> ids_{};
    unsigned count_ = 0, earth_ = 0, giant_ = 0, desert_ = 0, belt_parent_ = 0, planetoid_ = max_body_count;
};

} // namespace space::render
