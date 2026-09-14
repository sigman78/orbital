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
    assert(!parse({"orbital", "--bookmark", "6"}));
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
