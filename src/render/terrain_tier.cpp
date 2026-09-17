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
    // The projected size per radius of extent, from the patch's distance; the
    // cell's edge in those units orders the generation.
    const double distance = std::max(length(centre), cap);
    const float scale = float(view.radius * view.height_pixels / (distance * view.tan_y));
    // Facing the camera an error shows as parallax, edge-on it is the silhouette
    // itself: the tolerance is the strict one at the limb and twice it head-on.
    const float facing = float(std::clamp(-dot(normal, centre) / distance, 0.0, 1.0));
    return {.visible = true, .scale = scale, .pixels = float(b.angular_size) * scale, .tolerance = 1 + facing};
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

float TerrainTier::error_of(PatchKey key) const {
    const unsigned slot = slot_of(key);
    return slot == no_slot ? 0 : slots_[slot].error;
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
    // A resident patch splits while its error shows on screen; the valve holds
    // the tree under the pool's size.
    const bool split = key.level < patch_level_max && (nodes_[index].children || nodes() < slot_count - 64) &&
                       error_of(key) * seen.scale >
                           error_pixels * seen.tolerance * (nodes_[index].children ? hysteresis : 1);
    if (split && !nodes_[index].children)
        nodes_[index].children = allocate_children(nodes_[index]);
    if (!split && nodes_[index].children)
        collapse(nodes_[index]);
    if (const std::uint32_t children = nodes_[index].children) {
        // Descend only when every visible child is resident; keep those
        // children alive meanwhile, and ask for the missing ones.
        bool ready = true;
        for (unsigned i = 0; i < 4; i++) {
            const Node& child = nodes_[children + i];
            const Visibility child_seen = visibility(child, view);
            if (!child_seen.visible)
                continue;
            if (slot_of(child.key) == no_slot) {
                request(child.key, child_seen.pixels);
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
    if (const unsigned slot = slot_of(key); slot != no_slot)
        draws_.push_back({.slot = slot, .level = key.level});
    else
        request(key, seen.pixels); // a root, before the tier is active
}

// Coarse levels first, then the largest on screen; a slot comes from the free
// list or from the resident patch longest unused, never one needed this frame.
void TerrainTier::choose_generation() {
    std::sort(requests_.begin(), requests_.end(), [](const Request& a, const Request& b) {
        return a.key.level != b.key.level ? a.key.level < b.key.level : a.pixels > b.pixels;
    });
    for (const Request& r : requests_) {
        if (generate_.size() >= generate_per_frame)
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
                if (slots_[i].used < oldest) {
                    oldest = slots_[i].used;
                    slot = i;
                }
            if (slot == no_slot)
                break;
            slots_by_key_.erase(slots_[slot].key.packed());
        }
        slots_[slot] = {.key = r.key, .used = frame_, .resident = true};
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

void TerrainTier::update(const TierView& view, unsigned frame) {
    frame_ = frame;
    draws_.clear();
    requests_.clear();
    for (const Generation& g : generate_) // last frame's, generated since
        slots_[g.slot].error = g.error;
    generate_.clear();
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
    for (unsigned face = 0; face < 6; face++)
        if (slot_of(nodes_[face].key) == no_slot) {
            request(nodes_[face].key, 1e9f);
            active_ = false;
        }
    if (!active_)
        draws_.clear();
    choose_generation();
}

} // namespace space::render
