#pragma once
#include "platform/window.hpp"

#include <cstdint>

// Platform-internal access to the window: raw OS messages for the overlay
// backend. Nothing outside platform/win32 includes this header.
namespace space::platform::detail {

// Sees every message before the window handles it. Returning true consumes
// the message with *result as the window procedure's return value.
using MessageHook = bool (*)(void* user, void* handle, unsigned message, std::uintptr_t w, std::intptr_t l,
                             std::intptr_t* result);

struct WindowAccess {
    static void set_message_hook(Window& window, MessageHook hook, void* user);
};

} // namespace space::platform::detail
