#pragma once
#include <filesystem>

// Process-level services with one implementation per platform.
namespace space::platform {

// One-time setup before any window exists: DPI awareness and, in debug
// builds on MSVC, routing CRT assertion reports to stderr.
void init_process();

// Directory containing the running executable; panics if it cannot be found.
std::filesystem::path executable_directory();

} // namespace space::platform
