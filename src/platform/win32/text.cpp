#include "platform/text.hpp"

#include <windows.h>

#include <algorithm>
#include <string>

namespace space::platform {
namespace {

constexpr const wchar_t* font_face = L"Segoe UI";
constexpr int bold_weight = 600, regular_weight = 400;
constexpr COLORREF rule_color = RGB(96, 96, 96);

std::wstring to_wide(std::string_view utf8) {
    if (utf8.empty())
        return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), wide.data(), length);
    return wide;
}

struct SelectedFont {
    HDC dc;
    HFONT font = nullptr;
    HFONT previous = nullptr;
    SelectedFont(HDC device, int pixel_height, bool bold) : dc(device) {
        font = CreateFontW(-pixel_height, 0, 0, 0, bold ? bold_weight : regular_weight, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE,
                           font_face);
        if (font)
            previous = static_cast<HFONT>(SelectObject(dc, font));
    }
    ~SelectedFont() {
        if (previous)
            SelectObject(dc, previous);
        if (font)
            DeleteObject(font);
    }
    SelectedFont(const SelectedFont&) = delete;
    SelectedFont& operator=(const SelectedFont&) = delete;
};

// A 32-bit top-down DIB bound to a memory device context.
struct Canvas {
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous = nullptr;
    void* bits = nullptr;

    explicit Canvas(Extent2D size) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = LONG(size.width);
        info.bmiHeader.biHeight = -LONG(size.height);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bitmap)
            previous = SelectObject(dc, bitmap);
        if (bits)
            std::fill_n(static_cast<std::uint32_t*>(bits), std::size_t(size.width) * size.height, 0u);
    }
    ~Canvas() {
        if (previous)
            SelectObject(dc, previous);
        if (bitmap)
            DeleteObject(bitmap);
        if (dc)
            DeleteDC(dc);
        if (screen)
            ReleaseDC(nullptr, screen);
    }
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;
};

} // namespace

Bytes rasterize_overlay(const OverlaySpec& spec) {
    const std::size_t pixel_count = std::size_t(spec.size.width) * spec.size.height;
    Bytes rgba(pixel_count * 4, 0);
    Canvas canvas(spec.size);
    if (!canvas.bits)
        return rgba;
    SetTextColor(canvas.dc, RGB(255, 255, 255));
    SetBkMode(canvas.dc, TRANSPARENT);
    for (const auto& item : spec.text) {
        const SelectedFont font(canvas.dc, item.pixel_height, item.bold);
        const std::wstring text = to_wide(item.text);
        TextOutW(canvas.dc, item.x, item.y, text.c_str(), int(text.size()));
    }
    if (!spec.rules.empty()) {
        HPEN pen = CreatePen(PS_SOLID, 1, rule_color);
        HGDIOBJ previous = SelectObject(canvas.dc, pen);
        for (const auto& rule : spec.rules) {
            MoveToEx(canvas.dc, rule.x0, rule.y, nullptr);
            LineTo(canvas.dc, rule.x1, rule.y);
        }
        SelectObject(canvas.dc, previous);
        DeleteObject(pen);
    }
    GdiFlush();
    // The blue channel of the ClearType output is the coverage; lift anything
    // touched so thin strokes stay readable over bright scenery.
    const auto* source = static_cast<const std::uint32_t*>(canvas.bits);
    for (std::size_t i = 0; i < pixel_count; ++i) {
        const std::uint32_t blue = source[i] & 0xffu;
        const auto coverage = static_cast<std::uint8_t>(blue * 77u / 255u + (blue ? 178u : 0u));
        rgba[i * 4 + 0] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = coverage;
    }
    return rgba;
}

} // namespace space::platform
