// Phase 1C fixture: GPU FAST batched model raster vs sequential drawing.
// Each case renders one retail model list three ways and requires identical
// pixels, layer tags, write coverage, surfaces and depth:
//   1. full-frame sequential raster (bounded_raster off),
//   2. bounded in-place sequential raster (1B; STARFOX_TEST_UNBATCHED_MODEL_RASTER=1),
//   3. the default GPU FAST path (batched once 1C lands).
// Cases cover 1x/2x/4x/10x; flat, banked and near-plane views; overlapping
// and spread-out models; textured, line and surface-bearing faces; and batch
// splits (emissive model, surface-metadata change, small arena).
#include "starfox/render/gpu_scene.hpp"
#include "starfox/render/gpu_scene_counters.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <optional>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace starfox::render;
using starfox::simulation::MatrixQ15;

struct Readback {
    std::vector<std::uint8_t> pixels,tags,coverage;
    std::vector<SurfaceSample> surfaces;
};

Readback render(GpuScene& scene,void* device,unsigned scale,const std::vector<GpuSceneDraw>& draws,
    unsigned logical_width=224,unsigned logical_height=192) {
    const unsigned w=logical_width*scale,h=logical_height*scale;
    Framebuffer frame(logical_width,logical_height,scale);frame.enable_layer_tags(true);frame.begin_write_coverage();
    SurfaceBuffer surfaces(w,h);
    if(!scene.render_resident(device,w,h,draws) || !scene.readback(frame,&surfaces))
        throw std::runtime_error(scene.status());
    const auto coverage=frame.write_coverage();const auto samples=surfaces.samples();
    return {frame.pixels(),frame.layer_tags(),{coverage.begin(),coverage.end()},{samples.begin(),samples.end()}};
}

void compare(const Readback& a,const Readback& b,const std::string& what) {
    if(a.pixels.size()!=b.pixels.size()) throw std::runtime_error(what+": size mismatch");
    for(std::size_t i=0;i<a.pixels.size();++i) {
        const auto& x=a.surfaces[i];const auto& y=b.surfaces[i];
        if(a.pixels[i]!=b.pixels[i] || a.tags[i]!=b.tags[i] || a.coverage[i]!=b.coverage[i]
            || x.valid!=y.valid || (x.valid && (x.palette_index!=y.palette_index || x.depth!=y.depth
                || x.normal_x!=y.normal_x || x.normal_y!=y.normal_y || x.normal_z!=y.normal_z)))
            throw std::runtime_error(what+": mismatch at pixel "+std::to_string(i));
    }
}

// Counter totals since the last call (scene counters are always on here).
std::uint64_t take(scene_counters::Counter counter) {
    return scene_counters::frame_totals()[std::size_t(counter)].exchange(0);
}
std::uint64_t take_bounded() {return take(scene_counters::Counter::bounded_dispatches);}

// The renderer reads switches with std::getenv; on Windows SDL's environment
// is separate from the C runtime's, so set both.
void set_env(const char* name,const char* value) {
#if defined(_WIN32)
    _putenv_s(name,value?value:"");
#else
    if(value) setenv(name,value,1);else unsetenv(name);
#endif
    if(value) SDL_setenv_unsafe(name,value,1);else SDL_unsetenv_unsafe(name);
}

void set_unbatched(bool unbatched) {
    set_env("STARFOX_TEST_UNBATCHED_MODEL_RASTER",unbatched?"1":nullptr);
}
} // namespace

