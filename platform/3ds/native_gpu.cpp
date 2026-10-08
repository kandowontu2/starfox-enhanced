#include "native_gpu.hpp"
#include "starfox/platform/nintendo_3ds/pica_residency.hpp"
#include <atomic>
#include <cstring>
extern "C" {
#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/allocator/linear.h>
#include <3ds/gfx.h>
#include <3ds/gpu/gpu.h>
#include <3ds/gpu/shaderProgram.h>
#include <3ds/gpu/gx.h>
}
#include <citro3d.h>

namespace starfox::platform::nintendo_3ds {
namespace {
constexpr unsigned command_bytes=1024*1024;
constexpr u32 transfer_flags=GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)
    |GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)
    |GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
std::atomic_flag gpu_owned=ATOMIC_FLAG_INIT;
struct GpuLease {
    GpuLease() {if(gpu_owned.test_and_set()) throw std::logic_error("3DS GPU presenter already owned");}
    ~GpuLease() {gpu_owned.clear();}
};
struct FrameEnd {
    // All changed buffers/textures are explicitly flushed. Do not flush the
    // entire linear heap (including unrelated PCM/unchanged art) every frame.
    ~FrameEnd() {C3D_FrameEnd(GX_CMDLIST_FLUSH);}
};
u32 clear_colour(Rgb colour) {return (u32(colour.r)<<24)|(u32(colour.g)<<16)|(u32(colour.b)<<8)|255;}
void upload_matrix(int location,const PicaMatrix& rows) {
    C3D_Mtx matrix{};
    for(unsigned i=0;i<4;++i) {
        matrix.r[i].x=rows[i][0];matrix.r[i].y=rows[i][1];
        matrix.r[i].z=rows[i][2];matrix.r[i].w=rows[i][3];
    }
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,location,&matrix);
}
struct ResidentTexture {
    C3D_Tex texture{};
    C3D_Tex layers{};
    PicaTextureResidency cache;
    unsigned width{},height{},channels{};
    unsigned layer_width{},layer_height{},layer_classes{};
    bool ready{},repeat{};
    bool layers_ready{},layer_repeat{};
    void release_layers() noexcept {
        if(layers_ready) C3D_TexDelete(&layers);
        layers={};layers_ready=false;layer_repeat=false;cache.release_layers();layer_width=layer_height=layer_classes=0;
    }
    void release_colour() noexcept {
        if(ready) C3D_TexDelete(&texture);
        texture={};ready=false;cache.release_colour();width=height=channels=0;repeat=false;
    }
    void release() noexcept {
        release_colour();
        release_layers();
    }
    void prepare_layout(PicaImage image) {
        const auto keep=pica_texture_retention(image,ready?texture.width:0,ready?texture.height:0,
            layers_ready?layers.width:0,layers_ready?layers.height:0);
        if(!keep.colour) release_colour();
        if(!keep.layers) release_layers();
    }
    void update(PicaImage image) {
        cache.prepare(image,[&](PicaImage source,PicaTextureLayout layout) {
            if(!ready || texture.width!=layout.width || texture.height!=layout.height) {
                if(ready) C3D_TexDelete(&texture);
                texture={};ready=false;
                if(!C3D_TexInit(&texture,layout.width,layout.height,GPU_RGBA8))
                    throw std::runtime_error("3DS GPU texture allocation failed");
                ready=true;
            }
            pack_pica_texture(source,{static_cast<std::uint8_t*>(texture.data),layout.bytes});
            C3D_TexSetFilter(&texture,GPU_NEAREST,GPU_NEAREST);
            C3D_TexSetWrap(&texture,image.repeat?GPU_REPEAT:GPU_CLAMP_TO_EDGE,image.repeat?GPU_REPEAT:GPU_CLAMP_TO_EDGE);
            C3D_TexFlush(&texture);
            width=image.width;height=image.height;channels=image.channels;repeat=image.repeat;
        },[&](PicaImage source,PicaTextureLayout layout,unsigned classes) {
            if(source.source_layers.empty()) {
                if(layers_ready)C3D_TexDelete(&layers);
                layers={};layers_ready=false;layer_repeat=false;layer_width=layer_height=layer_classes=0;
            }
            else {
                if(!layers_ready || layers.width!=layout.width || layers.height!=layout.height) {
                    if(layers_ready)C3D_TexDelete(&layers);
                    layers={};layers_ready=false;
                    if(!C3D_TexInit(&layers,layout.width,layout.height,GPU_A8))
                        throw std::runtime_error("3DS GPU source-layer allocation failed");
                    layers_ready=true;
                }
                pack_pica_layers(source,{static_cast<std::uint8_t*>(layers.data),layout.width*layout.height});
                C3D_TexSetFilter(&layers,GPU_NEAREST,GPU_NEAREST);
                C3D_TexSetWrap(&layers,image.repeat?GPU_REPEAT:GPU_CLAMP_TO_EDGE,image.repeat?GPU_REPEAT:GPU_CLAMP_TO_EDGE);
                C3D_TexFlush(&layers);
                layer_width=image.width;layer_height=image.height;layer_classes=classes;layer_repeat=image.repeat;
            }
        });
    }
};
} // namespace
struct NativeGpu::Impl {
    GpuLease lease;
    bool initialized{},program_ready{};
    PicaVertex* vbo{};
    PicaVertexResidency vertices;
    std::vector<u32> shader_words;
    DVLB_s* library{};
    shaderProgram_s program{};
    int transform_location{-1},uv_location{-1},uv_mode_location{-1};
    std::array<C3D_RenderTarget*,2> top{};
    C3D_RenderTarget* bottom{};
    std::array<ResidentTexture,pica_texture_limit> textures;
    ResidentTexture dashboard;
    Impl(std::span<const std::uint8_t> shader) {
        if(shader.size()<32 || shader.size()>65536 || shader.size()%4 || !shader.data()
            || std::memcmp(shader.data(),"DVLB",4)) throw std::invalid_argument("Invalid 3DS GPU shader library");
        if(!C3D_Init(command_bytes)) throw std::runtime_error("Citro3D initialization failed");
        initialized=true;
        try {
            shader_words.resize(shader.size()/4);std::memcpy(shader_words.data(),shader.data(),shader.size());
            library=DVLB_ParseFile(shader_words.data(),shader.size());
            if(!library || library->numDVLE!=1) throw std::runtime_error("3DS GPU vertex shader parse failed");
            if(R_FAILED(shaderProgramInit(&program))) throw std::runtime_error("3DS GPU program allocation failed");
            program_ready=true;
            if(R_FAILED(shaderProgramSetVsh(&program,&library->DVLE[0]))) throw std::runtime_error("3DS GPU vertex shader binding failed");
            transform_location=shaderInstanceGetUniformLocation(program.vertexShader,"transform");
            uv_location=shaderInstanceGetUniformLocation(program.vertexShader,"uv_scale");
            uv_mode_location=shaderInstanceGetUniformLocation(program.vertexShader,"uv_mode");
            if(transform_location<0 || uv_location<0 || uv_mode_location<0)
                throw std::runtime_error("3DS GPU shader uniforms missing");
            top[0]=make_target(top_width,GFX_TOP,GFX_LEFT,true);
            bottom=make_target(bottom_width,GFX_BOTTOM,GFX_LEFT,false);
            vbo=static_cast<PicaVertex*>(linearAlloc((pica_vertex_limit+12)*sizeof(PicaVertex)));
            if(!vbo) throw std::runtime_error("3DS GPU vertex-buffer allocation failed");
            const std::array<Point3,4> corners{{{0,0,0},{float(bottom_width),0,0},
                {float(bottom_width),float(screen_height),0},{0,float(screen_height),0}}};
            const std::array<std::array<float,2>,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
            unsigned index=pica_vertex_limit;
            for(auto corner:{0U,1U,2U,0U,2U,3U}) vbo[index++]={corners[corner],{1,1,1,1},uv[corner]};
            const std::array<Point3,4> backdrop{{{0,0,0},{float(top_width),0,0},
                {float(top_width),float(screen_height),0},{0,float(screen_height),0}}};
            for(auto corner:{0U,1U,2U,0U,2U,3U}) vbo[index++]={backdrop[corner],{1,1,1,1},{}};
            if(R_FAILED(GSPGPU_FlushDataCache(vbo+pica_vertex_limit,12*sizeof(PicaVertex))))
                throw std::runtime_error("3DS GPU HUD vertex flush failed");
        } catch(...) {shutdown();throw;}
    }
    ~Impl() {shutdown();}
    static C3D_RenderTarget* make_target(unsigned width,gfxScreen_t screen,gfx3dSide_t side,bool depth) {
        auto* target=C3D_RenderTargetCreate(screen_height,width,GPU_RB_RGBA8,
            depth?C3D_DEPTHTYPE(GPU_RB_DEPTH24_STENCIL8):C3D_DEPTHTYPE(-1));
        if(!target) throw std::runtime_error("3DS GPU LCD/depth target allocation failed");
        C3D_RenderTargetSetOutput(target,screen,side,transfer_flags);return target;
    }
    void shutdown() noexcept {
        // Fini waits for command/transfer completion and owns target cleanup.
        // Shader data, textures and the shared VBO stay alive until it returns.
        if(initialized) {C3D_Fini();initialized=false;}
        for(auto& texture:textures) texture.release();
        dashboard.release();
        if(vbo) {linearFree(vbo);vbo=nullptr;}
        if(program_ready) {shaderProgramFree(&program);program_ready=false;}
        if(library) {DVLB_Free(library);library=nullptr;}
    }
    void configure() {
        C3D_BindProgram(&program);
        auto* attrs=C3D_GetAttrInfo();AttrInfo_Init(attrs);
        AttrInfo_AddLoader(attrs,0,GPU_FLOAT,3);AttrInfo_AddLoader(attrs,1,GPU_FLOAT,4);AttrInfo_AddLoader(attrs,2,GPU_FLOAT,2);
        auto* buffers=C3D_GetBufInfo();BufInfo_Init(buffers);
        if(BufInfo_Add(buffers,vbo,sizeof(PicaVertex),3,0x210)<0) throw std::runtime_error("3DS GPU vertex layout rejected");
        C3D_CullFace(GPU_CULL_NONE); // Source visibility is not generic winding.
        C3D_DepthMap(true,-1,0);C3D_AlphaTest(true,GPU_GREATER,0);
        C3D_EarlyDepthTest(false,GPU_EARLYDEPTH_GEQUAL,0);
        C3D_SetScissor(GPU_SCISSOR_DISABLE,0,0,0,0);
        for(unsigned stage=0;stage<6;++stage) C3D_TexEnvInit(C3D_GetTexEnv(stage));
    }
    void material(ResidentTexture* texture,bool alpha,bool depth,bool write,bool screen_dither=false,
        std::array<std::uint8_t,4> odd={},bool projected_uv=false) {
        auto* env=C3D_GetTexEnv(0);C3D_TexEnvInit(env);
        C3D_TexEnvInit(C3D_GetTexEnv(1));
        // Citro3D permits null on unit 0, but units 1/2 first dereference the
        // texture to check its type. The default TEV stage ignores unit 1;
        // bind the already-resident 2D dashboard until an ownership mask is
        // selected. This also retires stale mask pointers after layout changes,
        // with no extra texture storage or artificial sampled colour.
        C3D_TexBind(1,&dashboard.texture);
        C3D_AlphaTest(true,GPU_GREATER,0);
        if(texture) {
            texture->texture.param=(texture->texture.param&~GPU_TEXTURE_MODE(7))
                |GPU_TEXTURE_MODE(screen_dither || projected_uv?GPU_TEX_PROJECTION:GPU_TEX_2D);
            C3D_TexBind(0,&texture->texture);
            if(screen_dither) {
                C3D_TexEnvSrc(env,C3D_Both,GPU_CONSTANT,GPU_PRIMARY_COLOR,GPU_TEXTURE0);
                C3D_TexEnvFunc(env,C3D_Both,GPU_INTERPOLATE);
                C3D_TexEnvColor(env,u32(odd[0])|(u32(odd[1])<<8)|(u32(odd[2])<<16)|(u32(odd[3])<<24));
            } else {
                C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,GPU_TEXTURE0);
                C3D_TexEnvFunc(env,C3D_Both,GPU_MODULATE);
            }
            C3D_FVUnifSet(GPU_VERTEX_SHADER,uv_location,float(texture->width)/texture->texture.width,
                float(texture->height)/texture->texture.height,0,0);
        } else {
            C3D_TexBind(0,nullptr);C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);C3D_FVUnifSet(GPU_VERTEX_SHADER,uv_location,1,1,0,0);
        }
        const auto mode=pica_uv_mode(screen_dither,projected_uv);
        C3D_FVUnifSet(GPU_VERTEX_SHADER,uv_mode_location,mode[0],mode[1],mode[2],mode[3]);
        C3D_DepthTest(depth,GPU_GEQUAL,write?GPU_WRITE_ALL:GPU_WRITE_COLOR);
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,alpha?GPU_SRC_ALPHA:GPU_ONE,
            alpha?GPU_ONE_MINUS_SRC_ALPHA:GPU_ZERO,GPU_ONE,alpha?GPU_ONE_MINUS_SRC_ALPHA:GPU_ZERO);
    }
    static void source_layer(unsigned layer) {
        C3D_StencilTest(true,GPU_ALWAYS,layer,63,63);
        C3D_StencilOp(GPU_STENCIL_KEEP,GPU_STENCIL_KEEP,GPU_STENCIL_REPLACE);
    }
    static void colour_operation(PicaColourOp op) {
        // Test the winning pixel's actual source layer, not its palette index
        // or model bounding box. Colour effects never replace that ownership.
        C3D_StencilTest(true,GPU_NOTEQUAL,0,op.layers,0);
        C3D_StencilOp(GPU_STENCIL_KEEP,GPU_STENCIL_KEEP,GPU_STENCIL_KEEP);
        // Halve BOTH terms before the saturating add/subtract, not the already
        // clipped result. PICA's 8-bit blend constant rounds differently from
        // SNES 5-bit math; physical pixel fidelity remains an explicit gate.
        C3D_BlendingColor(0x80000000);
        const auto factor=op.half?GPU_CONSTANT_ALPHA:GPU_ONE;
        C3D_AlphaBlend(op.subtract?GPU_BLEND_REVERSE_SUBTRACT:GPU_BLEND_ADD,
            GPU_BLEND_ADD,factor,factor,GPU_ZERO,GPU_ONE);
    }
};
NativeGpu::NativeGpu(std::span<const std::uint8_t> shader):impl_(std::make_unique<Impl>(shader)) {}
NativeGpu::~NativeGpu()=default;
void NativeGpu::present(const PicaFrame& frame,ImageView lower) {
    validate_pica_frame(frame,lower);
    if(!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) throw std::runtime_error("3DS GPU frame unavailable");
    FrameEnd end; // Includes failure exits; uploads complete before targets are marked used.
    update_pica_texture_residency(frame.textures,{lower.pixels,lower.width,lower.height,lower.pitch,3},
        std::span(impl_->textures),impl_->dashboard);
    impl_->vertices.prepare(frame.vertices,[&](std::span<const PicaVertex> source) {
        std::memcpy(impl_->vbo,source.data(),source.size_bytes());
        if(R_FAILED(GSPGPU_FlushDataCache(impl_->vbo,source.size_bytes())))
            throw std::runtime_error("3DS GPU geometry flush failed");
    });
    if(frame.plan.stereo && !impl_->top[1]) impl_->top[1]=Impl::make_target(top_width,GFX_TOP,GFX_RIGHT,true);
    impl_->configure();gfxSet3D(frame.plan.stereo);
    for(unsigned eye=0;eye<frame.plan.eye_count;++eye) {
        auto* target=impl_->top[eye];C3D_RenderTargetClear(target,C3D_CLEAR_ALL,clear_colour(frame.clear),0);
        if(!C3D_FrameDrawOn(target)) throw std::runtime_error("3DS GPU eye target unavailable");
        // Seed untouched pixels as the SNES backdrop without depending on an
        // unverified packed depth/stencil clear word. The reserved six vertices
        // write only stencil: neither RGB nor reversed-Z depth is changed.
        C3D_SetScissor(GPU_SCISSOR_DISABLE,0,0,0,0);
        upload_matrix(impl_->transform_location,pica_screen_matrix(top_width));
        impl_->material(nullptr,false,false,false);Impl::source_layer(32);
        C3D_DepthTest(false,GPU_ALWAYS,static_cast<GPU_WRITEMASK>(0));
        C3D_DrawArrays(GPU_TRIANGLES,pica_vertex_limit+6,6);
        for(const auto& draw:frame.draws) {
            if(draw.clip) {
                const auto bounds=pica_screen_scissor(*draw.clip);
                C3D_SetScissor(GPU_SCISSOR_NORMAL,bounds[0],bounds[1],bounds[2],bounds[3]);
            } else C3D_SetScissor(GPU_SCISSOR_DISABLE,0,0,0,0);
            upload_matrix(impl_->transform_location,pica_draw_matrix(frame.plan,eye,draw));
            auto* texture=draw.texture==pica_no_texture?nullptr:&impl_->textures[draw.texture];
            impl_->material(texture,
                draw.alpha_blend,draw.depth_test,draw.depth_write,draw.screen_dither,draw.dither_odd,draw.projected_uv);
            if(draw.colour_op) {
                Impl::colour_operation(*draw.colour_op);
                C3D_DrawArrays(GPU_TRIANGLES,draw.first,draw.count);
            } else if(texture && texture->layers_ready) {
                auto* layer_env=C3D_GetTexEnv(1);
                C3D_TexBind(1,&texture->layers);
                C3D_TexEnvSrc(layer_env,C3D_Alpha,GPU_TEXTURE1);
                C3D_TexEnvFunc(layer_env,C3D_Alpha,GPU_REPLACE);
                // The alpha channel now carries a layer ID, not opacity. PPU
                // covered texels are opaque; RGB must not blend by that ID.
                C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);
                for(unsigned layer:{1U,2U,4U,8U,16U,32U}) if(texture->layer_classes&layer) {
                    C3D_AlphaTest(true,GPU_EQUAL,layer);Impl::source_layer(layer);
                    C3D_DrawArrays(GPU_TRIANGLES,draw.first,draw.count);
                }
            } else {
                Impl::source_layer(draw.source_layer);
                C3D_DrawArrays(GPU_TRIANGLES,draw.first,draw.count);
            }
        }
    }
    C3D_RenderTargetClear(impl_->bottom,C3D_CLEAR_COLOR,0,0);
    if(!C3D_FrameDrawOn(impl_->bottom)) throw std::runtime_error("3DS GPU HUD target unavailable");
    upload_matrix(impl_->transform_location,pica_screen_matrix(bottom_width));
    C3D_SetScissor(GPU_SCISSOR_DISABLE,0,0,0,0); // Never inherit upper-LCD effect masks into the cockpit HUD.
    C3D_StencilTest(false,GPU_ALWAYS,0,63,0);
    C3D_StencilOp(GPU_STENCIL_KEEP,GPU_STENCIL_KEEP,GPU_STENCIL_KEEP);
    impl_->material(&impl_->dashboard,false,false,false);
    C3D_DrawArrays(GPU_TRIANGLES,pica_vertex_limit,6);
}
} // namespace starfox::platform::nintendo_3ds
