# The extracted dependency is nested inside our Git checkout. `git apply`
# there can silently skip paths outside that subtree, even with exit code 0.
# Use exact, guarded source anchors for this pinned two-insertion extension.
set(backend "${SOURCE_DIR}/src/gpu/d3d12/SDL_gpu_d3d12.c")
file(READ "${backend}" code)
# Native display runtimes return an exact adapter LUID. Selecting SDL's
# default/high-performance adapter can target a different GPU on laptops.
# These opt-in create properties leave all ordinary device selection intact.
string(FIND "${code}" "starfox.d3d12.require_adapter_luid" luid_installed)
if(luid_installed LESS 0)
    set(luid_anchor "    // Get information about the selected adapter. Used for logging info.")
    string(FIND "${code}" "${luid_anchor}" luid_found)
    if(luid_found LESS 0)
        message(FATAL_ERROR "Pinned SDL D3D12 adapter selection anchor missing")
    endif()
    set(luid_selection [=[
    if (SDL_GetBooleanProperty(props, "starfox.d3d12.require_adapter_luid", false)) {
        const Uint32 required_low = (Uint32)SDL_GetNumberProperty(props, "starfox.d3d12.adapter_luid_low", 0);
        const Sint32 required_high = (Sint32)SDL_GetNumberProperty(props, "starfox.d3d12.adapter_luid_high", 0);
        IDXGIAdapter1 *candidate = NULL;
        bool found = false;
        IDXGIAdapter1_Release(renderer->adapter);
        renderer->adapter = NULL;
        for (UINT index = 0; SUCCEEDED(IDXGIFactory4_EnumAdapters1(renderer->factory, index, &candidate)); ++index) {
            DXGI_ADAPTER_DESC1 description;
            if (SUCCEEDED(IDXGIAdapter1_GetDesc1(candidate, &description))
                && description.AdapterLuid.LowPart == required_low && description.AdapterLuid.HighPart == required_high) {
                renderer->adapter = candidate;
                found = true;
                break;
            }
            IDXGIAdapter1_Release(candidate);
            candidate = NULL;
        }
        if (!found) {
            D3D12_INTERNAL_DestroyRenderer(renderer);
            SDL_SetError("Required native display D3D12 adapter is unavailable");
            return NULL;
        }
    }

]=])
    string(REPLACE "${luid_anchor}" "${luid_selection}${luid_anchor}" code "${code}")
    file(WRITE "${backend}" "${code}")
endif()
string(FIND "${code}" "starfox_present_hooks" present_hooks_installed)
if(present_hooks_installed LESS 0)
    set(create_anchor "    CHECK_D3D12_ERROR_AND_RETURN(\"Could not create IDXGISwapChain3\", false);")
    set(destroy_anchor "    IDXGISwapChain_Release(windowData->swapchain);\n    windowData->swapchain = NULL;")
    string(FIND "${code}" "${create_anchor}" create_found)
    string(FIND "${code}" "${destroy_anchor}" destroy_found)
    if(create_found LESS 0 OR destroy_found LESS 0)
        message(FATAL_ERROR "Pinned SDL swapchain hook anchors missing")
    endif()
    set(hook_lookup "    const StarfoxSdlD3D12PresentHooksV1 *starfox_present_hooks = (const StarfoxSdlD3D12PresentHooksV1 *)SDL_GetPointerProperty(SDL_GetGlobalProperties(), STARFOX_SDL_D3D12_PRESENT_HOOKS, NULL);")
    string(REPLACE "static void D3D12_INTERNAL_DestroySwapchain(" "#include \"starfox/render/sdl_d3d12_bridge.h\"\nstatic void D3D12_INTERNAL_DestroySwapchain(" code "${code}")
    string(REPLACE "${create_anchor}" "${create_anchor}\n${hook_lookup}\n    if (starfox_present_hooks && starfox_present_hooks->version == 1 && starfox_present_hooks->swapchain)\n        starfox_present_hooks->swapchain(starfox_present_hooks->user, renderer->device, (void **)&swapchain3, false);" code "${code}")
    string(REPLACE "${destroy_anchor}" "${hook_lookup}\n    if (starfox_present_hooks && starfox_present_hooks->version == 1 && starfox_present_hooks->swapchain)\n        starfox_present_hooks->swapchain(starfox_present_hooks->user, renderer->device, (void **)&windowData->swapchain, true);\n${destroy_anchor}" code "${code}")
    file(WRITE "${backend}" "${code}")
