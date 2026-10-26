#include "planet/cook.hpp"

#include "planet/crater_terrain.hpp"
#include "planet/procedural_provider.hpp"
#include "planet/tile_store.hpp"

#include <charconv>
#include <cstdio>
#include <string_view>

namespace planet {

namespace {

constexpr std::uint64_t default_seed = 20260911;

constexpr const char* usage_text =
    "usage: planet-forge --out DIR [options]\n"
    "  --out DIR            body directory to write (required)\n"
    "  --seed N             showcase seed (default 20260911), mapped to the terrain's seed\n"
    "  --terrain-seed N     the terrain's seed directly; overrides --seed\n"
    "  --levels A..B | A    levels to cook (default 0..5, at most 12)\n"
    "  --faces LIST         comma list of faces 0..5 (default all)\n"
    "  --threads N          worker threads (default every core)\n"
    "  --force              rewrite shards that are already current\n"
    "  --check              read every tile back and compare with the provider\n"
    "  --quiet              no per-level progress\n"
    "  --explain F,L,X,Y    print the store's index entry of a tile and exit\n"
    "  --help, -h           this text\n";

bool parse_number(std::string_view text, std::uint64_t& out) {
    if (text.empty())
        return false;
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2);
    }
    const char* end = text.data() + text.size();
    const auto [stop, ec] = std::from_chars(text.data(), end, out, base);
    return ec == std::errc{} && stop == end;
}

bool parse_unsigned(std::string_view text, unsigned& out) {
    std::uint64_t value = 0;
    if (!parse_number(text, value) || value > 0xffffffffu)
        return false;
    out = unsigned(value);
    return true;
}

// "A..B" or "A".
bool parse_levels(std::string_view text, unsigned& first, unsigned& last) {
    const std::size_t dots = text.find("..");
    if (dots == std::string_view::npos) {
        if (!parse_unsigned(text, first))
            return false;
        last = first;
    } else if (!parse_unsigned(text.substr(0, dots), first) || !parse_unsigned(text.substr(dots + 2), last)) {
        return false;
    }
    return first <= last && last < store::max_levels;
}

bool parse_faces(std::string_view text, unsigned& mask) {
    mask = 0;
    while (!text.empty()) {
        const std::size_t comma = text.find(',');
        unsigned face = 0;
        if (!parse_unsigned(text.substr(0, comma), face) || face >= store::face_count)
            return false;
        mask |= 1u << face;
        text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
        if (comma != std::string_view::npos && text.empty())
            return false; // a trailing comma
    }
    return mask != 0;
}

bool parse_key(std::string_view text, PatchKey& key) {
    unsigned part[4] = {};
    for (unsigned i = 0; i < 4; i++) {
        const std::size_t comma = text.find(',');
        if ((i < 3) != (comma != std::string_view::npos) || !parse_unsigned(text.substr(0, comma), part[i]))
            return false;
        if (comma != std::string_view::npos)
            text = text.substr(comma + 1);
    }
    if (part[0] >= store::face_count || part[1] > patch_level_max || part[2] >= (1u << part[1]) ||
        part[3] >= (1u << part[1]))
        return false;
    key = {.face = std::uint8_t(part[0]),
           .level = std::uint8_t(part[1]),
           .x = std::uint16_t(part[2]),
           .y = std::uint16_t(part[3])};
    return true;
}

int fail_usage(const char* what, const char* arg) {
    std::fprintf(stderr, "planet-forge: %s%s%s\n%s", what, arg ? " " : "", arg ? arg : "", usage_text);
    return 1;
}

int explain(const char* directory, PatchKey key) {
    TileStore store;
    const StoreStatus opened = store.open(directory);
    if (opened != StoreStatus::ok) {
        std::fprintf(stderr, "planet-forge: cannot open %s: %s\n", directory, store_status_name(opened));
        return 2;
    }
    store::TileEntry entry;
    if (!store.entry(key, entry)) {
        std::fprintf(stderr, "planet-forge: tile %u,%u,%u,%u is not stored\n", key.face, key.level, key.x, key.y);
        return 3;
    }
    std::printf("tile %u,%u,%u,%u: key 0x%08x, %u bytes at offset %llu, heights %g .. %g\n", key.face, key.level, key.x,
                key.y, entry.key, entry.bytes, (unsigned long long)entry.offset, entry.height_min, entry.height_max);
    return 0;
}

} // namespace

