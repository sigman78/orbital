#include "app/options.hpp"
#include <cassert>
#include <initializer_list>

using namespace space::app;

int main() {
    const auto parse = [](std::initializer_list<const char*> args) {
        return parse_options(int(args.size()), args.begin());
    };
    const auto defaults = parse({"orbital"});
    assert(defaults && defaults->size.width == 1600 && defaults->bookmark == -1);
    const auto sample = parse({"orbital", "--width",    "320",  "--height",       "4320",    "--taa",
                               "0",       "--spatial",  "2",    "--pan",          "-.25",    "--galaxy-view",
                               "-180",    "--bookmark", "5",    "--capture",      "a b.png", "--tour",
                               "--high",  "--no-hud",   "--ui", "--belt-sun-view"});
    assert(sample && sample->size.width == 320 && sample->size.height == 4320);
    assert(sample->taa == 0 && sample->spatial == 2 && sample->pan == -.25f);
    assert(sample->galaxy_view == -180 && sample->capture == "a b.png");
    assert(sample->tour && sample->high && sample->no_hud && sample->ui && sample->belt_sun_view);
    assert(parse({"orbital", "--width", "7680", "--height", "200", "--time", "-1"}));
    assert(!parse({"orbital", "--width", "319"}));
    assert(!parse({"orbital", "--height", "4321"}));
    assert(!parse({"orbital", "--unknown"}));
    assert(!parse({"orbital", "--frames"}));
    assert(!parse({"orbital", "--frames", "12x"}));
    assert(!parse({"orbital", "--frames", "-1"}));
    assert(!parse({"orbital", "--frames", "999999999999999999999999"}));
    assert(!parse({"orbital", "--capture", ""}));
    assert(parse({"orbital", "--bookmark", "7"}));
    assert(!parse({"orbital", "--bookmark", "8"}));
    assert(!parse({"orbital", "--back", "-1"}));
    assert(!parse({"orbital", "--fov-div", ".5"}));
    // Shot lines: keys as the options without dashes over the base, flags bare, commas for several values.
    const auto base = parse(
        {"orbital", "--width", "960", "--height", "540", "--frames", "80", "--capture", "base.png"});
    assert(base);
    const auto shot = parse_shot(*base, "name=mars bookmark=3 taa=0 high sun-at=0.7,-0.7 capture=m.png back=25", 4);
    assert(shot && shot->name == "mars" && shot->options.bookmark == 3 && shot->options.taa == 0);
    assert(shot->options.high && shot->options.sun_at && (*shot->options.sun_at)[0] == .7 &&
           (*shot->options.sun_at)[1] == -.7);
    assert(shot->options.capture == "m.png" && shot->options.back == 25);
    assert(shot->options.size.width == 960 && shot->options.frame_limit == 80); // the base's defaults
    const auto unnamed = parse_shot(*base, "bookmark=1", 7);
    assert(unnamed && unnamed->name == "shot 7" && unnamed->options.capture.empty()); // outputs are per shot
    assert(!parse_shot(*base, "# a comment", 1) && !parse_shot(*base, "   ", 2));
    assert(!parse_shot(*base, "bookmark=9", 3) && !parse_shot(*base, "unknown=1", 3) && !parse_shot(*base, "name=", 3));
    assert(!parse({"orbital", "--duration", "-1"}));
    assert(!parse({"orbital", "--exposure", "0"}));
    for (const char* flag : {"--taa", "--dust", "--disc", "--vsync"})
        assert(!parse({"orbital", flag, "2"}));
    for (const char* flag : {"--spatial", "--tone", "--galaxy"})
        assert(!parse({"orbital", flag, "3"}));
    assert(!parse({"orbital", "--splat", "4"}));
    for (const char* flag : {"--pan", "--lod-scale", "--time", "--duration", "--exposure", "--galaxy-view"})
        for (const char* value : {"nan", "inf", "-inf"})
            assert(!parse({"orbital", flag, value}));
    const auto repeated = parse({"orbital", "--pan", "1", "--pan", "2"});
    assert(repeated && repeated->pan == 2); // last occurrence still wins
}
