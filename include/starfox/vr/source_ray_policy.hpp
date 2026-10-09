#pragma once
#include "starfox/vr/source_models.hpp"
namespace starfox::vr {
// Indicators still draw normally, but aren't physical occluders. Match source
// identities, never broad procedural/texture flags.
class SourceRayPolicy {
public:
    explicit SourceRayPolicy(const assets::SymbolMap& symbols) {
        const auto flashes=symbols.find("FLASH_STRAT");if(!flashes.empty()) flash_=flashes.front();
        const auto reticles=symbols.find("XHAIR2");if(!reticles.empty()) reticle_=uint16_t(reticles.front());
    }
    void apply(SourceModelPackets& packets,const GameSceneSnapshot& snapshot) const {
        const auto nonphysical=[&](uint32_t key) {
            if(key==0x20000 || key==0x30000) return true;
            for(const auto& object:snapshot.objects) if(object.handle==(key&65535))
                return (flash_ && object.object.strategy_address==flash_)
                    || (reticle_ && object.presentation.shape==reticle_);
            return false;
        };
        for(size_t i=0;i<packets.handles.size();++i)
            if(nonphysical(packets.handles[i])) packets.packets[i].geometry={};
        std::erase_if(packets.compute_models,[&](const auto& model){return nonphysical(model.key);});
    }
private:
    uint32_t flash_{};uint16_t reticle_{};
};
}
