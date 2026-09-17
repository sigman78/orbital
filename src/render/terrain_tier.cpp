#include "render/terrain_tier.hpp"

#include <algorithm>
#include <cmath>

namespace space::render {

namespace {

constexpr unsigned no_slot = TerrainTier::slot_count;
// A peak at the terrain's top height shows past the limb by about this much.
const double horizon_allowance = std::acos(1 / (1 + double(MinorPlanetTerrain::height_max)));

Vec3d to_world(const TierView& view, Vec3d local) {
    return view.axes[0] * local.x + view.axes[1] * local.y + view.axes[2] * local.z;
}

} // namespace

void TerrainTier::ensure_roots() {
    if (!nodes_.empty())
        return;
    nodes_.reserve(4096);
    for (unsigned face = 0; face < 6; face++) {
        const PatchKey key{std::uint8_t(face), 0, 0, 0};
        nodes_.push_back({.key = key, .bounds = patch_bounds(key)});
    }
    slots_.resize(slot_count);
    for (unsigned slot = slot_count; slot-- > 0;)
        free_slots_.push_back(slot);
}

TerrainTier::Visibility TerrainTier::visibility(const Node& node, const TierView& view) const {
    const PatchBounds& b = node.bounds;
    // Beyond the horizon seen from the camera nothing of the cap shows; inside
    // the sphere everything can.
    const double d = length(view.camera_local);
    if (d > 1) {
        const double angle = std::acos(std::clamp(dot(b.centre, view.camera_local * (1 / d)), -1.0, 1.0));
        if (angle > std::acos(1 / d) + b.angular_radius + horizon_allowance)
            return {};
    }
    const Vec3d normal = to_world(view, b.centre);
    const Vec3d centre = view.body_centre + normal * view.radius;
    const double cap = view.radius * std::sin(std::min(b.angular_radius, pi<double> / 2));
    const float bound = float(cap + view.radius * 2 * MinorPlanetTerrain::height_max);
    if (!geometry::sphere_in_frustum(view.frustum, to_float(centre), bound))
        return {};
    const double distance = std::max(length(centre), cap);
    const float scale = float(view.radius * view.height_pixels / (distance * view.tan_y));
    return {.visible = true, .pixels = float(b.angular_size) * scale};
}

std::uint32_t TerrainTier::allocate_children(const Node& parent) {
    const PatchKey key = parent.key; // the vector may grow below
    std::uint32_t first;
    if (!free_blocks_.empty()) {
        first = free_blocks_.back();
        free_blocks_.pop_back();
    } else {
        first = std::uint32_t(nodes_.size());
        nodes_.resize(nodes_.size() + 4);
    }
    for (unsigned i = 0; i < 4; i++) {
        const PatchKey child = key.child(i);
        nodes_[first + i] = {.key = child, .bounds = patch_bounds(child)};
    }
    return first;
}

void TerrainTier::collapse(Node& node) {
    if (!node.children)
        return;
    for (unsigned i = 0; i < 4; i++)
        collapse(nodes_[node.children + i]);
    free_blocks_.push_back(node.children);
    node.children = 0;
}

unsigned TerrainTier::slot_of(PatchKey key) const {
    const auto found = slots_by_key_.find(key.packed());
    return found == slots_by_key_.end() ? no_slot : found->second;
}

unsigned TerrainTier::resident() const {
    unsigned count = 0;
    for (const auto& [_, slot] : slots_by_key_)
        if (slots_[slot].resident)
            count++;
    return count;
}

void TerrainTier::touch(PatchKey key) {
    if (const unsigned slot = slot_of(key); slot != no_slot)
        slots_[slot].used = frame_;
}

void TerrainTier::request(PatchKey key, float pixels) {
    requests_.push_back({key, pixels});
}

void TerrainTier::visit(std::uint32_t index, const TierView& view) {
    const Visibility seen = visibility(nodes_[index], view);
    if (!seen.visible) {
        collapse(nodes_[index]);
        return;
    }
    const PatchKey key = nodes_[index].key;
    touch(key);
    const double dist = length(view.camera_local - nodes_[index].bounds.centre);
    const bool split = key.level < patch_level_max && key.level + 1 < std::size(level_error) &&
                       (nodes_[index].children || nodes() < slot_count - 64) &&
                       dist < double(range_[key.level + 1]) * (nodes_[index].children ? 1.0 / double(hysteresis) : 1.0);
    if (split && !nodes_[index].children)
        nodes_[index].children = allocate_children(nodes_[index]);
    if (!split && nodes_[index].children)
        collapse(nodes_[index]);
    if (const std::uint32_t children = nodes_[index].children) {
        bool ready = true;
        for (unsigned i = 0; i < 4; i++) {
            const Node& child = nodes_[children + i];
            const Visibility child_seen = visibility(child, view);
            if (!child_seen.visible)
                continue;
            const unsigned child_slot = slot_of(child.key);
            if (child_slot == no_slot) {
                request(child.key, child_seen.pixels);
                ready = false;
            } else if (!slots_[child_slot].resident) {
                touch(child.key);
                ready = false;
            } else {
                touch(child.key);
            }
        }
        if (ready) {
            for (unsigned i = 0; i < 4; i++)
                visit(children + i, view);
            return;
        }
    }
    const unsigned slot = slot_of(key);
    if (slot != no_slot && slots_[slot].resident)
        draws_.push_back({.key = key, .slot = slot});
    else if (slot == no_slot)
        request(key, seen.pixels);
}

// Coarse levels first, then the largest on screen; a slot comes from the free
// list or from the resident patch longest unused, never one needed this frame
// or one still in flight (it isn't touched once its node collapses, but it may
// still land and get marked resident, so it stays until then).
void TerrainTier::choose_generation(unsigned budget) {
    const unsigned cap = std::min(budget, generate_per_frame);
    std::sort(requests_.begin(), requests_.end(), [](const Request& a, const Request& b) {
        return a.key.level != b.key.level ? a.key.level < b.key.level : a.pixels > b.pixels;
    });
    for (const Request& r : requests_) {
        if (generate_.size() >= cap)
            break;
        if (slot_of(r.key) != no_slot)
            continue; // requested twice, or already resident
        unsigned slot = no_slot;
        if (!free_slots_.empty()) {
            slot = free_slots_.back();
            free_slots_.pop_back();
        } else {
            unsigned oldest = frame_;
            for (unsigned i = 0; i < slot_count; i++)
                if (slots_[i].resident && slots_[i].used < oldest) {
                    oldest = slots_[i].used;
                    slot = i;
                }
            if (slot == no_slot)
                break; // nothing evictable: every other slot is pending
            slots_by_key_.erase(slots_[slot].key.packed());
        }
        slots_[slot] = {.key = r.key, .used = frame_, .resident = false};
        slots_by_key_[r.key.packed()] = slot;
        generate_.push_back({.key = r.key, .slot = slot});
    }
}

void TerrainTier::disable() {
    draws_.clear();
    generate_.clear();
    for (unsigned face = 0; face < std::min<std::size_t>(6, nodes_.size()); face++)
        collapse(nodes_[face]);
    active_ = wanted_ = false;
}

bool TerrainTier::mark_resident(unsigned slot, PatchKey key) {
    if (slots_[slot].key != key)
        return false; // recycled since the request went out
    slots_[slot].resident = true;
    return true;
}

void TerrainTier::update(const TierView& view, unsigned frame, unsigned budget) {
    frame_ = frame;
    draws_.clear();
    requests_.clear();
    generate_.clear();
    for (unsigned level = 0; level < std::size(level_error); level++)
        range_[level] = level_error[level] * view.height_pixels / (view.tan_y * error_pixels);
    ensure_roots();
    const double distance = std::max(length(view.body_centre), view.radius);
    const float projected = float(view.radius * view.height_pixels / (distance * view.tan_y));
    wanted_ = projected > activate_pixels * (wanted_ ? hysteresis : 1);
    if (!wanted_) {
        for (unsigned face = 0; face < 6; face++)
            collapse(nodes_[face]);
        active_ = false;
        return;
    }
    // The faces stay resident whether seen or not: they are the fallback for
    // everything under them, and a turn must never find a hole.
    for (unsigned face = 0; face < 6; face++)
        touch(nodes_[face].key);
    for (unsigned face = 0; face < 6; face++)
        visit(face, view);
    // The tier takes over once every face is resident; the sphere levels draw until then.
    active_ = true;
    for (unsigned face = 0; face < 6; face++) {
        const unsigned slot = slot_of(nodes_[face].key);
        if (slot == no_slot)
            request(nodes_[face].key, 1e9f);
        if (slot == no_slot || !slots_[slot].resident)
            active_ = false;
    }
    if (!active_)
        draws_.clear();
    choose_generation(budget);
}

} // namespace space::render
