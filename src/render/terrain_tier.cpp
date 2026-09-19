#include "render/terrain_tier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace space::render {

namespace {

constexpr unsigned no_slot = TerrainTier::slot_count;

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
    const Range<float> reach = reach_of(node.key);
    // Past the horizon, by the same support: a point p at radius r is on the near side of an
    // occluding sphere of radius R seen from C when dot(p * r, C) >= R * R, so the patch is
    // hidden when its support along the eye falls short of that.
    //
    // That is the plane through the sphere's horizon circle, which decides visibility for
    // points *on* the sphere and is a heuristic above it: a peak past the plane can still stand
    // over the horizon. The sound test is the cone, hidden only past acos(R / d) + acos(R / r),
    // and the plane always cuts inside it. What keeps this sound is slack of the other kind --
    // the occluder is the shell's floor, 0.95, while ground near a limb stands near 1.0 -- and
    // today that outweighs the error for the relief this body has. It is an accident of the
    // constants, not a guarantee: `terrain_tier_tests --truth` is what checks it, and taller
    // relief, a higher floor or a smaller descendant_relief would each have to be put through
    // it. Do not raise the occluder while the plane formula stands. The known fallback is the
    // cone test over the lowest ground within acos(R / d), at 10 to 40 percent more patches.
    const double d = length(view.camera_local);
    // The test is valid from anywhere outside the occluder, which is anywhere above ground.
    // Comparing with the reference surface instead switched the horizon off over every lowland,
    // half the body, and left the frustum alone to keep whatever it crossed on the far side.
    constexpr double occluder = horizon_occluder;
    if (d > occluder && patch_cell_support(b, view.camera_local * (1 / d), reach) * d < occluder * occluder)
        return {};
    // The patch is a cell, not a ball and not the cap around it. A cap pre-test stood here to
    // settle a plane on one dot, but it only ever rejects what the cell also rejects, and most
    // nodes pass most planes and paid for both: dropping it took 5 percent off update.
    for (const LocalPlane& plane : planes_) {
        if (plane.offset + patch_cell_support(b, plane.normal, reach) < 0)
            return {};
    }
    // Prioritize nearby patch coverage; distances and sizes are both in radii. The reach serves
    // here too, so the distance covers this patch and every descendant it may still grow.
    const double near = nearest_distance(view.camera_local, b, reach);
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

// A node's height reach: its own tile's range and margin, or the nearest resident ancestor's.
// An ancestor's margin covers its whole subtree, so the inherited bound stays sound, and it is
// far tighter than the global shell -- whose 1.05 top stands several degrees of arc further
// over the horizon than the 1.01 a loaded neighbour has. A root is resident while the tier is
// active, so the walk ends in a measured range; in practice it is a lookup or two, since a
// node is only reached through a parent that has a tile.
Range<float> TerrainTier::reach_of(PatchKey key) const {
    for (;; key = key.parent()) {
        const unsigned slot = resident_slot(key);
        if (slot == no_slot && key.level)
            continue;
        const Range<float> heights = slot != no_slot ? slots_[slot].heights : full_height_range;
        const float margin = relief_margin(key.level);
        return {heights.min - margin, heights.max + margin};
    }
}

const TerrainTier::Node* TerrainTier::find(PatchKey key) const {
    if (nodes_.size() < 6 || key.face >= 6)
        return nullptr;
    const Node* node = &nodes_[key.face];
    for (unsigned level = 1; level <= key.level; level++) {
        if (!node->children)
            return nullptr; // the tree stops here: everything below is covered by this node
        const unsigned shift = key.level - level;
        node = &nodes_[node->children + (((key.x >> shift) & 1) | ((key.y >> shift) & 1) << 1)];
    }
    return node;
}

void TerrainTier::explain(PatchKey key, const TierView& view, std::vector<Step>& steps) const {
    steps.clear();
    for (PatchKey k = key;; k = k.parent()) {
        steps.push_back({.key = k});
        if (!k.level)
            break;
    }
    std::reverse(steps.begin(), steps.end());
    const double d = length(view.camera_local);
    const Vec3d eye = d > 1e-12 ? view.camera_local * (1 / d) : Vec3d{0, 0, 1};
    for (Step& step : steps) {
        const PatchBounds b = patch_bounds(step.key);
        step.slot = resident_slot(step.key);
        step.reach = reach_of(step.key);
        for (PatchKey a = step.key;; a = a.parent())
            if (resident_slot(a) != no_slot || !a.level) {
                step.reach_level = a.level;
                break;
            }
        // Infinity where the camera stands inside the occluder: the test does not run at all,
        // which is worth seeing, since then only the frustum holds the far side out.
        step.horizon_margin = d > horizon_occluder
                                  ? patch_cell_support(b, eye, step.reach) * d - horizon_occluder * horizon_occluder
                                  : std::numeric_limits<double>::infinity();
        step.plane_margin = std::numeric_limits<double>::infinity();
        for (unsigned i = 0; i < std::size(planes_); i++)
            if (const double margin = planes_[i].offset + patch_cell_support(b, planes_[i].normal, step.reach);
                margin < step.plane_margin) {
                step.plane_margin = margin;
                step.plane = i;
            }
        step.distance = nearest_distance(view.camera_local, b, step.reach);
        step.split_range = step.key.level + 1 < std::size(range_) ? double(range_[step.key.level + 1]) : 0;
        const Node* node = find(step.key);
        step.split = node && node->children;
        for (const Draw& draw : draws_)
            if (draw.key == step.key) {
                step.drawn = true;
                step.quadrants = draw.quadrants;
            }
    }
}

TerrainTier::Audit TerrainTier::audit() const {
    Audit result{.allocated = nodes()};
    const auto walk = [&](auto&& self, std::uint32_t index) -> void {
        result.reachable++;
        if (const std::uint32_t children = nodes_[index].children)
            for (unsigned i = 0; i < 4; i++)
                self(self, children + i);
    };
    for (unsigned face = 0; face < std::min<std::size_t>(6, nodes_.size()); face++)
        walk(walk, face);
    std::unordered_map<std::uint32_t, unsigned> drawn;
    for (const Draw& draw : draws_)
        drawn[draw.key.packed()] = draw.quadrants;
    unsigned oldest = frame_;
    for (const auto& [packed, slot] : slots_by_key_) {
        if (!slots_[slot].resident)
            continue;
        result.resident++;
        result.resident_undrawn += !drawn.count(packed);
        oldest = std::min(oldest, slots_[slot].used);
    }
    result.oldest_age = frame_ - oldest;
    return result;
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

// `seen` is the caller's, which already has it: a node computed its children's visibility to
// choose between them, and recomputing it on entry paid for every plane of every node twice.
void TerrainTier::visit(std::uint32_t index, const TierView& view, const Visibility& seen, float fade) {
    if (!seen.visible) {
        collapse(nodes_[index]);
        return;
    }
    const PatchKey key = nodes_[index].key;
    touch(key);
    // The distance visibility measured covers the whole subtree, so it decides the split too.
    // Measuring against the full shell instead read zero straight below a camera inside it,
    // and every node around the nadir then asked for children it would never draw.
    const bool wants = key.level < patch_level_max && key.level + 1 < std::size(level_error) &&
                       seen.distance <
                           double(range_[key.level + 1]) * (nodes_[index].children ? 1.0 / double(hysteresis) : 1.0);
    const bool allowed = nodes_[index].children || nodes() < node_budget;
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
        Visibility child_seen[4];
        for (unsigned i = 0; i < 4; i++) {
            const Node& child = nodes_[children + i];
            child_seen[i] = visibility(child, view);
            if (!child_seen[i].visible) {
                collapse(nodes_[children + i]);
                continue;
            }
            const bool in_range = child_seen[i].distance < double(range_[key.level + 1]);
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
                    request(child.key, child_seen[i].pixels);
            }
            if (child_slot != no_slot)
                touch(child.key);
        }
        if (descend) {
            const unsigned age = frame_ - std::min(youngest, frame_);
            const float group = age >= fade_frames ? 0.f : 1.f - float(age) / float(fade_frames);
            for (unsigned i = 0; i < 4; i++)
                if (descend >> i & 1)
                    visit(children + i, view, child_seen[i], std::max(fade, group));
        }
        if (!quadrants)
            return;
        // Drop inherited arrival fade when children use this node as their reference; keep distance morphing.
        if (descend)
            fade = 0;
    }
    const unsigned slot = slot_of(key);
    if (slot != no_slot && slots_[slot].resident)
        draws_.push_back(
            {.key = key,
             .slot = slot,
             .quadrants = quadrants,
             .fade = fade,
             .near_distance = float(seen.distance),
             .far_distance = float(farthest_distance(view.camera_local, nodes_[index].bounds, height_range(key)))});
    else if (slot == no_slot)
        request(key, seen.pixels);
}

