#pragma once

#include <SDL3/SDL.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace starfox::app {

// SDL's Android GPU window claim goes straight to vkCreateAndroidSurfaceKHR;
// unlike the GLES renderer it does not recreate the window. Destroying only
// the GLES renderer releases its context, but leaves the window's EGLSurface
// connected to ANativeWindow. Vulkan must receive a fresh, unconnected window.
inline bool android_renderer_window_needs_reset(SDL_WindowFlags flags,
    std::string_view requested_renderer) noexcept {
    return (flags & SDL_WINDOW_OPENGL) != 0 && requested_renderer == "gpu";
}

struct AndroidRendererWindowIo {
    decltype(&SDL_CreateWindow) create{SDL_CreateWindow};
};

// The renderer and all its resource owners must already have been released.
// Android only allows one SDL window, so destroy before create (the Windows
// flip-swapchain helper intentionally uses the opposite order). Null out the
// caller's handle first, including on creation failure, to prevent double free.
inline void reset_android_renderer_window(SDL_Window*& window,
    AndroidRendererWindowIo io = {}) {
    int x{}, y{}, width{}, height{};
    if (!SDL_GetWindowPosition(window, &x, &y)
        || !SDL_GetWindowSize(window, &width, &height))
        throw std::runtime_error{std::string{"Read Android renderer window: "} + SDL_GetError()};
    const std::string title{SDL_GetWindowTitle(window)};
    const auto flags = SDL_GetWindowFlags(window);
    const auto preserved = flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE
        | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIGH_PIXEL_DENSITY
        | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_HIDDEN);
    SDL_DestroyWindow(std::exchange(window, nullptr));
    window = io.create(title.c_str(), width, height, preserved);
    if (!window)
        throw std::runtime_error{std::string{"Recreate Android renderer window: "} + SDL_GetError()};
    if (!(preserved & SDL_WINDOW_FULLSCREEN)) SDL_SetWindowPosition(window, x, y);
    SDL_SyncWindow(window);
}

} // namespace starfox::app
