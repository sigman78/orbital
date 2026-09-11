#pragma once
#include <source_location>
#include <string_view>

namespace space {

// Reports an unrecoverable failure with its source location, breaks into an
// attached debugger, and terminates the process with a non-zero exit code.
// Used where no caller could do anything useful with the failure: missing
// shaders or assets, GPU initialization, exhausted fixed budgets.
[[noreturn]] void panic(std::string_view message, std::source_location location = std::source_location::current());

// panic() when the condition holds. Reads better than an if/panic pair for
// checks with a constant message; keep the pair when the message needs
// std::format, so nothing is formatted on the success path.
inline void panic_if(bool condition, std::string_view message,
                     std::source_location location = std::source_location::current()) {
    if (condition)
        panic(message, location);
}

} // namespace space

// Invariant check that stays active in every build configuration.
#define ORBITAL_ASSERT(condition) ((condition) ? static_cast<void>(0) : ::space::panic("assertion failed: " #condition))