endif()
set(anchor "static SDL_GPUDevice *D3D12_CreateDevice(bool debugMode, bool preferLowPower, SDL_PropertiesID props)")
set(tail "    renderer->sdlGPUDevice = result;\n\n    return result;")
set(inclusion "#include \"sdl_d3d12_bridge.inc\"")
set(properties "    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_DEVICE, renderer->device);\n    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_BRIDGE, (void *)&starfox_d3d12_bridge);")
string(FIND "${code}" "${inclusion}" included)
string(FIND "${code}" "${properties}" installed)
set(texture_property "    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_TEXTURE_BRIDGE, (void *)&starfox_d3d12_texture_bridge);")
set(compute_property "    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_COMPUTE_BRIDGE, (void *)&starfox_d3d12_compute_bridge);")
set(present_property "    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_PRESENT_BRIDGE, (void *)&starfox_d3d12_present_bridge);")
set(xr_property "    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_XR_BRIDGE, (void *)&starfox_d3d12_xr_bridge);")
set(timestamps_property "    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_TIMESTAMPS, (void *)&starfox_d3d12_timestamps);")
set(weave_property "    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_OWNED_WEAVE, (void *)&starfox_d3d12_owned_weave_bridge);")
if(included GREATER_EQUAL 0 AND installed GREATER_EQUAL 0)
    string(FIND "${code}" "STARFOX_SDL_D3D12_OWNED_WEAVE," weave_installed)
    if(weave_installed LESS 0)
        string(REPLACE "${properties}" "${properties}\n${weave_property}" code "${code}")
        file(WRITE "${backend}" "${code}")
    endif()
    string(FIND "${code}" "STARFOX_SDL_D3D12_TIMESTAMPS," timestamps_installed)
    if(timestamps_installed LESS 0)
        string(REPLACE "${properties}" "${properties}\n${timestamps_property}" code "${code}")
        file(WRITE "${backend}" "${code}")
    endif()
    string(FIND "${code}" "STARFOX_SDL_D3D12_GEOMETRY_BRIDGE," geometry_installed)
    if(geometry_installed LESS 0)
        string(REPLACE "${properties}" "${properties}\n    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_GEOMETRY_BRIDGE, (void *)&starfox_d3d12_geometry_bridge);" code "${code}")
        file(WRITE "${backend}" "${code}")
    endif()
    string(FIND "${code}" "STARFOX_SDL_D3D12_TEXTURE_BRIDGE," texture_installed)
    if(texture_installed LESS 0)
        string(REPLACE "${properties}" "${properties}\n${texture_property}" code "${code}")
        file(WRITE "${backend}" "${code}")
    endif()
    string(FIND "${code}" "STARFOX_SDL_D3D12_COMPUTE_BRIDGE," compute_installed)
    if(compute_installed LESS 0)
        string(REPLACE "${properties}" "${properties}\n${compute_property}" code "${code}")
        file(WRITE "${backend}" "${code}")
    endif()
    string(FIND "${code}" "STARFOX_SDL_D3D12_PRESENT_BRIDGE," present_installed)
    if(present_installed LESS 0)
        string(REPLACE "${properties}" "${properties}\n${present_property}" code "${code}")
        file(WRITE "${backend}" "${code}")
    endif()
    string(FIND "${code}" "STARFOX_SDL_D3D12_XR_BRIDGE," xr_installed)
    if(xr_installed LESS 0)
        string(REPLACE "${properties}" "${properties}\n${xr_property}" code "${code}")
        file(WRITE "${backend}" "${code}")
    endif()
    return()
endif()
string(FIND "${code}" "${anchor}" found_anchor)
string(FIND "${code}" "${tail}" found_tail)
if(included GREATER_EQUAL 0 OR installed GREATER_EQUAL 0 OR found_anchor LESS 0 OR found_tail LESS 0)
    message(FATAL_ERROR "SDL D3D12 backend is not the expected pinned source or is partially patched")
endif()
string(REPLACE "${anchor}" "${inclusion}\n\n${anchor}" code "${code}")
string(REPLACE "${tail}" "    renderer->sdlGPUDevice = result;\n\n${properties}\n    SDL_SetPointerProperty(renderer->props, STARFOX_SDL_D3D12_GEOMETRY_BRIDGE, (void *)&starfox_d3d12_geometry_bridge);\n    return result;" code "${code}")
string(REPLACE "${properties}" "${properties}\n${texture_property}\n${compute_property}\n${present_property}\n${xr_property}\n${timestamps_property}\n${weave_property}" code "${code}")
file(WRITE "${backend}" "${code}")
