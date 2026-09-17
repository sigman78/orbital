#pragma once
#include "scene/geometry.hpp"
#include "scene/terrain_patch.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace space::render {

// The tier's view of one frame: the body relative to the camera, its spin, and
// the projection, in the units cull_bodies uses.
struct TierView {
    Vec3d body_centre;  // camera-relative, units
    double radius = 0;  // the body's, units
    Vec3d axes[3];      // the body's local x, y and z in world
    Vec3d camera_local; // the camera in the body's local frame, radii
    geometry::Frustum frustum;
    float height_pixels = 0, tan_y = 0;
};

// The near tier's CPU side: which patches of the cube sphere to draw this frame
// and which to generate for the next, over a fixed pool of vertex slots kept as
// a cache by patch (least recently used out). The quadtree splits a patch while
// its projected size is over a threshold, with hysteresis, and collapses it out
// of view; a patch whose visible children are not all resident draws itself,
// so the surface is always complete. Nothing here touches the GPU: the caller
// generates the requested patches into the slots and draws the listed ones.
class TerrainTier {
public:
    static constexpr unsigned slot_count = 512;
    static constexpr unsigned generate_per_frame = 8;
    // Sizes are in cull_bodies' units: a projected radius over the half height, twice the pixels.
    static constexpr float activate_pixels = 1200; // the body's, where the finest sphere level runs out
    static constexpr float split_pixels = 400;     // a patch's edge: it splits above 200 px, 12 px a quad
    static constexpr float hysteresis = .8f;       // the fraction of either the way back

    struct Generation {
        PatchKey key;
        unsigned slot;
    };

    void update(const TierView& view, unsigned frame);
    void disable(); // the switch off: the tree collapses, the cache stays
    // True once the tier covers the body: the sphere levels draw until then.
    bool active() const { return active_; }
    std::span<const unsigned> draws() const { return draws_; } // slots, this frame
    std::span<const Generation> generate() const { return generate_; }
    unsigned resident() const { return unsigned(slots_by_key_.size()); }
    unsigned nodes() const { return unsigned(nodes_.size() - free_blocks_.size() * 4); }

private:
    struct Node {
        PatchKey key;
        PatchBounds bounds;
        std::uint32_t children = 0; // index of the first of four, 0 none (the roots are never children)
    };
    struct Slot {
        PatchKey key;
        unsigned used = 0; // the frame it was last needed
        bool resident = false;
    };
    struct Request {
        PatchKey key;
        float pixels;
    };
    struct Visibility {
        bool visible = false;
        float pixels = 0;
    };

    void ensure_roots();
    Visibility visibility(const Node& node, const TierView& view) const;
    void visit(std::uint32_t index, const TierView& view);
    void collapse(Node& node);
    std::uint32_t allocate_children(const Node& node);
    unsigned slot_of(PatchKey key) const; // slot_count when not resident
    void touch(PatchKey key);
    void request(PatchKey key, float pixels);
    void choose_generation();

    std::vector<Node> nodes_;
    std::vector<std::uint32_t> free_blocks_;
    std::vector<Slot> slots_;
    std::vector<unsigned> free_slots_;
    std::unordered_map<std::uint32_t, unsigned> slots_by_key_;
    std::vector<unsigned> draws_;
    std::vector<Request> requests_;
    std::vector<Generation> generate_;
    unsigned frame_ = 0;
    bool active_ = false, wanted_ = false;
};

} // namespace space::render
