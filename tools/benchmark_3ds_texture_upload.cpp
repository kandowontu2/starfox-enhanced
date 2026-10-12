// Asset-free CPU microbenchmark. This is not ARM hardware or game FPS proof.
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>

namespace {
using namespace starfox::platform::nintendo_3ds;
using Clock=std::chrono::steady_clock;
// Exact predecessor traversal, retained only here for alternating A/B timing.
void reference_pack(PicaImage source,std::span<std::uint8_t> destination) {
    const auto layout=pica_texture_layout(source);
    unsigned texel=0;
    for(unsigned ty=0;ty<layout.height;ty+=8) {
        std::array<const std::uint8_t*,8> rows{};
        for(unsigned y=0;y<8;++y) {
            const unsigned sy=source.repeat?(ty+y)&(source.height-1):std::min(ty+y,source.height-1);
            rows[y]=source.pixels.data()+std::size_t(sy)*source.pitch;
        }
        for(unsigned tx=0;tx<layout.width;tx+=8) {
            std::array<unsigned,8> columns{};
            for(unsigned x=0;x<8;++x)
                columns[x]=(source.repeat?(tx+x)&(source.width-1):std::min(tx+x,source.width-1))*source.channels;
            for(const auto& xy:detail::pica_morton_coordinates)
                detail::pica_store_abgr(destination.data()+4*texel++,rows[xy[1]]+columns[xy[0]],source.channels);
        }
    }
}
template<class Pack>
double time(PicaImage image,std::span<std::uint8_t> output,Pack pack) {
    const auto begin=Clock::now();
    for(unsigned i=0;i<1000;++i) pack(image,output);
    return std::chrono::duration<double,std::milli>(Clock::now()-begin).count()/1000;
}
}
int main() {
    try {
        std::cout<<std::fixed<<std::setprecision(4);
        for(unsigned channels:{3U,4U}) for(auto size:{std::array{320U,240U},std::array{400U,240U},std::array{256U,256U}}) {
            const unsigned pitch=size[0]*channels+7;
            std::vector<std::uint8_t> source(std::size_t(pitch)*size[1]+1);
            for(std::size_t i=0;i<source.size();++i) source[i]=std::uint8_t(i*37+i/113);
            const PicaImage image{std::span(source).subspan(1),size[0],size[1],pitch,channels};
            const auto layout=pica_texture_layout(image);
            std::vector<std::uint8_t> reference(layout.bytes),candidate(layout.bytes);
            reference_pack(image,reference);pack_pica_texture(image,candidate);
            if(candidate!=reference) throw std::runtime_error("Packed bytes differ from predecessor");
            std::array<double,9> ratios{};double old_total=0,new_total=0;
            for(unsigned pair=0;pair<ratios.size();++pair) {
                double old_time,new_time;
                if(pair%2) {new_time=time(image,candidate,pack_pica_texture);old_time=time(image,reference,reference_pack);}
                else {old_time=time(image,reference,reference_pack);new_time=time(image,candidate,pack_pica_texture);}
                if(candidate!=reference) throw std::runtime_error("Timed packing changed bytes");
                old_total+=old_time;new_total+=new_time;ratios[pair]=old_time/new_time;
            }
            std::sort(ratios.begin(),ratios.end());
            std::cout<<size[0]<<'x'<<size[1]<<" channels="<<channels
                <<" reference_ms="<<old_total/ratios.size()<<" candidate_ms="<<new_total/ratios.size()
                <<" median_speedup="<<ratios[4]<<" min_speedup="<<ratios.front()<<'\n';
        }
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
