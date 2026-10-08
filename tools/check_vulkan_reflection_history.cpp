// Production SDL/Vulkan owner, fixture-only readback, input-only binary64
// geometry/motion oracle. Does not prove Linux, application reuse or FPS.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "starfox/render/vulkan_hardware_rt.hpp"
#include "native_ray_owner_fixture.hpp"
#include "starfox/render/vulkan_ray_support.hpp"
#include "starfox/render/sdl_vulkan_bridge.h"
#include "native_environment_cube_fixture.hpp"
#include "native_ground_fixture.hpp"
#include <SDL3/SDL.h>
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace native_reflection_fixture;
using namespace starfox::render::shadows;
using Position=std::array<float,4>;
constexpr unsigned capacity=16384,image_bytes=width*height*52,download_bytes=width*height*460;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct Device {SDL_GPUDevice* value{};~Device(){if(value)SDL_DestroyGPUDevice(value);SDL_Quit();}};
void wait(SDL_GPUDevice* device,SDL_GPUFence* fence) {
    check(fence,SDL_GetError());const bool ok=SDL_WaitForGPUFences(device,true,&fence,1);
    SDL_ReleaseGPUFence(device,fence);check(ok,SDL_GetError());
}
struct Source {
    SDL_GPUDevice* device{};SDL_GPUBuffer* buffer{};SDL_GPUTransferBuffer *upload{},*download{};
    starfox::render::RayMaterials materials;
    explicit Source(SDL_GPUDevice* d):device(d) {
        materials.encoding=starfox::render::RayMaterialEncoding::native_rgba;
        SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,capacity,0};
        buffer=SDL_CreateGPUBuffer(d,&info);
        SDL_GPUTransferBufferCreateInfo up{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,capacity,0},down{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,download_bytes,0};
        upload=SDL_CreateGPUTransferBuffer(d,&up);download=SDL_CreateGPUTransferBuffer(d,&down);
        check(buffer && upload && download,SDL_GetError());
    }
    void set(const NativeCubeInput& input,const std::array<Position,12>& old,const std::array<unsigned,4>& indices,RayReflectionHistory& history) {
        const unsigned material_bytes=unsigned(input.source.source.words.size()*4),cube_bytes=unsigned(input.cube.size()*4);
        history.previous_vertex_offset=(192+material_bytes+cube_bytes+15)&~15U;
        const bool mapping=history.previous_index_offset!=0;
        history.previous_index_offset=mapping?history.previous_vertex_offset+192:0;
        const auto used=history.previous_vertex_offset+192+(mapping?16:0);check(used<=capacity,"History fixture exceeds source buffer");
        auto* data=static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device,upload,false));check(data,SDL_GetError());
        std::memset(data,0,used);std::memcpy(data,input.source.source.geometry.vertices.data(),192);
        std::memcpy(data+192,input.source.source.words.data(),material_bytes);
        if(cube_bytes)std::memcpy(data+192+material_bytes,input.cube.data(),cube_bytes);
        std::memcpy(data+history.previous_vertex_offset,old.data(),192);
        if(mapping)std::memcpy(data+history.previous_index_offset,indices.data(),16);
        SDL_UnmapGPUTransferBuffer(device,upload);
        auto* command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());
        auto* copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
        SDL_GPUTransferBufferLocation from{upload,0};SDL_GPUBufferRegion to{buffer,0,used};
        SDL_UploadToGPUBuffer(copy,&from,&to,false);SDL_EndGPUCopyPass(copy);
        wait(device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
    }
    bool render(NativeRayOwner& rays,const NativeCubeInput& input,const Camera& camera,unsigned encoding,const RayReflectionHistory* history,
        float roughness=0,unsigned metallic=0,bool specular_models=false,PrimaryRayRange range={1,700},
        std::array<float,9> rotation={1,0,0,0,1,0,0,0,1},const RayWater* water=nullptr,
        std::optional<ReceiverPlane> ground={},unsigned environment=0xff346b98) {
        const starfox::render::GpuScene::RayGeometryOutput geometry{device,buffer,12,true,&materials,192,unsigned(input.source.source.words.size()*4)};
        const ResidentEnvironmentCube cube{unsigned(input.source.source.words.size()*4),input.size,rotation};std::array<unsigned,256> palette{};
        return rays.render_reflections(device,geometry,camera,palette,environment,3,roughness,metallic,ground,nullptr,water,false,encoding,
            range,input.size?&cube:nullptr,specular_models,history);
    }
    std::vector<unsigned> get(const GpuReflectionOutput& output) {
        const auto bytes=output.reflection_history.storage_bytes?output.reflection_history.storage_bytes:
            output.water_layers.storage_bytes?output.water_layers.storage_bytes:width*height*4;
        auto* command=SDL_AcquireGPUCommandBuffer(device);check(command,SDL_GetError());auto* copy=SDL_BeginGPUCopyPass(command);check(copy,SDL_GetError());
        SDL_GPUBufferRegion from{static_cast<SDL_GPUBuffer*>(output.buffer),0,bytes};SDL_GPUTransferBufferLocation to{download,0};
        SDL_DownloadFromGPUBuffer(copy,&from,&to);SDL_EndGPUCopyPass(copy);wait(device,SDL_SubmitGPUCommandBufferAndAcquireFence(command));
        const auto* data=static_cast<const unsigned*>(SDL_MapGPUTransferBuffer(device,download,false));check(data,SDL_GetError());
        std::vector<unsigned> result(data,data+bytes/4);SDL_UnmapGPUTransferBuffer(device,download);return result;
    }
    ~Source(){if(buffer)SDL_ReleaseGPUBuffer(device,buffer);if(upload)SDL_ReleaseGPUTransferBuffer(device,upload);if(download)SDL_ReleaseGPUTransferBuffer(device,download);}
};
V position(const Position& v){return {v[0],v[1],v[2]};}
// Solve old eye->virtual feature against the finite accepted face with a
// Moller segment intersection, not the shader's Gram-matrix containment math.
std::array<double,4> old_feature(const std::array<Position,12>& old,unsigned receiver,unsigned hit,
    std::array<double,2> uv,const RayReflectionHistory& history,bool& edge) {
    const unsigned pa=receiver*3,qa=hit*3;
    for(unsigned i:{pa,pa+1,pa+2,qa,qa+1,qa+2})for(unsigned c=0;c<4;++c)
        if(!std::isfinite(old[i][c]) || old[i][3]!=1)return {};
    const V a=position(old[pa]),b=position(old[pa+1]),c=position(old[pa+2]),q0=position(old[qa]),q1=position(old[qa+1]),q2=position(old[qa+2]);
    const V ab=sub(b,a),ac=sub(c,a),crossed=cross(ab,ac);
    if(dot(crossed,crossed)<=1e-20 || dot(cross(sub(q1,q0),sub(q2,q0)),cross(sub(q1,q0),sub(q2,q0)))<=1e-20)return {};
    const V normal=unit(crossed),feature=add(scale(q0,1-uv[0]-uv[1]),add(scale(q1,uv[0]),scale(q2,uv[1])));
    const V virtual_point=sub(feature,scale(normal,2*dot(sub(feature,a),normal)));
    if(virtual_point[2]<=0)return {};
    const V e=cross(virtual_point,ac),s=scale(a,-1),r=cross(s,ab);const double determinant=dot(ab,e);
    if(std::abs(determinant)<=1e-12)return {};
    const double u=dot(s,e)/determinant,v=dot(virtual_point,r)/determinant,fraction=dot(ac,r)/determinant;
    edge|=std::min({std::abs(u),std::abs(v),std::abs(1-u-v)})<1e-4;
    if(u<0 || v<0 || u+v>1 || fraction<=0 || fraction>=1)return {};
    const double depth=virtual_point[2]*fraction;
    edge|=std::min(std::abs(depth-history.near_plane),std::abs(depth-history.far_plane))<1e-4;
    if(depth<history.near_plane || depth>history.far_plane)return {};
    const double x=history.projection[0]*virtual_point[0]/virtual_point[2]+history.projection[2];
    const double y=history.projection[1]*virtual_point[1]/virtual_point[2]+history.projection[3];
    edge|=std::min({std::abs(x),std::abs(y),std::abs(x-history.extent[0]),std::abs(y-history.extent[1])})<1e-4;
    if(x<0 || y<0 || x>=history.extent[0] || y>=history.extent[1])return {};
    return {x,y,depth,1};
}
std::array<double,2> barycentric(const NativeCubeInput& input,unsigned primitive,V origin,V direction) {
    const auto& vertices=input.source.source.geometry.vertices;const unsigned first=primitive*3;
    const V a=position(vertices[first]),e1=sub(position(vertices[first+1]),a),e2=sub(position(vertices[first+2]),a);
    const V q=cross(direction,e2),s=sub(origin,a),r=cross(s,e1);const double d=dot(e1,q);
    return {dot(s,q)/d,dot(direction,r)/d};
}
struct Counts {unsigned cases{},checked{},clear{},secondary{},motion{},no_motion{},mapped{},unmapped{},edges{},sky{},max_rgb{};double max_motion{},max_bary{},max_depth{};};
void verify(const NativeCubeInput& input,const std::array<Position,12>& old,const std::array<unsigned,4>& indices,
    const Camera& camera,unsigned encoding,const RayReflectionHistory& history,const std::vector<unsigned>& actual,Counts& n) {
    check(actual.size()==width*height*13,"History output layout is not 52 bytes/pixel");auto params=cube_parameters(input,encoding,0,0);params.environment[0]=0xff346b98;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const unsigned i=y*width+x,ma=width*height+i*4,ia=width*height*5+i*4,wa=width*height*9+i*4;
        const V raw{(x+.5-camera.center_x)/camera.focal_length,(y+.5-camera.center_y)/camera.vertical_focal_length(),1};
        const auto primary=trace_native_colour(input.source,{},raw,1,x,y,700);bool edge=primary.edge;
        unsigned colour=0;std::array<unsigned,4> identity{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
        std::array<double,4> motion{},witness{};
        if(primary.found) {
            const V normal=cube_model_normal(input,primary.primitive,raw),origin=add(scale(raw,primary.t),scale(normal,std::max(.01,primary.t*std::sqrt(dot(raw,raw))*1e-5)));
            const V direction=reflected_liquid_oracle::reflect(unit(raw),normal);
            const auto secondary=trace_native_colour(input.source,origin,direction,std::max(.01,primary.t*std::sqrt(dot(raw,raw))*1e-5),x,y);
            edge|=secondary.edge;
            if(secondary.found) {
                const auto uv=barycentric(input,secondary.primitive,origin,direction);
                witness={uv[0],uv[1],primary.t,1};identity[0]=primary.primitive;identity[1]=secondary.primitive;
                if(history.previous_index_offset){identity[2]=indices[primary.primitive];identity[3]=indices[secondary.primitive];++n.mapped;}else ++n.unmapped;
                motion=old_feature(old,primary.primitive,secondary.primitive,uv,history,edge);++n.secondary;
                colour=native_pack(native_linear(secondary.colour,encoding),encoding,255);
            } else {++n.sky;colour=input.size?cube_packed_sample(input,params,direction):params.environment[0];}
        } else ++n.clear;
        if(edge){++n.edges;continue;}
        if((actual[i]>>24)!=(colour>>24))throw std::runtime_error("History source coverage mismatch pixel="+std::to_string(i));
        for(unsigned c=0;c<3;++c){const unsigned error=std::abs(int((actual[i]>>(8*c))&255)-int((colour>>(8*c))&255));n.max_rgb=std::max(n.max_rgb,error);check(error<=1,"History RGB mismatch");}
        for(unsigned c=0;c<4;++c) {
            check(actual[ia+c]==identity[c],"History inferred an identity or lost explicit accepted mapping");
            const double mw=std::bit_cast<float>(actual[ma+c]),ww=std::bit_cast<float>(actual[wa+c]);
            check(std::isfinite(mw) && std::isfinite(ww),"History emitted non-finite motion/witness");
            if(c==3){check(mw==motion[c] && ww==witness[c],"History validity mismatch");continue;}
            const double me=std::abs(mw-motion[c]),we=std::abs(ww-witness[c]);
            n.max_motion=std::max(n.max_motion,me);if(c<2)n.max_bary=std::max(n.max_bary,we);else n.max_depth=std::max(n.max_depth,we);
            check(me<=std::max(.002,std::abs(motion[c])*2e-5),"Accepted reflected-feature motion mismatch");
            check(we<=(c<2?2e-5:std::max(.002,std::abs(witness[c])*2e-5)),"Current reflected-hit witness mismatch");
        }
        ++n.checked;if(motion[3])++n.motion;else ++n.no_motion;
    }
    ++n.cases;
}

