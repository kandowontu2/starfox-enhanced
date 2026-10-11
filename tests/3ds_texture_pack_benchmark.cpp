#include "starfox/platform/nintendo_3ds/pica_texture_update.hpp"
#include "3ds_texture_pack_baseline.inc"
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace starfox::platform::nintendo_3ds;
struct Fixture {unsigned width,height,channels,pad,offset;bool repeat;};
using Time=std::chrono::steady_clock;
std::uint64_t checksum(std::span<const std::uint8_t> data) {
    std::uint64_t result=1469598103934665603ULL;
    for(auto byte:data)result=(result^byte)*1099511628211ULL;
    return result;
}
struct Result {double seconds{};std::uint64_t checksum{};};
template<class Pack> Result measure(const Fixture& fixture,const std::vector<std::vector<std::uint8_t>>& frames,Pack pack) {
    const unsigned pitch=fixture.width*fixture.channels+fixture.pad;
    const auto image=[&](unsigned frame) {return PicaImage{
        std::span(frames[frame]).subspan(fixture.offset),fixture.width,fixture.height,pitch,fixture.channels,fixture.repeat};};
    const auto layout=pica_texture_layout(image(0));
    std::vector<std::uint8_t> result(layout.bytes);
    Result measured;
    for(unsigned frame=0;frame<frames.size();++frame) {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        const auto begin=Time::now();pack(image(frame),result);
        const auto end=Time::now();std::atomic_signal_fence(std::memory_order_seq_cst);
        measured.seconds+=std::chrono::duration<double>(end-begin).count();
        measured.checksum^=checksum(result)+0x9e3779b97f4a7c15ULL+frame;
    }
    return measured;
}
void run(Fixture fixture) {
    const unsigned pitch=fixture.width*fixture.channels+fixture.pad;
    std::vector<std::vector<std::uint8_t>> frames;
    for(unsigned f=0;f<64;++f) {
        auto& data=frames.emplace_back(fixture.offset+std::size_t(pitch)*fixture.height,0x9b);
        if(f)data=frames[f-1];
        for(unsigned y=0;y<fixture.height;++y)for(unsigned x=0;x<fixture.width;++x)
            for(unsigned c=0;c<fixture.channels;++c)
                if(f%4==0 || (f%4==1 && x==fixture.width-1 && y==fixture.height-1)
                    || (f%4==2 && x<9 && y<9) || (f%4==3 && y==fixture.height/2))
                data[fixture.offset+std::size_t(y)*pitch+x*fixture.channels+c]=std::uint8_t(13*x+7*y+29*f+73*c);
    }
    const auto image=[&](unsigned frame) {return PicaImage{
        std::span(frames[frame]).subspan(fixture.offset),fixture.width,fixture.height,pitch,fixture.channels,fixture.repeat};};
    const auto layout=pica_texture_layout(image(0));
    std::vector<std::uint8_t> previous(std::size_t(fixture.width)*fixture.height*fixture.channels);
    std::vector<std::uint8_t> a(layout.bytes),b(layout.bytes),fullA(layout.bytes),fullB(layout.bytes);
    PicaTexturePacker current;BaselinePicaTexturePacker baseline;
    for(unsigned f=0;f<64;++f) {
        const auto source=image(f);
        pack_pica_texture(source,fullA);baseline_pack_pica_texture(source,fullB);
        if(fullA!=fullB)throw std::runtime_error("Full word pack differs from byte baseline");
        const auto rangeA=current.pack(source,f?std::span<const std::uint8_t>(previous):std::span<const std::uint8_t>{},a);
        const auto rangeB=baseline.pack(source,f?std::span<const std::uint8_t>(previous):std::span<const std::uint8_t>{},b);
        if(a!=fullB || b!=fullB || rangeA.offset!=rangeB.offset || rangeA.bytes!=rangeB.bytes || rangeA.tiles!=rangeB.tiles)
            throw std::runtime_error("Changed/dense pack bytes or flush range differ from baseline");
        for(unsigned y=0;y<fixture.height;++y)
            std::copy_n(source.pixels.begin()+std::size_t(y)*pitch,fixture.width*fixture.channels,
                previous.begin()+std::size_t(y)*fixture.width*fixture.channels);
    }
    for(unsigned pair=0;pair<16;++pair) {
        Result old,newer;
        if(pair&1) {
            newer=measure(fixture,frames,[](PicaImage i,std::span<std::uint8_t> d){pack_pica_texture(i,d);});
            old=measure(fixture,frames,[](PicaImage i,std::span<std::uint8_t> d){baseline_pack_pica_texture(i,d);});
        }else {
            old=measure(fixture,frames,[](PicaImage i,std::span<std::uint8_t> d){baseline_pack_pica_texture(i,d);});
            newer=measure(fixture,frames,[](PicaImage i,std::span<std::uint8_t> d){pack_pica_texture(i,d);});
        }
        if(old.checksum!=newer.checksum || !old.checksum || old.seconds<=0 || newer.seconds<=0)
            throw std::runtime_error("Complete changing-frame upload checksum or timing invalid");
        std::cout<<"PACK pair="<<pair<<" width="<<fixture.width<<" height="<<fixture.height
            <<" channels="<<fixture.channels<<" row_pad="<<fixture.pad<<" source_offset="<<fixture.offset
            <<" repeat="<<fixture.repeat<<" baseline_seconds="<<old.seconds
            <<" candidate_seconds="<<newer.seconds<<" ratio="<<old.seconds/newer.seconds
            <<" checksum="<<old.checksum<<'\n';
    }
}
}
int main()try {
    for(const auto fixture:{Fixture{256,224,4,0,0,false},Fixture{250,218,4,7,1,false},
        Fixture{128,128,4,0,3,true},Fixture{320,240,3,5,2,false}})run(fixture);
    std::cout<<"PASS: unchanged whole-image/changed-tile bytes and flush ranges; 16 alternating complete changing-image host pairs per fixture. Not ARM/GPU/console FPS.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
