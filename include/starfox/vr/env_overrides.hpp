#pragma once
#include "sfvr/sfvr_settings.h"
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>

namespace starfox::vr {
// Standard env overrides (steam-frame-vr-port standard, section 7):
// `SFX_VR_<KEY>` for a canonical sfvr settings key, named by
// sfvr_settings_env_name("SFX", key). A variable that is unset, empty or
// unparseable is ignored; a numeric value is clamped to the key's registry
// range. Overrides win over the saved preference without being written to it.
//
// This port honours: haptics (SFX_VR_HAPTICS, 0..1), refresh_rate
// (SFX_VR_REFRESH_RATE, Hz) and timing_gpu (SFX_VR_TIMING_GPU, 0/1). Other
// registry keys are not implemented by this port and are ignored.
using EnvGetter=const char*(*)(const char*);
inline const char* process_getenv(const char* name) noexcept {return std::getenv(name);}

inline std::optional<std::string> env_override_text(const char* key,
    EnvGetter getter=process_getenv) {
    char name[64];
    if(!sfvr_settings_env_name("SFX",key,name,sizeof name)) return std::nullopt;
    const char* value=getter(name);
    if(!value) return std::nullopt;
    std::string text(value);
    const auto first=text.find_first_not_of(" \t\r\n");
    if(first==std::string::npos) return std::nullopt;
    const auto last=text.find_last_not_of(" \t\r\n");
    return text.substr(first,last-first+1);
}
inline std::optional<float> env_override_float(const char* key,
    EnvGetter getter=process_getenv) {
    const auto text=env_override_text(key,getter);
    if(!text) return std::nullopt;
    char* end{};
    const double value=std::strtod(text->c_str(),&end);
    if(end==text->c_str() || *end!='\0' || !std::isfinite(value)) return std::nullopt;
    return sfvr_settings_clamp_float(sfvr_settings_find(key),static_cast<float>(value));
}
inline std::optional<bool> env_override_bool(const char* key,
    EnvGetter getter=process_getenv) {
    const auto text=env_override_text(key,getter);
    if(!text) return std::nullopt;
    const int value=sfvr_settings_parse_bool(text->c_str(),-1);
    if(value<0) return std::nullopt;
    return value!=0;
}
}
