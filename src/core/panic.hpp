#pragma once
#include <format>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <utility>

namespace space {

// Reports an unrecoverable failure with its source location, breaks into an
// attached debugger, and terminates the process with a non-zero exit code.
// Used where no caller could do anything useful with the failure: missing
// shaders or assets, GPU initialization, exhausted fixed budgets.
[[noreturn]] void panic(std::string_view message, std::source_location location = std::source_location::current());

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

// Invariant check that stays active in every build configuration.
#define ORBITAL_ASSERT(condition) ((condition) ? static_cast<void>(0) : ::space::panic("assertion failed: " #condition))
