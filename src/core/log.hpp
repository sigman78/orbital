#pragma once
#include <cstdio>
#include <format>
#include <string_view>
#include <utility>

// The only text output layer. Every call writes one line; arguments use
// std::format syntax. info and warn go to stdout, error to stderr.
namespace space::log {

void write(std::FILE* stream, std::string_view prefix, std::string_view line);

template <class... Args> void info(std::format_string<Args...> format, Args&&... args) {
    write(stdout, {}, std::format(format, std::forward<Args>(args)...));
}

template <class... Args> void warn(std::format_string<Args...> format, Args&&... args) {
    write(stdout, "warning: ", std::format(format, std::forward<Args>(args)...));
}

template <class... Args> void error(std::format_string<Args...> format, Args&&... args) {
    write(stderr, "error: ", std::format(format, std::forward<Args>(args)...));
}

} // namespace space::log
