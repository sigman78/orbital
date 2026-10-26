#include "planet/streamer.hpp"
#include "planet/clod.hpp"
#include "planet/procedural_provider.hpp"
#include "planet/tile_sampling.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

using namespace space;
using namespace planet;

namespace {

int failures = 0;

bool check(bool ok, const char* test, const char* what) {
    if (!ok) {
        std::printf("FAIL %s: %s\n", test, what);
        failures++;
    }
    return ok;
}

// A provider tile of one small plane, so the streamer's own bookkeeping is what is measured.
constexpr TileLayout fake_layout{
    .planes = {{.id = PlaneId::height, .format = TileFormat::r16_unorm, .side = 4}},
    .plane_count = 1,
};
constexpr unsigned fake_words = fake_layout.bytes() / 4;

class FakeProvider final : public TileProvider {
public:
    const BodyShape& shape() const override { return MinorPlanetTerrain::shape; }
    const LevelTable& levels() const override { return MinorPlanetTerrain::levels; }
    const TileLayout& layout(unsigned) const override { return fake_layout; }
    bool produce(const TileRequest& request, TileOutput& out) override {
        const std::uint32_t pattern = request.key.packed();
        for (unsigned i = 0; i < fake_words; i++)
            std::memcpy(out.planes[0].data() + i * 4, &pattern, 4);
        out.heights = {-.001f, .002f};
        return request.key.packed() != fail_key;
    }
    std::uint32_t fail_key = ~0u;
};

class FakeSink final : public TileSink {
public:
    struct Done {
        TileRequest request;
        unsigned token;
        Range<float> heights;
        std::array<std::uint32_t, fake_words> words;
    };
    explicit FakeSink(unsigned count) : storage_(count * fake_layout.bytes()), used_(count, false) {}
    bool acquire(unsigned bytes, TileStaging& staging) override {
        acquired_bytes.push_back(bytes);
        for (unsigned i = 0; i < used_.size(); i++)
            if (!used_[i]) {
                used_[i] = true;
                staging = {.bytes = {storage_.data() + i * fake_layout.bytes(), fake_layout.bytes()}, .token = i};
                return true;
            }
        return false;
    }
    void complete(const TileRequest& request, const TileStaging& staging, Range<float> heights, float) override {
        Done done{.request = request, .token = staging.token, .heights = heights, .words = {}};
        std::memcpy(done.words.data(), staging.bytes.data(), fake_layout.bytes());
        completed.push_back(done);
        used_[staging.token] = false;
    }
    void abandon(const TileStaging& staging) override {
        abandoned++;
        used_[staging.token] = false;
    }
    std::vector<Done> completed;
    std::vector<unsigned> acquired_bytes;
    unsigned abandoned = 0;

private:
    std::vector<std::byte> storage_;
    std::vector<bool> used_;
};

ClodView near_view() {
    const Vec3d local{1.2, 0, 0};
    ClodView view{.body_centre = local * -1.0,
                  .radius = 1,
                  .axes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
                  .camera_local = local,
                  .height_pixels = 900,
                  .tan_y = .5f};
    for (ViewPlane& plane : view.planes)
        plane = ViewPlane{.normal = {0, 0, 0}, .distance = 1}; // dot + 1 >= 0: nothing is culled
    return view;
}

Clod make_clod() {
    return Clod{
        ClodConfig{.shape = MinorPlanetTerrain::shape, .levels = MinorPlanetTerrain::levels, .cache_capacity = 64}};
}

// Pumps until every job is back (in_flight() drops only when a result is polled), bounded so a lost
// job fails the test instead of hanging it.
bool wait_idle(TileStreamer& streamer, Clod& clod) {
    for (unsigned spin = 0; spin < 10'000'000; spin++) {
        streamer.pump(clod);
        if (streamer.in_flight() == 0)
            return true;
        std::this_thread::yield();
    }
    return false;
}

struct Rig {
    FakeProvider provider;
    FakeSink sink;
    Clod clod = make_clod();
    TileStreamer streamer;
    explicit Rig(unsigned buffers, unsigned workers = 1)
        : sink(buffers), streamer({.provider = &provider, .sink = &sink, .workers = workers}) {}
    // One frame without the pump after it, so the caller sees the clod between dispatch and pump.
    std::vector<Clod::Generation> frame(unsigned number) {
        streamer.pump(clod);
        clod.update(near_view(), number, 8);
        const auto span = clod.generate();
        std::vector<Clod::Generation> generations(span.begin(), span.end());
        streamer.dispatch(clod, 1.f);
        return generations;
    }
};

std::uint16_t quantize(float h) {
    const Range<float> shell = MinorPlanetTerrain::shape.heights;
    return std::uint16_t(std::clamp((h - shell.min) / shell.span(), 0.0f, 1.0f) * 65535.0f + .5f);
}

TileOutput output_for(std::vector<std::byte>& bytes) {
    const TileLayout& layout = ProceduralProvider::tile_layout;
    TileOutput out;
    for (unsigned i = 0; i < layout.plane_count; i++)
        out.planes[i] = {bytes.data() + layout.offset(i), layout.planes[i].bytes()};
    return out;
}

void test_layout_bytes() {
    const TileLayout& layout = ProceduralProvider::tile_layout;
    check(layout.offset(0) == 0, "layout_bytes", "height offset");
    check(layout.offset(1) == 2180, "layout_bytes", "albedo offset literal");
    check(layout.offset(2) == 19080, "layout_bytes", "slope offset literal");
    check(layout.bytes() == 35980, "layout_bytes", "total literal");
    const unsigned height = (tile_side * tile_side * 2 + 3) & ~3u;
    const unsigned albedo = tile_colour_side * tile_colour_side * 4;
    const unsigned slope = tile_colour_side * tile_colour_side * 4;
    check(layout.offset(1) == height && layout.offset(2) == height + albedo &&
              layout.bytes() == height + albedo + slope,
          "layout_bytes", "offsets from the constants");
}

void test_produce_matches_direct() {
    const MinorPlanetTerrain terrain(1007);
    ProceduralProvider provider(terrain);
    const TileLayout& layout = provider.layout(0);
    for (const PatchKey key : {PatchKey{0, 0, 0, 0}, PatchKey{3, 2, 1, 2}, PatchKey{5, 5, 17, 9},
                               PatchKey{1, 8, 100, 37}, PatchKey{4, 11, 2047, 1}}) {
        std::vector<std::byte> bytes(layout.bytes());
        TileOutput out = output_for(bytes);
        check(provider.produce({.key = key}, out), "produce_matches_direct", "produce returned false");

        std::vector<float> heights(tile_side * tile_side);
        std::vector<std::uint8_t> albedo(tile_colour_side * tile_colour_side * 4);
        std::vector<std::uint16_t> slope(tile_colour_side * tile_colour_side * 2);
        generate_height_tile(terrain, key, heights);
        generate_colour_tiles(terrain, key, albedo, slope, 1);
        std::vector<std::uint16_t> quantized(heights.size());
        for (unsigned i = 0; i < heights.size(); i++)
            quantized[i] = quantize(heights[i]);
        check(std::memcmp(out.planes[0].data(), quantized.data(), out.planes[0].size()) == 0, "produce_matches_direct",
              "height plane");
        check(std::memcmp(out.planes[1].data(), albedo.data(), out.planes[1].size()) == 0, "produce_matches_direct",
              "albedo plane");
        check(std::memcmp(out.planes[2].data(), slope.data(), out.planes[2].size()) == 0, "produce_matches_direct",
              "slope plane");
        const Range<float> expected = height_tile_range(heights);
        check(out.heights.min == expected.min && out.heights.max == expected.max, "produce_matches_direct",
              "height range");
    }
}

// generate_colour_tiles takes the strength into the micro-relief, so it reaches the slope plane.
void test_detail_forwarded() {
    const MinorPlanetTerrain terrain(1007);
    ProceduralProvider provider(terrain);
    const PatchKey key{2, 9, 100, 200};
    std::vector<std::byte> full(provider.layout(0).bytes()), none(full.size());
    TileOutput full_out = output_for(full), none_out = output_for(none);
    check(provider.produce({.key = key, .detail = 1}, full_out), "detail_forwarded", "full produce");
    check(provider.produce({.key = key, .detail = 0}, none_out), "detail_forwarded", "zero produce");
    const unsigned colour = ProceduralProvider::tile_layout.offset(1);
    check(std::memcmp(full.data() + colour, none.data() + colour, full.size() - colour) != 0, "detail_forwarded",
          "colour planes differ with detail");
}

void test_completions_reach_sink() {
    Rig rig(16);
    const auto generations = rig.frame(1);
    check(!generations.empty() && wait_idle(rig.streamer, rig.clod), "completions_reach_sink", "no work or timeout");
    rig.streamer.pump(rig.clod);
    check(rig.sink.completed.size() == generations.size(), "completions_reach_sink", "completion count");
    for (const Clod::Generation& g : generations) {
        const auto it = std::find_if(rig.sink.completed.begin(), rig.sink.completed.end(), [&](const auto& done) {
            return done.request.key == g.key && done.request.slot == g.slot && done.request.stamp == g.stamp;
        });
        if (!check(it != rig.sink.completed.end(), "completions_reach_sink", "generation missing from sink"))
            continue;
        const bool pattern = std::all_of(it->words.begin(), it->words.end(),
                                         [&](std::uint32_t w) { return w == g.key.packed(); });
        check(pattern, "completions_reach_sink", "staging held the provider's pattern");
        check(it->request.detail == 1.f && it->heights.min == -.001f && it->heights.max == .002f,
              "completions_reach_sink", "detail and heights forwarded");
    }
    check(rig.sink.abandoned == 0, "completions_reach_sink", "nothing abandoned");
}

void test_mark_resident_after_pump() {
    Rig rig(16);
    const auto generations = rig.frame(1);
    check(wait_idle(rig.streamer, rig.clod), "mark_resident_after_pump", "timeout");
    rig.streamer.pump(rig.clod);
    const unsigned before = rig.clod.resident();
    check(rig.clod.pending() == generations.size(), "mark_resident_after_pump", "pending until marked");
    for (const FakeSink::Done& done : rig.sink.completed)
        check(rig.clod.mark_resident(done.request.slot, done.request.key, done.request.stamp, done.heights),
              "mark_resident_after_pump", "mark_resident rejected a completed tile");
    check(rig.clod.resident() == before + generations.size() && rig.clod.pending() == 0, "mark_resident_after_pump",
          "resident grew");
}

void test_acquire_failure_releases() {
    Rig rig(1);
    const auto generations = rig.frame(1);
    check(generations.size() > 1, "acquire_failure_releases", "needs several generations");
    check(wait_idle(rig.streamer, rig.clod), "acquire_failure_releases", "timeout");
    check(rig.clod.pending() == 1, "acquire_failure_releases", "only the served slot stays pending");
    check(rig.sink.acquired_bytes.size() == generations.size(), "acquire_failure_releases", "acquire per generation");
    unsigned served = 0;
    for (const Clod::Generation& g : generations)
        served += rig.clod.mark_resident(g.slot, g.key, g.stamp, {0, 0});
    check(served == 1, "acquire_failure_releases", "released stamps are rejected, the served one accepted");
    rig.streamer.pump(rig.clod);
    rig.clod.update(near_view(), 2, 8);
    check(!rig.clod.generate().empty(), "acquire_failure_releases", "released keys are asked for again");
}

void test_produce_failure_abandons() {
    Rig rig(16);
    rig.clod.update(near_view(), 1, 8);
    const Clod::Generation failed = rig.clod.generate().front();
    const std::size_t total = rig.clod.generate().size();
    rig.provider.fail_key = failed.key.packed();
    rig.streamer.dispatch(rig.clod, 1.f);
    check(wait_idle(rig.streamer, rig.clod), "produce_failure_abandons", "timeout");
    rig.streamer.pump(rig.clod);
    check(rig.sink.abandoned == 1, "produce_failure_abandons", "abandon once");
    check(rig.sink.completed.size() == total - 1, "produce_failure_abandons", "the others complete");
    check(std::none_of(rig.sink.completed.begin(), rig.sink.completed.end(),
                       [&](const auto& done) { return done.request.key == failed.key; }),
          "produce_failure_abandons", "failed tile not completed");
    check(!rig.clod.mark_resident(failed.slot, failed.key, failed.stamp, {0, 0}), "produce_failure_abandons",
          "slot released");
}

void test_workers() {
    Rig three(1, 3), automatic(1, 0);
    check(three.streamer.workers() == 3, "workers", "explicit count");
    check(automatic.streamer.workers() >= 1, "workers", "automatic count is at least one");
}

void test_acquire_bytes() {
    Rig rig(16);
    rig.frame(1);
    check(wait_idle(rig.streamer, rig.clod), "acquire_bytes", "timeout");
    check(!rig.sink.acquired_bytes.empty(), "acquire_bytes", "no acquire");
    check(std::all_of(rig.sink.acquired_bytes.begin(), rig.sink.acquired_bytes.end(),
                      [](unsigned bytes) { return bytes == fake_layout.bytes(); }),
          "acquire_bytes", "layout bytes requested");
}

} // namespace

int main() {
    test_layout_bytes();
    test_produce_matches_direct();
    test_detail_forwarded();
    test_completions_reach_sink();
    test_mark_resident_after_pump();
    test_acquire_failure_releases();
    test_produce_failure_abandons();
    test_workers();
    test_acquire_bytes();
    std::printf("streamer tests: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
