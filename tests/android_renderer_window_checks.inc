namespace {
SDL_WindowID old_android_test_window{};
bool fail_android_test_window{};
unsigned android_test_creations{};

SDL_Window* create_single_android_test_window(const char* title, int width,
    int height, SDL_WindowFlags flags) {
    require(SDL_GetWindowFromID(old_android_test_window) == nullptr,
        "Android replacement was created before its EGL window was destroyed");
    require((flags & (SDL_WINDOW_OPENGL | SDL_WINDOW_VULKAN | SDL_WINDOW_METAL)) == 0,
        "Android replacement inherited a connected graphics surface");
    ++android_test_creations;
    if (fail_android_test_window) {
        SDL_SetError("injected Android window creation failure");
        return nullptr;
    }
    return SDL_CreateWindow(title,width,height,flags);
}

void check_android_renderer_window() {
    using starfox::app::android_renderer_window_needs_reset;
    for (const auto flags : {SDL_WindowFlags{0},SDL_WINDOW_OPENGL,
             SDL_WINDOW_VULKAN,SDL_WINDOW_METAL,
             SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIDDEN}) {
        require(android_renderer_window_needs_reset(flags,"gpu")
                == ((flags & SDL_WINDOW_OPENGL) != 0),
            "Android Vulkan transition missed a retained EGL surface");
        for (const auto renderer : {"opengles2","software","","direct3d12"})
            require(!android_renderer_window_needs_reset(flags,renderer),
                "Android GLES/Software or unrelated renderer reset its window");
    }
    // Actual SDL window lifetime, with a creation seam imposing Android's
    // one-window rule. This is not an Android EGL/Vulkan device test.
    require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),
        "Could not initialize Android window lifetime fixture");
    const starfox::app::AndroidRendererWindowIo io{create_single_android_test_window};
    auto* window=SDL_CreateWindow("Android handoff fixture",320,240,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS);
    require(window!=nullptr,"Could not create Android window lifetime fixture");
    for (unsigned retry=0;retry<4;++retry) {
        old_android_test_window=SDL_GetWindowID(window);
        starfox::app::reset_android_renderer_window(window,io);
        int width{},height{};
        require(window && SDL_GetWindowID(window)!=old_android_test_window
            && SDL_GetWindowSize(window,&width,&height) && width==320 && height==240
            && std::string_view{SDL_GetWindowTitle(window)}=="Android handoff fixture",
            "Android handoff lost its window identity/title/size");
        require((SDL_GetWindowFlags(window) & (SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE
                | SDL_WINDOW_BORDERLESS))
                == (SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS),
            "Android handoff lost presentation flags");
    }
    SDL_DestroyWindow(window);
    window=SDL_CreateWindow("Android fullscreen fixture",320,240,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIDDEN);
    require(window!=nullptr,"Could not create Android fullscreen lifetime fixture");
    old_android_test_window=SDL_GetWindowID(window);
    starfox::app::reset_android_renderer_window(window,io);
    require((SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIDDEN))
            == (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIDDEN),
        "Android handoff lost fullscreen/hidden presentation");
    const auto journal_root=std::filesystem::temp_directory_path()
        / ("sfe-android-window-"+std::to_string(SDL_GetTicksNS()));
    require(std::filesystem::create_directory(journal_root),
        "Could not create Android window recovery journal fixture");
    starfox::app::GpuLaunchGuard guard{journal_root/"pregame.cfg","pending"};
    require(guard.arm(),"Could not arm Android window recovery fixture");
    old_android_test_window=SDL_GetWindowID(window);
    fail_android_test_window=true;
    bool caught=false;
    try {starfox::app::reset_android_renderer_window(window,io);}
    catch (const std::runtime_error& error) {
        caught=std::string_view{error.what()}.find("injected Android window creation failure")
            !=std::string_view::npos;
    }
    require(caught && window==nullptr && android_test_creations==6,
        "Failed Android handoff retained a destroyed window or lost its error");
    require(guard.armed() && guard.needs_safe_start(),
        "Failed Android handoff lost its next-launch Software recovery marker");
    guard.disarm();
    require(std::filesystem::remove(journal_root),
        "Could not clean up Android window recovery fixture");
    SDL_Quit();
    SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
    fail_android_test_window=false;
    std::cout<<"Android renderer handoff: EGL policy, single-window ordering, fullscreen, "
        "creation failure and retained GPU recovery checks passed\n";
}
} // namespace
