#include "starfox/vr/scene_packet_validation.hpp"
#include "starfox/vr/backdrop_texture.hpp"
#include "starfox/vr/source_span_layout.hpp"
#include "starfox/render/calibrated_ground.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <optional>
#include <stdexcept>
namespace starfox::vr {
void ScenePacketValidator::add(const DrawPacket& packet) {
    const auto& mesh=packet.geometry;
    const auto vertices=mesh.vertex_view();
    const auto texture_words=mesh.texel_view();
    const bool ground_packet=!vertices.empty()
        && (vertices.front().texture[3]&(render::calibrated_ground_flag|8U))==(render::calibrated_ground_flag|8U);
    if(ground_packet && !mesh.line_view().empty())
        throw std::runtime_error("Calibrated ground requires a plain triangle landscape");
    if((mesh.shared_texels && !mesh.texels.empty()) || (mesh.shared_vertices && !mesh.vertices.empty()) || (mesh.shared_line_vertices && !mesh.line_vertices.empty()) || !mesh.deferred.empty() || vertices.size()%3 || mesh.line_view().size()%2
        || !model_eye_camera(EyeCamera{},packet.model))
        throw std::runtime_error("Invalid native scene packet: triangles="+std::to_string(vertices.size())
            +" lines="+std::to_string(mesh.line_view().size())+" deferred="+std::to_string(mesh.deferred.size())
            +" shared="+std::to_string(bool(mesh.shared_vertices))+" owned="+std::to_string(mesh.vertices.size())
            +" transform="+std::to_string(bool(model_eye_camera(EyeCamera{},packet.model))));
    const bool artwork=std::any_of(vertices.begin(),vertices.end(),[](const auto& v) {
        return (v.texture[3]&backdrop_texture_flag)==backdrop_texture_flag;
    });
    if(vertices.size()>4'000'000 || mesh.line_view().size()>4'000'000
        || texture_words.size()>(artwork?backdrop_texture_word_limit:4'000'000))
        throw std::runtime_error("Native scene packet exceeds upload budget");
    vertex_count_+=vertices.size()+mesh.line_view().size();
    if(!mesh.shared_texels || counted_images_.insert(mesh.shared_texels.get()).second) {
        if(artwork) artwork_words_+=texture_words.size();else texel_count_+=texture_words.size();
    }
    if(vertex_count_>4'000'000 || texel_count_>4'000'000 || artwork_words_>backdrop_texture_word_limit)
        throw std::runtime_error("Native scene exceeds upload budget");
    if(artwork) {
        const auto& first=vertices.front();
        if(!mesh.shared_texels || !mesh.line_view().empty()
            || first.texture[1]>=4096 || first.texture[2]>=4096
            || !backdrop_texture_valid(texture_words,first.texture[1]+1,first.texture[2]+1))
            throw std::runtime_error("Invalid immutable backdrop texture");
        for(const auto& v:vertices) {
            if(v.texture[0]!=0 || v.texture[1]!=first.texture[1] || v.texture[2]!=first.texture[2]
                || (v.texture[3]!=backdrop_texture_flag && v.texture[3]!=(backdrop_texture_flag|2U))
                || v.visibility_enabled || v.group_enabled || v.dither_scale)
                throw std::runtime_error("Mixed or invalid backdrop attributes");
            for(float value:v.uv) if(!std::isfinite(value) || std::abs(value)>65536)
                throw std::runtime_error("Invalid backdrop coordinate");
            const float* ramp[]{v.visibility_a,v.visibility_b,v.visibility_c,v.group_a,v.group_b,v.group_c};
            for(unsigned i=0;i<18;++i) {
                const float value=ramp[i/3][i%3];
                if(!std::isfinite(value) || value<0 || value>(i==0?8:i<16?32767:0)
                    || std::floor(value)!=value) throw std::runtime_error("Invalid backdrop cloud shade");
            }
            for(float value:v.position) if(!std::isfinite(value))
                throw std::runtime_error("Invalid backdrop position");
            for(float value:v.color) if(!std::isfinite(value) || value<0 || value>4096)
                throw std::runtime_error("Invalid backdrop response");
            for(float value:v.odd_color) if(!std::isfinite(value) || std::abs(value)>4096)
                throw std::runtime_error("Invalid backdrop palette shift");
            if(v.odd_color[3]<0 || v.odd_color[3]>4 || std::floor(v.odd_color[3])!=v.odd_color[3])
                throw std::runtime_error("Invalid backdrop pole flags");
        }
        return;
    }
    std::optional<std::size_t> checked_grid_words;
    std::optional<std::size_t> checked_dust_words;
    bool checked_connected_rows=false;
    const bool compute_grid=std::any_of(vertices.begin(),vertices.end(),[](const auto& v){return v.texture[3]==gpu_connected_grid_flag;});
    if(compute_grid && !compute_connected_grid_) throw std::runtime_error("Calibrated connected-grid compute producer not integrated");
    if(compute_grid && (vertices.size()!=6 || !mesh.line_view().empty()
        || !std::all_of(vertices.begin(),vertices.end(),[](const auto& v){return v.texture[3]==gpu_connected_grid_flag
            && std::isfinite(v.uv[0]) && std::isfinite(v.uv[1]);})))
        throw std::runtime_error("Mixed or invalid compute connected-grid packet");
    std::unordered_set<uint64_t> checked_span_headers;
    for(const auto list:{vertices,mesh.line_view()}) for(const auto& v:list) {
        const bool ground_vertex=(v.texture[3]&(render::calibrated_ground_flag|8U))==(render::calibrated_ground_flag|8U);
        if(ground_vertex!=ground_packet || (ground_packet && v.texture[0]!=vertices.front().texture[0]))
            throw std::runtime_error("Mixed calibrated ground/source packet");
        if((v.texture[3]&(render::calibrated_ground_flag|8U))==(render::calibrated_ground_flag|8U)
            && (!calibrated_ground_
                || (v.texture[3]&~(render::calibrated_ground_flag|268435456U|10U))))
            throw std::runtime_error("Calibrated ground used on an unsupported vertex");
        if((v.texture[3]&268435456U) && (!(v.texture[3]&8U)
            || (v.texture[3]&~(268435456U|10U|(calibrated_ground_?render::calibrated_ground_flag:0U)))))
            throw std::runtime_error("GPU landscape receiver used on a non-tile vertex");
    }
    for(const auto list:{vertices,mesh.line_view()})
        for(const auto& v:list) if(v.texture[3]&65536U) {
            if(v.texture[3]!=65536U || v.texture[2]!=0 || !std::isfinite(v.billboard[0]) || v.billboard[0]<0)
                throw std::runtime_error("Invalid source span face attributes");
            const uint64_t key=(uint64_t(v.texture[0])<<32)|v.texture[1];
            if(checked_span_headers.insert(key).second && !source_span_payload_valid(texture_words,v.texture[0],v.texture[1]))
                throw std::runtime_error("Invalid source span coverage payload");
        } else if(v.texture[3]&1024U) {
            const auto start=size_t(v.texture[0]);
            const auto flags=v.texture[3]&~134217728U;
            if((flags!=1024U && flags!=1026U && flags!=1028U && flags!=1030U)
                || v.texture[1]!=15 || v.texture[2]!=15
                || start>texture_words.size() || texture_words.size()-start<9)
                throw std::runtime_error("Invalid packed source-font glyph");
            if((v.texture[3]&134217728U) && (!(flags&4U)
                || !std::isfinite(v.group_a[0]) || !std::isfinite(v.group_a[1])
                || !std::isfinite(v.group_b[0]) || !std::isfinite(v.billboard[0]) || !std::isfinite(v.billboard[1])))
                throw std::runtime_error("Invalid GPU scaled-text sizing parameters");
        } else if(v.texture[3]&512U) {
            if(v.texture[3]==gpu_connected_grid_flag) {
                if(v.texture[0]!=0 || v.texture[1]!=0 || v.texture[2]!=0 || texture_words.size()!=14 || !mesh.line_view().empty())
                    throw std::runtime_error("Invalid compute connected-grid header");
                for(auto word:texture_words) if(int32_t(word)<-32768 || int32_t(word)>32767)
                    throw std::runtime_error("Invalid compute connected-grid source word");
                continue;
            }
            if(v.texture[3]!=512U || v.texture[0]!=0 || texture_words.size()<384)
                throw std::runtime_error("Invalid binned connected-grid header");
            if(!checked_connected_rows) {
                for(size_t row=0;row<192;++row) {
                    const auto start=size_t(texture_words[row*2]),count=size_t(texture_words[row*2+1]);
                    if(count>675 || start<384 || start>texture_words.size() || count>texture_words.size()-start)
                        throw std::runtime_error("Invalid connected-grid row list");
                    for(size_t i=0;i<count;++i) {
                        const auto record=size_t(texture_words[start+i]);
                        if(record<384 || record>texture_words.size() || texture_words.size()-record<5 || texture_words[record]>1)
                            throw std::runtime_error("Invalid connected-grid row primitive");
                        for(size_t word=1;word<5;++word) {
                            const auto value=int32_t(texture_words[record+word]);
                            if(value< -8192 || value>8191) throw std::runtime_error("Connected-grid coordinate overflow");
                        }
                    }
                }
                checked_connected_rows=true;
            }
        } else if(v.texture[3]&256U) {
            const auto start=std::size_t(v.texture[0]);
            if(v.texture[3]!=256U || start>texture_words.size() || texture_words.size()-start<4)
                throw std::runtime_error("Invalid GPU connected-grid line payload");
            for(unsigned word=0;word<4;++word) {
                const auto coordinate=static_cast<int32_t>(texture_words[start+word]);
                if(coordinate< -8192 || coordinate>8191)
                    throw std::runtime_error("GPU connected-grid endpoint exceeds arithmetic bounds");
            }
        } else if(v.texture[3]&128U) {
            const auto start=std::size_t(v.texture[0]);
            if((v.texture[3]&121U) || !(v.texture[3]&4U) || v.texture[1]>3 || v.texture[2]>1
                || start>texture_words.size() || texture_words.size()-start<268)
                throw std::runtime_error("Invalid GPU dust payload");
            for(float coordinate:v.position)
                if(!std::isfinite(coordinate) || coordinate< -32768 || coordinate>32767
                    || coordinate!=std::trunc(coordinate))
                    throw std::runtime_error("Invalid GPU source dust point");
            if(checked_dust_words!=start) {
                for(unsigned word=0;word<268;++word) {
                    if(word>=3 && word<12) {
                        const auto value=static_cast<int32_t>(texture_words[start+word]);
                        if(value< -32768 || value>32767) throw std::runtime_error("Invalid GPU dust matrix");
                    } else {
                        const float value=std::bit_cast<float>(texture_words[start+word]);
                        if(!std::isfinite(value) || (word<3?std::abs(value)>65536.F:(value<0 || value>1)))
                            throw std::runtime_error("Invalid GPU dust camera/colour");
                    }
                }
                checked_dust_words=start;
            }
        } else if(v.texture[3]&64U) {
            const auto start=std::size_t(v.texture[0]);
            const float low_z=(v.texture[3]&8192U)?-24.F:0.F;
            if((v.texture[3]&57U) || !(v.texture[3]&4U)
                || start>texture_words.size() || texture_words.size()-start<12
                || v.texture[1]>1 || v.position[0]<0 || v.position[0]>14
                || v.position[2]<low_z || v.position[2]>14
                || !std::isfinite(v.position[0]) || !std::isfinite(v.position[2])
                || v.position[0]!=std::trunc(v.position[0]) || v.position[2]!=std::trunc(v.position[2]))
                throw std::runtime_error("Invalid GPU source grid payload");
            if(checked_grid_words!=start) {
                for(unsigned word=0;word<12;++word) {
                    const auto value=static_cast<int32_t>(texture_words[start+word]);
                    if(value< -32768 || value>32767) throw std::runtime_error("Invalid GPU grid source word");
                }
                checked_grid_words=start;
            }
        } else if(v.texture[3]&8U) {
            if(v.texture[3]&268435456U) {
                const auto receiver=size_t(v.texture[0])+272+16384;
                if((v.texture[3]&~(268435456U|10U|(calibrated_ground_?render::calibrated_ground_flag:0U))) || receiver>=texture_words.size())
                    throw std::runtime_error("Invalid GPU landscape receiver payload");
                const auto height=std::bit_cast<float>(texture_words[receiver]);
                if(!std::isfinite(height) || height>=0 || height< -8.F)
                    throw std::runtime_error("Invalid GPU landscape receiver height");
            }
            if(v.texture[3]&48U) throw std::runtime_error("Conflicting tile/sprite/solid payload flags");
            const auto start=std::size_t(v.texture[0]);
            if(start>texture_words.size() || texture_words.size()-start<272+16384)
                throw std::runtime_error("Truncated GPU tile background payload");
            if(v.texture[3]&render::calibrated_ground_flag) {
                const auto offset=start+render::calibrated_ground_offset;
                if(!calibrated_ground_ || !(texture_words[start+15]&0x10000000U)
                    || (texture_words[start+15]&0x80000000U) || texture_words[start+10]
                    || offset>texture_words.size() || texture_words.size()-offset!=render::calibrated_ground_words)
                    throw std::runtime_error("Invalid/unsupported calibrated ground payload");
                const auto plane_height=std::bit_cast<float>(texture_words[offset-1]);
                if(!std::isfinite(plane_height) || plane_height>=0 || plane_height< -8.F)
                    throw std::runtime_error("Invalid calibrated ground receiver height");
                for(unsigned word=0;word<8;++word) {
                    const auto value=std::bit_cast<float>(texture_words[offset+word]);
                    if(!std::isfinite(value) || (word<6?(value<0 || value>1):word==6?
                            (value<0 || value>223):(value<=0 || value>4096)))
                        throw std::runtime_error("Invalid calibrated ground gradient");
                }
                const auto material=texture_words[offset+8];
                if((material!=0 && (material<5 || material>9)) || texture_words[offset+13]>3)
                    throw std::runtime_error("Invalid calibrated ground material/motion");
                for(unsigned word=9;word<13;++word) {
                    const auto value=std::bit_cast<float>(texture_words[offset+word]);
                    if(!std::isfinite(value) || (word<11?std::abs(value)>65536:
                            word==11?value<0:(value<0 || value>1)))
                        throw std::runtime_error("Invalid calibrated ground surface metadata");
                }
            }
            const auto bpp=texture_words[start+5];
            const auto scanlines=texture_words[start+10];
            if(texture_words[start+12]>2)
                throw std::runtime_error("Invalid GPU offset-per-tile mode");
            if(texture_words[start+13]>15)
                throw std::runtime_error("Invalid GPU background attenuation");
            if(texture_words[start+14]>256)
                throw std::runtime_error("Invalid GPU tunnel border control");
            const auto unique_rows=(texture_words[start+15]>>8)&65535U;
            if(unique_rows>224 && unique_rows!=512)
                throw std::runtime_error("Invalid GPU tile coverage controls");
            if((texture_words[start+15]&64U)!=0) {
                const auto atlas_width=((texture_words[start+2]&1U)?64U:32U)
                    *(texture_words[start+8]?16U:8U);
                if(atlas_width!=512U && atlas_width!=1024U)
                    throw std::runtime_error("Unsupported unique landscape atlas width");
            }
            if(scanlines>3 || (scanlines && texture_words.size()-start<272+16384+448))
                throw std::runtime_error("Invalid GPU scanline payload");
            // Word 7 packs the two-bit priority and the selective
            // face-planet and game-over star continuation flags.
            if((bpp!=2 && bpp!=4 && bpp!=8) || texture_words[start+2]>3
                || (texture_words[start+7]&~768U)>2
                || texture_words[start+8]>1 || texture_words[start+9]>16 || texture_words[start+11]>1)
                throw std::runtime_error("Invalid GPU tile background controls");
        } else if(v.texture[3]&16U) {
            if(v.texture[3]&32U) throw std::runtime_error("Conflicting sprite/solid payload flags");
            const auto start=std::size_t(v.texture[0]);
            if(start>texture_words.size() || texture_words.size()-start<272+16384)
                throw std::runtime_error("Truncated GPU sprite payload");
            const auto size=v.texture[2]>>8;
            if(v.texture[1]>0x1ffffU
                || (size!=8 && size!=16 && size!=32 && size!=64) || texture_words[start+13]>15)
                throw std::runtime_error("Invalid GPU sprite controls");
        } else if(v.texture[3]&32U) {
            const auto start=std::size_t(v.texture[0]);
            if(start>texture_words.size() || texture_words.size()-start<272
                || v.texture[1]>255 || texture_words[start+13]>15)
                throw std::runtime_error("Invalid GPU solid palette payload");
        } else if(v.texture[3]&1073741824U) {
            const uint64_t width=uint64_t(v.texture[1])+1,height=uint64_t(v.texture[2])+1;
            if(width>16384 || height>16384 || uint64_t(v.texture[0])+((width+3)/4)*height>texture_words.size())
                throw std::runtime_error("Packed shadow mask reference out of bounds");
        } else if(v.texture[3]&1U) {
            const auto width=uint64_t(v.texture[1])+1,height=uint64_t(v.texture[2])+1;
            // Source texture masks are 8-bit. Reject before multiplication.
            const uint64_t palette_words=(v.texture[3]&536870912U)?256:0;
            const auto pixel_words=palette_words?(width*height+3)/4:width*height;
            if(width>256 || height>256 || uint64_t(v.texture[0])+palette_words+pixel_words>texture_words.size())
                throw std::runtime_error("Native scene texture reference out of bounds");
        }
}
}