// Serve coarse/large patches first; LRU eviction protects pending and currently used slots.
void TerrainTier::choose_generation(unsigned budget) {
    const unsigned cap = std::min(budget, generate_per_frame);
    // Reclaim slots whose generation never came back, whatever lost it. `used` cannot say:
    // a visible pending patch is touched every frame, so the assignment frame decides.
    // A cursor over a window per frame rather than the whole array: there is no queue to
    // overrun, and a slot waits at most slot_count / sweep_window extra frames.
    for (unsigned n = 0; n < sweep_window; n++) {
        Slot& slot = slots_[sweep_cursor_];
        sweep_cursor_ = (sweep_cursor_ + 1) % slot_count;
        if (!slot.resident && slot.stamp && slot.assigned_frame + pending_timeout < frame_) {
            slots_by_key_.erase(slot.key.packed());
            free_slots_.push_back(unsigned(&slot - slots_.data()));
            slot = {};
        }
    }
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
        slots_[slot] = {.key = r.key, .used = frame_, .assigned_frame = frame_, .stamp = ++stamp_, .resident = false};
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

void TerrainTier::release(unsigned slot, PatchKey key, std::uint32_t stamp) {
    // Stamps start at one, so a cleared slot matches nothing; a resident slot is not ours to free.
    if (slot >= slot_count || slots_[slot].stamp != stamp || slots_[slot].key != key || slots_[slot].resident)
        return;
    slots_by_key_.erase(key.packed());
    slots_[slot] = {};
    free_slots_.push_back(slot);
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
    const float bias = std::exp2(view.lod_bias);
    for (unsigned level = 0; level < std::size(level_error); level++)
        range_[level] = level_error[level] * view.height_pixels / (view.tan_y * error_pixels) * bias;
    ensure_roots();
    // Carry the frustum into the body's frame and into radii, once: the axes are orthonormal,
    // so a plane normal transposes by three dots and a patch test becomes one more.
    for (std::size_t i = 0; i < std::size(planes_); i++) {
        const geometry::Plane& plane = view.frustum.planes[i];
        const Vec3d n{double(plane.normal.x), double(plane.normal.y), double(plane.normal.z)};
        planes_[i] = {.normal = {dot(n, view.axes[0]), dot(n, view.axes[1]), dot(n, view.axes[2])},
                      .offset = (dot(n, view.body_centre) + double(plane.distance)) / std::max(view.radius, 1e-12)};
    }
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
        visit(face, view, visibility(nodes_[face], view), 0); // a face has no parent to fade from
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
    pressure_.nodes = nodes();
    pressure_.node_budget = node_budget;
    pressure_.requested = unsigned(requests_.size());
    pressure_.served = unsigned(generate_.size());
    double behind = 0, fade = 0;
    for (const Draw& draw : draws_) {
        pressure_.deepest = std::max(pressure_.deepest, unsigned(draw.key.level));
        const double near = double(draw.near_distance);
        const double over = level_at(near) - double(draw.key.level);
        behind += over;
        pressure_.behind_one += over > 1;
        fade += double(draw.fade);
        // Estimate morph variation from the patch's distance bounds.
        const double end = double(range_[draw.key.level]), start = .7 * end;
        const auto morph = [&](double d) {
            return std::max(std::clamp((d - start) / std::max(end - start, 1e-9), 0.0, 1.0), double(draw.fade));
        };
        const double lo = morph(near), hi = morph(double(draw.far_distance));
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
