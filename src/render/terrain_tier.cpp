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
    // Cull caps beyond the horizon only when the camera is outside the reference sphere.
    const double d = length(view.camera_local);
    if (d > 1) {
        const double angle = std::acos(std::clamp(dot(b.centre, view.camera_local * (1 / d)), -1.0, 1.0));
        if (angle > std::acos(1 / d) + b.angular_radius + horizon_allowance)
            return {};
    }
    const Vec3d normal = to_world(view, b.centre);
    const Vec3d centre = view.body_centre + normal * view.radius;
    if (!geometry::sphere_in_frustum(view.frustum, to_float(centre), float(view.radius * b.bound_radius)))
        return {};
    // Prioritize nearby patch coverage; distances and sizes are both in radii.
    const double near = nearest_distance(view.camera_local, b, height_range(node.key));
    const float scale = float(view.height_pixels / (std::max(near, 1e-4) * view.tan_y));
    return {.visible = true, .pixels = float(b.angular_size) * scale, .distance = near};
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

double TerrainTier::nearest_distance(Vec3d camera_local, const PatchBounds& bounds, Range<float> heights) {
    const double len = length(camera_local);
    if (len < 1e-9)
        return 0;
    const double theta = std::acos(std::clamp(dot(camera_local, bounds.centre) / len, -1.0, 1.0));
    const double t = std::max(0.0, theta - bounds.angular_radius);
    const double r = std::clamp(len * std::cos(t), 1 + double(heights.min), 1 + double(heights.max));
    return std::sqrt(std::max(0.0, len * len + r * r - 2 * len * r * std::cos(t)));
}

// Interpolate the distance ranges in log space to estimate the desired level.
double TerrainTier::level_at(double distance) const {
    if (distance >= double(range_[0]))
        return 0;
    for (std::size_t i = 1; i < std::size(range_); i++)
        if (range_[i] > 0 && distance >= double(range_[i])) {
            const double t = std::log(distance / double(range_[i - 1])) /
                             std::log(double(range_[i]) / double(range_[i - 1]));
            return double(i - 1) + t;
        }
    return double(std::size(range_) - 1);
}

double TerrainTier::level_span(const TierView& view, const PatchBounds& bounds, Range<float> heights) const {
    return level_at(nearest_distance(view.camera_local, bounds, heights)) -
           level_at(farthest_distance(view.camera_local, bounds, heights));
}

double TerrainTier::farthest_distance(Vec3d camera_local, const PatchBounds& bounds, Range<float> heights) {
    const double len = length(camera_local);
    if (len < 1e-9)
        return 1 + double(heights.max);
    const double theta = std::acos(std::clamp(dot(camera_local, bounds.centre) / len, -1.0, 1.0));
    const double t = std::min(theta + bounds.angular_radius, pi<double>);
    double far = 0;
    for (double r : {1 + double(heights.min), 1 + double(heights.max)})
        far = std::max(far, std::sqrt(std::max(0.0, len * len + r * r - 2 * len * r * std::cos(t))));
    return far;
}

unsigned TerrainTier::resident_slot(PatchKey key) const {
    const unsigned slot = slot_of(key);
    return slot != no_slot && slots_[slot].resident ? slot : no_slot;
}

Range<float> TerrainTier::height_range(PatchKey key) const {
    const unsigned slot = resident_slot(key);
    return slot != no_slot ? slots_[slot].heights : full_height_range;
}

