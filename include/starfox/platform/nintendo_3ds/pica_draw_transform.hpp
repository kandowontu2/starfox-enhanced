#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include <cstring>

namespace starfox::platform::nintendo_3ds {
// Adjacent draws often share a camera-space transform. The plan and eye must
// remain fixed until reset(); the native owner resets after each eye's backdrop
// seed, and never uses this cache across frames, shader binds or LCD targets.
// Skip only redundant matrix calculation/upload, never a draw or source face.
class PicaDrawTransform {
public:
    void reset() noexcept {valid_=false;}
    template<class Compute,class Upload>
    bool prepare(const PicaDraw& draw,Compute compute,Upload upload) {
        static_assert(sizeof(PicaMatrix)==16*sizeof(float));
        if(valid_ && space_==draw.space
            && std::memcmp(model_.data(),draw.model.data(),sizeof(PicaMatrix))==0)
            return false;
        // Failure must not leave a valid key after a partially changed uniform.
        valid_=false;
        const auto matrix=compute();
        upload(matrix);
        model_=draw.model;space_=draw.space;valid_=true;
        return true;
    }
    [[nodiscard]] bool valid() const noexcept {return valid_;}
private:
    PicaMatrix model_{};
    PicaSpace space_{};
    bool valid_{};
};
} // namespace starfox::platform::nintendo_3ds