// Independent binary64 finite source intersections, source coverage and cube
// optics. No GPU hit/record/position participates in this reference.
struct ModelPath {
    std::array<unsigned,4> mirrors{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
    unsigned control{},terminal{UINT32_MAX},incoming{},current{};V feature{},response{};bool edge{};double bary_allowance{2e-5};
    V origin{},direction{};double distance{},altitude{},incidence{};
};
ModelPath model_path(const NativeCubeInput& input,const Parameters& params,V origin,V direction,double minimum,
    unsigned x,unsigned y,bool transport,double origin_allowance,const reflected_liquid_oracle::Frame* ground=nullptr,bool escaping=false) {
    ModelPath path;V throughput{1,1,1};
    const auto finish=[&](unsigned kind,unsigned hops,unsigned terminal,V feature,unsigned source) {
        path.control=(kind<<8)|hops;path.terminal=terminal;path.feature=feature;path.incoming=source;path.response=throughput;
        path.current=native_pack(product(throughput,native_linear(source,params.material_info[2])),params.material_info[2],255);
    };
    for(unsigned hop=0;hop<4;++hop) {
        const auto hit=trace_native_colour(input.source,origin,direction,minimum,x,y);path.edge|=hit.edge;
        if(ground) {
            const double den=dot(direction,ground->normal),floor=std::abs(den)>1.e-6?dot(sub(ground->point,origin),ground->normal)/den:-1;
            path.edge|=std::min(std::abs(floor-minimum),std::abs(floor-hit.t))<1.e-4;
            if(floor>minimum && floor<hit.t && !(escaping && hop==0 && den>0)) {
                path.mirrors[hop]=UINT32_MAX-1;
                V normal=unit(ground->normal);if(dot(normal,direction)>0)normal=scale(normal,-1);
                if((unsigned(params.water[3])&15)==2) {const double grazing=std::pow(1-std::clamp(dot(scale(direction,-1),normal),0.,1.),5);
                    throughput=product(throughput,add(V{1,.766,.336},scale(V{0,.234,.664},grazing)));}
                minimum=std::max(.05,floor*1.e-5);origin=add(add(origin,scale(direction,floor)),scale(normal,minimum));
                direction=reflected_liquid_oracle::reflect(direction,normal);continue;
            }
        }
        if(!hit.found) {finish(2,hop,UINT32_MAX,direction,input.size?cube_packed_sample(input,params,direction):params.environment[0]);return path;}
        if(!transport) {
            const auto uv=barycentric(input,hit.primitive,origin,direction);finish(1,hop,hit.primitive,{uv[0],uv[1],0},hit.colour);
            const auto& vertices=input.source.source.geometry.vertices;const unsigned first=hit.primitive*3;
            const V a=position(vertices[first]),b=position(vertices[first+1]),c=position(vertices[first+2]);
            const double area=std::sqrt(dot(cross(sub(b,a),sub(c,a)),cross(sub(b,a),sub(c,a))));
            const double longest=std::max({std::sqrt(dot(sub(b,a),sub(b,a))),std::sqrt(dot(sub(c,a),sub(c,a))),std::sqrt(dot(sub(c,b),sub(c,b)))});
            double scale=1;for(const auto& v:{origin,a,b,c})for(double component:v)scale=std::max(scale,std::abs(component));
            // Quantized barycentrics have the fixed 2e-5 floor. The ray query's
            // float origin also has an input-scale rounding allowance (~eight
            // float epsilons), projected through the intersection's incidence
            // conditioning and converted by the actual minimum altitude.
            // No GPU-derived error, fitted coefficient or new exclusion.
            const V normal=unit(cross(sub(b,a),sub(c,a)));
            const double incidence=std::abs(dot(normal,direction));
            path.bary_allowance+=std::max(origin_allowance,scale*1.e-6)*(1+1/std::max(incidence,1.e-12))*longest/area;
            path.origin=origin;path.direction=direction;path.distance=hit.t;path.altitude=area/longest;path.incidence=incidence;return path;
        }
        path.mirrors[hop]=hit.primitive;const V normal=cube_model_normal(input,hit.primitive,direction);
        if(params.material_info[3] && params.dimensions[3]) {const V f0=native_model_f0(hit.colour,params.dimensions[3],params.material_info[2]);
            const double grazing=std::pow(1-std::clamp(dot(scale(direction,-1),normal),0.,1.),5);
            throughput=product(throughput,add(f0,scale(sub(V{1,1,1},f0),grazing)));}
        minimum=std::max(.05,hit.t*1e-5);origin=add(add(origin,scale(direction,hit.t)),scale(normal,minimum));
        direction=reflected_liquid_oracle::reflect(direction,normal);
    }
    finish(3,4,UINT32_MAX,direction,input.size?cube_packed_sample(input,params,direction):params.environment[0]);return path;
}
struct ModelCounts {
    unsigned cases{},checked{},clear{},records{},finite{},escape{},budget{},hops{},edges{},rough{},clips{},max_rgb{};
    std::array<unsigned,4> conductors{};double max_feature{},max_response{},max_depth{},max_bary_allowance{};
};
void verify_models(const NativeCubeInput& input,const Parameters& params,const RayReflectionHistory& history,
    const std::vector<unsigned>& actual,ModelCounts& n) {
    constexpr unsigned count=width*height;const unsigned lobes=history.model_lobes,stride=history.model_paths?13:3;
    check(actual.size()==count*(7+lobes*stride),"MODEL history is not its canonical compact/path size");
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const unsigned id=y*width+x;
        const V raw{(x+.5-params.camera[2])/params.camera[0],(y+.5-params.camera[3])/params.camera[1],1},incoming=unit(raw);
        const auto primary=trace_native_colour(input.source,{},raw,params.primary_range[0],x,y,params.primary_range[1]);
        bool boundary=primary.edge;std::array<ModelPath,8> paths{};V sum{},response{};unsigned expected=0;
        if(primary.found) {
            const V normal=cube_model_normal(input,primary.primitive,raw);const double bias=std::max(.01,primary.t*std::sqrt(dot(raw,raw))*1e-5);
            const V origin=add(scale(raw,primary.t),scale(normal,bias));const auto directions=native_model_lobes(incoming,normal,params.settings[0]);
            // A nearby hit on a very large receiver can subtract large source
            // coordinates. Bound its float query/origin arithmetic from the
            // INPUT receiver vertices, not just the small resulting hit point.
            double receiver_scale=1;for(unsigned corner=0;corner<3;++corner)for(unsigned c=0;c<3;++c)
                receiver_scale=std::max(receiver_scale,std::abs(double(input.source.source.geometry.vertices[primary.primitive*3+corner][c])));
            for(double c:origin)receiver_scale=std::max(receiver_scale,std::abs(c));
            const double origin_allowance=receiver_scale*1.e-6*(1+1/std::max(std::abs(dot(normal,incoming)),1.e-12));
            check(directions.count==lobes,"MODEL fixture lobe contract mismatch");n.clips+=directions.hemisphere_clips;
            response={1,1,1};if(params.dimensions[3]) {const V f0=native_model_f0(primary.colour,params.dimensions[3],params.material_info[2]);
                const double grazing=std::pow(1-std::clamp(dot(scale(incoming,-1),normal),0.,1.),5);response=add(f0,scale(sub(V{1,1,1},f0),grazing));}
            for(unsigned lobe=0;lobe<lobes;++lobe) {
                paths[lobe]=model_path(input,params,origin,directions.direction[lobe],bias,x,y,params.material_info[3]!=0,origin_allowance);
                boundary|=paths[lobe].edge;sum=add(sum,scale(native_linear(paths[lobe].current,params.material_info[2]),1./lobes));
            }
            expected=native_pack(product(sum,response),params.material_info[2],255);
        }
        if(boundary){++n.edges;continue;}
        if(!primary.found) {
            check(actual[id]==0 && actual[count*2+id]==0,"MODEL clear primary retained colour/depth");
            for(unsigned c=0;c<4;++c)check(actual[count*3+id*4+c]==0,"MODEL clear primary retained response");
            for(unsigned lobe=0;lobe<lobes;++lobe) {
                const unsigned at=count*7+(id*lobes+lobe)*stride;
                for(unsigned c=0;c<stride;++c) {
                    const bool identity=history.model_paths?(c<4 || c==5):c==0;
                    check(actual[at+c]==(identity?UINT32_MAX:0),"MODEL clear primary retained a lobe/path record");
                }
            }
        }
        check(actual[count+id]==(primary.found?primary.primitive:UINT32_MAX),"MODEL history primary ID/clear mismatch");
        const double depth=std::bit_cast<float>(actual[count*2+id]),de=std::abs(depth-(primary.found?primary.t:0));n.max_depth=std::max(n.max_depth,de);
        check(std::isfinite(depth) && de<=std::max(.002,std::abs(primary.t)*2e-5),"MODEL history primary forward-depth mismatch");
        for(unsigned c=0;c<4;++c) {
            const double value=std::bit_cast<float>(actual[count*3+id*4+c]);check(std::isfinite(value),"MODEL history response is non-finite");
            if(c==3)check(value==double(primary.found),"MODEL history primary response validity mismatch");
            else {const double e=std::abs(value-response[c]);n.max_response=std::max(n.max_response,e);check(e<=2e-5,"MODEL history primary response mismatch");}
        }
        check(actual[id]>>24==expected>>24,"MODEL history source ownership mismatch");
        for(unsigned c=0;c<3;++c){const unsigned e=std::abs(int((actual[id]>>(8*c))&255)-int((expected>>(8*c))&255));n.max_rgb=std::max(n.max_rgb,e);check(e<=1,"MODEL history current RGB mismatch");}
        for(unsigned lobe=0;lobe<lobes;++lobe) {
            const unsigned at=count*7+(id*lobes+lobe)*stride;const auto& path=paths[lobe];const unsigned kind=path.control>>8;
            if(kind==1)n.max_bary_allowance=std::max(n.max_bary_allowance,path.bary_allowance);
            if(history.model_paths) {
                for(unsigned c=0;c<4;++c)check(actual[at+c]==path.mirrors[c],"MODEL history lost/reordered a finite mirror hop");
                check(actual[at+4]==path.control && actual[at+5]==path.terminal,"MODEL path kind/count/terminal mismatch");
                for(unsigned c=0;c<3;++c) {
                    const double feature=std::bit_cast<float>(actual[at+6+c]),weight=std::bit_cast<float>(actual[at+10+c]);
                    const double fe=std::abs(feature-path.feature[c]),we=std::abs(weight-path.response[c]);
                    check(std::isfinite(feature) && std::isfinite(weight),"MODEL ordered path feature/response is non-finite");
                    n.max_feature=std::max(n.max_feature,fe);n.max_response=std::max(n.max_response,we);
                    check(fe<=(kind==1 && c<2?path.bary_allowance:2e-5) && we<=2e-5,"MODEL path feature/current throughput mismatch");
                }
            } else {
                check(actual[at]==(kind==1?path.terminal:UINT32_MAX),"MODEL compact terminal/clear ID mismatch");
                const unsigned packed=actual[at+1];check((packed&65535)+(packed>>16)<=65535,"MODEL compact barycentrics left their triangle");
                if(kind==1)for(unsigned c=0;c<2;++c) {const double uv=double(c?packed>>16:packed&65535)/65535,e=std::abs(uv-path.feature[c]);
                    n.max_feature=std::max(n.max_feature,e);
                    if(e>path.bary_allowance)throw std::runtime_error("MODEL compact quantized barycentrics mismatch pixel/lobe/component="+std::to_string(id)+"/"+std::to_string(lobe)+"/"+std::to_string(c)
                        +" actual/reference="+std::to_string(uv)+"/"+std::to_string(path.feature[c])+" error="+std::to_string(e)+" allowance="+std::to_string(path.bary_allowance)
                        +" primary/secondary="+std::to_string(primary.primitive)+"/"+std::to_string(path.terminal)+" depth="+std::to_string(primary.t)+"/"+std::to_string(depth)
                        +" origin="+std::to_string(path.origin[0])+","+std::to_string(path.origin[1])+","+std::to_string(path.origin[2])
                        +" ray="+std::to_string(path.direction[0])+","+std::to_string(path.direction[1])+","+std::to_string(path.direction[2])
                        +" travel/altitude/incidence="+std::to_string(path.distance)+"/"+std::to_string(path.altitude)+"/"+std::to_string(path.incidence));}
                else check(packed==0,"MODEL compact miss retained stale barycentrics");
            }
            const unsigned word=actual[at+(history.model_paths?9:2)];check(word>>24==path.incoming>>24,"MODEL incident radiance ownership mismatch");
            for(unsigned c=0;c<3;++c){const unsigned e=std::abs(int((word>>(8*c))&255)-int((path.incoming>>(8*c))&255));n.max_rgb=std::max(n.max_rgb,e);check(e<=1,"MODEL incident radiance was tinted or stale");}
            ++n.records;n.finite+=kind==1;n.escape+=kind==2;n.budget+=kind==3;n.hops+=path.control&7;
        }
        ++n.checked;if(primary.found){++n.conductors[params.dimensions[3]];n.rough+=lobes==8;}else ++n.clear;
    }
    ++n.cases;
}
void run_model_history(Source& source,NativeRayOwner& rays) {
    ModelCounts n;constexpr std::array<unsigned,4> mapping{17,9,6,12};
    for(unsigned encoding:{1U,2U})for(unsigned variant:{0U,3U,4U})for(float roughness:{0.f,.35f,.85f})for(unsigned metallic=0;metallic<4;++metallic)
    for(unsigned context=0;context<6;++context)for(unsigned view=0;view<2;++view)for(unsigned mode=0;mode<3;++mode) {
        auto input=cube_input(16,variant,encoding);auto params=cube_parameters(input,encoding,view,6);
        params.dimensions={width,height,3,metallic};params.settings={roughness,0,0,0};params.material_info[3]=mode==2;params.environment[0]=0xff346b98;
        params.primary_range={1,65536,1,0};if(context==3)params.primary_range={250,500,1,0};if(context==4)params.primary_range={450,800,1,0};
        if(context==0){params.cube_info={};input.cube.clear();input.size=0;}
        if(context==2) {
            for(unsigned i=0;i<12;++i)input.source.source.geometry.vertices[i][2]=i<6?400.f:-200.f;
            constexpr std::array<std::array<float,2>,6> rect{{{-300,-220},{300,-220},{300,220},{-300,-220},{300,220},{-300,220}}};
            for(unsigned i=0;i<6;++i){auto& v=input.source.source.geometry.vertices[i+6];v[0]=rect[i][0];v[1]=rect[i][1];}
        }
        if(context==5)for(unsigned i=0;i<6;++i){auto& v=input.source.source.geometry.vertices[i];v[0]*=6;v[1]*=6;v[2]=400+5*v[0];}
        auto old=input.source.source.geometry.vertices;for(auto& vertex:old)vertex[3]=context==1?0:1;
        if(context==1)for(auto& vertex:old)vertex[0]=std::numeric_limits<float>::quiet_NaN();
        RayReflectionHistory history{16,{173,119},{141,132,83.7,58.2},1,900,view?4U:0U,true};history.model_lobes=roughness>0?8:1;history.model_paths=mode!=0;
        source.set(input,old,mapping,history);const Camera camera{width,height,params.camera[0],params.camera[2],params.camera[3],params.camera[1]};
        const PrimaryRayRange range{params.primary_range[0],params.primary_range[1]};
        const std::array<float,9> rotation{params.cube_row0[0],params.cube_row0[1],params.cube_row0[2],params.cube_row1[0],params.cube_row1[1],params.cube_row1[2],
            params.cube_row2[0],params.cube_row2[1],params.cube_row2[2]};
        check(source.render(rays,input,camera,encoding,&history,roughness,metallic,mode==2,range,rotation),rays.status().c_str());
        const auto layout=native_reflection_history(width,height,history.extent,true,{},history.model_lobes,history.model_paths);
        check(layout && rays.reflection_output().reflection_history==*layout,"MODEL producer returned the wrong record ABI");
        const auto actual=source.get(rays.reflection_output());
        try {verify_models(input,params,history,actual,n);}catch(const std::exception& e){throw std::runtime_error(std::string(e.what())+" encoding/variant/roughness/metal/context/view/mode="
            +std::to_string(encoding)+"/"+std::to_string(variant)+"/"+std::to_string(roughness)+"/"+std::to_string(metallic)+"/"+std::to_string(context)+"/"+std::to_string(view)+"/"+std::to_string(mode));}
        check(source.render(rays,input,camera,encoding,nullptr,roughness,metallic,mode==2,range,rotation),rays.status().c_str());
        const auto ordinary=source.get(rays.reflection_output());const auto changed=std::mismatch(ordinary.begin(),ordinary.end(),actual.begin());
        if(changed.first!=ordinary.end())throw std::runtime_error("MODEL history changed current reflection RGBA pixel="+std::to_string(changed.first-ordinary.begin())
            +" current/plain="+std::to_string(*changed.second)+"/"+std::to_string(*changed.first)+" case="+std::to_string(n.cases));
        check(!rays.reflection_output().reflection_history.storage_bytes,"MODEL history OFF retained stale record layout");
    }
    std::cout<<"Native MODEL history: cases="<<n.cases<<" checked="<<n.checked<<" clear="<<n.clear<<" lobe/path records="<<n.records
        <<" finite/escape/budget="<<n.finite<<'/'<<n.escape<<'/'<<n.budget<<" mirror hops="<<n.hops<<" rough="<<n.rough<<" hemisphere clips="<<n.clips
        <<" boundary exclusions="<<n.edges<<" max RGB/feature/response/depth="<<n.max_rgb<<'/'<<n.max_feature<<'/'<<n.max_response<<'/'<<n.max_depth
        <<" max input-derived bary allowance="<<n.max_bary_allowance<<'\n'<<std::flush;
    check(n.cases==2592 && n.checked>1000000 && n.clear>100000 && n.records>1000000 && n.finite>100000 && n.escape>10000
        && n.budget>1000 && n.hops>10000 && n.rough>500000 && n.clips>100,"Insufficient compact/ordered MODEL history witnesses");
    for(auto count:n.conductors)check(count>100000,"Missing MODEL history conductor");
    unsigned negatives=0,cycles=0;
    auto input=cube_input(16,4,2);auto old=input.source.source.geometry.vertices;for(auto& v:old)v[3]=1;
    const Camera camera{width,height,96,31.3,22.7,85};
    for(unsigned lobes:{1U,8U})for(bool paths:{false,true}) {
        RayReflectionHistory history{16,{173,119},{141,132,83.7,58.2},1,900,4,true};history.model_lobes=lobes;history.model_paths=paths;
        source.set(input,old,mapping,history);const float roughness=lobes==8?.35f:0;
        check(source.render(rays,input,camera,2,&history,roughness,2,paths),rays.status().c_str());
        const auto reference=source.get(rays.reflection_output());
        for(unsigned failure=0;failure<12;++failure) {
            auto bad=history;auto frame=camera;float rough=roughness;bool specular=paths;
            switch(failure) {
                case 0:bad.separated=false;break;
                case 1:bad.model_lobes=4;break;
                case 2:bad.model_lobes=lobes==8?1:8;break;
                case 3:bad.previous_vertex_offset=0;break;
                case 4:bad.previous_vertex_offset=192;break;
                case 5:bad.previous_index_offset=bad.previous_vertex_offset+4;break;
                case 6:bad.previous_index_offset=capacity;break;
                case 7:bad.scene_paths=true;break;
                case 8:bad.projection[0]=1.e-300;break;
                case 9:rough=std::numeric_limits<float>::quiet_NaN();break;
                case 10:if(paths)bad.model_paths=false;else specular=true;break;
                case 11:frame.width=frame.height=16384;break;
            }
            check(!source.render(rays,input,frame,2,&bad,rough,2,specular),"Malformed/unsupported MODEL history accepted");
            check(!rays.reflection_output().buffer && !rays.reflection_output().reflection_history.storage_bytes,"Rejected MODEL history exposed stale output");
            check(source.render(rays,input,camera,2,&history,roughness,2,paths),rays.status().c_str());
            check(source.get(rays.reflection_output())==reference,"MODEL history failed exact record recovery");++negatives;
        }
        check(rays.native_work_complete() && rays.try_release_device(),"Completed MODEL history cannot release nonblocking");
        check(!rays.working_image_bytes() && !rays.reflection_output().buffer,"MODEL history release retained borrowed images");
        check(source.render(rays,input,camera,2,&history,roughness,2,paths),rays.status().c_str());
        check(source.get(rays.reflection_output())==reference,"MODEL history reinitialize changed records");++cycles;
    }
    rays.release_device();check(!rays.working_image_bytes() && rays.native_work_complete(),"MODEL history release retained images");
    std::cout<<"MODEL malformed/unsupported requests="<<negatives<<", exact record recoveries="<<negatives<<", release/reinitialize cycles="<<cycles<<'\n';
    std::cout<<"Current RGBA exactly unchanged for compact/ordered history; independent incident radiance/current response; NOT Linux/application/history reuse/FPS acceptance\n";
}
#include "native_scene_history_fixture.inc"
#include "native_planar_motion_fixture.inc"
#include "native_liquid_motion_fixture.inc"
}
int main(int argc,char** argv) try {
    bool integrated=false,model_only=false,scene_only=false,scene_recovery=false,planar_only=false,planar_recovery=false,liquid_only=false,liquid_recovery=false;for(int i=1;i<argc;++i){if(std::string(argv[i])=="--integrated" && !integrated)integrated=true;
        else if(std::string(argv[i])=="--model-only" && !model_only && !scene_only && !scene_recovery && !planar_only && !planar_recovery)model_only=true;
        else if(std::string(argv[i])=="--scene-only" && !scene_only && !model_only && !scene_recovery && !planar_only && !planar_recovery)scene_only=true;
        else if(std::string(argv[i])=="--scene-recovery-only" && !scene_recovery && !model_only && !scene_only && !planar_only && !planar_recovery)scene_recovery=true;
        else if(std::string(argv[i])=="--planar-only" && !planar_only && !model_only && !scene_only && !scene_recovery && !planar_recovery)planar_only=true;
        else if(std::string(argv[i])=="--planar-recovery-only" && !planar_recovery && !planar_only && !model_only && !scene_only && !scene_recovery)planar_recovery=true;
        else if(std::string(argv[i])=="--liquid-only" && !liquid_only)liquid_only=true;
        else if(std::string(argv[i])=="--liquid-recovery-only" && !liquid_recovery)liquid_recovery=true;
        else check(false,"Usage: starfox_vulkan_reflection_history_check [--integrated] [--model-only | --scene-only | --scene-recovery-only | --planar-only | --planar-recovery-only | --liquid-only | --liquid-recovery-only]");}
    check(unsigned(model_only)+scene_only+scene_recovery+planar_only+planar_recovery+liquid_only+liquid_recovery<=1,"Diagnostic selectors are mutually exclusive");
    Device device;check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());const auto props=SDL_CreateProperties();check(props,SDL_GetError());
    SDL_SetStringProperty(props,SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,"vulkan");SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN,true);
    SDL_SetBooleanProperty(props,SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN,integrated);check(request_vulkan_ray_query(props),"Cannot request native rays");
    device.value=SDL_CreateGPUDeviceWithProperties(props);SDL_DestroyProperties(props);check(device.value,SDL_GetError());
    SDL_SetBooleanProperty(SDL_GetGPUDeviceProperties(device.value),"starfox.vulkan.ray_query.enabled",true);
    const auto support=query_vulkan_ray_query(device.value);if(!support.available){std::cerr<<support.status<<"; SKIP, not acceptance\n";return 2;}
    const auto* bridge=static_cast<const StarfoxSdlVulkanBridgeV2*>(SDL_GetPointerProperty(SDL_GetGPUDeviceProperties(device.value),STARFOX_SDL_VULKAN_BRIDGE,nullptr));
    check(bridge && bridge->version==2,"Missing pinned Vulkan device bridge");
    const auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(bridge->get_instance_proc(bridge->instance,"vkGetPhysicalDeviceProperties"));
    VkPhysicalDeviceProperties physical{};check(properties,"Missing device properties");properties(bridge->physical_device,&physical);
    std::cout<<((liquid_only || liquid_recovery)?"Native separated curved liquid optical motion on ":(planar_only || planar_recovery)?"Native separated planar optical motion on ":(scene_only || scene_recovery)?"Native MODEL/planar reflection records on ":model_only?"Native MODEL reflection records on ":"Native sharp reflection history on ")<<physical.deviceName<<'\n'<<std::flush;
    if(integrated && physical.deviceType!=VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)return 2;
    Source source(device.value);NativeRayOwner rays;Counts counts;
