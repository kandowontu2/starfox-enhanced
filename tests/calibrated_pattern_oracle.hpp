#pragma once
#include <array>
#include <cstdint>
// Independent authored screen-pattern specification, not the production
// shader/descriptor helper. Kept in tests for both component and owner oracles.
inline std::uint32_t pattern_oracle_phase(unsigned x,unsigned y,unsigned scale,unsigned layer,
    const std::array<std::array<unsigned,2>,3>& effects) {
    if(layer!=1 && layer!=2) return 0;
    x/=scale;y/=scale;unsigned phase=0,multiplier=1;
    for(const auto& pass:effects) {
        unsigned code=0;
        switch(pass[layer-1]) {
        case 5: {
            constexpr unsigned thresholds[4][4]{{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
            code=1+thresholds[y%4][x%4];break;
        }
        case 10:case 25:code=17+y%2;break;
        case 70:code=19+x%3+3*(y%3==2);break;
        }
        phase+=code*multiplier;multiplier*=32;
    }
    return phase?phase+65536*layer:0;
}
