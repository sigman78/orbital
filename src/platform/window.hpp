#pragma once
#include "core/math.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

// Platform-agnostic desktop window with the events the demo consumes. The
// implementation lives under platform/<os>/ and is the only code that talks
// to the windowing system.
namespace space::platform {

// Keys the demo reacts to. Letters and digits use their ASCII codes so tables
// and range checks stay readable; everything else is above the ASCII range.
enum class Key : std::uint16_t {
    none = 0,
    space = ' ',
    digit_0 = '0',
    a = 'A',
    escape = 0x100,
    shift,
    plus,
    minus,
    f1,
    f2,
    f3,
    f4,
    f5,
    f6,
    f7,
    f8,
    f12,
};

constexpr Key letter_key(char letter) {
    return Key(std::uint16_t(Key::a) + std::uint16_t(letter - 'A'));
}

// Digit value of a key, or nothing for non-digit keys.
constexpr std::optional<unsigned> digit_of(Key key) {
    const auto code = std::uint16_t(key);
    if (code >= std::uint16_t(Key::digit_0) && code < std::uint16_t(Key::digit_0) + 10)
        return code - std::uint16_t(Key::digit_0);
    return std::nullopt;
}

struct MouseDelta {
    float dx = 0, dy = 0; // pixels since last taken
};

struct WindowDesc {
    Extent2D client_size{1280, 720};
    std::string_view title;
};

class Window {
public:
    // Panics if the window cannot be created; there is no demo without one.
    static std::unique_ptr<Window> create(const WindowDesc& desc);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void* native_handle() const; // what the graphics backend needs to attach a swapchain

    // Processes pending events; false once the user has closed the window.
    bool pump_events();
    // Keys pressed since the last pump, without auto-repeat.
    std::span<const Key> key_presses() const;
    // Mouse look is held with the right button; began is true for the pump in which it started.
    bool mouse_look_active() const;
    bool mouse_look_began() const;
    MouseDelta take_mouse_look_delta();
    // Polled state, true only while this window is in the foreground.
    bool key_down(Key key) const;

    bool minimized() const;
    void wait_for_events() const; // blocks until the next event, for a minimized window
    void set_title(std::string_view utf8);

    struct Impl;

private:
    explicit Window(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace space::platform
