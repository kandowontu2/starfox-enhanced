#pragma once
#include "starfox/vr/startup_menu.hpp"
#include <filesystem>
#include <fstream>

namespace starfox::vr {
// Share the application's actual on-disk loader with the regression tests.
// The current serializer bounds the read; restore_preferences owns version
// and field validation, including migration of shorter legacy records.
inline bool load_startup_preferences(StartupMenu& menu,const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    if(!input) return false;
    std::array<uint8_t,std::tuple_size_v<decltype(menu.preferences())>+1> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    if(input.bad()) return false;
    return menu.restore_preferences(std::span<const uint8_t>(bytes.data(),
        static_cast<size_t>(input.gcount())));
}
}
