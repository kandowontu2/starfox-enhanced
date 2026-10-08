// Real owner/presentation lifetime check. Authored CURRENT ray inputs below
// are diagnostic fixtures. Downloads are assertions only: no CPU-built index,
// accepted-source feature table or CPU lookup enters the production owner.
#include "starfox/render/gpu_calibrated_reflection_history.hpp"
#include "starfox/render/gpu_preparation.hpp"
#include "reflected_curved_device.hpp"
#include "../tests/reflected_curved_owner_oracle.hpp"
#include "full_curved_owner_replay.hpp"
#include <algorithm>
#include <bit>
#include <cfloat>
#include <cstring>
#include <iostream>
#include <memory>
#include <unordered_set>
#include <vector>
#if defined(STARFOX_SOURCE_SCHEDULE_CHECK_AVAILABLE)
#include "native_reflection_index_optical_schedule_dxil.hpp"
#include "native_reflection_index_optical_schedule_spirv.hpp"
#endif
#if defined(_WIN32)
#include "starfox/render/sdl_d3d12_bridge.h"
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#undef near
#undef far
#endif
namespace {
using namespace starfox::render;
void check(bool b,const char* message) {if(!b)throw std::runtime_error(message);}
using Words=std::vector<unsigned>;
struct Gpu {
    SDL_GPUDevice* device{};
#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> validation;
#endif
    ~Gpu(){if(device)SDL_DestroyGPUDevice(device);SDL_Quit();}
    Gpu(const char* backend,bool low) {
        check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());device=create_reflected_curved_device(backend,low);check(device,SDL_GetError());
        require_reflected_curved_precision(device,backend);
        std::cout<<"Source owner GPU="<<SDL_GetStringProperty(SDL_GetGPUDeviceProperties(device),SDL_PROP_GPU_DEVICE_NAME_STRING,"unknown")
            <<" backend="<<backend<<std::endl;
#if defined(_WIN32)
        if(std::string_view(backend)=="direct3d12") {
            auto* native=static_cast<ID3D12Device*>(SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(device),STARFOX_SDL_D3D12_DEVICE,nullptr));
            check(native && SUCCEEDED(native->QueryInterface(IID_ID3D12InfoQueue,reinterpret_cast<void**>(validation.GetAddressOf()))),
                "Missing D3D12 validation queue");
            D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
            D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=severities;
            check(SUCCEEDED(validation->PushStorageFilter(&filter)),"Missing D3D12 storage filter");
        }
#endif
    }
    void finish() {
        check(SDL_WaitForGPUIdle(device),SDL_GetError());SDL_DestroyGPUDevice(device);device=nullptr;
#if defined(_WIN32)
        if(validation) {
            D3D12_MESSAGE_SEVERITY severities[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
            D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=severities;
            check(SUCCEEDED(validation->PushRetrievalFilter(&filter)),"Missing D3D12 retrieval filter");
            const auto count=validation->GetNumStoredMessagesAllowedByRetrievalFilter();
            for(UINT64 n=0;n<count;++n) {
                SIZE_T bytes{};check(SUCCEEDED(validation->GetMessage(n,nullptr,&bytes)),"Missing D3D12 message");
                std::vector<unsigned char> storage(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                check(SUCCEEDED(validation->GetMessage(n,message,&bytes)),"Missing D3D12 message content");std::cerr<<message->pDescription<<'\n';
            }
            check(!count && !validation->GetNumMessagesDiscardedByMessageCountLimit(),"Critical/discarded D3D12 validation messages");
            std::cout<<"Zero critical/discarded D3D12 messages through device teardown.\n";
        }
#endif
    }
    void submit(SDL_GPUCommandBuffer* command) {
        auto* fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);check(fence,SDL_GetError());
        const bool waited=SDL_WaitForGPUFences(device,true,&fence,1);SDL_ReleaseGPUFence(device,fence);check(waited,SDL_GetError());
    }
    Words read(void* buffer,unsigned bytes) {
        SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,bytes,0};
        auto* transfer=SDL_CreateGPUTransferBuffer(device,&info);check(transfer,SDL_GetError());
        struct Release {Gpu& gpu;SDL_GPUTransferBuffer* transfer;~Release(){SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);}} release{*this,transfer};
        auto* command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
        const SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(buffer),0,bytes};const SDL_GPUTransferBufferLocation to{transfer,0};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);submit(command);
        const auto* mapped=SDL_MapGPUTransferBuffer(device,transfer,false);check(mapped,SDL_GetError());
        Words words(bytes/4);std::memcpy(words.data(),mapped,bytes);SDL_UnmapGPUTransferBuffer(device,transfer);return words;
    }
};
struct Buffer {
    Gpu& gpu;SDL_GPUBuffer* value{};
    ~Buffer(){if(value)SDL_ReleaseGPUBuffer(gpu.device,value);}
    Buffer(Gpu& g,const Words& words,SDL_GPUBufferUsageFlags extra_usage=0):gpu(g) {
        const unsigned bytes=unsigned(words.size()*4);
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE|extra_usage,bytes,0};
        value=SDL_CreateGPUBuffer(gpu.device,&info);check(value,SDL_GetError());
        SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,bytes,0};auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&ti);check(transfer,SDL_GetError());
        auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);check(mapped,SDL_GetError());std::memcpy(mapped,words.data(),bytes);SDL_UnmapGPUTransferBuffer(gpu.device,transfer);
        auto* command=SDL_AcquireGPUCommandBuffer(gpu.device);check(command,SDL_GetError());auto* pass=SDL_BeginGPUCopyPass(command);check(pass,SDL_GetError());
        const SDL_GPUTransferBufferLocation from{transfer,0};const SDL_GPUBufferRegion to{value,0,bytes};
        SDL_UploadToGPUBuffer(pass,&from,&to,false);SDL_EndGPUCopyPass(pass);gpu.submit(command);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
    }
};
struct OwnerTexture {
    Gpu& gpu;SDL_GPUTexture* value{};
    ~OwnerTexture(){if(value)SDL_ReleaseGPUTexture(gpu.device,value);}
    OwnerTexture(Gpu& g,unsigned w,unsigned h,bool curved,bool liquid_patch=false,const Words* raw_owners=nullptr):gpu(g) {
        SDL_GPUTextureCreateInfo info{};info.type=SDL_GPU_TEXTURETYPE_2D;info.format=SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;info.width=w;info.height=h;info.layer_count_or_depth=info.num_levels=1;
        value=SDL_CreateGPUTexture(gpu.device,&info);check(value,SDL_GetError());
        Words owners(w*h,0xff0280ffU);for(unsigned y=0;y<h;++y)owners[y*w]=0;
        if(curved) {
            for(unsigned y=0;y<h;++y)owners[y*w+4]=0xff010001U;
            if(liquid_patch) {
                check(w>=6 && h>=8,"Diagnostic liquid source patch exceeds the owner image");
                for(unsigned y=5;y<=7;++y)for(unsigned x=3;x<=5;++x)owners[y*w+x]=0xff010001U;
            }
            owners[9]=0; // A second independently protected CURRENT sample.
        }
        if(raw_owners) {
            check(raw_owners->size()==owners.size(),"Diagnostic ownership input has a different extent");
            owners=*raw_owners;
        }
        const unsigned bytes=w*h*4;SDL_GPUTransferBufferCreateInfo ti{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,bytes,0};
        auto* transfer=SDL_CreateGPUTransferBuffer(gpu.device,&ti);check(transfer,SDL_GetError());
        auto* mapped=SDL_MapGPUTransferBuffer(gpu.device,transfer,false);check(mapped,SDL_GetError());std::memcpy(mapped,owners.data(),bytes);SDL_UnmapGPUTransferBuffer(gpu.device,transfer);
        auto* command=SDL_AcquireGPUCommandBuffer(gpu.device);check(command,SDL_GetError());auto* pass=SDL_BeginGPUCopyPass(command);check(pass,SDL_GetError());
        SDL_GPUTextureTransferInfo from{};from.transfer_buffer=transfer;from.pixels_per_row=w;from.rows_per_layer=h;
        SDL_GPUTextureRegion to{};to.texture=value;to.w=w;to.h=h;to.d=1;
        SDL_UploadToGPUTexture(pass,&from,&to,false);SDL_EndGPUCopyPass(pass);gpu.submit(command);SDL_ReleaseGPUTransferBuffer(gpu.device,transfer);
    }
};
shadows::NativeReflectionHistory native_layout(unsigned w,unsigned h,unsigned lobes,bool curved,bool scene=false) {
    const auto water=curved?*shadows::native_water_layers(w,h,true):shadows::NativeWaterLayers{};
    return *shadows::native_reflection_history(w,h,{w,h},true,water,lobes,true,scene,curved,curved);
}
Words fixture(unsigned w,unsigned h,unsigned lobes,unsigned feature_salt,unsigned colour_salt,bool curved,unsigned liquid_material,bool scene=false) {
    const auto r=native_layout(w,h,lobes,curved,scene);Words words(r.storage_bytes/4,0);
    const unsigned count=w*h;
    for(unsigned p=0;p<count;++p) {
        words[p]=0xff336699U+colour_salt;words[r.identity_offset/4+p]=0;words[r.witness_offset/4+p]=std::bit_cast<unsigned>(400.F);
        for(unsigned c=0;c<4;++c)words[r.weight_offset/4+p*4+c]=std::bit_cast<unsigned>(c==3?1.F:.5F);
        if(curved) {
            words[count+p]=0xff123456U+colour_salt;
            for(unsigned c=0;c<4;++c)words[count*2+p*4+c]=std::bit_cast<unsigned>(float(c));
            if(p%w==4) {words[r.identity_offset/4+p]=0xfffffffdU;words[p]=(liquid_material==0?0xfd000000U:0xfe000000U)|0x336699U|colour_salt;}
        }
        if(curved || scene)words[r.base_offset/4+p*4+3]=std::bit_cast<unsigned>(1.F);
        if(scene && p%w==4){words[r.identity_offset/4+p]=0xfffffffeU;words[p]=0xfe336699U|colour_salt;}
        for(unsigned l=0;l<lobes;++l) {
            const unsigned at=r.incoming_offset/4+(p*lobes+l)*(curved?16:13);
            for(unsigned n=0;n<4;++n)words[at+n]=UINT32_MAX;
            words[at+4]=1U<<8;words[at+5]=0;
            words[at+6]=std::bit_cast<unsigned>(.125F+float((p+feature_salt)%7)/64);
            words[at+7]=std::bit_cast<unsigned>(.125F+float(l)/64);words[at+8]=0;
            words[at+9]=0xff00aabbU+colour_salt;for(unsigned c=10;c<13;++c)words[at+c]=std::bit_cast<unsigned>(.5F);
        }
    }
    if(curved || scene) {
        words[r.witness_offset/4+1]=0x7fc01234U; // Preserve the authored NaN payload.
        words[r.witness_offset/4+2]=std::bit_cast<unsigned>(-1.F);
        words[r.weight_offset/4+3*4+3]=0;
        words[r.weight_offset/4+5*4]=std::bit_cast<unsigned>(1.01F);
        words[r.base_offset/4+6*4+3]=0;
        words[r.identity_offset/4+7]=17;
        words[8]&=0x00ffffffU;
    }
    return words;
}
Words expected_index(const Words& source,const ReflectionSourceIndexLayout& r) {
    Words result(r.storage_bytes/4);const unsigned count=r.width*r.height;
    for(unsigned l=0;l<r.lobes;++l)for(unsigned level=0;level<r.level_count;++level)
    for(unsigned y=0;y<r.levels[level][2];++y)for(unsigned x=0;x<r.levels[level][1];++x) {
        std::array<float,3> low{FLT_MAX,FLT_MAX,FLT_MAX},high{-FLT_MAX,-FLT_MAX,-FLT_MAX};unsigned a=0,b=0;
        const auto add=[&](const std::array<float,3>& lo,const std::array<float,3>& hi,unsigned aa,unsigned bb) {
            for(unsigned c=0;c<3;++c){low[c]=std::min(low[c],lo[c]);high[c]=std::max(high[c],hi[c]);}a|=aa;b|=bb;};
        if(!level) {
            for(unsigned yy=y*8;yy<std::min(y*8+8,r.height);++yy)for(unsigned xx=x*8;xx<std::min(x*8+8,r.width);++xx) {
                const unsigned p=yy*r.width+xx,primary=source[count*r.primary_prefix/4+p];
                if(primary==UINT32_MAX)continue;
                const unsigned at=count*r.record_prefix/4+(p*r.lobes+l)*r.path_stride/4;
                unsigned hash=(2166136261U^primary)*16777619U;
                for(unsigned c=0;c<6;++c)hash=(hash^source[at+c])*16777619U;
                std::array<float,3> lo{},hi{};for(unsigned c=0;c<3;++c){const float f=std::bit_cast<float>(source[at+6+c]);lo[c]=f-.05001F;hi[c]=f+.05001F;}
                add(lo,hi,1U<<(hash&31U),1U<<((hash>>8)&31U));
            }
        } else {
            for(unsigned yy=0;yy<2;++yy)for(unsigned xx=0;xx<2;++xx) {
                const unsigned cx=x*2+xx,cy=y*2+yy;if(cx>=r.levels[level-1][1] || cy>=r.levels[level-1][2])continue;
                const unsigned at=(l*r.total_nodes+r.levels[level-1][0]+cy*r.levels[level-1][1]+cx)*8;
                std::array<float,3> lo{},hi{};for(unsigned c=0;c<3;++c){lo[c]=std::bit_cast<float>(result[at+c]);hi[c]=std::bit_cast<float>(result[at+4+c]);}
                add(lo,hi,result[at+3],result[at+7]);
            }
        }
        const unsigned at=(l*r.total_nodes+r.levels[level][0]+y*r.levels[level][1]+x)*8;
        for(unsigned c=0;c<3;++c){result[at+c]=std::bit_cast<unsigned>(low[c]);result[at+4+c]=std::bit_cast<unsigned>(high[c]);}
        result[at+3]=a;result[at+7]=b;
    }
    return result;
}
void exercise(Gpu& gpu,unsigned lobes,bool curved,unsigned liquid_material=0,bool scene=false) {
    constexpr unsigned w=17,h=9;std::array<std::unique_ptr<GpuCalibratedReflectionHistory>,4> owners;
    Words geometry(25);const std::array<float,12> vertices{-100,-100,400,1,200,-100,400,1,-100,200,400,1};
    std::memcpy(geometry.data(),vertices.data(),48);std::memcpy(geometry.data()+12,vertices.data(),48);geometry[24]=0;Buffer positions(gpu,geometry);
    const auto stage=[&](GpuCalibratedReflectionHistory& owner,unsigned width,unsigned height,unsigned salt,bool indexed=true,bool colours_only=false,float weight=0,std::optional<unsigned> epoch={}) {
        const auto layout=native_layout(width,height,lobes,curved,scene);
        auto authored=fixture(width,height,lobes,colours_only?salt-1:salt,salt,curved,liquid_material,scene);
        if(epoch) {
            // Deliberately distinct CURRENT RGB with unchanged source features.
            // A compatible nonzero-weight old-colour consumer cannot hide behind
            // a one-byte change that rounds back to the authored incident word.
            for(unsigned p=0;p<width*height;++p) {
                authored[p]^=0x00ffffffU;
                for(unsigned l=0;l<lobes;++l)authored[layout.incoming_offset/4+(p*lobes+l)*(curved?16:13)+9]^=0x00ffffffU;
                if(curved)authored[width*height+p]^=0x00ffffffU;
            }
        }
        Buffer source(gpu,authored);OwnerTexture ink(gpu,width,height,curved || scene);
        shadows::RayReflectionHistory previous{48,{width,height},{256,272,8,4},1,65536,96,true};previous.model_lobes=lobes;previous.model_paths=true;
        previous.curved_paths=previous.curved_receivers=curved;
        previous.scene_paths=scene;
        if(scene)previous.previous_ground=shadows::RayReflectionGround{{0,180,0},{0,-1,0}};
        if(curved) {previous.previous_liquid=shadows::RayReflectionLiquid{{{0,180,0},{0,-1,0}}};previous.previous_liquid->material=liquid_material;}
        CalibratedReflectionLobeGeometry guide{positions.value,100,3,previous,lobes==8?.5F:0.F};
        if(scene)guide.current_ground=previous.previous_ground;
        if(curved){guide.current_liquid=previous.previous_liquid;guide.current_projection=previous.projection;guide.current_clip={1,65536};}
        const auto water=curved?*shadows::native_water_layers(width,height,true):shadows::NativeWaterLayers{};
        const shadows::GpuReflectionOutput rays{gpu.device,source.value,width,height,width*4,water,layout};
        auto* command=SDL_AcquireGPUCommandBuffer(gpu.device);check(command,SDL_GetError());
        const auto frame_epoch=epoch.value_or(salt);
        if(curved)check(!owner.enqueue(command,rays,ink.value,{frame_epoch,weight,false,false,indexed},&guide),"Index bypassed ordinary curved-colour guard");
        check(owner.enqueue(command,rays,ink.value,{frame_epoch,weight,false,curved,indexed},&guide),owner.status().c_str());
        check(!owner.enqueue(command,rays,ink.value,{frame_epoch,weight,false,curved,indexed},&guide),"Pending index overwritten");
        const auto pending=owner.source_index();
        check(indexed?pending.buffer && pending.source_buffer==owner.output().buffer:!pending.buffer,"Candidate/source pair mismatched");
        gpu.submit(command);
        if(indexed || (curved && weight==0) || (curved && !epoch)) {
            // Independent whole-bank oracle: only ineligible primary IDs are
            // invalidated. Canonical/incident RGB, optional liquid/base, raw
            // NaN payload, response and every 52/64-byte lobe stay exact.
            for(unsigned y=0;y<height;++y)authored[layout.identity_offset/4+y*width]=UINT32_MAX;
            if(curved || scene)for(unsigned p:{1U,2U,3U,5U,6U,7U,8U,9U})authored[layout.identity_offset/4+p]=UINT32_MAX;
            check(gpu.read(owner.output().buffer,layout.storage_bytes)==authored,"Fresh CURRENT capture changed raw colour/base/liquid/path fields");
        }
        if(indexed) {
            const auto resolved=gpu.read(pending.source_buffer,layout.storage_bytes);
            for(unsigned y=0;y<height;++y)check(resolved[layout.identity_offset/4+y*width]==UINT32_MAX,"Protected ink retained index ownership");
            check(gpu.read(pending.buffer,pending.layout.storage_bytes)==expected_index(resolved,pending.layout),"Native paired index differs from accepted source fields");
        }
        return authored;
    };
    std::unordered_set<void*> independent;
    const unsigned stride=curved?64+64*lobes:(scene?44:28)+52*lobes;
    for(unsigned eye_sample=0;eye_sample<owners.size();++eye_sample) {
        auto& owner=owners[eye_sample];owner=std::make_unique<GpuCalibratedReflectionHistory>();check(owner->initialize(gpu.device),owner->status().c_str());
        stage(*owner,w,h,eye_sample,true,false,curved?.85F:0);
        check(!owner->accepted_source_index().buffer,"Unpresented candidate published its source index");owner->commit();
        const auto old=owner->accepted_source_index();
        check(independent.insert(old.buffer).second && independent.insert(old.source_buffer).second,"Eye/sample index or source aliases another owner");
    }
    for(unsigned eye_sample=0;eye_sample<owners.size();++eye_sample) {
        auto& owner=owners[eye_sample];
        const auto old=owner->accepted_source_index();const auto bytes=owner->working_image_bytes();const auto old_words=gpu.read(old.buffer,old.layout.storage_bytes);
        const auto old_source=gpu.read(old.source_buffer,owner->accepted_output().reflection_history.storage_bytes);
        check(bytes==(std::uint64_t(w)*h*stride+old.layout.storage_bytes)*2,"Index bank bytes omitted from residency");
        for(unsigned held=0;held<3;++held)check(owner->accepted_source_index().buffer==old.buffer && owner->working_image_bytes()==bytes
            && gpu.read(old.buffer,old.layout.storage_bytes)==old_words,"Held accepted index changed/allocated");
        stage(*owner,w,h,eye_sample+1,true,true,.85F,eye_sample);
        check(gpu.read(owner->source_index().buffer,old.layout.storage_bytes)==old_words,"Fresh compatible RGB changed source features");
        check(gpu.read(old.source_buffer,owner->accepted_output().reflection_history.storage_bytes)==old_source,"Pending compatible capture mutated accepted RGB");
        owner->discard();
        check(owner->accepted_source_index().buffer==old.buffer && gpu.read(old.source_buffer,owner->accepted_output().reflection_history.storage_bytes)==old_source,
            "Compatible capture discard changed the accepted bank");
        stage(*owner,w,h,eye_sample+1,true,true);
        check(gpu.read(owner->source_index().buffer,old.layout.storage_bytes)==old_words,"RGB-only changes altered source candidates");
        check(owner->source_index().buffer!=old.buffer && owner->accepted_source_index().buffer==old.buffer,"Pending write replaced accepted index");
        owner->discard();check(owner->accepted_source_index().buffer==old.buffer && gpu.read(old.buffer,old.layout.storage_bytes)==old_words,"Discard mutated accepted index");
        const auto next_layout=*reflection_source_index_layout(25,17,stride);
        const auto resize_bytes=(25ULL*17*stride+next_layout.storage_bytes)*2;
        check(owner->allocation_bytes(25,17,stride,true)==bytes+resize_bytes,"Resize preflight lost retained index");
        stage(*owner,25,17,eye_sample+2);check(owner->working_image_bytes()==bytes+resize_bytes,"Pending resize bytes omitted");owner->discard();
        check(owner->working_image_bytes()==bytes && owner->accepted_source_index().buffer==old.buffer,"Discarded resize retained allocations");
        stage(*owner,25,17,eye_sample+3);owner->commit();check(owner->working_image_bytes()==resize_bytes
            && owner->accepted_source_index().layout==next_layout,"Committed resize retained old index bank");
        stage(*owner,25,17,eye_sample+4);owner->reset();check(!owner->accepted_source_index().buffer,"Cut exposed old source index");owner->commit();
        check(!owner->source_index().buffer && !owner->accepted_source_index().buffer,"Reset-while-pending accepted a cut index");
        stage(*owner,w,h,eye_sample+5,true,false,curved?.85F:0);owner->commit();
        check(owner->accepted_source_index().source_buffer==owner->accepted_output().buffer,"Cold replacement detached index/source");
        const auto committed_current=stage(*owner,w,h,eye_sample+6,true,true,.85F,eye_sample+5);owner->commit();
        check(gpu.read(owner->accepted_output().buffer,owner->accepted_output().reflection_history.storage_bytes)==committed_current,
            "Compatible nonzero-weight commit did not preserve fresh CURRENT evidence");
        stage(*owner,w,h,eye_sample+6,false);owner->commit();check(!owner->accepted_source_index().buffer
            && owner->working_image_bytes()==std::uint64_t(w)*h*stride*2,"Disabled staging retained index memory");
        owner->release_device();check(!owner->accepted_source_index().buffer && !owner->working_image_bytes(),"Released owner exposed a stale index");
    }
    std::cout<<"Source owner lobes="<<lobes<<" curved="<<curved<<" liquid="<<liquid_material<<" scene="<<scene
        <<": 2 eyes x 2 samples, fresh compatible nonzero-weight capture, exact nodes/ink, RGB-independent, pending/discard/commit/held/cut/resize/disable/release passed.\n";
}
// Reuse this diagnostic's GPU transfer helpers without adding a production
// query upload or readback to the history owner.
#include "check_reflection_source_queries.inc"
#include "check_reflection_source_local_roots.inc"
#include "check_reflection_source_root_replay.inc"
#include "check_reflection_source_optical_schedule.inc"
}
int main(int argc,char** argv)try {
    check(argc>=2 && argc<=16,"Use direct3d12|vulkan [--low-power] [--capture-probe] [--curved] [--optical] [--local-roots|--root-hull|--source-witness|--source-folds|--source-guide|--source-colour|--source-compose] [--srgb] [--rough-fold-probe|--ordered-fold-probe|--mixed-compose-probe|--frame-publication-probe] [--ambiguous-root-replay FILE]");
    bool low=false,curved=false,optical=false,roots=false,hull=false,witness=false,folds=false,guides=false,colours=false,compose=false,srgb=false,rough_probe=false,ordered_probe=false,mixed_probe=false,capture_probe=false,publication_probe=false,admission_probe=false;const char* replay=nullptr;
    for(int n=2;n<argc;++n) {
        if(std::string_view(argv[n])=="--low-power" && !low)low=true;
        else if(std::string_view(argv[n])=="--capture-probe" && !capture_probe)capture_probe=true;
        else if(std::string_view(argv[n])=="--curved" && !curved)curved=true;
        else if(std::string_view(argv[n])=="--optical" && !optical)optical=true;
        else if(std::string_view(argv[n])=="--local-roots" && !roots)roots=true;
        else if(std::string_view(argv[n])=="--root-hull" && !hull)hull=true;
        else if(std::string_view(argv[n])=="--source-witness" && !witness)witness=true;
        else if(std::string_view(argv[n])=="--source-folds" && !folds)folds=true;
        else if(std::string_view(argv[n])=="--source-guide" && !guides)guides=true;
        else if(std::string_view(argv[n])=="--source-colour" && !colours)colours=true;
        else if(std::string_view(argv[n])=="--source-compose" && !compose)compose=true;
        else if(std::string_view(argv[n])=="--srgb" && !srgb)srgb=true;
        else if(std::string_view(argv[n])=="--rough-fold-probe" && !rough_probe)rough_probe=true;
        else if(std::string_view(argv[n])=="--ordered-fold-probe" && !ordered_probe)ordered_probe=true;
        else if(std::string_view(argv[n])=="--mixed-compose-probe" && !mixed_probe)mixed_probe=true;
        else if(std::string_view(argv[n])=="--frame-publication-probe" && !publication_probe)publication_probe=true;
        else if(std::string_view(argv[n])=="--admission-probe" && !admission_probe)admission_probe=true;
        else if(std::string_view(argv[n])=="--ambiguous-root-replay" && !replay) {
            check(n+1<argc,"Ambiguous root replay requires a complete raw INPUT file");replay=argv[++n];
        }
        else check(false,"Unknown/duplicate source-owner option");
    }
    compose=compose || publication_probe;colours=colours || compose;guides=guides || colours;folds=folds || guides;witness=witness || folds;
    check(!srgb || colours,"sRGB colour fixture requires source-colour");
    check(!admission_probe || publication_probe,"Admission probe requires the complete frame-publication probe");
    check(!roots || (!hull && !replay && !witness),"Select per-region roots or whole-source hull diagnostics, not both");
    check(!witness || (!curved && !optical),"Source witness selects its complete family suite, not another fixture mode");
    hull=hull || witness;
    check(!replay || (!curved && !optical),"Ambiguous root replay cannot select a different fixture mode");
    std::cout.setf(std::ios::unitbuf); // Diagnostic progress; no player or timing-benchmark change.
    check(!capture_probe || (!curved && !optical && !roots && !hull && !replay && !rough_probe && !ordered_probe && !mixed_probe && !publication_probe),
        "Capture probe is a CURRENT-only owner suite, not optical/colour qualification");
    check(!mixed_probe || (compose && !replay && !rough_probe && !ordered_probe && !publication_probe),
        "Mixed composition probe requires source-compose without another targeted probe");
    check(!publication_probe || (!replay && !rough_probe && !ordered_probe),"Full-frame publication probe cannot select another targeted suite");
    Gpu gpu(argv[1],low);
    if(publication_probe) {
        exercise_optical_schedule(gpu);
        for(unsigned lobes:{1U,8U}) {
            exercise_local_roots(gpu,lobes,false,0,true,true,false,true,false,false,true,0,0,true,true,srgb,true,false,true,admission_probe);
            exercise_local_roots(gpu,lobes,true,0,false,true,false,true,false,false,true,0,0,true,true,srgb,true,false,true,admission_probe);
            exercise_local_roots(gpu,lobes,true,3,false,true,false,true,false,false,true,0,0,true,true,srgb,true,false,true,admission_probe);
        }
        gpu.finish();std::cout<<"Complete reflection frame publication probe PASS; real producer/XR/backend/cost acceptance remains separate.\n";return 0;
    }
    if(capture_probe) {
        for(unsigned lobes:{1U,8U}) {
            exercise(gpu,lobes,false);exercise(gpu,lobes,false,0,true);
            exercise(gpu,lobes,true,0);exercise(gpu,lobes,true,3);
        }
        gpu.finish();std::cout<<"Fresh CURRENT planar/scene/water/lava owner capture PASS; no old-colour or frame-rate acceptance.\n";return 0;
    }
    if(ordered_probe) {
        check(folds && !replay && !rough_probe,"Ordered fold probe requires source-folds alone; not full qualification");
        for(unsigned lobes:{1U,8U})for(unsigned hops=1;hops<=4;++hops)
            exercise_local_roots(gpu,lobes,false,0,false,true,false,true,false,false,true,0,hops,guides,colours,srgb,compose);
        gpu.finish();std::cout<<"Ordered finite-face environment probe PASS guide-mode="<<guides<<" colour-mode="<<colours
            <<" compose-mode="<<compose<<"; not liquid-hop/full-backend/player-FPS acceptance.\n";return 0;
    }
    if(rough_probe) {
        check(folds && !replay,"Rough fold probe requires source-folds without other replay; not full qualification");
        exercise_local_roots(gpu,8,false,0,false,true,false,true,false,false,true,0,0,guides,colours,srgb,compose);
        gpu.finish();std::cout<<"Targeted rough probe PASS colour-mode="<<colours<<" compose-mode="<<compose
            <<"; not full-suite/backend/player-FPS acceptance.\n";return 0;
    }
    if(mixed_probe) {
        exercise_local_roots(gpu,8,false,0,true,true,false,true,false,false,true,1,0,true,true,srgb,true,true);
        gpu.finish();std::cout<<"Mixed complete-pixel composition probe PASS; not full-suite/backend/player-FPS acceptance.\n";return 0;
    }
    if(replay && !hull) {exercise_root_hull_replay(gpu,replay);gpu.finish();return 0;}
    if(hull) {
        for(unsigned lobes:{1U,8U})for(bool boundary:{false,true}) {
            exercise_local_roots(gpu,lobes,false,0,false,true,boundary,witness,false,false,folds,0,0,guides,colours,srgb,compose);
            exercise_local_roots(gpu,lobes,false,0,true,true,boundary,witness,false,false,folds,0,0,guides,colours,srgb,compose);
            exercise_local_roots(gpu,lobes,true,0,false,true,boundary,witness,false,false,folds,0,0,guides,colours,srgb,compose);
            exercise_local_roots(gpu,lobes,true,3,false,true,boundary,witness,false,false,folds,0,0,guides,colours,srgb,compose);
        }
        if(witness)for(unsigned lobes:{1U,8U}) {
            exercise_local_roots(gpu,lobes,false,0,false,true,false,true,true,false,folds,0,0,guides,colours,srgb,compose);
            exercise_local_roots(gpu,lobes,false,0,true,true,false,true,true,false,folds,0,0,guides,colours,srgb,compose);
            exercise_local_roots(gpu,lobes,true,0,false,true,false,true,true,false,folds,0,0,guides,colours,srgb,compose);
            exercise_local_roots(gpu,lobes,true,3,false,true,false,true,true,false,folds,0,0,guides,colours,srgb,compose);
            exercise_local_roots(gpu,lobes,false,0,true,true,false,true,false,true,folds,0,0,guides,colours,srgb,compose);
            if(folds)for(unsigned fault:{1U,2U})exercise_local_roots(gpu,lobes,false,0,true,true,false,true,false,false,true,fault,0,guides,colours,srgb,compose);
        }
        if(compose)exercise_local_roots(gpu,8,false,0,true,true,false,true,false,false,true,1,0,true,true,srgb,true,true);
        if(replay)exercise_root_hull_replay(gpu,replay,witness,folds,guides,colours,srgb,compose);
        gpu.finish();std::cout<<"Native whole-source root hull PASS including shared boundaries; source membership/colour/FPS unqualified.\n";return 0;
    }
    if(roots) {
        for(unsigned lobes:{1U,8U}) {
            exercise_local_roots(gpu,lobes,false,0,false);exercise_local_roots(gpu,lobes,false,0,true);
            exercise_local_roots(gpu,lobes,true,0,false);exercise_local_roots(gpu,lobes,true,3,false);
        }
        gpu.finish();std::cout<<"Native local-root owner integration PASS; no global source/colour or frame-rate acceptance.\n";return 0;
    }
    exercise(gpu,1,curved);exercise(gpu,8,curved);
    exercise_queries(gpu,1,curved,0,optical);exercise_queries(gpu,8,curved,0,optical);
    if(curved){exercise(gpu,1,true,3);exercise(gpu,8,true,3);
        exercise_queries(gpu,1,true,3,optical);exercise_queries(gpu,8,true,3,optical);}gpu.finish();
    std::cout<<"Native accepted source-index ownership PASS; cold/cut/zero-weight CURRENT capture, candidate enclosure only, no curved root/colour acceptance or frame-rate claim.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
