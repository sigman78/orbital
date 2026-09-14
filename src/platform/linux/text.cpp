#include "platform/text.hpp"

#include "core/panic_if.hpp"

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>

namespace space::platform {
namespace {

struct Font {
    FT_Library library = nullptr;
    FT_Face regular = nullptr, bold = nullptr;

    FT_Face match(bool heavy) {
        FcPattern* pattern = FcNameParse(reinterpret_cast<const FcChar8*>(heavy ? "sans:weight=bold" : "sans"));
        panic_if(!pattern, "cannot create the HUD font pattern");
        FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
        FcDefaultSubstitute(pattern);
        FcResult result;
        FcPattern* matched = FcFontMatch(nullptr, pattern, &result);
        FcPatternDestroy(pattern);
        panic_if(!matched, "cannot find a sans-serif HUD font");
        FcChar8* path = nullptr;
        int face_index = 0;
        panic_if(FcPatternGetString(matched, FC_FILE, 0, &path) != FcResultMatch, "HUD font has no file");
        FcPatternGetInteger(matched, FC_INDEX, 0, &face_index);
        FT_Face face = nullptr;
        const auto error = FT_New_Face(library, reinterpret_cast<const char*>(path), face_index, &face);
        FcPatternDestroy(matched);
        panic_if(error != 0, "cannot load the HUD font: FreeType error {}", error);
        return face;
    }

    Font() {
        panic_if(FT_Init_FreeType(&library) != 0, "cannot initialize FreeType");
        regular = match(false);
        bold = match(true);
    }
    ~Font() {
        FT_Done_Face(bold);
        FT_Done_Face(regular);
        FT_Done_FreeType(library);
    }
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
};

// Invalid UTF-8 consumes one byte and draws the replacement glyph.
std::uint32_t next_codepoint(std::string_view& text) {
    const auto first = static_cast<unsigned char>(text.front());
    unsigned length = first < 0x80                     ? 1
                      : first >= 0xc2 && first < 0xe0  ? 2
                      : first < 0xf0 && first >= 0xe0  ? 3
                      : first >= 0xf0 && first <= 0xf4 ? 4
                                                       : 0;
    std::uint32_t code = length == 1 ? first : first & ((1u << (7 - length)) - 1);
    bool valid = length != 0 && text.size() >= length;
    for (unsigned i = 1; valid && i < length; ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        valid = (byte & 0xc0) == 0x80;
        code = (code << 6) | (byte & 0x3f);
    }
    valid = valid && !(length == 2 && code < 0x80) && !(length == 3 && code < 0x800) &&
            !(length == 4 && code < 0x10000) && code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff);
    text.remove_prefix(valid ? length : 1);
    return valid ? code : 0xfffd;
}

} // namespace

Bytes rasterize_overlay(const OverlaySpec& spec) {
    static Font font;
    Bytes rgba(std::size_t(spec.size.width) * spec.size.height * 4, 255);
    for (std::size_t i = 3; i < rgba.size(); i += 4)
        rgba[i] = 0;
    const auto cover = [&](int x, int y, std::uint8_t alpha) {
        if (x >= 0 && y >= 0 && unsigned(x) < spec.size.width && unsigned(y) < spec.size.height) {
            auto& destination = rgba[(std::size_t(y) * spec.size.width + unsigned(x)) * 4 + 3];
            destination = std::max(destination, alpha);
        }
    };
    for (const auto& item : spec.text) {
        FT_Face face = item.bold ? font.bold : font.regular;
        if (FT_Set_Pixel_Sizes(face, 0, unsigned(std::max(item.pixel_height, 1))) != 0)
            continue;
        int pen = item.x;
        const int baseline = item.y + int(face->size->metrics.ascender >> 6);
        auto text = item.text;
        while (!text.empty()) {
            if (FT_Load_Char(face, next_codepoint(text), FT_LOAD_RENDER) != 0)
                continue;
            const auto& glyph = *face->glyph;
            for (unsigned y = 0; y < glyph.bitmap.rows; ++y)
                for (unsigned x = 0; x < glyph.bitmap.width; ++x) {
                    const auto alpha = glyph.bitmap.buffer[int(y) * glyph.bitmap.pitch + int(x)];
                    cover(pen + glyph.bitmap_left + int(x), baseline - glyph.bitmap_top + int(y), alpha);
                }
            pen += int(glyph.advance.x >> 6);
        }
    }
    for (const auto& rule : spec.rules)
        for (int x = std::max(rule.x0, 0); x < std::min(rule.x1, int(spec.size.width)); ++x)
            cover(x, rule.y, 192);
    return rgba;
}

} // namespace space::platform
