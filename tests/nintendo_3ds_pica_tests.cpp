#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include "starfox/platform/nintendo_3ds/pica_composite.hpp"
#include "starfox/platform/nintendo_3ds/pica_residency.hpp"
#include "starfox/platform/nintendo_3ds/pica_texture_update.hpp"
#include <iostream>
#include <cstdlib>
#include <new>
#include <type_traits>

namespace {
using namespace starfox::platform::nintendo_3ds;
unsigned checks{};
std::size_t texture_allocations{};
bool count_texture_allocations{};
void require(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
void word_texture_store() {
    std::uint32_t random=0x142857abU;
    for(unsigned channels:{3U,4U}) for(unsigned source_offset=0;source_offset<8;++source_offset)
        for(unsigned destination_offset=0;destination_offset<8;++destination_offset)
            for(unsigned sample=0;sample<2048;++sample) {
                std::array<std::uint8_t,16> source{},destination{},expected{};
                source.fill(0xa5);destination.fill(0x5a);expected=destination;
                std::array<std::uint8_t,4> pixel{};
                if(sample<1024) {
                    pixel={0x01,0x23,0x45,0x67};pixel[sample/256]=std::uint8_t(sample);
                } else {
                    random^=random<<13;random^=random>>17;random^=random<<5;
                    for(unsigned i=0;i<4;++i)pixel[i]=std::uint8_t(random>>(i*8));
                }
                std::copy_n(pixel.begin(),channels,source.begin()+source_offset);
                const auto retained=source;
                expected[destination_offset]=channels==4?pixel[3]:255;
                expected[destination_offset+1]=pixel[2];expected[destination_offset+2]=pixel[1];
                expected[destination_offset+3]=pixel[0];
                detail::pica_store_abgr(destination.data()+destination_offset,source.data()+source_offset,channels);
                require(destination==expected,"Word upload changed a source byte, alpha or surrounding destination guard");
                require(source==retained,"Word upload mutated borrowed source bytes");
            }
}
template<class F> void rejects(F action,const char* message) {
    bool rejected=false;try {action();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,message);
}
void validated_composition_handoff() {
    static_assert(!std::is_default_constructible_v<PicaValidatedFrame>);
    static_assert(!std::is_constructible_v<PicaValidatedFrame,PicaFrame>);
    static_assert(!std::is_copy_constructible_v<PicaComposite>
        && !std::is_move_constructible_v<PicaComposite>);
    Canvas lower;
    std::array<PicaVertex,6> vertices{};
    std::array<PicaDraw,2> draws{};
    std::array<std::uint8_t,8*8*4> pixels{};pixels.fill(255);
    std::array<std::uint8_t,8*8> layers{};layers.fill(2);
    PicaImage image{pixels,8,8,8*4,4};
    draws[0].count=draws[1].count=3;draws[1].first=3;draws[1].texture=0;
    for(const auto plan:{plan_frame(0,false,ScreenUse::world),plan_frame(0,true,ScreenUse::world),
        plan_frame(1,true,ScreenUse::world,StereoSettings{1,16,512})}) {
        PicaFrame source{plan,vertices,draws,std::span(&image,1)};
        PicaComposite composite;
        auto prepared=composite.prepare_validated(plan,std::span(&source,1),lower.view(),{3,5,7});
        const auto& frame=prepared.checked_frame(lower.view());
        require(frame.vertices.data()!=vertices.data() && frame.draws.data()!=draws.data()
            && frame.textures.data()!=&image,"Validated geometry/metadata still aliases mutable inputs");
        require(std::equal(frame.vertices.begin(),frame.vertices.end(),vertices.begin())
            && frame.draws[1].first==3 && frame.draws[1].texture==0
            && frame.textures[0].pixels.data()==pixels.data() && same_pica_plan(frame.plan,plan)
            && frame.clear==Rgb{3,5,7},"Validated handoff changed geometry/metadata/borrowed art/eye plan");
        validate_pica_frame(frame,lower.view()); // Independent ordinary submission gate.
        auto bad_lower=lower.view();bad_lower.width=319;
        rejects([&]{static_cast<void>(prepared.checked_frame(bad_lower));},"Validated handoff accepted wrong lower LCD extent");
        bad_lower=lower.view();bad_lower.pixels={};
        rejects([&]{static_cast<void>(prepared.checked_frame(bad_lower));},"Validated handoff accepted missing lower LCD bytes");
        vertices[0].position[0]=std::numeric_limits<float>::infinity();draws[1].texture=99;image.width=0;
        validate_pica_frame(prepared.checked_frame(lower.view()),lower.view());
        require(frame.vertices[0].position[0]==0 && frame.draws[1].texture==0 && frame.textures[0].width==8,
            "Mutating source metadata changed already-owned validated composition");
        rejects([&]{composite.prepare_validated(plan,std::span(&source,1),lower.view());},
            "Invalid input minted a validated composition");
        static_cast<void>(prepared.checked_frame(lower.view())); // Failed publication keeps the previous complete frame.
        vertices[0].position[0]=0;draws[1].texture=0;image.width=8;
        composite.prepare_layers(plan,std::span(&source,1));
        rejects([&]{static_cast<void>(prepared.checked_frame(lower.view()));},"Layer-only publication did not expire the prior handle");
        prepared=composite.prepare_validated(plan,std::span(&source,1),lower.view());
        composite.prepare(plan,std::span(&source,1),lower.view());
        rejects([&]{static_cast<void>(prepared.checked_frame(lower.view()));},"Ordinary publication did not expire the prior handle");
        prepared=composite.prepare_validated(plan,std::span(&source,1),lower.view());
        auto moved=std::move(prepared);static_cast<void>(moved.checked_frame(lower.view()));
        rejects([&]{static_cast<void>(prepared.checked_frame(lower.view()));},"Moved-from handle was accepted or dereferenced");
        auto copy=moved;
        composite.prepare_validated(plan,std::span(&source,1),lower.view());
        rejects([&]{static_cast<void>(moved.checked_frame(lower.view()));},"Validated publication left an older handle live");
        rejects([&]{static_cast<void>(copy.checked_frame(lower.view()));},"Copied handle escaped publication invalidation");
        const auto doomed=[&] {
            PicaComposite temporary;
            return temporary.prepare_validated(plan,std::span(&source,1),lower.view());
        }();
        rejects([&]{static_cast<void>(doomed.checked_frame(lower.view()));},"Destroyed owner left borrowed geometry readable");
        image.source_layers=layers;image.layer_pitch=8;
        draws[1].space=PicaSpace::screen;draws[1].depth_test=draws[1].depth_write=false;
        vertices[3].colour[3]=.5F;
        rejects([&]{composite.prepare_validated(plan,std::span(&source,1),lower.view());},
            "Validated handoff bypassed source-layer opacity validation");
        vertices[3].colour[3]=1;
        auto opaque=composite.prepare_validated(plan,std::span(&source,1),lower.view());
        validate_pica_frame(opaque.checked_frame(lower.view()),lower.view());
        pixels[3]=0;
        rejects([&]{validate_pica_layers(opaque.checked_frame(lower.view()).textures[0]);},
            "Borrowed changed artwork escaped the existing upload ownership/opacity gate");
        pixels[3]=255;image.source_layers={};image.layer_pitch=0;draws[1].space=PicaSpace::world;
        draws[1].depth_test=draws[1].depth_write=true;
    }
}
void vertex_validation_bits() {
    // Independent previous floating-point contract, not another bit classifier.
    const auto reference=[](const PicaVertex& vertex,bool clamp_uv) {
        for(float value:vertex.position) if(!std::isfinite(value)) return 1U;
        for(float value:vertex.colour) if(!std::isfinite(value) || value<0 || value>1) return 2U;
        for(float value:vertex.uv) if(!std::isfinite(value) || std::abs(value)>65536
            || (clamp_uv && (value<0 || value>1))) return 3U;
        return 0U;
    };
    const auto check=[&](std::uint32_t bits) {
        const float value=std::bit_cast<float>(bits);
        for(unsigned lane=0;lane<9;++lane) for(bool clamp_uv:{false,true}) {
            PicaVertex vertex{};
            if(lane<3) vertex.position[lane]=value;
            else if(lane<7) vertex.colour[lane-3]=value;
            else vertex.uv[lane-7]=value;
            require(unsigned(detail::pica_vertex_error(vertex,clamp_uv))==reference(vertex,clamp_uv),
                "Integer vertex envelope differs from independent floating-point contract");
        }
    };
    // Every exponent/sign with representative mantissas, including signaling
    // and quiet NaNs, subnormals, signed zero and both largest finite values.
    for(unsigned sign:{0U,0x80000000U}) for(unsigned exponent=0;exponent<256;++exponent)
        for(unsigned mantissa:{0U,1U,0x3fffffU,0x400000U,0x7ffffeU,0x7fffffU})
            check(sign|(exponent<<23)|mantissa);
    for(unsigned boundary:{0U,0x00800000U,0x3f800000U,0x47800000U,0x7f7fffffU,0x7f800000U})
        for(unsigned delta:{0U,1U,2U}) for(unsigned sign:{0U,0x80000000U}) {
            check(sign|(boundary+delta));
            if(boundary>=delta) check(sign|(boundary-delta));
        }
    std::uint32_t random=0x962de84bU;
    for(unsigned sample=0;sample<131072;++sample) {
        random^=random<<13;random^=random>>17;random^=random<<5;check(random);
    }
    // Preserve first-error ownership when several fields are invalid together.
    PicaVertex bad{};bad.position[0]=std::numeric_limits<float>::infinity();bad.colour[0]=-1;bad.uv[0]=70000;
    require(detail::pica_vertex_error(bad,true)==detail::PicaVertexError::position,"Position error was reordered");
    bad.position[0]=0;
    require(detail::pica_vertex_error(bad,true)==detail::PicaVertexError::colour,"Colour error was reordered");
    bad.colour[0]=1;
    require(detail::pica_vertex_error(bad,true)==detail::PicaVertexError::uv,"UV error was lost");
}
void vertex_validation_draw_policy() {
    Canvas lower;
    std::array<PicaVertex,3> vertices{};
    std::array<std::uint8_t,8*8*4> pixels{};pixels.fill(255);
    std::array<std::uint8_t,8*8> layers{};layers.fill(2);
    PicaImage image{pixels,8,8,8*4,4};
    PicaDraw draw;draw.count=3;draw.texture=0;
    PicaFrame frame{plan_frame(1,true,ScreenUse::world),vertices,std::span(&draw,1),std::span(&image,1)};
    const auto valid=[&]{validate_pica_frame(frame,lower.view());};
    for(float uv:{-65536.F,-1.F,-0.F,0.F,1.F,2.F,65536.F}) {
        vertices.back().uv={uv,uv};image.repeat=false;
        if(uv<0 || uv>1) rejects(valid,"Clamped draw accepted out-of-range UVs");else valid();
        image.repeat=true;valid(); // Signed/large UVs are valid on the repeat sampler.
        draw.texture=pica_no_texture;valid();draw.texture=0;
    }
    vertices.back().uv={std::nextafter(65536.F,std::numeric_limits<float>::infinity()),0};
    rejects(valid,"Repeat UV exceeded the original absolute envelope");
    draw.texture=pica_no_texture;rejects(valid,"Untextured UV exceeded the original absolute envelope");
    draw.texture=0;vertices.back().uv={0,0};image.repeat=false;
    draw.space=PicaSpace::screen;draw.depth_test=draw.depth_write=false;
    image.source_layers=layers;image.layer_pitch=8;
    vertices.back().colour[3]=.5F;rejects(valid,"Opaque source provenance allowed interpolated opacity");
    vertices.back().colour[3]=1;valid();
    image.source_layers={};image.layer_pitch=0;vertices.back().colour[3]=.5F;valid();
    vertices.back().colour[3]=1;draw.texture=pica_no_texture;draw.source_layer=0;
    draw.colour_op=PicaColourOp{false,false,1};valid();
    vertices.back().colour[0]=.5F;rejects(valid,"Fixed colour-operation equality check was omitted");
}
void vertex_residency() {
    PicaVertexResidency cache;
    std::vector<PicaVertex> source(96),gpu;
    unsigned uploads{};bool fail{};
    const auto upload=[&](std::span<const PicaVertex> submitted) {
        require(submitted.data()==source.data() && submitted.size()==source.size(),
            "Vertex upload used a complete-scene temporary instead of borrowed source");
        ++uploads;
        // A failed flush may already have changed native VBO bytes. CPU cache
        // equality alone cannot certify that the previous scene is resident.
        gpu.assign(submitted.begin(),submitted.end());
        if(fail)throw std::runtime_error("Injected geometry flush failure");
    };
    require(!cache.valid() && cache.vertices().empty(),"New vertex cache fabricated valid residency");
    require(cache.prepare(source,upload) && cache.valid() && uploads==1,
        "Initial scene did not upload/commit exactly once");
    const auto address=cache.vertices().data();const auto capacity=cache.capacity();
    const auto equal=[&]{return std::equal(source.begin(),source.end(),cache.vertices().begin(),cache.vertices().end());};
    require(equal() && !cache.prepare(source,upload) && uploads==1,"Held vertex scene was copied/uploaded again");
    for(unsigned phase=1;phase<=180;++phase) {
        source[phase%source.size()].position[0]=float(phase)*.25F;
        require(cache.prepare(source,upload) && cache.valid() && equal(),"Changed vertices were not exactly committed");
        require(cache.vertices().data()==address && cache.capacity()==capacity,
            "Steady vertex updates replaced the comparison-cache allocation");
        require(!cache.prepare(source,upload),"Held changed scene performed a second upload");
    }
    require(uploads==181,"Steady vertices did not have one upload per change");
    const auto old=source;
    source[0].position[2]=917;fail=true;
    bool failed=false;try{cache.prepare(source,upload);}catch(const std::runtime_error&){failed=true;}
    require(failed && !cache.valid()
        && std::equal(old.begin(),old.end(),cache.vertices().begin(),cache.vertices().end()),
        "Failed flush committed newer CPU vertices or retained valid GPU residency");
    require(gpu!=old,"Failure fixture did not actually mutate its GPU bytes");
    source=old;fail=false;const auto before_retry=uploads;
    require(cache.prepare(source,upload) && cache.valid() && gpu==old && uploads==before_retry+1,
        "Retry of the old CPU scene skipped the required VBO repair");
    source.resize(12);
    require(cache.prepare(source,upload) && equal() && cache.vertices().data()==address,
        "Shrinking scene replaced storage or retained obsolete vertices");
    source.clear();const auto before_empty=uploads;
    require(cache.prepare(source,upload) && cache.valid() && cache.vertices().empty()
        && cache.capacity()==capacity && uploads==before_empty,
        "Empty scene uploaded fake vertices or freed reusable storage");
    require(!cache.prepare(source,upload),"Held empty scene was not reused");
    source.resize(96);source[2].position[1]=713;
    require(cache.prepare(source,upload) && cache.valid() && equal() && cache.vertices().data()==address,
        "Restored scene did not reuse its bounded vertex storage");
    source.resize(pica_vertex_limit+1);const auto before_invalid=uploads;
    rejects([&]{cache.prepare(source,upload);},"Oversized vertex scene accepted before upload");
    require(cache.valid() && cache.vertices().size()==96 && cache.capacity()==capacity && uploads==before_invalid,
        "Invalid preflight mutated residency or allocated beyond the scene bound");
}
void texture_upload() {
    // Independent Morton lookup, including source row order and ABGR bytes.
    constexpr unsigned spread[]{0,1,4,5,16,17,20,21};
    for(unsigned channels:{3U,4U}) for(unsigned width:{1U,7U,8U,9U,31U,64U})
        for(unsigned height:{1U,7U,8U,17U,32U}) {
            const unsigned pitch=width*channels+7;
            std::vector<std::uint8_t> source(pitch*height,0xAD);
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x)
                for(unsigned c=0;c<channels;++c) source[y*pitch+x*channels+c]=std::uint8_t(x*17+y*29+c*53);
            const PicaImage image{source,width,height,pitch,channels};
            const auto layout=pica_texture_layout(image);
            require(layout.width>=8 && layout.height>=8 && layout.bytes==layout.width*layout.height*4,"Padded allocation");
            require(layout.uv_scale==std::array<float,2>{float(width)/layout.width,float(height)/layout.height},"Logical UV scale");
            std::vector<std::uint8_t> packed(layout.bytes+2,0xDD);
            pack_pica_texture(image,std::span(packed).subspan(1,layout.bytes));
            require(packed.front()==0xDD && packed.back()==0xDD,"Upload must not cross storage boundary");
            for(unsigned y=0;y<layout.height;++y) for(unsigned x=0;x<layout.width;++x) {
                const unsigned texture_y=y;
                const unsigned tile=(texture_y/8)*(layout.width/8)+x/8;
                const unsigned offset=1+4*(tile*64+spread[x%8]+2*spread[texture_y%8]);
                const unsigned from=std::min(y,height-1)*pitch+std::min(x,width-1)*channels;
                require(packed[offset]==(channels==4?source[from+3]:255),"Alpha or RGB opacity");
                for(unsigned c=0;c<3;++c) require(packed[offset+3-c]==source[from+c],"Swizzle, row padding, edge fill and ABGR");
            }
            auto repeat=image;repeat.repeat=true;
            if(std::has_single_bit(width) && std::has_single_bit(height)) {
                pack_pica_texture(repeat,std::span(packed).subspan(1,layout.bytes));
                for(unsigned y=0;y<layout.height;++y) for(unsigned x=0;x<layout.width;++x) {
                    const unsigned ty=y;
                    const unsigned offset=1+4*(((ty/8)*(layout.width/8)+x/8)*64+spread[x%8]+2*spread[ty%8]);
                    require(packed[offset+3]==source[(y%height)*pitch+(x%width)*channels],"Repeat padding wraps source artwork");
                }
            } else rejects([&]{pica_texture_layout(repeat);},"Non-power-of-two repeat rejected");
            rejects([&]{pack_pica_texture(image,std::span(packed).first(layout.bytes-1));},"Short upload rejected");
        }
    std::array<std::uint8_t,256> storage{};
    PicaImage image{storage,8,8,32,4};
    rejects([&]{pack_pica_texture(image,storage);},"Overlapping source cannot be swizzled in place");
    for(unsigned width:{0U,1025U,~0U}) {
        image.width=width;rejects([&]{pica_texture_layout(image);},"Invalid width rejected before arithmetic");
    }
    image={storage,8,8,32,4};image.pitch=31;
    rejects([&]{pica_texture_layout(image);},"Short pitch rejected");
    image={std::span(storage).first(255),8,8,32,4};
    rejects([&]{pica_texture_layout(image);},"Short logical rows rejected");
    image={storage,8,8,32,2};rejects([&]{pica_texture_layout(image);},"Unsupported pixel format rejected");
}
void reusable_texture_cache() {
    constexpr unsigned width=27,height=17,pitch=width*4+7,layer_pitch=width+5;
    std::vector<std::uint8_t> pixels(pitch*height,0xA7),layers(layer_pitch*height,0xD9);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const unsigned p=y*pitch+x*4;
        pixels[p]=x*7;pixels[p+1]=y*11;pixels[p+2]=(x+y)*3;pixels[p+3]=255;
        layers[y*layer_pitch+x]=1U<<((x+y)%6);
    }
    PicaImage image{pixels,width,height,pitch,4,false,layers,layer_pitch};
    PicaTextureResidency cache;
    std::vector<std::uint8_t> packed,packed_layers;
    unsigned colour_uploads{},layer_uploads{},removed{};
    bool fail_colour{},fail_layers{};
    const auto colour=[&](PicaImage source,PicaTextureLayout layout) {
        require(source.pixels.data()==pixels.data(),"Texture upload used a complete-image staging copy");
        ++colour_uploads;packed.resize(layout.bytes);pack_pica_texture(source,packed);
        if(fail_colour)throw std::runtime_error("Injected colour flush failure");
    };
    const auto ownership=[&](PicaImage source,PicaTextureLayout layout,unsigned classes) {
        require(source.source_layers.data()==layers.data() || source.source_layers.empty(),
            "Ownership upload used a complete-image staging copy");
        ++layer_uploads;
        if(source.source_layers.empty()) {packed_layers.clear();++removed;}
        else {
            packed_layers.resize(layout.width*layout.height);
            require(pack_pica_layers(source,packed_layers)==classes,"Cache changed source ownership classes");
        }
        if(fail_layers)throw std::runtime_error("Injected ownership flush failure");
    };
    require(cache.prepare(image,colour,ownership) && colour_uploads==1 && layer_uploads==1,
        "Initial texture planes did not upload exactly once");
    const auto* colour_address=cache.pixels().data();const auto* layer_address=cache.layers().data();
    const auto colour_capacity=cache.colour_capacity(),layer_capacity=cache.layer_capacity();
    require(cache.colour_matches(image) && cache.layers_match(image) && cache.classes()==63,
        "Initial texture comparison cache omitted source rows/classes");
    PicaImage self{cache.pixels(),8,8,width*4,4,false,cache.layers(),width};
    rejects([&]{cache.prepare(self,colour,ownership);},"Layout change accepted an input aliasing its own comparison cache");
    require(colour_uploads==1 && layer_uploads==1 && cache.colour_matches(image) && cache.layers_match(image),
        "Aliased preflight changed native or comparison storage");
    texture_allocations=0;count_texture_allocations=true;
    for(unsigned phase=0;phase<180;++phase) {
        const auto old_colour=colour_uploads,old_layers=layer_uploads;
        pixels[0]^=1;require(cache.prepare(image,colour,ownership),"Changed palette did not upload");
        require(colour_uploads==old_colour+1 && layer_uploads==old_layers,
            "Palette change unnecessarily uploaded the A8 ownership plane");
        layers[1]=layers[1]==1?2:1;
        require(cache.prepare(image,colour,ownership) && colour_uploads==old_colour+1 && layer_uploads==old_layers+1,
            "Ownership-only change uploaded RGBA or skipped A8");
        pixels[3]=pixels[3]?0:255;layers[0]=pixels[3]?1:0;
        require(cache.prepare(image,colour,ownership) && colour_uploads==old_colour+2 && layer_uploads==old_layers+2,
            "Opacity/ownership change did not update both planes");
        require(cache.colour_matches(image) && cache.layers_match(image)
            && cache.pixels().data()==colour_address && cache.layers().data()==layer_address
            && cache.colour_capacity()==colour_capacity && cache.layer_capacity()==layer_capacity,
            "Warm texture updates replaced comparison storage or failed exact commit");
        require(!cache.prepare(image,colour,ownership),"Held image performed another texture upload");
        // Independent Morton/ABGR checks on every padded texel. No temporary
        // full-image oracle allocations inside the measured update boundary.
        constexpr unsigned spread[]{0,1,4,5,16,17,20,21};
        const auto layout=pica_texture_layout(image);
        for(unsigned y=0;y<layout.height;++y)for(unsigned x=0;x<layout.width;++x) {
            const unsigned at=(((y/8)*(layout.width/8)+x/8)*64+spread[x%8]+2*spread[y%8]);
            const unsigned sx=std::min(x,width-1),sy=std::min(y,height-1),from=sy*pitch+sx*4;
            require(packed[at*4]==pixels[from+3] && packed[at*4+1]==pixels[from+2]
                && packed[at*4+2]==pixels[from+1] && packed[at*4+3]==pixels[from]
                && packed_layers[at]==layers[sy*layer_pitch+sx],"Changed upload lost exact colour or ownership bytes");
        }
    }
    count_texture_allocations=false;
    require(texture_allocations==0,"Warmed native texture updates still allocate complete-image staging copies");
    std::cout<<"Warmed native texture comparison/packing: "<<texture_allocations
        <<" allocations over 540 changed plane preparations; exact Morton/ABGR/A8 bytes\n";
    const auto old_pixels=pixels,old_layers=layers;
    pixels[0]^=64;fail_colour=true;bool failed=false;
    try{cache.prepare(image,colour,ownership);}catch(const std::runtime_error&){failed=true;}
    require(failed && !cache.colour_matches(image),"Failed colour upload retained valid CPU equality");
    pixels=old_pixels;image.pixels=pixels;fail_colour=false;const auto colour_retry=colour_uploads;
    require(cache.prepare(image,colour,ownership) && colour_uploads==colour_retry+1 && cache.colour_matches(image),
        "Restoring old RGB skipped repair after a failed native write");
    layers[1]=layers[1]==1?2:1;fail_layers=true;failed=false;
    try{cache.prepare(image,colour,ownership);}catch(const std::runtime_error&){failed=true;}
    require(failed && !cache.layers_match(image),"Failed ownership upload retained valid CPU equality");
    layers=old_layers;image.source_layers=layers;fail_layers=false;const auto layer_retry=layer_uploads;
    require(cache.prepare(image,colour,ownership) && layer_uploads==layer_retry+1 && cache.layers_match(image),
        "Restoring old ownership skipped repair after a failed native write");
    const auto before_invalid=colour_uploads;pixels[3]=128;
    rejects([&]{cache.prepare(image,colour,ownership);},"Palette-only edit bypassed ownership/alpha validation");
    require(colour_uploads==before_invalid,"Invalid provenance reached the native upload callback");
    pixels=old_pixels;image.pixels=pixels;
    auto without_layers=image;without_layers.source_layers={};without_layers.layer_pitch=0;
    require(cache.prepare(without_layers,colour,ownership) && removed==1 && cache.layers().empty()
        && cache.layer_capacity()==0 && cache.classes()==0 && cache.colour_matches(without_layers),
        "Removed ownership retained GPU or peak CPU storage");
    require(!cache.prepare(without_layers,colour,ownership),"Held unmasked image performed an upload");
    require(cache.prepare(image,colour,ownership) && cache.layers_match(image),"Restored ownership failed to upload");
    cache.release_colour();cache.release_layers();
    require(!cache.colour_matches(image) && cache.pixels().empty() && cache.layers().empty()
        && !cache.colour_capacity() && !cache.layer_capacity(),"Inactive texture cache retained allocations");
}
void full_size_texture_upload() {
    // LCD/dashboard sizes, padded edge tiles, thin maximum-width/height images
    // and the largest accepted allocation. The oracle deliberately retains
    // the old row-major address calculation, independent of tile traversal.
    constexpr std::array<std::array<unsigned,2>,7> sizes{{
        {320,240},{400,240},{511,255},{1024,9},{9,1024},{1023,1023},{1024,1024}}};
    constexpr unsigned spread[]{0,1,4,5,16,17,20,21};
    for(const auto size:sizes) for(unsigned channels:{3U,4U}) {
        const auto width=size[0],height=size[1],pitch=width*channels+11,layer_pitch=width+7;
        // No trailing row padding: clamping must not read beyond the last
        // logical texel, even when the padded allocation is much taller.
        std::vector<std::uint8_t> pixels(std::size_t(pitch)*(height-1)+width*channels,0xA7);
        std::vector<std::uint8_t> layers;
        if(channels==4)layers.resize(std::size_t(layer_pitch)*(height-1)+width,0xB9);
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto at=std::size_t(y)*pitch+x*channels;
            for(unsigned c=0;c<3;++c)pixels[at+c]=std::uint8_t(x*17+y*29+c*53);
            if(channels==4) {
                const auto layer=(x+y)%7?1U<<((x+3*y)%6):0;
                layers[std::size_t(y)*layer_pitch+x]=std::uint8_t(layer);
                pixels[at+3]=layer?255:0;
            }
        }
        PicaImage image{pixels,width,height,pitch,channels,false,layers,channels==4?layer_pitch:0};
        const auto original_pixels=pixels,original_layers=layers;
        const auto layout=pica_texture_layout(image);
        for(bool repeat:{false,true}) {
            if(repeat && (!std::has_single_bit(width) || !std::has_single_bit(height)))continue;
            image.repeat=repeat;
            std::vector<std::uint8_t> packed(layout.bytes+2,0xDD),ownership;
            pack_pica_texture(image,std::span(packed).subspan(1,layout.bytes));
            if(channels==4) {
                ownership.assign(layout.width*layout.height+2,0xCC);
                require(pack_pica_layers(image,std::span(ownership).subspan(1,ownership.size()-2))==63,
                    "Full-size upload lost a populated source class");
            }
            for(unsigned y=0;y<layout.height;++y) for(unsigned x=0;x<layout.width;++x) {
                const auto sx=repeat?x%width:std::min(x,width-1),sy=repeat?y%height:std::min(y,height-1);
                const auto from=std::size_t(sy)*pitch+sx*channels;
                const auto texel=((y/8)*(layout.width/8)+x/8)*64+spread[x%8]+2*spread[y%8];
                const auto to=1+texel*4;
                require(packed[to]==(channels==4?pixels[from+3]:255),"Full-size alpha/padded row changed");
                for(unsigned c=0;c<3;++c)require(packed[to+3-c]==pixels[from+c],"Full-size Morton/ABGR changed");
                if(channels==4)require(ownership[1+texel]==layers[std::size_t(sy)*layer_pitch+sx],
                    "Full-size source ownership no longer matches colour artwork");
            }
            require(packed.front()==0xDD && packed.back()==0xDD,"Full-size colour crossed allocation guard");
            if(channels==4)require(ownership.front()==0xCC && ownership.back()==0xCC,
                "Full-size ownership crossed allocation guard");
            require(pixels==original_pixels && layers==original_layers,"Packing mutated borrowed artwork");
        }
    }
}
void changed_tile_upload() {
    constexpr std::array<std::array<unsigned,2>,12> sizes{{{1,1},{4,4},{7,9},{8,8},{9,17},
        {32,16},{320,240},{400,240},{511,255},{1024,9},{9,1024},{1023,1023}}};
    for(const auto size:sizes) for(unsigned channels:{3U,4U}) for(bool repeat:{false,true}) {
        const auto width=size[0],height=size[1],pitch=width*channels+13;
        if(repeat && (!std::has_single_bit(width) || !std::has_single_bit(height)))continue;
        std::vector<std::uint8_t> source(std::size_t(pitch)*(height-1)+width*channels,0xA7);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)for(unsigned c=0;c<channels;++c)
            source[std::size_t(y)*pitch+x*channels+c]=std::uint8_t(x*19+y*37+c*53);
        PicaImage image{source,width,height,pitch,channels,repeat};
        const auto layout=pica_texture_layout(image);
        PicaTexturePacker packer;
        std::vector<std::uint8_t> packed(layout.bytes+2,0xDC),oracle(layout.bytes),previous(width*height*channels);
        const auto destination=std::span(packed).subspan(1,layout.bytes);
        const auto save=[&] {
            for(unsigned y=0;y<height;++y)std::copy_n(source.data()+std::size_t(y)*pitch,width*channels,
                previous.data()+std::size_t(y)*width*channels);
        };
        const auto initial=packer.pack(image,{},destination);save();
        require(initial.offset==0 && initial.bytes==layout.bytes && initial.tiles==layout.bytes/256,
            "Missing successful residency did not fully initialize padded GPU bytes");
        const auto equal=[&](PicaTextureUploadRange range,const std::vector<std::uint8_t>& old) {
            pack_pica_texture(image,oracle);
            require(std::equal(destination.begin(),destination.end(),oracle.begin()),
                "Changed-tile upload differs from complete Morton/ABGR packing");
            require(packed.front()==0xDC && packed.back()==0xDC,"Changed upload crossed GPU allocation guards");
            require(range.offset<=layout.bytes && range.bytes<=layout.bytes-range.offset
                && range.offset%256==0 && range.bytes%256==0 && range.tiles<=layout.bytes/256,
                "Changed upload returned an unbounded/unaligned cache flush range");
            for(unsigned i=0;i<layout.bytes;++i)if(i<range.offset || i>=range.offset+range.bytes)
                require(destination[i]==old[i+1],"Changed upload modified bytes outside its flush range");
        };
        for(const auto point:std::array<std::array<unsigned,2>,6>{{{0,0},{width-1,height-1},
            {width/2,height/2},{std::min(7U,width-1),std::min(7U,height-1)},
            {std::min(8U,width-1),std::min(8U,height-1)},{width-1,height/2}}}) {
            const auto old=packed;
            const auto pixel=std::size_t(point[1])*pitch+point[0]*channels;
            source[pixel+(point[0]+point[1])%channels]^=0x6B;
            const auto range=packer.pack(image,previous,destination);equal(range,old);save();
            require(range.tiles>0,"Changed source pixel was mistaken for held residency");
            if(width==400 && point==std::array<unsigned,2>{width/2,height/2})
                require(range.tiles==1 && range.bytes==256,"One HUD tile still repacked/flushed the full LCD texture");
            const auto held=packed;const auto no_change=packer.pack(image,previous,destination);
            require(!no_change.tiles && !no_change.bytes && packed==held,"Held texture was rewritten");
        }
        // Source pitch/padding is not a residency identity. Padding-only edits
        // and a compact replacement must leave the uploaded logical image held.
        if(height>1)source[width*channels]^=0x7F;
        require(packer.pack(image,previous,destination).bytes==0,"Row-padding edit triggered a texture write");
        auto compact=image;compact.pixels=previous;compact.pitch=width*channels;
        require(packer.pack(compact,previous,destination).bytes==0,"Equivalent row stride changed native texture bytes");
        const auto old=packed;
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)source[std::size_t(y)*pitch+x*channels]^=0xA3;
        const auto dense=packer.pack(image,previous,destination);equal(dense,old);
        require(dense.offset==0 && dense.bytes==layout.bytes,"Dense palette change bypassed full packing fallback");
        const auto invalid_before=packed;
        rejects([&]{packer.pack(image,std::span(previous).first(previous.size()-1),destination);},
            "Truncated successful-residency base accepted");
        rejects([&]{packer.pack(image,previous,destination.first(destination.size()-1));},
            "Truncated changed-upload destination accepted");
        require(packed==invalid_before,"Invalid upload changed existing native bytes");
        packer.release();require(!packer.capacity(),"Retired tile scratch retained peak allocation");
    }
    // Cache validity, not mere equality/padded allocation size, controls use
    // of the incremental base. A partially failed flush must be fully repaired.
    std::vector<std::uint8_t> source(400*240*4,255),gpu(512*256*4);
    PicaImage image{source,400,240,400*4,4};PicaTextureResidency residency;PicaTexturePacker packer;
    bool fail=false;PicaTextureUploadRange changed;
    const auto update=[&] {
        const auto previous=residency.colour_base(image);
        return residency.prepare(image,[&](PicaImage next,PicaTextureLayout) {
            changed=packer.pack(next,previous,gpu);
            if(fail)throw std::runtime_error("Injected changed-tile flush failure");
        },[](PicaImage,PicaTextureLayout,unsigned){});
    };
    require(residency.colour_base(image).empty() && update() && changed.bytes==gpu.size(),
        "Initial incremental cache fabricated a successful base");
    const auto original=source;source[101*1600+201*4]^=31;fail=true;bool failed=false;
    try{update();}catch(const std::runtime_error&){failed=true;}
    require(failed && changed.tiles==1 && residency.colour_base(image).empty(),
        "Failed sparse flush retained a usable residency base");
    source=original;image.pixels=source;fail=false;
    require(update() && changed.offset==0 && changed.bytes==gpu.size(),"Retry skipped complete failed-upload repair");
    // Warm the sparse scratch once, then count real residency/packing calls.
    source[101*1600+201*4]^=31;update();
    texture_allocations=0;count_texture_allocations=true;
    for(unsigned i=0;i<180;++i) {
        source[101*1600+201*4]^=31;
        require(update() && changed.tiles==1 && changed.bytes==256,"Warm sparse edit lost its exact one-tile update");
    }
    count_texture_allocations=false;
    require(texture_allocations==0,"Warm changed-tile uploads allocate staging storage every frame");
    std::vector<std::uint8_t> complete(gpu.size());pack_pica_texture(image,complete);
    require(gpu==complete,"Warm changed-tile residency lost complete-image byte equivalence");
    auto reshaped=image;reshaped.width=399;reshaped.pitch=400*4;
    auto rgb=image;rgb.channels=3;
    auto wrapping=image;wrapping.repeat=true;
    require(residency.colour_base(reshaped).empty() && residency.colour_base(rgb).empty()
        && residency.colour_base(wrapping).empty(),"Logical layout change reused an incompatible residency base");
    residency.release_colour();require(residency.colour_base(image).empty(),"Released GPU texture retained an incremental base");
    PicaImage alias{gpu,512,256,512*4,4};
    rejects([&]{packer.pack(alias,{},gpu);},"Changed upload accepted source/native aliasing");
    rejects([&]{packer.pack(image,gpu,gpu);},"Changed upload accepted old-base/native aliasing");
}
void texture_residency() {
    struct Ledger {
        std::size_t live{},peak{};unsigned allocations{},releases{},prepares{},updates{},fail_at{};
        void allocate(unsigned bytes) {
            if(++allocations==fail_at) throw std::runtime_error("Injected texture allocation failure");
            live+=bytes;peak=std::max(peak,live);
        }
        void release(unsigned bytes) {require(bytes<=live,"Resident ledger underflow");live-=bytes;++releases;}
    } ledger;
    struct Resident {
        Ledger* ledger{};unsigned w{},h{},lw{},lh{};
        std::vector<std::uint8_t> colour_cache,layer_cache;
        void release_colour() {
            if(w) ledger->release(w*h*4);
            w=h=0;release_pica_texture_cache(colour_cache);
        }
        void release_layers() {
            if(lw) ledger->release(lw*lh);
            lw=lh=0;release_pica_texture_cache(layer_cache);
        }
        void release() {release_colour();release_layers();}
        void prepare_layout(PicaImage image) {
            ++ledger->prepares;
            const auto keep=pica_texture_retention(image,w,h,lw,lh);
            if(!keep.colour) release_colour();
            if(!keep.layers) release_layers();
        }
        void update(PicaImage image) {
            ++ledger->updates;const auto layout=pica_texture_layout(image);
            if(w!=layout.width || h!=layout.height) {
                release_colour();ledger->allocate(layout.bytes);w=layout.width;h=layout.height;
            }
            if(!image.source_layers.empty() && (lw!=layout.width || lh!=layout.height)) {
                release_layers();ledger->allocate(layout.width*layout.height);lw=layout.width;lh=layout.height;
            } else if(image.source_layers.empty()) release_layers();
            colour_cache.assign(image.width*image.height*image.channels,19);
            if(!image.source_layers.empty()) layer_cache.assign(image.width*image.height,2);
        }
    };
    std::array<Resident,6> resident{};Resident dashboard;
    for(auto& texture:resident) texture.ledger=&ledger;
    dashboard.ledger=&ledger;
    const auto cleanup=[&]{for(auto& texture:resident) texture.release();dashboard.release();};
    std::vector<std::uint8_t> pixels(1024*1024*4,255),layers(1024*1024,2);
    const auto image=[&](unsigned w,unsigned h,bool masked=false,unsigned channels=4) {
        return PicaImage{pixels,w,h,w*channels,channels,false,masked?std::span<const std::uint8_t>(layers):std::span<const std::uint8_t>{},masked?w:0};
    };
    const auto lower=image(bottom_width,screen_height,false,3);
    const auto update=[&](std::span<const PicaImage> source){update_pica_texture_residency(source,lower,std::span(resident),dashboard);};
    // Independent rounded-size reference: logical dimensions/stride may change
    // without replacing the same power-of-two allocation.
    for(unsigned w:{1U,8U,9U,255U,256U,257U,1024U}) for(unsigned h:{1U,7U,16U,129U,511U}) {
        unsigned aw=8,ah=8;while(aw<w) aw*=2;while(ah<h) ah*=2;
        const auto source=image(w,h,true);
        for(unsigned cw:{0U,aw,aw/2}) for(unsigned ch:{0U,ah,ah/2})
            for(unsigned lw:{0U,aw}) for(unsigned lh:{0U,ah}) {
                const auto keep=pica_texture_retention(source,cw,ch,lw,lh);
                require(keep.colour==(cw==aw && ch==ah) && keep.layers==(lw==aw && lh==ah),
                    "Residency confused logical/padded colour or ownership dimensions");
            }
        require(!pica_texture_retention(image(w,h),aw,ah,aw,ah).layers,"Removed ownership remained resident");
    }
    std::array old{image(128,512),image(512,512),image(512,512),image(512,512)};
    std::array next{image(512,512),image(512,512),image(512,512),image(128,512)};
    // Both frames are valid 3.75 MiB. The former slot-at-a-time sequence has
    // a 4.5 MiB intermediate allocation, even deleting each changed slot first.
    const unsigned old_bytes=(128*512+3*512*512+512*256)*4;
    require(old_bytes==3'932'160 && old_bytes+(512*512-128*512)*4>pica_texture_budget,
        "Transition fixture no longer reproduces old over-budget allocation");
    update(old);require(ledger.live==old_bytes,"Initial resident allocation sum differs from independent sizes");
    ledger.peak=ledger.live;const auto allocated=ledger.allocations;
    update(next);require(ledger.live==old_bytes && ledger.peak<=pica_texture_budget && ledger.allocations==allocated+2,
        "Replacement allocated before all obsolete slots were released");
    const auto unchanged=ledger.allocations;update(next);
    require(ledger.allocations==unchanged,"Unchanged palette/layout reallocated GPU textures");
    std::array masked{image(512,256,true),image(512,256,true),image(256,256,true)};
    update(masked);require(resident[0].lw==512 && resident[0].lh==256,"Source ownership was not allocated");
    auto unmasked=masked;for(auto& source:unmasked) {source.source_layers={};source.layer_pitch=0;}
    const auto colour_allocations=ledger.allocations;update(unmasked);
    require(ledger.allocations==colour_allocations && resident[0].lw==0 && resident[0].layer_cache.capacity()==0,
        "Removing A8 ownership replaced colour or retained its CPU cache");
    require(resident[3].colour_cache.capacity()==0 && resident[3].layer_cache.capacity()==0,
        "Inactive texture slots retained peak CPU allocations");
    const auto live=ledger.live;
    const auto prepares=ledger.prepares,updates=ledger.updates;
    auto malformed=next;malformed[3].pixels={};
    rejects([&]{update(malformed);},"Malformed tail was accepted during resident preflight");
    std::array oversized{image(1024,1024)};
    rejects([&]{update(oversized);},"Resident preflight omitted padded dashboard bytes");
    rejects([&]{update_pica_texture_residency(next,lower,std::span(resident).first(2),dashboard);},
        "Resident preflight accepted insufficient slots");
    auto bad_lower=lower;bad_lower.pitch=1;
    rejects([&]{update_pica_texture_residency(next,bad_lower,std::span(resident),dashboard);},
        "Invalid dashboard was accepted");
    require(ledger.live==live && ledger.prepares==prepares && ledger.updates==updates,
        "Failed preflight mutated live resident allocations");
    // Failure after the release phase must not leak, exceed the texture budget,
    // or prevent a clean retry with the same immutable source.
    ledger.fail_at=ledger.allocations+2;bool failed=false;
    try {update(next);} catch(const std::runtime_error&) {failed=true;}
    require(failed && ledger.live<=pica_texture_budget,"Allocation failure exceeded residency budget");
    ledger.fail_at=0;update(next);require(ledger.live==old_bytes,"Failed replacement could not recover");
    // Repeated shape/ownership/front-end switches, including all inactive slots.
    for(unsigned step=0;step<180;++step) {
        std::array<PicaImage,6> batch;
        for(unsigned slot=0;slot<6;++slot) batch[slot]=image(32U<<((step+slot)%5),32U<<((step*3+slot)%4),(step+slot)%3==0);
        ledger.peak=ledger.live;update(std::span(batch).first(step%7));
        require(ledger.peak<=pica_texture_budget,"Repeated transitions exceeded live resident budget");
    }
    update({});require(ledger.live==512*256*4,"Empty frontend retained upper-scene GPU textures");
    for(const auto& texture:resident) require(texture.colour_cache.capacity()==0 && texture.layer_cache.capacity()==0,
        "Empty frontend retained upper-scene CPU texture caches");
    cleanup();require(ledger.live==0 && dashboard.colour_cache.capacity()==0,"Resident shutdown leaked allocations");
}
void projection_and_draws() {
    std::vector<std::uint8_t> lower(bottom_width*screen_height*3);
    const ImageView dashboard{lower,bottom_width,screen_height,bottom_width*3};
    std::array<PicaVertex,6> vertices{};
    for(auto& vertex:vertices) vertex.position={16,24,512};
    std::array<PicaDraw,2> draws{{{0,3},{3,3}}};
    PicaFrame frame{plan_frame(1,true,ScreenUse::world),vertices,draws,{}};
    validate_pica_frame(frame,dashboard);require(frame.plan.eye_count==2,"One geometry stream, two projections");
    for(unsigned eye=0;eye<2;++eye) {
        const auto matrix=pica_multiply(PicaProjection(frame.plan,eye).rows(),pica_identity);
        const auto clip=PicaProjection(frame.plan,eye).clip_position(vertices[0].position);
        require(clip.has_value(),"GPU projection accepts finite geometry");
        const auto expected=project(frame.plan,eye,16,24,512);
        require(expected.has_value(),"Independent off-axis projection");
        require(std::abs(top_width*.5*(1-(*clip)[1]/(*clip)[3])-(*expected)[0])<.0001,"GPU eye horizontal projection");
        require(std::abs(screen_height*.5*(1-(*clip)[0]/(*clip)[3])-(*expected)[1])<.0001,"GPU LCD vertical projection");
        require(matrix==PicaProjection(frame.plan,eye).rows(),"Identity model cannot alter projection");
    }
    const auto screen=pica_screen_matrix(top_width);
    require(screen[0][1]==-2.F/screen_height && screen[1][0]==-2.F/top_width
        && screen[2][3]==-.5F && screen[3][3]==1,"Mono overlays use rotated LCD and PICA depth");
    draws[1].space=PicaSpace::screen;draws[1].depth_test=false;draws[1].depth_write=false;
    validate_pica_frame(frame,dashboard);require(draws[1].first==3,"Authored screen/world pass order retained");
    require(pica_draw_matrix(frame.plan,0,draws[1])==pica_draw_matrix(frame.plan,1,draws[1]),"HUD is at screen depth, not sky depth");
    draws[1].space=PicaSpace::scenery;validate_pica_frame(frame,dashboard);
    for(unsigned eye=0;eye<2;++eye) {
        const auto scenery=pica_draw_matrix(frame.plan,eye,draws[1]);
        const double displayed_x=top_width*.5*(1-scenery[1][3]);
        require(std::abs(displayed_x-background_offset(frame.plan,eye))<.0001,"Background projects at infinity independently for each eye");
    }
    draws[1].space=PicaSpace::screen;
    const auto valid=[&]{validate_pica_frame(frame,dashboard);};
    draws[1].depth_write=true;rejects(valid,"Screen overlay cannot write world depth");draws[1].depth_write=false;
    draws[1].first=2;rejects(valid,"Overlapping geometry ranges rejected");draws[1].first=4;rejects(valid,"Missing primitive rejected");draws[1].first=3;
    draws[1].count=2;rejects(valid,"Partial triangle rejected");draws[1].count=6;rejects(valid,"Out-of-range draw rejected");draws[1].count=3;
    draws[1].texture=0;rejects(valid,"Unknown texture rejected");draws[1].texture=pica_no_texture;
    draws[0].model[0][0]=std::numeric_limits<float>::quiet_NaN();rejects(valid,"Non-finite model rejected");draws[0].model=pica_identity;
    draws[0].model[3][0]=1;rejects(valid,"Non-affine model rejected");draws[0].model=pica_identity;
    draws[0].model[0][0]=std::numeric_limits<float>::max();
    frame.plan.focal_x=1000;
    rejects(valid,"Reused eye projection failed to reject overflowing model uniforms");
    draws[0].model=pica_identity;frame.plan=plan_frame(1,true,ScreenUse::world);
    vertices[0].position[2]=std::numeric_limits<float>::infinity();rejects(valid,"Invalid position rejected");vertices[0].position[2]=512;
    vertices[0].colour[0]=1.1F;rejects(valid,"Invalid vertex colour rejected");vertices[0].colour[0]=1;
    frame.plan.eye_count=0;rejects(valid,"Missing eye rejected");frame.plan.eye_count=3;rejects(valid,"Third eye rejected");frame.plan.eye_count=2;
    frame.plan=plan_frame(0,true,ScreenUse::world);valid();require(frame.plan.eye_count==1,"Slider zero has no second eye work");
    frame.plan=plan_frame(1,true,ScreenUse::setup);valid();require(!frame.plan.stereo,"Setup labels remain mono");
    auto invalid_dashboard=dashboard;invalid_dashboard.pixels=std::span(lower).first(lower.size()-1);
    rejects([&]{validate_pica_frame(frame,invalid_dashboard);},"Incomplete dashboard rejected before GPU submission");
    frame.draws=std::span(draws).first(1);rejects(valid,"Unclaimed tail geometry rejected");frame.draws=draws;
    std::vector<PicaVertex> too_many(pica_vertex_limit+1);frame.vertices=too_many;rejects(valid,"Geometry budget is enforced without dropping faces");frame.vertices=vertices;
    std::vector<PicaDraw> too_many_draws(pica_draw_limit+1);frame.draws=too_many_draws;rejects(valid,"Draw budget is enforced");frame.draws=draws;
    std::vector<std::uint8_t> pixels(512*512*4);
    const PicaImage image{pixels,512,512,512*4,4};
    std::array<PicaImage,4> images{image,image,image,image};
    frame.textures=std::span(images).first(3);valid();
    frame.textures=images;rejects(valid,"Texture budget includes the padded lower-screen upload");frame.textures={};
    frame.vertices={};frame.draws={};valid();require(true,"Clear/dashboard-only native frames are allowed");
}
void layer_upload() {
    constexpr unsigned spread[]{0,1,4,5,16,17,20,21};
    for(unsigned width:{1U,7U,8U,9U,31U}) for(unsigned height:{1U,7U,17U}) {
        const unsigned pitch=width*4+3,layer_pitch=width+5;
        std::vector<std::uint8_t> pixels(pitch*height,0xAA),layers(layer_pitch*height,0xBB);
        constexpr unsigned classes[]{0,1,2,4,8,16,32};unsigned mask=0;
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto layer=classes[(x+y*3)%7];layers[y*layer_pitch+x]=std::uint8_t(layer);mask|=layer;
            pixels[y*pitch+x*4+3]=layer?255:0;
        }
        const PicaImage image{pixels,width,height,pitch,4,false,layers,layer_pitch};
        const auto layout=pica_texture_layout(image);
        require(pica_resident_texture_bytes(image)==layout.width*layout.height*5,"PPU budget must include one-byte A8 provenance, not six RGBA copies");
        std::vector<std::uint8_t> packed(layout.width*layout.height+2,0xCC);
        require(pack_pica_layers(image,std::span(packed).subspan(1,packed.size()-2))==mask,"Source layer population mask incorrect");
        require(packed.front()==0xCC && packed.back()==0xCC,"Layer upload crossed allocated storage");
        for(unsigned y=0;y<layout.height;++y) for(unsigned x=0;x<layout.width;++x) {
            const auto ty=y;
            const auto offset=1+((ty/8)*(layout.width/8)+x/8)*64+spread[x%8]+2*spread[ty%8];
            require(packed[offset]==layers[std::min(y,height-1)*layer_pitch+std::min(x,width-1)],"A8 Morton/source row order/row stride/edge padding differs from colour texture");
        }
        if(std::has_single_bit(width) && std::has_single_bit(height)) {
            auto repeated=image;repeated.repeat=true;
            pack_pica_layers(repeated,std::span(packed).subspan(1,packed.size()-2));
            for(unsigned y=0;y<layout.height;++y) for(unsigned x=0;x<layout.width;++x) {
                const auto ty=y;
                const auto offset=1+((ty/8)*(layout.width/8)+x/8)*64+spread[x%8]+2*spread[ty%8];
                require(packed[offset]==layers[(y%height)*layer_pitch+(x%width)],"Repeated A8 guards disagree with repeated RGBA artwork");
            }
        }
        auto bad=image;bad.layer_pitch=width-1;
        rejects([&]{pica_texture_layout(bad);},"Short layer pitch accepted");
        bad=image;bad.source_layers=std::span(layers).first(layer_pitch*(height-1)+width-1);
        rejects([&]{pica_texture_layout(bad);},"Truncated source ownership accepted");
        layers[0]=3;pixels[3]=255;
        rejects([&]{pack_pica_layers(image,std::span(packed).subspan(1,packed.size()-2));},"Ambiguous multi-layer pixel accepted");
        layers[0]=1;pixels[3]=0;
        rejects([&]{validate_pica_layers(image);},"Invisible pixel may not overwrite stencil ownership");
        layers[0]=0;pixels[3]=255;
        rejects([&]{validate_pica_layers(image);},"Opaque unclassified PPU pixel accepted");
        layers[0]=1;pixels[3]=128;
        rejects([&]{validate_pica_layers(image);},"Source layer ID cannot replace translucent alpha");
    }
    std::vector<std::uint8_t> rgba(8*8*4),layers(8*8);PicaImage image{rgba,8,8,32,4,false,layers,8};
    rejects([&]{pack_pica_layers(image,layers);},"A8 upload may not overwrite source provenance");
    rejects([&]{pack_pica_layers(image,std::span(rgba).first(64));},"A8 upload may not overwrite RGBA input");
    Canvas dashboard;const auto plan=plan_frame(1,true,ScreenUse::world);
    std::array<PicaVertex,3> vertices{{{{0,0,0}},{{400,0,0}},{{0,240,0}}}};
    PicaDraw draw{0,3,0,pica_identity,PicaSpace::screen,false,false,true};
    PicaFrame frame{plan,vertices,std::span(&draw,1),std::span(&image,1)};
    validate_pica_frame(frame,dashboard.view());
    draw.space=PicaSpace::world;
    rejects([&]{validate_pica_frame(frame,dashboard.view());},"Per-pixel PPU stencil mask accepted on perspective world geometry");
    draw.space=PicaSpace::screen;vertices[0].colour[3]=.5F;
    rejects([&]{validate_pica_frame(frame,dashboard.view());},"Per-pixel PPU stencil masks cannot silently discard vertex opacity");
    vertices[0].colour[3]=1;draw.source_layer=3;
    rejects([&]{validate_pica_frame(frame,dashboard.view());},"Ambiguous native draw layer accepted");
    std::vector<std::uint8_t> large_rgba(512*512*4),large_layers(512*512);
    const PicaImage large{large_rgba,512,512,512*4,4,false,large_layers,512};
    const std::array<PicaImage,3> images{large,large,large};
    frame.vertices={};frame.draws={};frame.textures=std::span(images).first(2);
    validate_pica_frame(frame,dashboard.view());
    frame.textures=images;
    rejects([&]{validate_pica_frame(frame,dashboard.view());},"Combined 3DS resident budget omitted A8 provenance bytes");
}
void sampled_texture_orientation() {
    // Independent complete sampling path, not just a byte-packing assertion:
    // logical top-left UV -> shader 1-V with padding -> PICA bottom-up sampler
    // -> Morton storage. Asymmetric colour/layer rows catch a double flip.
    constexpr unsigned spread[]{0,1,4,5,16,17,20,21};
    for(unsigned width:{13U,320U,400U}) for(unsigned height:{9U,17U,240U}) {
        std::vector<std::uint8_t> rgba(width*height*4),layers(width*height);
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const auto index=std::size_t(y)*width+x;
            rgba[index*4]=std::uint8_t(x);rgba[index*4+1]=std::uint8_t(y);
            rgba[index*4+2]=std::uint8_t(x+3*y);rgba[index*4+3]=255;
            layers[index]=std::uint8_t(1U<<((x+2*y)%6));
        }
        const PicaImage image{rgba,width,height,width*4,4,false,layers,width};
        const auto layout=pica_texture_layout(image);
        std::vector<std::uint8_t> packed(layout.bytes),packed_layers(layout.bytes/4);
        pack_pica_texture(image,packed);pack_pica_layers(image,packed_layers);
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const double u=(x+.5)/width,v=(y+.5)/height;
            const double shader_u=u*width/layout.width,shader_v=1-v*height/layout.height;
            const auto sample_s=unsigned(std::floor(shader_u*layout.width));
            const auto sample_t=unsigned(std::floor(shader_v*layout.height));
            const unsigned stored_y=layout.height-1-sample_t;
            const auto texel=((stored_y/8)*(layout.width/8)+sample_s/8)*64
                +spread[sample_s%8]+2*spread[stored_y%8];
            const auto source=std::size_t(y)*width+x;
            for(unsigned channel=0;channel<4;++channel)
                require(packed[texel*4+3-channel]==rgba[source*4+channel],
                    "Actual shader/PICA sampling flips or offsets logical artwork");
            require(packed_layers[texel]==layers[source],"Sampled colour and ownership rows disagree");
        }
    }
}
}
#if defined(_MSC_VER)
#define STARFOX_TEXTURE_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define STARFOX_TEXTURE_NOINLINE __attribute__((noinline))
#else
#define STARFOX_TEXTURE_NOINLINE
#endif
STARFOX_TEXTURE_NOINLINE void* operator new(std::size_t count) {
    if(count_texture_allocations)++texture_allocations;
    if(auto* memory=std::malloc(std::max(count,std::size_t{1})))return memory;
    throw std::bad_alloc{};
}
STARFOX_TEXTURE_NOINLINE void* operator new[](std::size_t count) {return ::operator new(count);}
STARFOX_TEXTURE_NOINLINE void operator delete(void* memory) noexcept {std::free(memory);}
STARFOX_TEXTURE_NOINLINE void operator delete[](void* memory) noexcept {std::free(memory);}
STARFOX_TEXTURE_NOINLINE void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
STARFOX_TEXTURE_NOINLINE void operator delete[](void* memory,std::size_t) noexcept {std::free(memory);}
#undef STARFOX_TEXTURE_NOINLINE
int main() try {
    word_texture_store();
    validated_composition_handoff();
    vertex_validation_bits();vertex_validation_draw_policy();texture_upload();full_size_texture_upload();changed_tile_upload();texture_residency();reusable_texture_cache();vertex_residency();projection_and_draws();layer_upload();sampled_texture_orientation();
    require(pica_uv_mode(false,false)==std::array<float,4>{1,0,0,0},"Ordinary texture projection changed");
    require(pica_uv_mode(true,false)==std::array<float,4>{0,1,0,0},"LCD parity texture lost its homogeneous Q");
    require(pica_uv_mode(false,true)==std::array<float,4>{0,0,1,0},"Source terrain mode zeroed ordinary UVs before projection");
    rejects([&]{pica_uv_mode(true,true);},"Conflicting texture projection modes accepted");
    Canvas lower;std::vector<std::uint8_t> pixels(8*8*4,255);
    PicaImage image{pixels,8,8,32,4};
    std::array<PicaVertex,3> terrain{{{{-10,-40,256},{1,1,1,1},{0,0}},{{10,-40,256},{1,1,1,1},{1,0}},{{10,-40,512},{1,1,1,1},{1,1}}}};
    PicaDraw draw;draw.count=3;draw.texture=0;draw.projected_uv=true;
    PicaFrame frame{plan_frame(1,true,ScreenUse::world),terrain,std::span(&draw,1),std::span(&image,1)};
    validate_pica_frame(frame,lower.view());
    const auto valid=[&]{validate_pica_frame(frame,lower.view());};
    draw.screen_dither=true;rejects(valid,"Source terrain projection mixed with parity checker");draw.screen_dither=false;
    draw.texture=pica_no_texture;rejects(valid,"Source terrain projection without artwork");draw.texture=0;
    image.repeat=true;rejects(valid,"A wrapping model texture was mistaken for projected terrain");image.repeat=false;
    draw.space=PicaSpace::screen;draw.depth_test=draw.depth_write=false;
    rejects(valid,"Projected source terrain cannot become a mono overlay");
    std::cout<<"3DS PICA upload/projection/pass contracts: "<<checks<<" checks passed\n";
} catch(const std::exception& error) {count_texture_allocations=false;std::cerr<<error.what()<<'\n';return 1;}
