#include "app/options.hpp"
#include "app/camera.hpp"
#include "core/log.hpp"
#include <charconv>
#include <cmath>
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
    return size_ok && time_ok && exposure_ok && bookmark_ok;
}

} // namespace

std::optional<Options> parse_options(int argc, const char* const* argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const auto value = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= argc)
                return std::nullopt;
            return std::string_view(argv[++i]);
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
        else if (arg == "--tone")
            ok = parse_choice(value(), options.tone, unsigned(render::ToneCurve::Count));
        else if (arg == "--galaxy-view") {
            auto& longitude = options.galaxy_view.emplace();
            ok = parse_number(value(), longitude) && longitude >= -180 && longitude <= 180;
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
        else {
            log::error("unknown option {}", arg);
            return std::nullopt;
        }
        if (!ok) {
            log::error("missing or invalid value for {}", arg);
            return std::nullopt;
        }
    }
    if (!options_valid(options)) {
        log::error("invalid dimensions, time, duration, exposure, or bookmark (expected 0..{})", bookmark_count - 1);
        return std::nullopt;
    }
    return options;
}

} // namespace space::app