int cook_main(int argc, char** argv, const CookHost& host) {
    CookOptions options;
    const char* out = nullptr;
    const char* explain_key = nullptr;
    std::uint64_t seed = default_seed, terrain_seed = 0;
    bool have_terrain_seed = false;

    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];
        std::string_view inline_value;
        bool has_inline = false;
        if (arg.starts_with("--")) {
            if (const std::size_t eq = arg.find('='); eq != std::string_view::npos) {
                inline_value = arg.substr(eq + 1);
                arg = arg.substr(0, eq);
                has_inline = true;
            }
        }
        // The value of an option that takes one: after '=' or the next argument.
        auto value = [&](std::string_view& text) {
            if (has_inline) {
                text = inline_value;
                return true;
            }
            if (i + 1 >= argc)
                return false;
            text = argv[++i];
            return true;
        };
        std::string_view text;
        if (arg == "--help" || arg == "-h") {
            std::fputs(usage_text, stdout);
            return 0;
        } else if (arg == "--force") {
            options.force = true;
        } else if (arg == "--check") {
            options.check = true;
        } else if (arg == "--quiet") {
            options.quiet = true;
        } else if (arg == "--out") {
            if (!value(text) || text.empty())
                return fail_usage("--out needs a directory", nullptr);
            out = text.data(); // a view of argv, null-terminated
        } else if (arg == "--explain") {
            if (!value(text))
                return fail_usage("--explain needs a key", nullptr);
            explain_key = text.data();
        } else if (arg == "--seed") {
            if (!value(text) || !parse_number(text, seed))
                return fail_usage("--seed needs a number", nullptr);
        } else if (arg == "--terrain-seed") {
            if (!value(text) || !parse_number(text, terrain_seed))
                return fail_usage("--terrain-seed needs a number", nullptr);
            have_terrain_seed = true;
        } else if (arg == "--levels") {
            if (!value(text) || !parse_levels(text, options.level_min, options.level_max))
                return fail_usage("--levels needs A..B or A", nullptr);
        } else if (arg == "--faces") {
            if (!value(text) || !parse_faces(text, options.face_mask))
                return fail_usage("--faces needs a comma list of 0..5", nullptr);
        } else if (arg == "--threads") {
            if (!value(text) || !parse_unsigned(text, options.threads))
                return fail_usage("--threads needs a number", nullptr);
        } else {
            return fail_usage("unknown argument", argv[i]);
        }
    }

    if (!out)
        return fail_usage("--out is required", nullptr);
    if (explain_key) {
        PatchKey key;
        if (!parse_key(explain_key, key))
            return fail_usage("--explain needs face,level,x,y:", explain_key);
        return explain(out, key);
    }

    if (!have_terrain_seed) {
        terrain_seed = seed;
        if (host.terrain_seed && !host.terrain_seed(host.user, seed, terrain_seed)) {
            std::fprintf(stderr, "planet-forge: showcase seed 0x%llx has no minor planet\n", (unsigned long long)seed);
            return 2;
        }
    }
    std::printf("terrain seed 0x%016llx\n", (unsigned long long)terrain_seed);

    const MinorPlanetTerrain terrain(terrain_seed);
    ProceduralProvider provider(terrain);
    options.directory = out;
    options.seed = terrain_seed;

    CookReport report;
    const CookStatus status = cook(options, provider, report);
    if (status != CookStatus::ok && status != CookStatus::mismatch) {
        std::fprintf(stderr, "planet-forge: %s\n", cook_status_name(status));
        return status == CookStatus::conflict ? 3 : 2;
    }
    std::printf("cooked %u shards, skipped %u, %u tiles, %.1f MiB in %.1f s\n", report.shards_cooked,
                report.shards_skipped, report.tiles_written, double(report.bytes_written) / (1024 * 1024),
                report.seconds);
    if (options.check)
        std::printf("checked %u tiles, %u mismatched\n", report.tiles_checked, report.tiles_mismatched);
    if (status == CookStatus::mismatch) {
        std::fprintf(stderr, "planet-forge: %s\n", cook_status_name(status));
        return 3;
    }
    return 0;
}

} // namespace planet