unsigned TerrainTier::pending() const {
    return unsigned(slots_by_key_.size()) - resident();
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

void TerrainTier::visit(std::uint32_t index, const TierView& view, float fade) {
    const Visibility seen = visibility(nodes_[index], view);
    if (!seen.visible) {
        collapse(nodes_[index]);
        return;
    }
    const PatchKey key = nodes_[index].key;
    touch(key);
    // Seam::span requests subdivision when a patch spans more than one level's morph.
    const bool wide = seam_ == Seam::span && level_span(view, nodes_[index].bounds, height_range(key)) >
                                                 seam_span * (nodes_[index].children ? double(hysteresis) : 1.0);
    // Use the full shell for undiscovered children; resident tile bounds cover only their own geometry.
    const double descendant_distance = nearest_distance(view.camera_local, nodes_[index].bounds);
    const bool wants = key.level < patch_level_max && key.level + 1 < std::size(level_error) &&
                       (wide || descendant_distance < double(range_[key.level + 1]) *
                                                          (nodes_[index].children ? 1.0 / double(hysteresis) : 1.0));
    const bool allowed = nodes_[index].children || nodes() < slot_count - 64;
    pressure_.splits_blocked += wants && !allowed;
    const bool split = wants && allowed;
    if (split && !nodes_[index].children)
        nodes_[index].children = allocate_children(nodes_[index]);
    if (!split && nodes_[index].children)
        collapse(nodes_[index]);
    // Visit resident children within range; cover the other visible quadrants here.
    unsigned quadrants = 0xf;
    if (const std::uint32_t children = nodes_[index].children) {
        quadrants = 0;
        // The youngest resident sibling sets the shared fade, bounded below by the parent's.
        unsigned descend = 0, youngest = 0;
        for (unsigned i = 0; i < 4; i++) {
            const Node& child = nodes_[children + i];
            const Visibility child_seen = visibility(child, view);
            if (!child_seen.visible) {
                collapse(nodes_[children + i]);
                continue;
            }
            const double gate = seam_ == Seam::farthest
                                    ? farthest_distance(view.camera_local, child.bounds, height_range(child.key))
                                    : child_seen.distance;
            // Seam::span also admits children whose distance spread requires subdivision.
            const bool in_range = gate < double(range_[key.level + 1]) ||
                                  (seam_ == Seam::span &&
                                   level_span(view, child.bounds, height_range(child.key)) > seam_span);
            const unsigned child_slot = slot_of(child.key);
            if (in_range && child_slot != no_slot && slots_[child_slot].resident) {
                descend |= 1u << i;
                youngest = std::max(youngest, slots_[child_slot].resident_frame);
                continue;
            }
            quadrants |= 1u << i;
            if (!in_range) {
                collapse(nodes_[children + i]);
                pressure_.out_of_range++;
            } else {
                pressure_.starved++; // it wanted this child and has no tile for it
                if (child_slot == no_slot)
                    request(child.key, child_seen.pixels);
            }
            if (child_slot != no_slot)
                touch(child.key);
        }
        if (descend) {
            const unsigned age = frame_ - std::min(youngest, frame_);
            const float group = age >= fade_frames ? 0.f : 1.f - float(age) / float(fade_frames);
            for (unsigned i = 0; i < 4; i++)
                if (descend >> i & 1)
                    visit(children + i, view, std::max(fade, group));
        }
        if (!quadrants)
            return;
        // Drop inherited arrival fade when children use this node as their reference; keep distance morphing.
        if (descend)
            fade = 0;
    }
    const unsigned slot = slot_of(key);
    if (slot != no_slot && slots_[slot].resident)
        draws_.push_back({.key = key, .slot = slot, .quadrants = quadrants, .fade = fade});
    else if (slot == no_slot)
        request(key, seen.pixels);
}

// Classify quadrant neighbors by covering ancestors; missing coverage is treated as finer.
void TerrainTier::mark_finer_sides() {
    std::unordered_map<std::uint32_t, unsigned> cover;
    for (const Draw& draw : draws_)
        for (unsigned i = 0; i < 4; i++)
            if (draw.quadrants >> i & 1)
                cover[draw.key.child(i).packed()] = draw.key.level;
    const auto covering = [&](PatchKey cell) {
        for (;;) {
            if (const auto found = cover.find(cell.packed()); found != cover.end())
                return int(found->second);
            if (!cell.level)
                return -1; // nothing covers it at or above its level: split under it
            cell = {cell.face, std::uint8_t(cell.level - 1), std::uint16_t(cell.x / 2), std::uint16_t(cell.y / 2)};
        }
    };
    for (Draw& draw : draws_) {
        draw.finer = 0;
        const unsigned level = draw.key.level + 1u, cells = 1u << level;
        const double size = 2.0 / cells;
        for (unsigned i = 0; i < 4; i++) {
            if (!(draw.quadrants >> i & 1))
                continue;
            const PatchKey q = draw.key.child(i);
            const unsigned qx = i & 1, qy = i >> 1;
            // Test only sides on the patch perimeter.
            for (unsigned axis = 0; axis < 2; axis++) {
                const bool high = axis == 0 ? qx : qy;
                const int dx = axis == 0 ? (high ? 1 : -1) : 0, dy = axis == 1 ? (high ? 1 : -1) : 0;
                const int nx = int(q.x) + dx, ny = int(q.y) + dy;
                PatchKey neighbour{q.face, std::uint8_t(level), std::uint16_t(nx), std::uint16_t(ny)};
                if (nx < 0 || ny < 0 || nx >= int(cells) || ny >= int(cells)) {
                    const CubeCoord c = cube_coordinates(
                        cube_direction(q.face, -1 + (q.x + .5) * size + dx * size, -1 + (q.y + .5) * size + dy * size));
                    const auto index = [&](double v) {
                        return std::uint16_t(std::clamp(int((v + 1) / size), 0, int(cells) - 1));
                    };
                    neighbour = {std::uint8_t(c.face), std::uint8_t(level), index(c.s), index(c.t)};
                }
                const int found = covering(neighbour);
                if (found < 0 || found > int(draw.key.level))
                    draw.finer |= 1u << (axis == 0 ? (high ? 1 : 0) : (high ? 3 : 2));
            }
        }
    }
}

// Serve coarse/large patches first; LRU eviction protects pending and currently used slots.
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
            pressure_.evictions++;
            pressure_.evicted_recent += slots_[slot].used + 60 > frame_;
            slots_by_key_.erase(slots_[slot].key.packed());
        }
        slots_[slot] = {.key = r.key, .used = frame_, .stamp = ++stamp_, .resident = false};
        slots_by_key_[r.key.packed()] = slot;
        generate_.push_back({.key = r.key, .slot = slot, .stamp = stamp_});
    }
}

