#include "platform/process.hpp"

#include "core/panic.hpp"

#include <SDL.h>

namespace space::platform {

void init_process() {}

std::filesystem::path executable_directory() {
    char* base = SDL_GetBasePath();
    panic_if(!base, "cannot locate the executable directory: {}", SDL_GetError());
    const std::filesystem::path directory(base);
    SDL_free(base);
    return directory;
}

} // namespace space::platform
