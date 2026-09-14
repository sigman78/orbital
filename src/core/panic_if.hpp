#pragma once
#include "core/panic.hpp"
#include <format>
#include <type_traits>
#include <utility>

namespace space {

// A compile-time-checked format string that also remembers where it was
// written, so panic_if can take a parameter pack and still report the caller.
template <class... Args> struct PanicFormat {
    std::format_string<std::type_identity_t<Args>...> format;
    std::source_location location;

    template <class Text>
    consteval PanicFormat(const Text& text, std::source_location where = std::source_location::current())
        : format(text), location(where) {}
};

// panic() with a formatted message when the condition holds. The message is
// only formatted on the failure path; the arguments are evaluated either way,
// so keep an explicit if/panic pair when an argument is expensive to build.
template <class... Args>
void panic_if(bool condition, PanicFormat<std::type_identity_t<Args>...> message, Args&&... args) {
    if (condition)
        panic(std::format(message.format, std::forward<Args>(args)...), message.location);
}

} // namespace space