#if defined(STARFOX_NATIVE_SDL_VULKAN_ADAPTER_PROBE)
    std::cout<<"Routing through the live calibrated SDL ray adapter (native Vulkan)\n"<<std::flush;
#endif
    if(model_only){run_model_history(source,rays);return 0;}
    if(scene_only){run_scene_history(source,rays);return 0;}
    if(scene_recovery){run_scene_recovery(source,rays);return 0;}
    if(planar_only){run_planar_motion(source,rays);return 0;}
    if(planar_recovery){run_planar_recovery(source,rays);return 0;}
    if(liquid_only){run_liquid_motion(source,rays);return 0;}
    if(liquid_recovery){run_liquid_recovery(source,rays);return 0;}
    constexpr std::array<unsigned,4> mapping{17,9,6,12};
    for(unsigned encoding:{1U,2U})for(unsigned variant:{0U,3U,4U})for(unsigned view=0;view<2;++view)
    for(bool cube:{false,true})for(unsigned pose=0;pose<8;++pose)for(bool matched:{false,true}) {
        auto input=cube_input(16,variant,encoding);if(!cube){input.cube.clear();input.size=0;}
        constexpr std::array<std::array<float,2>,6> corners{{{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}}};
        auto& current=input.source.source.geometry.vertices;
        for(unsigned i=0;i<12;++i){current[i]={corners[i%6][0]*(i<6?450.f:1200.f),corners[i%6][1]*(i<6?300.f:900.f),i<6?400.f:-200.f,0};
            if(view && i<6)current[i][2]+=.22f*current[i][1]+.08f*current[i][0];}
        auto old=current;for(auto& p:old)p[3]=1;
        if(pose==1 || pose==2)for(unsigned i=0;i<12;++i){old[i][0]+=i<6?30:10;old[i][1]+=i<6?-20:8;old[i][2]+=i<6?25:-15;
            if(pose==2){old[i][0]*=.9f;old[i][2]+=.11f*old[i][1]-.07f*old[i][0];}}
        if(pose==3)for(unsigned i=0;i<6;++i){old[i][0]*=.33f;old[i][1]*=.33f;}
        if(pose==4)for(unsigned i=6;i<12;++i)old[i][3]=0;
        if(pose==5)for(unsigned i=1;i<6;++i)old[i]=old[0];
        if(pose==7)for(unsigned i=6;i<12;++i)old[i][0]=std::numeric_limits<float>::quiet_NaN();
        RayReflectionHistory history{16,{173,119},{141,132,83.7,58.2},pose==6?460.:1.,900.,matched?4U:0U};
        source.set(input,old,mapping,history);const Camera camera{width,height,view?46.:96.,31.3,22.7,view?39.:85.};
        const bool ok=source.render(rays,input,camera,encoding,&history);check(ok,rays.status().c_str());
        check(rays.reflection_output().reflection_history==*native_reflection_history(width,height,history.extent),"Incorrect public native history layout");
        check(!rays.reflection_output().water_layers.storage_bytes,"Sharp model history invented a water prefix");
        const auto actual=source.get(rays.reflection_output());verify(input,old,mapping,camera,encoding,history,actual,counts);
        if(pose==0 && matched){const bool plain=source.render(rays,input,camera,encoding,nullptr);check(plain,rays.status().c_str());
            const auto ordinary=source.get(rays.reflection_output());check(std::equal(ordinary.begin(),ordinary.end(),actual.begin()),"History changed current reflection RGB");
            check(!rays.reflection_output().reflection_history.storage_bytes && rays.working_image_bytes()==image_bytes,"History OFF lost capacity accounting or retained stale layout");}
    }
    std::cout<<"Native sharp history: cases="<<counts.cases<<" checked="<<counts.checked<<" clear="<<counts.clear<<" secondary="<<counts.secondary
        <<" valid/invalid motion="<<counts.motion<<'/'<<counts.no_motion<<" mapped/unmapped="<<counts.mapped<<'/'<<counts.unmapped
        <<" sky="<<counts.sky<<" boundary exclusions="<<counts.edges<<" max RGB/motion/bary/depth="<<counts.max_rgb<<'/'<<counts.max_motion<<'/'<<counts.max_bary<<'/'<<counts.max_depth<<'\n'<<std::flush;
    check(counts.cases==384 && counts.checked>100000 && counts.secondary>10000 && counts.motion>10000 && counts.no_motion>10000
        && counts.mapped>10000 && counts.unmapped>10000 && counts.clear>1000 && counts.sky>1000,"Insufficient sharp reflection history witnesses");
    // Reject both malformed metadata and otherwise valid richer contracts;
    // they cannot be silently interpreted as single-hit MODEL history.
    auto input=cube_input(16,4,2);auto old=input.source.source.geometry.vertices;for(auto& p:old)p[3]=1;
    RayReflectionHistory history{16,{173,119},{141,132,83.7,58.2},1,900,4};source.set(input,old,mapping,history);
    const Camera camera{width,height,96,31.3,22.7,85};
    check(source.render(rays,input,camera,2,&history),rays.status().c_str());const auto baseline=source.get(rays.reflection_output());
    unsigned negatives=0;
    for(unsigned failure=0;failure<26;++failure) {
        auto bad=history;auto frame=camera;unsigned encoding=2,metallic=0;float roughness=0;bool specular_models=false;
        switch(failure) {
            case 0:bad.previous_vertex_offset=0;break;
            case 1:++bad.previous_vertex_offset;break;
            case 2:bad.extent[0]=0;break;
            case 3:bad.extent[1]=16385;break;
            case 4:bad.projection[0]=0;break;
            case 5:bad.projection[2]=std::numeric_limits<double>::quiet_NaN();break;
            case 6:bad.projection[1]=1.e-300;break; // Positive double collapses to zero float.
            case 7:bad.near_plane=-1;break;
            case 8:bad.far_plane=bad.near_plane;break;
            case 9:bad.near_plane=1.e10;bad.far_plane=1.e10+1;break; // Collapsed GPU interval.
            case 10:bad.previous_vertex_offset=192;break; // Overlaps current materials/cube.
            case 11:bad.previous_index_offset=bad.previous_vertex_offset+4;break;
            case 12:++bad.previous_index_offset;break;
            case 13:bad.previous_vertex_offset=0xfffffff0U;bad.previous_index_offset=0;break;
            case 14:bad.previous_vertex_offset=capacity;bad.previous_index_offset=0;break; // Actual SDK source range.
            case 15:bad.previous_index_offset=capacity;break;
            case 16:bad.separated=true;break;
            case 17:bad.separated=true;bad.model_lobes=1;roughness=.4f;break;
            case 18:bad.separated=true;bad.model_lobes=8;break;
            case 19:bad.separated=true;bad.model_paths=true;break;
            case 20:bad.separated=true;bad.model_lobes=1;bad.model_paths=bad.scene_paths=true;break;
            case 21:encoding=0;break;
            case 22:roughness=.1f;break;
            case 23:metallic=1;break;
            case 24:specular_models=true;break;
            case 25:frame.width=frame.height=16384;break; // Output byte count overflows, no allocation.
        }
        const bool accepted=source.render(rays,input,frame,encoding,&bad,roughness,metallic,specular_models);
        if(accepted)throw std::runtime_error("Malformed/unsupported history accepted case="+std::to_string(failure));
        const auto output=rays.reflection_output();check(!output.buffer && !output.reflection_history.storage_bytes && !output.water_layers.storage_bytes,
            "Rejected history exposed stale borrowed output");
        check(source.render(rays,input,camera,2,&history),rays.status().c_str());
        check(source.get(rays.reflection_output())==baseline,"History failed exact recovery after malformed request");++negatives;
    }
    for(unsigned cycle=0;cycle<4;++cycle) {
        check(rays.native_work_complete() && rays.try_release_device(),"Completed history owner cannot release nonblocking");
        check(!rays.working_image_bytes() && !rays.reflection_output().buffer,"History release retained borrowed output or image accounting");
        check(source.render(rays,input,camera,2,&history),rays.status().c_str());
        check(source.get(rays.reflection_output())==baseline,"History reinitialize changed records");
    }
    std::cout<<"Malformed/unsupported history rejected="<<negatives<<", exact recoveries="<<negatives
        <<", release/reinitialize cycles=4\n";
    rays.release_device();check(!rays.working_image_bytes() && rays.native_work_complete(),"History owner release retained live images");
    std::cout<<"Sharp current RGB unchanged, explicit accepted mapping and GPU previous poses; NOT Linux/application/history-resolver/FPS acceptance\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
