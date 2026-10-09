#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
// Independent authored edge/grid specification. Read PRE-style byte colours,
// never infer branches from the styled output or the production shader/helper.
inline constexpr std::array<unsigned,13> edge_oracle_styles{1,2,3,6,12,13,34,37,39,41,66,67,68};
inline bool edge_oracle_style(unsigned effect) {
    return std::find(edge_oracle_styles.begin(),edge_oracle_styles.end(),effect)!=edge_oracle_styles.end();
}
inline unsigned edge_oracle_branch(std::span<const unsigned char> rgb,std::span<const unsigned char> layers,
    unsigned width,unsigned height,unsigned x,unsigned y,unsigned step,unsigned effect) {
    if(!edge_oracle_style(effect)) return 0;
    const auto index=[&](int px,int py) {return unsigned(std::clamp(py,0,int(height)-1))*width+unsigned(std::clamp(px,0,int(width)-1));};
    const auto light=[&](unsigned p) {return (int(rgb[p*4])*77+int(rgb[p*4+1])*150+int(rgb[p*4+2])*29)/256;};
    const int centre=light(y*width+x);bool edge=false;
    if(effect==1 || effect==12) {
        for(const auto delta:{std::array{-1,0},std::array{1,0},std::array{0,-1},std::array{0,1}}) {
            const unsigned p=index(int(x)+delta[0]*int(step),int(y)+delta[1]*int(step));
            edge|=layers[p]!=0 && centre-light(p)>40;
        }
    } else edge=std::abs(centre-light(index(int(x)+int(step),int(y))))>28
        || std::abs(centre-light(index(int(x),int(y)+int(step))))>28;
    const bool grid=effect==6?(!edge && (x/step%16==0 || y/step%16==0)):
        effect==39 && y/step%4==3;
    return 1+2*unsigned(edge)+4*unsigned(grid);
}
inline std::uint32_t edge_oracle_key(unsigned bits,unsigned layer,unsigned present) {
    if(!present || (layer!=1 && layer!=2)) return 0;
    return (bits&present)==present && (bits&~(present*7U))==0?bits+layer*1024:UINT32_MAX;
}