void TerrainTier::invalidate() {
    for (Slot& slot : slots_)
        slot = {};
    slots_by_key_.clear();
    free_slots_.clear();
    for (unsigned slot = slot_count; slot-- > 0;)
        free_slots_.push_back(slot);
    draws_.clear();
    generate_.clear();
    active_ = false;
}

void TerrainTier::disable() {
    draws_.clear();
    generate_.clear();
    for (unsigned face = 0; face < std::min<std::size_t>(6, nodes_.size()); face++)
        collapse(nodes_[face]);
    active_ = wanted_ = false;
}

bool TerrainTier::mark_resident(unsigned slot, PatchKey key, std::uint32_t stamp, Range<float> heights) {
    // Stamps start at one, so a slot cleared by invalidate() matches nothing in flight.
    if (slots_[slot].stamp != stamp || slots_[slot].key != key)
        return false; // recycled, or regenerated at another detail, since the request went out
    slots_[slot].resident = true;
    slots_[slot].heights = heights;
    // The frame it can first be drawn in: residency is marked between updates.
    slots_[slot].resident_frame = frame_ + 1;
    return true;
}

void TerrainTier::update(const TierView& view, unsigned frame, unsigned budget) {
    frame_ = frame;
    draws_.clear();
    requests_.clear();
    generate_.clear();
    pressure_ = {};
    seam_ = view.seam < unsigned(Seam::count) ? Seam(view.seam) : Seam::none;
    const float bias = std::exp2(view.lod_bias);
    for (unsigned level = 0; level < std::size(level_error); level++)
        range_[level] = level_error[level] * view.height_pixels / (view.tan_y * error_pixels) * bias;
    ensure_roots();
    const double distance = std::max(length(view.body_centre), view.radius);
    const float projected = float(view.radius * view.height_pixels / (distance * view.tan_y));
    wanted_ = projected > view.activate_pixels * (wanted_ ? hysteresis : 1);
    if (!wanted_) {
        for (unsigned face = 0; face < 6; face++)
            collapse(nodes_[face]);
        active_ = false;
        return;
    }
    // Pin all six roots as fallback coverage, including faces outside the current view.
    for (unsigned face = 0; face < 6; face++)
        touch(nodes_[face].key);
    for (unsigned face = 0; face < 6; face++)
        visit(face, view, 0); // a face has no parent to fade from
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
    if (seam_ == Seam::clamp)
        mark_finer_sides();
    choose_generation(budget);
    pressure_.nodes = nodes();
    pressure_.node_budget = slot_count - 64;
    pressure_.requested = unsigned(requests_.size());
    pressure_.served = unsigned(generate_.size());
    double behind = 0, fade = 0;
    for (const Draw& draw : draws_) {
        pressure_.deepest = std::max(pressure_.deepest, unsigned(draw.key.level));
        const PatchBounds bounds = patch_bounds(draw.key);
        const Range<float> heights = height_range(draw.key);
        const double near = nearest_distance(view.camera_local, bounds, heights);
        const double over = level_at(near) - double(draw.key.level);
        behind += over;
        pressure_.behind_one += over > 1;
        fade += double(draw.fade);
        // Estimate morph variation from the patch's distance bounds.
        const double end = double(range_[draw.key.level]), start = .7 * end;
        const auto morph = [&](double d) {
            return std::max(std::clamp((d - start) / std::max(end - start, 1e-9), 0.0, 1.0), double(draw.fade));
        };
        const double lo = morph(near), hi = morph(farthest_distance(view.camera_local, bounds, heights));
        if (hi - lo > .02)
            pressure_.graded++;
        else if (lo > .5)
            pressure_.flat_far++; // standing at its parent's shape
        else
            pressure_.flat_near++; // standing at its own
    }
    pressure_.fade_mean = draws_.empty() ? 0 : float(fade / double(draws_.size()));
    pressure_.behind_count = unsigned(draws_.size());
    pressure_.behind_mean = draws_.empty() ? 0 : float(behind / double(draws_.size()));
}

} // namespace space::render
