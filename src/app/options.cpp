#include "app/options.hpp"
#include "app/camera.hpp"
#include "core/log.hpp"
#include <charconv>
#include <cmath>
#include <fstream>
#include <type_traits>

namespace space::app {
namespace {
namespace window_limits {
constexpr Range<unsigned> width{320, 7680}, height{200, 4320};
}
template <class T> bool parse_number(std::optional<std::string_view> text, T& out) {
    if (!text)
        return false;
    const char* end = text->data() + text->size();
    const auto result = std::from_chars(text->data(), end, out);
    if (result.ec != std::errc{} || result.ptr != end)
        return false;
    if constexpr (std::is_floating_point_v<T>)
        return std::isfinite(out);
    return true;
}

template <class T> bool parse_choice(std::optional<std::string_view> text, T& out, unsigned count) {
    return parse_number(text, out) && out >= 0 && unsigned(out) < count;
}

bool parse_path(std::optional<std::string_view> text, std::filesystem::path& out) {
    if (!text || text->empty())
        return false;
    out = *text;
    return true;
}

bool options_valid(const Options& options) {
    const bool size_ok = window_limits::width.contains(options.size.width) &&
                         window_limits::height.contains(options.size.height);
    const bool time_ok = std::isfinite(options.fixed_time) && options.fixed_time >= -1 &&
                         std::isfinite(options.duration) && options.duration >= 0;
    const bool exposure_ok = std::isfinite(options.exposure) && options.exposure > 0;
    const bool bookmark_ok = options.bookmark >= -1 && options.bookmark < int(bookmark_count);
    const bool view_ok = std::isfinite(options.back) && options.back >= 0 && std::isfinite(options.fov_div) &&
                         options.fov_div >= 1;
    return size_ok && time_ok && exposure_ok && bookmark_ok && view_ok;
}

// Applies --option [value...] arguments over the options; false after logging the first bad one.
bool apply_arguments(Options& options, std::span<const std::string_view> args) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        const auto value = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= args.size())
                return std::nullopt;
            return args[++i];
        };
        bool ok = true;
        if (arg == "--help")
            options.help = true;
        else if (arg == "--tour")
            options.tour = true;
        else if (arg == "--ui")
            options.ui = true;
        else if (arg == "--belt-sun-view")
            options.belt_sun_view = true;
        else if (arg == "--high")
            options.high = true;
        else if (arg == "--no-hud")
            options.no_hud = true;
        else if (arg == "--headless")
            options.headless = true;
        else if (arg == "--seed")
            ok = parse_number(value(), options.seed);
        else if (arg == "--frames")
            ok = parse_number(value(), options.frame_limit);
        else if (arg == "--rocks")
            ok = parse_number(value(), options.rocks);
        else if (arg == "--lod-scale")
            ok = parse_number(value(), options.lod_scale) && options.lod_scale > 0;
        else if (arg == "--disc")
            ok = parse_choice(value(), options.disc, 2);
        else if (arg == "--near-tier")
            ok = parse_choice(value(), options.near_tier, 2);
        else if (arg == "--wireframe")
            ok = parse_choice(value(), options.wireframe, 2);
        else if (arg == "--terrain-debug")
            ok = parse_choice(value(), options.terrain_debug, 6);
        else if (arg == "--tier-activate")
            ok = parse_number(value(), options.tier_activate) && options.tier_activate > 0;
        else if (arg == "--terrain-detail")
            ok = parse_number(value(), options.terrain_detail) && options.terrain_detail >= 0;
        else if (arg == "--lod-bias")
            ok = parse_number(value(), options.lod_bias) && options.lod_bias >= -4 && options.lod_bias <= 4;
        else if (arg == "--dust")
            ok = parse_choice(value(), options.dust, 2);
        else if (arg == "--vsync")
            ok = parse_choice(value(), options.vsync, 2);
        else if (arg == "--taa")
            ok = parse_choice(value(), options.taa, 2);
        else if (arg == "--spatial")
            ok = parse_choice(value(), options.spatial, unsigned(render::SpatialAA::Count));
        else if (arg == "--pan-stop-frame")
            ok = parse_number(value(), options.pan_stop_frame);
        else if (arg == "--pan")
            ok = parse_number(value(), options.pan);
        else if (arg == "--maximize-at")
            ok = parse_number(value(), options.maximize_at);
        else if (arg == "--fullscreen-at")
            ok = parse_number(value(), options.fullscreen_at);
        else if (arg == "--back")
            ok = parse_number(value(), options.back);
        else if (arg == "--fov-div")
            ok = parse_number(value(), options.fov_div);
        else if (arg == "--tone")
            ok = parse_choice(value(), options.tone, unsigned(render::ToneCurve::Count));
        else if (arg == "--hdr")
            ok = parse_choice(value(), options.hdr, unsigned(render::HdrOutput::Count));
        else if (arg == "--galaxy-view") {
            auto& longitude = options.galaxy_view.emplace();
            ok = parse_number(value(), longitude) && longitude >= -180 && longitude <= 180;
        } else if (arg == "--sun-at") {
            auto& at = options.sun_at.emplace();
            ok = parse_number(value(), at[0]) && parse_number(value(), at[1]);
        } else if (arg == "--galaxy")
            ok = parse_choice(value(), options.galaxy, unsigned(render::GalaxyMode::Count));
        else if (arg == "--splat")
            ok = parse_choice(value(), options.splat, unsigned(render::SplatMode::Count));
        else if (arg == "--width")
            ok = parse_number(value(), options.size.width);
        else if (arg == "--height")
            ok = parse_number(value(), options.size.height);
        else if (arg == "--time")
            ok = parse_number(value(), options.fixed_time);
        else if (arg == "--duration")
            ok = parse_number(value(), options.duration);
        else if (arg == "--bookmark")
            ok = parse_number(value(), options.bookmark);
        else if (arg == "--exposure")
            ok = parse_number(value(), options.exposure);
        else if (arg == "--capture")
            ok = parse_path(value(), options.capture);
        else if (arg == "--benchmark")
            ok = parse_path(value(), options.benchmark);
        else if (arg == "--shots")
            ok = parse_path(value(), options.shots);
        else if (arg == "--report")
            ok = parse_path(value(), options.report);
        else {
            log::error("unknown option {}", arg);
            return false;
        }
        if (!ok) {
            log::error("missing or invalid value for {}", arg);
            return false;
        }
    }
    if (!options_valid(options)) {
        log::error("invalid dimensions, time, duration, exposure, view, or bookmark (expected 0..{})",
                   bookmark_count - 1);
        return false;
    }
    return true;
}

} // namespace

