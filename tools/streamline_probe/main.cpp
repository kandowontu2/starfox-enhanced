// Standalone capability and optional evaluated-frame evidence, not a game toggle.
// Requires the separately obtained, user-approved NVIDIA SDK. No SDK binaries
// are copied, installed, or downloaded by this tool.
#include <sl.h>
#include <sl_helpers.h>
#include <dxgi1_2.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "evaluate.hpp"
#include "starfox/render/dlss_native.h"

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

template<class T> T* entry(HMODULE module, const char* name) {
    auto result = reinterpret_cast<T*>(GetProcAddress(module, name));
    if (!result) throw std::runtime_error(std::string("Missing Streamline export: ") + name);
    return result;
}

int wmain(int argc, wchar_t** argv) try {
    if (argc != 2 && argc != 3) {
        std::cerr << "Usage: starfox_streamline_probe <official-sdk-root> [evaluation-output-directory]\n";
        return 2;
    }
    const auto binaries = fs::canonical(fs::path(argv[1]) / "bin/x64");
    struct Module {
        HMODULE handle{};
        ~Module() { if (handle) {char message[512]{};if(starfox_dlss_close_v1(handle,message,sizeof(message))) std::cerr<<message<<'\n';} }
    } module;
    void* opened{};char lifecycle_error[512]{};
    if(starfox_dlss_open_v1(binaries.c_str(),&opened,lifecycle_error,sizeof(lifecycle_error))) throw std::runtime_error(lifecycle_error);
    module.handle=static_cast<HMODULE>(opened);
    auto supported = entry<PFun_slIsFeatureSupported>(module.handle, "slIsFeatureSupported");
    auto requirements = entry<PFun_slGetFeatureRequirements>(module.handle, "slGetFeatureRequirements");
    // Adapter retains the bound device until SDK shutdown.
    ComPtr<ID3D12Device> device;
    sl::FeatureRequirements required{};
    const auto requirement_result = requirements(sl::kFeatureDLSS, required);
    std::cout << "DLSS requirements: " << sl::getResultAsStr(requirement_result)
              << "; required tags=" << required.numRequiredTags << '\n';
    // SDK initialization must precede even adapter enumeration.
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        throw std::runtime_error("DXGI enumeration unavailable");
    bool any_supported = false;
    ComPtr<IDXGIAdapter1> selected;
    for (UINT index = 0;; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        const auto enumerated = factory->EnumAdapters1(index, &adapter);
        if (enumerated == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(enumerated)) throw std::runtime_error("Adapter enumeration failed");
        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) throw std::runtime_error("Adapter description failed");
        sl::AdapterInfo info{};
        info.deviceLUID = reinterpret_cast<uint8_t*>(&desc.AdapterLuid);
        info.deviceLUIDSizeInBytes = sizeof(desc.AdapterLuid);
        const auto support = supported(sl::kFeatureDLSS, info);
        // Hybrid/remote drivers can expose multiple adapters with the same name;
        // feature gating must use this adapter's LUID, never a name substring.
        std::wcout << desc.Description << L" [adapter " << index
                   << L", software=" << ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
                   << L"]: ";
        std::cout << sl::getResultAsStr(support) << '\n';
        any_supported |= support == sl::Result::eOk;
        if (support == sl::Result::eOk && !selected) selected = adapter;
    }
    if (!any_supported) return 5;
    if (FAILED(D3D12CreateDevice(selected.Get(), D3D_FEATURE_LEVEL_12_0,
                                IID_PPV_ARGS(&device))))
        throw std::runtime_error("D3D12 device creation failed on supported adapter");
    if(starfox_dlss_bind_device_v1(module.handle,device.Get(),lifecycle_error,sizeof(lifecycle_error))) throw std::runtime_error(lifecycle_error);
    void* optimal_function{};
    auto get_function = entry<PFun_slGetFeatureFunction>(module.handle, "slGetFeatureFunction");
    const auto function_result = get_function(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", optimal_function);
    if (function_result != sl::Result::eOk || !optimal_function) {
        std::cout << "DLSS device initialization: " << sl::getResultAsStr(function_result) << '\n';
        return 7;
    }
    auto optimal = reinterpret_cast<PFun_slDLSSGetOptimalSettings*>(optimal_function);
    for (const auto mode : {sl::DLSSMode::eMaxQuality, sl::DLSSMode::eBalanced,
                            sl::DLSSMode::eMaxPerformance, sl::DLSSMode::eDLAA}) {
        sl::DLSSOptions options{};
        options.mode = mode;
        options.outputWidth = 1920;
        options.outputHeight = 1080;
        options.colorBuffersHDR = sl::Boolean::eFalse;
        sl::DLSSOptimalSettings settings{};
        const auto queried = optimal(options, settings);
        std::cout << "Mode " << static_cast<unsigned>(mode) << ": "
                  << sl::getResultAsStr(queried) << "; render="
                  << settings.optimalRenderWidth << 'x' << settings.optimalRenderHeight << '\n';
        if (queried != sl::Result::eOk || !settings.optimalRenderWidth || !settings.optimalRenderHeight)
            return 8;
    }
    if (argc == 3) {
        evaluate_dlss(module.handle, device.Get(), fs::path(argv[2]));
    } else {
        std::cout << "Capability only: no DLSS frame evaluated; renderer integration remains required.\n";
    }
    return any_supported ? 0 : 5;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
