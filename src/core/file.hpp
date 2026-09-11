#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// Whole-file reads and writes over C stdio, keyed on std::filesystem::path so
// Unicode paths work on Windows. Failures are reported through the return
// value; callers decide whether that is fatal.
namespace space::file {

std::optional<std::vector<std::uint8_t>> read(const std::filesystem::path& path);

// Creates missing parent directories. Returns false if anything failed.
bool write(const std::filesystem::path& path, std::span<const std::uint8_t> bytes);
bool write_text(const std::filesystem::path& path, std::string_view text);

} // namespace space::file