std::optional<Options> parse_options(int argc, const char* const* argv) {
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);
    Options options;
    if (!apply_arguments(options, args))
        return std::nullopt;
    return options;
}

std::optional<Shot> parse_shot(const Options& base, std::string_view line, unsigned line_number) {
    // Tokens are whitespace separated; a key=value token becomes --key value, a
    // bare key a flag, and commas in a value split it into several values.
    std::vector<std::string> words; // owns the storage the views below point into
    std::vector<std::string_view> tokens;
    for (std::size_t i = 0; i < line.size();) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r'))
            ++i;
        std::size_t end = i;
        while (end < line.size() && line[end] != ' ' && line[end] != '\t' && line[end] != '\r')
            ++end;
        if (end > i)
            tokens.push_back(line.substr(i, end - i));
        i = end;
    }
    if (tokens.empty() || tokens.front().starts_with('#'))
        return std::nullopt;
    Shot shot{.name = "shot " + std::to_string(line_number), .options = base};
    // The base's own single-run outputs do not carry into a shot: each names its own.
    shot.options.capture.clear();
    shot.options.benchmark.clear();
    words.reserve(tokens.size() * 3);
    std::vector<std::string_view> args;
    for (const std::string_view token : tokens) {
        const auto equals = token.find('=');
        const std::string_view key = token.substr(0, equals);
        if (key == "name") {
            if (equals == std::string_view::npos || equals + 1 >= token.size()) {
                log::error("shot list line {}: name needs a value", line_number);
                return std::nullopt;
            }
            shot.name = std::string(token.substr(equals + 1));
            continue;
        }
        words.push_back("--" + std::string(key));
        args.push_back(words.back());
        if (equals == std::string_view::npos)
            continue;
        std::string_view rest = token.substr(equals + 1);
        while (true) {
            const auto comma = rest.find(',');
            words.emplace_back(rest.substr(0, comma));
            args.push_back(words.back());
            if (comma == std::string_view::npos)
                break;
            rest = rest.substr(comma + 1);
        }
    }
    if (!apply_arguments(shot.options, args)) {
        log::error("shot list line {} ({}) does not parse", line_number, shot.name);
        return std::nullopt;
    }
    return shot;
}

std::optional<std::vector<Shot>> load_shots(const Options& base, const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        log::error("cannot read the shot list {}", path.string());
        return std::nullopt;
    }
    std::vector<Shot> shots;
    std::string line;
    for (unsigned number = 1; std::getline(file, line); ++number) {
        const bool blank = line.find_first_not_of(" \t\r") == std::string::npos;
        if (blank || line[line.find_first_not_of(" \t\r")] == '#')
            continue;
        auto shot = parse_shot(base, line, number);
        if (!shot)
            return std::nullopt;
        shots.push_back(std::move(*shot));
    }
    if (shots.empty()) {
        log::error("the shot list {} holds no shots", path.string());
        return std::nullopt;
    }
    return shots;
}
} // namespace space::app
