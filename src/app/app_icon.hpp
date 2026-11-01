#pragma once
#include <SDL3/SDL.h>
#include "../../assets/icons/window-icon.hpp"

// Portable desktop builds need no working-directory image files.
inline void starfox_set_window_icon(SDL_Window* window) {
    auto* io = SDL_IOFromConstMem(starfox_window_icon_png, sizeof(starfox_window_icon_png));
    if (!io) return;
    if (auto* image = SDL_LoadPNG_IO(io, true)) {
        SDL_SetWindowIcon(window, image);
        SDL_DestroySurface(image);
    }
}
