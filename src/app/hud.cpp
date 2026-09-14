#include "app/hud.hpp"

#include "platform/text.hpp"

namespace space::assets {
namespace {

constexpr Extent2D hud_size{1024, 256};

constexpr platform::TextItem hud_text[] = {
    {.text = "ORBITAL", .x = 4, .y = 4, .pixel_height = 40, .bold = true},
    {.text = "A STUDY OF DISTANT WORLDS", .x = 6, .y = 56, .pixel_height = 14},
    {.text = "01 / TERRA     02 / JOVIAN     03 / SELENE     04 / ARES", .x = 6, .y = 124, .pixel_height = 14},
    {.text = "RMB + WASD  NAVIGATE     1-6  VIEWS     T  TOUR     F12  PANEL", .x = 6, .y = 174, .pixel_height = 13},
    {.text = "SPACE  PAUSE     F2  QUALITY     +/-  EXPOSURE     F1  HIDE", .x = 6, .y = 202, .pixel_height = 13},
};

constexpr platform::RuleItem hud_rules[] = {{.x0 = 6, .y = 94, .x1 = 256}};

} // namespace

Rgba8Image make_hud() {
    return {hud_size, platform::rasterize_overlay({.size = hud_size, .text = hud_text, .rules = hud_rules})};
}

} // namespace space::assets
