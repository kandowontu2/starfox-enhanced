#include "starfox/render/software_renderer.hpp"
#include "starfox/render/source_polygon_spans.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace starfox;
unsigned checks{},frames{};
void require(bool value,const char* message) {
    ++checks;if(!value) throw std::runtime_error(message);
}
std::uint64_t mix(std::uint64_t hash,std::span<const std::uint8_t> bytes) {
    for(auto byte:bytes) {hash^=byte;hash*=1099511628211ULL;}return hash;
}
void span_rules() {
    constexpr std::array<render::SourceSpanPoint,4> rectangle{{{2,2},{2,7},{10,7},{10,2}}};
    const auto collect=[&](render::SourceSpanModes modes) {
        std::vector<render::SourcePolygonSpan> spans;
        render::source_polygon_spans(rectangle,224,modes,[&](auto span){spans.push_back(span);});
        return spans;
    };
    auto spans=collect({});require(spans.size()==5,"Solid source rectangle row count changed");
    for(unsigned row=0;row<spans.size();++row)
        require(spans[row].left==2 && spans[row].right==10 && spans[row].y==int(row+2)
            && spans[row].source_y==spans[row].y,"Source spans lost inclusive ink or unwarped coordinates");
    render::SourceSpanModes modes;modes.wireframe=1;spans=collect(modes);
    require(spans.size()==9 && spans[0].left==2 && spans[0].right==10,"Wireframe first edge lost its authored chord");
    for(unsigned i=1;i<spans.size();++i)
        require(spans[i].left==spans[i].right && spans[i].left==((i&1)?2:10),"Wireframe continuation filled the interior");
    modes={};modes.wobble=2;spans=collect(modes);
    require(spans.size()==4 && spans.front().y==3,"Wobble-2 starts before its previous-left continuation");
    for(const auto& span:spans) require(span.left==2 && span.right==2,"Wobble-2 emitted a filled chord");
    modes={};modes.wobble=1;spans=collect(modes);
    require(spans.size()==7,"Wobble-1 collapsed trapezoid and crossing-edge no-op changed");
    for(unsigned i=0;i<spans.size();++i)
        require(spans[i].y==int(i<5?2:4) && spans[i].source_y==spans[i].y,
            "Wobble-1 no longer repeats on its collapsed source rows");
    modes={};modes.cel=true;spans=collect(modes);
    for(const auto& span:spans) require(span.left==3 && span.right==9,"Cel omitted source outline incorrectly");
    constexpr std::array<int,16> independent_sine{0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1};
    for(auto offset:{-32768,-7,0,3,32767}) for(unsigned frame=0;frame<32;++frame) {
        modes={};modes.wave=true;modes.wave_offset=std::int16_t(offset);modes.animation_frame=frame;
        spans=collect(modes);unsigned pixels{};
        for(const auto& span:spans) {
            require(span.left<=span.right,"Empty wave group was emitted");
            for(int x=span.left;x<=span.right;++x) {
                // Independent signed-word/ASR/period arithmetic, not the production helpers.
                int phase=(offset+x)&65535;if(phase>=32768) phase-=65536;
                phase=int(std::floor(phase/2.0))+int(frame&15)-1;
                phase&=65535;if(phase>=32768) phase-=65536;
                const auto index=((phase%16)+16)%16;
                require(span.y-span.source_y==independent_sine[index],"Wave metadata flattened or shifted source depth coordinates");
                ++pixels;
            }
        }
        require(pixels==45,"Wave chunks omitted or duplicated source ink");
    }
}
std::array<std::uint64_t,3> raster_fingerprints() {
    constexpr std::array<std::array<unsigned,2>,3> sizes{{{224,192},{256,224},{400,240}}};
    std::array<std::uint64_t,3> hashes{14695981039346656037ULL,14695981039346656037ULL,14695981039346656037ULL};
    assets::Shape shape;
    shape.vertices={{0,-60,0},{-70,-10,0},{-50,45,0},{35,70,0},{75,70,0}};
    shape.faces.push_back({-1,0xa3,{0,0,-127},{0,1,2,3,4}});
    for(unsigned size=0;size<sizes.size();++size) {
        const auto width=sizes[size][0],height=sizes[size][1];
        render::SoftwareRenderer renderer;
        for(unsigned configuration=0;configuration<4;++configuration)
            for(unsigned mode=0;mode<48;++mode) for(unsigned phase=0;phase<8;++phase) {
                render::RenderPose pose;
                pose.wireframe_mode=mode%3;pose.wobble_mode=(mode/3)%4;
                pose.wave_mode=(mode/12)%2;pose.cel_mode=(mode/24)%2;
                pose.animation_frame=phase*3;pose.wave_offset=phase%2?-32768:32767;
                pose.x=configuration==1?-175:configuration==2?175:0;
                pose.y=configuration==3?-150:0;pose.z=256;
                pose.roll=configuration==3?4000:0;
                pose.continuous_geometry=phase%2;pose.terrain_geometry=configuration%2;
                render::Framebuffer pixels(width,height);pixels.enable_layer_tags(true);
                pixels.enable_dither_pairs(true);pixels.begin_write_coverage();
                renderer.draw(shape,pose,pixels);
                hashes[size]=mix(hashes[size],pixels.pixels());
                hashes[size]=mix(hashes[size],pixels.layer_tags());
                hashes[size]=mix(hashes[size],pixels.write_coverage());
                for(auto pair:pixels.dither_pairs()) {
                    const std::array<std::uint8_t,2> bytes{std::uint8_t(pair),std::uint8_t(pair>>8)};
                    hashes[size]=mix(hashes[size],bytes);
                }
                render::RasterCommands commands;commands.reset(width,height);
                render::Framebuffer recording(width,height);recording.record_to(&commands);
                renderer.draw(shape,pose,recording);
                std::vector<std::uint8_t> reproduced(std::size_t(width)*height);
                for(const auto& command:commands.commands) {
                    require(!command.textured,"Synthetic solid unexpectedly uses a texture");
                    for(int y=std::max(0,command.top);y<std::min(int(height),command.bottom);++y)
                        for(int x=std::max(0,command.left);x<std::min(int(width),command.right);++x)
                            reproduced[std::size_t(y)*width+x]=std::uint8_t(command.dither && ((x^y)&1)?command.odd:command.even);
                }
                require(reproduced==pixels.pixels(),"Ordered span commands differ from direct source coverage/inks");
                ++frames;
            }
    }
    return hashes;
}
}
int main() try {
    span_rules();
    const auto actual=raster_fingerprints();
    // Filled from the unchanged pre-extraction source renderer, before the
    // shared generator is called by any production path.
    constexpr std::array<std::uint64_t,3> original{0xa331187d433ea249ULL,0x51e7800f3d66e82dULL,0x3f9bab92d78ef0f1ULL};
    for(unsigned i=0;i<actual.size();++i) {
        std::cout<<"Raster "<<i<<" fingerprint: "<<std::hex<<actual[i]<<std::dec<<'\n';
        require(actual[i]==original[i],"Source raster pixels/provenance changed during span extraction");
    }
    std::cout<<frames<<" source frames, "<<checks<<" checks passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