int main(int argc,char** argv) try {
    if(argc!=3) throw std::runtime_error("usage: starfox_gpu_model_batches_check ROM SYMBOLS");
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    const starfox::assets::ShapeDecoder decoder(rom,symbols);
    // Retail shapes in address order: the first solid models with faces, plus
    // one shape made only of lines.
    std::vector<starfox::assets::Shape> solids;
    std::optional<starfox::assets::Shape> lines;
    std::set<std::uint32_t> seen;
    for(const auto& [name,addresses]:symbols.entries()) for(auto address:addresses) {
        if(!seen.insert(address).second || !decoder.looks_like_shape_header(address)) continue;
        auto shape=decoder.decode(address,name);
        if(shape.faces.empty() && shape.face_batches.empty()) continue;
        const auto all_lines=std::all_of(shape.faces.begin(),shape.faces.end(),[](const auto& f){return f.is_line();});
        if(all_lines && !shape.faces.empty()) {if(!lines) lines=std::move(shape);}
        else if(solids.size()<16) solids.push_back(std::move(shape));
    }
    if(solids.size()<16 || !lines) throw std::runtime_error("Fixture needs 16 solid shapes and one line shape");

    // Count bounded dispatches, so a silent fallback cannot pass.
    set_env("STARFOX_TRACE_SCENE_COST","1");
    if(!scene_counters::enabled()) throw std::runtime_error("Scene counters unavailable");
    if(!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    auto* device=SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV|SDL_GPU_SHADERFORMAT_DXIL|SDL_GPU_SHADERFORMAT_MSL,true,nullptr);
    if(!device) throw std::runtime_error(SDL_GetError());
    std::cout<<"GPU adapter: "<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")<<'\n';
    GpuScene full_frame,sequential,batched;
    unsigned cases=0;std::uint64_t bounded_total=0,compact_total=0;
    const char* view_names[]{"flat","banked","near-plane"};
    const char* layout_names[]{"overlap","spread","splits"};
    for(unsigned scale:{1U,2U,4U,10U}) for(unsigned view=0;view<3;++view) for(unsigned layout=0;layout<3;++layout) {
        RenderSettings settings;settings.render_scale=scale;
        RenderPose base;base.use_rotation_matrix=true;base.continuous_geometry=base.subpixel_projection=true;
        base.vanish_x=112;base.vanish_y=96;
        base.rotation_matrix=view==1?MatrixQ15{32137,-6393,0,6393,32137,0,0,0,32767}:MatrixQ15{32767,0,0,0,32767,0,0,0,32767};
        const std::string what=std::string(layout_names[layout])+" "+view_names[view]+" "+std::to_string(scale)+"x";
        std::vector<GpuSceneDraw> draws;
        const auto add=[&](const starfox::assets::Shape& shape,double x,double y,double z,bool surfaces,bool emissive) {
            auto pose=base;pose.x=x;pose.y=y;pose.z=z;
            GpuModelDraw draw{&shape,pose,settings,surfaces};
            draw.geometry_depth=true;draw.emissive=emissive;
            draws.emplace_back(draw);
        };
        // The first model establishes the running scene; later ones fuse.
        add(solids[0],0,0,view==2?40:900,true,false);
        for(unsigned k=1;k<16;++k) {
            const auto& shape=solids[k];
            const double near=view==2?24+3.0*k:180+40.0*k;
            if(layout==0) add(shape,(int(k%3)-1)*6.0,(int(k%2)*2-1)*4.0,near,true,false);
            else if(layout==1) add(shape,(int(k%5)-2)*90.0,(int(k/5)-1)*60.0,near*2,true,false);
            else add(shape,(int(k%3)-1)*8.0,0,near,k!=9,k==4 || k==5); // k==4 lands on the renderer holding the running scene
            if(layout==2 && k==7) add(*lines,0,0,near,true,false);
        }
        for(auto& draw:draws) std::get<GpuModelDraw>(draw).bounded_raster=false;
        take_bounded();take(scene_counters::Counter::compact_tile_lists);
        const auto reference=render(full_frame,device,scale,draws);
        if(take_bounded()) throw std::runtime_error("Full-frame reference took the bounded path: "+what);
        for(auto& draw:draws) std::get<GpuModelDraw>(draw).bounded_raster=true;
        set_unbatched(true);
        const auto bounded=render(sequential,device,scale,draws);
        set_unbatched(false);
        const auto dispatches=take_bounded();
        if(!dispatches) throw std::runtime_error("No bounded model raster dispatched: "+what);
        bounded_total+=dispatches;
        // STARFOX_TEST_COMPACT_SPAN_TILES must really switch the bounded
        // rasters to compact tile lists.
        const auto compact=take(scene_counters::Counter::compact_tile_lists);
        if(std::getenv("STARFOX_TEST_COMPACT_SPAN_TILES") && compact<dispatches)
            throw std::runtime_error("Compact tile lists not used: "+what);
        compact_total+=compact;
        // Splits also run with an arena too small for more than a few models.
        if(layout==2) set_env("STARFOX_TEST_MODEL_BATCH_ARENA_BYTES","4194304");
        const auto candidate=render(batched,device,scale,draws);
        set_env("STARFOX_TEST_MODEL_BATCH_ARENA_BYTES",nullptr);
        compare(reference,bounded,"bounded vs full-frame "+what);
        compare(bounded,candidate,"batched vs sequential "+what);
        const auto drawn=std::count_if(reference.coverage.begin(),reference.coverage.end(),[](std::uint8_t b){return b!=0;});
        if(!drawn) throw std::runtime_error("Fixture case drew nothing: "+what);
        ++cases;
    }
    // Compact row kernels own only 128 columns. Wider public-API frames
    // must keep the exact dense/serial fallback, not drop the right edge.
    {
        constexpr unsigned width=8256,height=64;
        RenderSettings settings;settings.render_scale=1;
        std::vector<GpuSceneDraw> draws;
        for(unsigned k=0;k<16;++k) {
            RenderPose pose;pose.continuous_geometry=pose.subpixel_projection=true;
            pose.vanish_x=8232;pose.vanish_y=32;pose.z=160+40*k;
            GpuModelDraw draw{&solids[k],pose,settings,true};draw.geometry_depth=true;
            draws.emplace_back(draw);
        }
        const auto reference=render(full_frame,device,1,draws,width,height);
        for(auto& draw:draws) std::get<GpuModelDraw>(draw).bounded_raster=true;
        take(scene_counters::Counter::compact_tile_lists);
        const auto fallback=render(sequential,device,1,draws,width,height);
        if(take(scene_counters::Counter::compact_tile_lists))
            throw std::runtime_error("Oversized frame entered the 128-column compact kernel");
        compare(reference,fallback,"wide right-edge fallback");
        bool right=false;
        for(unsigned y=0;y<height;++y) for(unsigned x=8192;x<width;++x)
            right|=reference.coverage[std::size_t(y)*width+x]!=0;
        if(!right) throw std::runtime_error("Wide fallback fixture never drew past compact column 128");
        ++cases;
    }
    full_frame.release_device();sequential.release_device();batched.release_device();
    SDL_DestroyGPUDevice(device);SDL_Quit();
    std::cout<<cases<<" model batch cases: full-frame, bounded and batched rasters identical"
        " (colours, layer tags, coverage, surfaces, depth); "<<bounded_total<<" bounded dispatches, "
        <<compact_total<<" compact tile lists\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
