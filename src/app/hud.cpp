#include "hud.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>

namespace space::assets {
namespace {
struct SelectFont {
    HDC dc = nullptr;
    HFONT old = nullptr;
    HFONT font = nullptr;
    SelectFont(HDC d, int px, int weight) : dc(d) {
        font = CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, L"Segoe UI");
        if (font)
            old = static_cast<HFONT>(SelectObject(dc, font));
    }
    ~SelectFont() {
        if (old)
            SelectObject(dc, old);
        if (font)
            DeleteObject(font);
    }
    SelectFont(const SelectFont&) = delete;
};
void text(HDC dc, int x, int y, const wchar_t* s) {
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    TextOutW(dc, x, y, s, static_cast<int>(lstrlenW(s)));
}
} // namespace

Image make_hud() {
    constexpr int width = 1024, height = 256;
    Image image{width, height, std::vector<std::uint8_t>(width * height * 4, 0)};
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old_bitmap = bitmap ? SelectObject(dc, bitmap) : nullptr;
    if (bits)
        std::fill_n(static_cast<std::uint32_t*>(bits), width * height, 0u);
    if (bitmap) {
        {
            SelectFont font(dc, 40, 600);
            text(dc, 4, 4, L"ORBITAL");
        }
        {
            SelectFont font(dc, 14, 400);
            text(dc, 6, 56, L"A STUDY OF DISTANT WORLDS");
            text(dc, 6, 124, L"01 / TERRA     02 / JOVIAN     03 / SELENE");
        }
        {
            SelectFont font(dc, 13, 400);
            text(dc, 6, 174, L"RMB + WASD  NAVIGATE     1-3  WORLDS     T  TOUR");
            text(dc, 6, 202, L"SPACE  PAUSE     F2  QUALITY     +/-  EXPOSURE     F1  HIDE");
        }
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(96, 96, 96));
        HGDIOBJ old_pen = SelectObject(dc, pen);
        MoveToEx(dc, 6, 94, nullptr);
        LineTo(dc, 256, 94);
        SelectObject(dc, old_pen);
        DeleteObject(pen);
        GdiFlush();
        const auto* source = static_cast<const std::uint32_t*>(bits);
        for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i) {
            const std::uint8_t coverage = static_cast<std::uint8_t>((source[i] & 0xffu) * 77u / 255u +
                                                                    ((source[i] & 0xffu) ? 178u : 0u));
            image.pixels[i * 4 + 0] = image.pixels[i * 4 + 1] = image.pixels[i * 4 + 2] = 255;
            image.pixels[i * 4 + 3] = coverage;
        }
    }
    if (old_bitmap)
        SelectObject(dc, old_bitmap);
    if (bitmap)
        DeleteObject(bitmap);
    if (dc)
        DeleteDC(dc);
    if (screen)
        ReleaseDC(nullptr, screen);
    return image;
}
} // namespace space::assets
#else
namespace space::assets {
Image make_hud() {
    return {};
}
} // namespace space::assets
#endif
