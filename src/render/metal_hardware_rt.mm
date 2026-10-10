#import <Metal/Metal.h>
#include <SDL3/SDL.h>

#include "starfox/render/metal_hardware_rt.hpp"
#include "metal_water_shared.hpp"
#include "metal_native_material_shared.hpp"
#include "starfox/render/gpu_background.hpp"
#include "starfox/render/environment_effects.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

// These are supplied by the pinned SDL Metal backend patch. They expose no
// Metal struct layout to the game and preserve SDL's buffer-cycle tracking.
extern "C" {
void* SDL_StarfoxMetalDevice(SDL_GPUDevice*);
void* SDL_StarfoxMetalCommandBuffer(SDL_GPUCommandBuffer*);
void* SDL_StarfoxMetalBuffer(SDL_GPUBuffer*);
bool SDL_StarfoxMetalTrackBuffer(SDL_GPUCommandBuffer*, SDL_GPUBuffer*);
bool SDL_StarfoxMetalPrepareWrite(SDL_GPUDevice*, SDL_GPUCommandBuffer*, SDL_GPUBuffer*);
}

namespace starfox::render::shadows {
namespace {
struct alignas(16) Float4 { float x{}, y{}, z{}, w{}; };
struct alignas(16) Parameters {
    std::uint32_t width{}, height{};
    float focal_x{}, focal_y{};
    float center_x{}, center_y{}, has_ground{}, ground_only{};
    Float4 ground_point{}, ground_normal{};
    std::array<Float4,16> lights{};
    std::array<std::uint32_t,4> coverage{};
};
static_assert(sizeof(Parameters)==336);
static_assert(offsetof(Parameters,lights)==64 && offsetof(Parameters,coverage)==320);
struct alignas(16) ReflectionParameters {
    std::uint32_t width{},height{},quality{},metallic{};
    float focal_x{},focal_y{},center_x{},center_y{};
    float roughness{},has_ground{};
    std::uint32_t environment{},texel_count{};
    Float4 ground_point{},ground_normal{};
    Float4 water_settings{},water_row0{},water_row1{},water_row2{};
    std::array<std::uint32_t,4> backdrop{};
    std::array<std::uint32_t,4> enhanced_size{};
    Float4 enhanced_motion{},enhanced_plane{},enhanced_projection{},enhanced_palette{},enhanced_keep0{},enhanced_keep1{};
};
static_assert(sizeof(ReflectionParameters)==272);

// Shared by both native kernels and the exact-source Apple compiler/runtime
// checks. Reject candidates in-place: restarting past a transparent hit with
// an epsilon can discard a nearby or coincident opaque triangle.
constexpr char kIndexedShader[] = R"METAL(
#include <metal_stdlib>
#include <metal_raytracing>
using namespace metal;
using namespace metal::raytracing;
struct RayMaterial {
    float uv[6];
    uint textured,dither,even,odd,colour_base,face;
    uint offset,u_mask,v_mask,reserved;
};
uint indexed_source_index(RayMaterial material,float2 bary,uint2 pixel,
    device const uchar* texels,uint texel_count) {
    if(material.reserved!=0u || material.textured>1u || material.colour_base>255u) return 256u;
    uint index=((pixel.x+pixel.y)&1u)&&material.dither!=0u?material.odd:material.even;
    if(material.textured!=0u) {
        if(material.u_mask>4095u || material.v_mask>4095u
            || (material.u_mask&(material.u_mask+1u))!=0u
            || (material.v_mask&(material.v_mask+1u))!=0u
            || material.offset>texel_count
            || (material.u_mask+1u)*(material.v_mask+1u)>texel_count-material.offset) return 256u;
        float2 uv=float2(material.uv[0],material.uv[1])*(1.0f-bary.x-bary.y)
            +float2(material.uv[2],material.uv[3])*bary.x
            +float2(material.uv[4],material.uv[5])*bary.y;
        if(!all(isfinite(uv)) || any(abs(uv)>float2(1e8f))) return 256u;
        uint2 tile=uint2(int2(floor(uv)))&uint2(material.u_mask,material.v_mask);
        uint at=material.offset+tile.y*(material.u_mask+1u)+tile.x;
        uint texel=uint(texels[at]);
        // Texel zero is a hole BEFORE palette-base addition. Solid ink zero
        // and nonzero texture ink wrapping to palette zero remain opaque.
        if(texel==0u) return 256u;
        index=(texel+material.colour_base)&255u;
    }
    return index<=255u?index:256u;
}
struct VisibleHit {
    intersection_type type;
    float distance;
    uint primitive_id;
    float2 triangle_barycentric_coord;
};
VisibleHit visible_hit(ray cast,primitive_acceleration_structure scene,uint2 pixel,
    device const RayMaterial* materials,device const uchar* texels,uint texel_count,
    bool indexed,bool any_hit,uint4 native_coverage=uint4(0u)) {
    VisibleHit result={intersection_type::none,65536.0f,0u,float2(0.0f)};
    if(!indexed) {
        // Geometry-only/solid shadow casters need neither material uploads
        // nor per-candidate texture tests.
        intersector<triangle_data> tracer;
        tracer.force_opacity(forced_opacity::opaque);
        tracer.accept_any_intersection(any_hit);
        auto hit=tracer.intersect(cast,scene);
        result.type=hit.type;
        if(hit.type==intersection_type::triangle) {
            result.distance=hit.distance;result.primitive_id=hit.primitive_id;
            result.triangle_barycentric_coord=hit.triangle_barycentric_coord;
        }
        return result;
    }
    intersection_params params;
    params.force_opacity(forced_opacity::non_opaque);
    params.assume_geometry_type(geometry_type::triangle);
    params.accept_any_intersection(any_hit);
    intersection_query<triangle_data> query(cast,scene,params);
    while(query.next()) {
        if(query.get_candidate_intersection_type()!=intersection_type::triangle) continue;
        uint primitive=query.get_candidate_primitive_id();
        float2 bary=query.get_candidate_triangle_barycentric_coord();
        bool opaque=native_coverage.z==2u
            ?starfox_native_material_coverage(primitive,bary.x,bary.y,pixel.x,pixel.y,
                reinterpret_cast<device const uint*>(materials),native_coverage.x,
                native_coverage.y,native_coverage.w)!=0u
            :indexed_source_index(materials[primitive],bary,pixel,texels,texel_count)<=255u;
        if(opaque) {
            query.commit_triangle_intersection();
            if(any_hit) break;
        }
    }
    result.type=query.get_committed_intersection_type();
    if(result.type==intersection_type::triangle) {
        result.distance=query.get_committed_distance();
        result.primitive_id=query.get_committed_primitive_id();
        result.triangle_barycentric_coord=query.get_committed_triangle_barycentric_coord();
    }
    return result;
}
)METAL";

constexpr char kShadowShader[] = R"METAL(
#include <metal_stdlib>
#include <metal_raytracing>
using namespace metal;
using namespace metal::raytracing;
struct Parameters {
    uint width, height;
    float focal_x, focal_y;
    float center_x, center_y, has_ground, ground_only;
    float4 ground_point, ground_normal;
    float4 lights[16];
    uint4 coverage;
};
kernel void starfox_hardware_shadow(
    primitive_acceleration_structure scene [[buffer(0)]],
    device uint* output [[buffer(1)]],
    constant Parameters& p [[buffer(2)]],
    device const RayMaterial* materials [[buffer(3)]],
    device const uchar* texels [[buffer(4)]],
    uint id [[thread_position_in_grid]]) {
    if (id >= p.width*p.height) return;
    uint x=id%p.width, y=id/p.width;
    float3 direction=float3((float(x)+0.5f-p.center_x)/p.focal_x,
        (float(y)+0.5f-p.center_y)/p.focal_y,1.0f);
    float receiver=65536.0f;
    if (p.ground_only<0.5f) {
        ray primary(float3(0.0f),direction,1.0f,65536.0f);
        auto hit=visible_hit(primary,scene,uint2(x,y),materials,texels,p.coverage.y,p.coverage.z!=0u,false,p.coverage);
        if (hit.type==intersection_type::triangle) receiver=hit.distance;
    }
    if (p.has_ground>0.5f) {
        float denominator=dot(direction,p.ground_normal.xyz);
        if (abs(denominator)>1.e-10f) {
            float ground=dot(p.ground_point.xyz,p.ground_normal.xyz)/denominator;
            if (ground>1.0f && ground<receiver) receiver=ground;
        }
    }
    if (receiver>=65536.0f) {output[id]=0;return;}
    float3 point=direction*receiver;
    float bias=max(0.1f,receiver*1.e-5f);
    uint blocked=0;
    for(uint sample=0;sample<uint(p.lights[0].w);++sample) {
        ray shadow(point,p.lights[sample].xyz,bias,65536.0f);
        auto obstacle=visible_hit(shadow,scene,uint2(x,y),materials,texels,p.coverage.y,p.coverage.z!=0u,true,p.coverage);
        blocked+=obstacle.type!=intersection_type::none;
    }
    output[id]=160u*blocked/uint(p.lights[0].w);
}
)METAL";

constexpr char kReflectionShader[] = R"METAL(
#include <metal_stdlib>
#include <metal_raytracing>
using namespace metal;
using namespace metal::raytracing;
struct Parameters {
    uint width,height,quality,metallic;
    float focal_x,focal_y,center_x,center_y;
    float roughness,has_ground;
    uint environment,texel_count;
    float4 ground_point,ground_normal;
    float4 water_settings,water_row0,water_row1,water_row2;
    uint4 backdrop;
    uint4 enhanced_size;
    float4 enhanced_motion,enhanced_plane,enhanced_projection,enhanced_palette,enhanced_keep0,enhanced_keep1;
};
float3 rgb(uint packed) {
    return float3(float(packed&255u),float((packed>>8)&255u),
        float((packed>>16)&255u))/255.0f;
}
uint rgba(float3 colour) {
    uint3 c=uint3(round(clamp(colour,0.0f,1.0f)*255.0f));
    return c.x|(c.y<<8)|(c.z<<16)|0xff000000u;
}
uint palette_hit(uint primitive,float2 bary,uint2 pixel,
    device const RayMaterial* materials,device const uint* palette,
    device const uchar* texels,uint texel_count) {
    uint index=indexed_source_index(materials[primitive],bary,pixel,texels,texel_count);
    return index<=255u?palette[index]|0xff000000u:0u;
}
#define SF_VISIBLE_HIT(NAME, ORIGIN, DIRECTION, MINIMUM, MAXIMUM) \
    auto NAME=visible_hit(ray(ORIGIN,DIRECTION,MINIMUM,MAXIMUM),scene,pixel,materials,texels,p.texel_count,true,false);
float3 enhanced_bilinear(float2 at,constant Parameters& p,device const uint* enhanced) {
    uint2 lo=min(uint2(at),p.enhanced_size.xy-1u),hi=min(lo+1u,p.enhanced_size.xy-1u);
    float2 f=fract(at);uint w=p.enhanced_size.x;
    return mix(mix(rgb(enhanced[lo.y*w+lo.x]),rgb(enhanced[lo.y*w+hi.x]),f.x),
        mix(rgb(enhanced[hi.y*w+lo.x]),rgb(enhanced[hi.y*w+hi.x]),f.x),f.y);
}
bool inside_keep(float2 point,float4 ellipse) {
    if(ellipse.z<=0.0f || ellipse.w<=0.0f) return false;
    float2 offset=(point-ellipse.xy)/ellipse.zw;return dot(offset,offset)<=1.0f;
}
uint sample_backdrop(float2 at,constant Parameters& p,device const uint* panorama,device const uint* palette,device const uint* enhanced) {
    float x=at.x-128.0f,slope=p.enhanced_plane.x;
    if(p.enhanced_size.z!=0u && at.y<p.enhanced_motion.x+slope*x
        && !inside_keep(float2(x,at.y),p.enhanced_keep0) && !inside_keep(float2(x,at.y),p.enhanced_keep1)) {
        float inverse=rsqrt(1.0f+slope*slope),dy=at.y-p.enhanced_motion.x;
        float u=(inverse*(x+slope*dy)+p.enhanced_plane.z)*p.enhanced_projection.x;
        float v=p.enhanced_projection.z+inverse*(dy-slope*x)*p.enhanced_projection.y;
        uint overlap=max(1u,p.enhanced_size.x/32u),period=max(1u,p.enhanced_size.x-overlap);
        float tx=fract(u)*float(period),ty=clamp(v,0.0f,1.0f)*float(p.enhanced_size.y-1u);
        float3 sky=enhanced_bilinear(float2(tx,ty),p,enhanced);
        if(tx<float(overlap)) {
            float blend=tx/float(overlap);blend=blend*blend*(3.0f-2.0f*blend);
            sky=mix(enhanced_bilinear(float2(float(period)+tx,ty),p,enhanced),sky,blend);
        }
        return rgba((sky*p.enhanced_palette.w+p.enhanced_palette.xyz)*p.enhanced_plane.w);
    }
    if(p.backdrop.x==0u || p.backdrop.y==0u) return p.environment;
    int2 pixel=clamp(int2(floor(at))+int2(p.backdrop.z,0),int2(0),int2(p.backdrop.xy)-1);
    uint value=panorama[uint(pixel.y)*p.backdrop.x+uint(pixel.x)];
    return (value&0x04000000u)!=0u?palette[value&255u]:p.environment;
}
uint reflected_backdrop(float3 direction,constant Parameters& p,device const uint* panorama,device const uint* palette,device const uint* enhanced) {
    return sample_backdrop(float2(128.0f+atan2(direction.x,direction.z)*256.0f,
        112.0f+atan2(direction.y,length(direction.xz))*256.0f),p,panorama,palette,enhanced);
}
float3 view_to_world(float3 value,constant Parameters& p) {
    return float3(dot(value,float3(p.water_row0.x,p.water_row1.x,p.water_row2.x)),
        dot(value,float3(p.water_row0.y,p.water_row1.y,p.water_row2.y)),
        dot(value,float3(p.water_row0.z,p.water_row1.z,p.water_row2.z)));
}
float3 world_to_view(float3 value,constant Parameters& p) {
    return float3(dot(p.water_row0.xyz,value),dot(p.water_row1.xyz,value),dot(p.water_row2.xyz,value));
}
struct LavaOptics {
    float3 hit,normal,radiance;
    float share,bias;
};
LavaOptics lava_optics(float3 origin,float3 direction,float distance,constant Parameters& p) {
    LavaOptics result;
    result.hit=origin+direction*distance;
    float3 position=view_to_world(result.hit,p)+float3(p.water_row0.w,p.water_row1.w,p.water_row2.w);
    float footprint=length(result.hit)/max(p.focal_x,1.0f)
        /max(abs(dot(direction,p.ground_normal.xyz)),.04f);
    LavaSample liquid=lava_surface(position.x,position.z,p.water_settings.x,footprint);
    float3 travel=view_to_world(direction,p);
    float shift=clamp(-liquid.height/max(travel.y,.12f),-distance*.2f,distance*.2f);
    position+=travel*shift;result.hit+=direction*shift;
    liquid=lava_surface(position.x,position.z,p.water_settings.x,footprint);
    result.normal=normalize(world_to_view(float3(liquid.dx,-1,liquid.dz),p));
    if(dot(result.normal,direction)>0.0f) result.normal=-result.normal;
    float3 viewer=view_to_world(-direction,p);
    LavaColour colour=lava_shade(liquid,viewer.x,viewer.y,viewer.z);
    float3 molten=float3(colour.r,colour.g,colour.b)*p.water_settings.z;
    result.radiance=molten*molten;
    result.share=(.035f+.40f*pow(1.0f-clamp(dot(-direction,result.normal),0.0f,1.0f),5.0f))
        *p.water_settings.y*(.3f+.7f*liquid.crust);
    result.bias=max(.05f,distance*1e-5f);
    return result;
}
uint reflected_accumulated(uint terminal,float3 throughput,float3 accumulated) {
    float3 colour=rgb(terminal);
    return rgba(sqrt(clamp(accumulated+throughput*colour*colour,0.0f,1.0f)));
}
uint trace_mirrors(float3 origin,float3 direction,uint2 pixel,float minimum,bool escaping_source_ground,
    primitive_acceleration_structure scene,constant Parameters& p,device const float4* vertices,
    device const RayMaterial* materials,device const uint* palette,device const uchar* texels,
    device const uint* panorama,device const uint* enhanced) {
    uint flags=uint(p.water_settings.w);
    float3 throughput=float3(1.0f),accumulated=float3(0.0f);
    for(uint bounce=0u;bounce<4u;++bounce) {
        SF_VISIBLE_HIT(obstacle,origin,direction,minimum,65536.0f)
        bool model=obstacle.type==intersection_type::triangle;
        float distance=model?obstacle.distance:65536.0f;
        float denominator=dot(direction,p.ground_normal.xyz);
        float ground_t=abs(denominator)>1e-6f?dot(p.ground_point.xyz-origin,p.ground_normal.xyz)/denominator:-1.0f;
        bool floor_hit=p.has_ground>.5f && ground_t>minimum && ground_t<distance
            && !(escaping_source_ground && bounce==0u && denominator>0.0f);
        float3 normal;
        if(floor_hit) {
            distance=ground_t;
            float3 hit=origin+direction*distance;
            if((flags&15u)==3u) {
                LavaOptics lava=lava_optics(origin,direction,distance,p);
                accumulated+=throughput*lava.radiance;throughput*=lava.share;
                if(all(throughput<float3(1e-5f))) return rgba(sqrt(clamp(accumulated,0.0f,1.0f)));
                origin=lava.hit+lava.normal*lava.bias;
                direction=reflect(direction,lava.normal);minimum=lava.bias;
                continue;
            }
            if((flags&15u)!=1u && (flags&15u)!=2u) {
                uint colour=hit.z>1.0f?sample_backdrop(float2(128.0f,112.0f)+256.0f*hit.xy/hit.z,p,panorama,palette,enhanced)
                    :reflected_backdrop(hit,p,panorama,palette,enhanced);
                return reflected_accumulated(colour,throughput,accumulated);
            }
            normal=normalize(p.ground_normal.xyz);
            if(dot(normal,direction)>0.0f) normal=-normal;
            if((flags&15u)==2u) {
                float grazing=pow(1.0f-clamp(dot(-direction,normal),0.0f,1.0f),5.0f);
                float3 f0=float3(1.0f,.766f,.336f);throughput*=f0+(1.0f-f0)*grazing;
            }
        } else if(model) {
            uint primitive=obstacle.primitive_id;
            if((flags&16u)==0u) return reflected_accumulated(palette_hit(primitive,
                obstacle.triangle_barycentric_coord,pixel,materials,palette,texels,p.texel_count),throughput,accumulated);
            uint first=primitive*3u;
            normal=normalize(cross(vertices[first+1u].xyz-vertices[first].xyz,
                vertices[first+2u].xyz-vertices[first].xyz));
        } else return reflected_accumulated(reflected_backdrop(direction,p,panorama,palette,enhanced),throughput,accumulated);
        if(dot(normal,direction)>0.0f) normal=-normal;
        minimum=max(.05f,distance*1e-5f);
        origin+=direction*distance+normal*minimum;direction=reflect(direction,normal);
    }
    return reflected_accumulated(reflected_backdrop(direction,p,panorama,palette,enhanced),throughput,accumulated);
}
float hash(uint n) {
    n=(n^61u)^(n>>16u);n*=9u;n^=n>>4u;n*=0x27d4eb2du;n^=n>>15u;
    return float(n&65535u)/65535.0f;
}
kernel void starfox_hardware_reflection(
    primitive_acceleration_structure scene [[buffer(0)]],
    device uint* output [[buffer(1)]],
    constant Parameters& p [[buffer(2)]],
    device const float4* vertices [[buffer(3)]],
    device const RayMaterial* materials [[buffer(4)]],
    device const uint* palette [[buffer(5)]],
    device const uchar* texels [[buffer(6)]],
    device const uint* panorama [[buffer(7)]],
    device const uint* enhanced [[buffer(8)]],
    uint id [[thread_position_in_grid]]) {
    if(id>=p.width*p.height) return;
    uint2 pixel=uint2(id%p.width,id/p.width);
    float3 direction=float3((float(pixel.x)+0.5f-p.center_x)/p.focal_x,
        (float(pixel.y)+0.5f-p.center_y)/p.focal_y,1.0f);
    VisibleHit hit={intersection_type::none,65536.0f,0u,float2(0.0f)};
    if(p.ground_point.w<0.5f) {
        SF_VISIBLE_HIT(primary,float3(0.0f),direction,1.0f,65536.0f)
        hit=primary;
    }
    // Ground-only exposure excludes primary models, retaining all secondary
    // reflected/transmitted casters. ground_point.w is the receiver flag.
    float distance=hit.type==intersection_type::triangle?hit.distance:65536.0f;
    bool ground_hit=false;
    if(p.has_ground>0.5f) {
        float denominator=dot(direction,p.ground_normal.xyz);
        if(abs(denominator)>1.e-10f) {
            float ground=dot(p.ground_point.xyz,p.ground_normal.xyz)/denominator;
            if(ground>1.0f && ground<distance) {
                distance=ground;ground_hit=true;
            }
        }
    }
    if(distance>=65536.0f) {output[id]=0u;return;}
    float3 normal;
    if(ground_hit) normal=normalize(p.ground_normal.xyz);
    else {
        uint first=hit.primitive_id*3u;
        float3 a=vertices[first].xyz,b=vertices[first+1u].xyz,c=vertices[first+2u].xyz;
        normal=normalize(cross(b-a,c-a));
    }
    if(dot(normal,direction)>0.0f) normal=-normal;
    float3 point=direction*distance;
    uint water_material=uint(p.water_settings.w)&15u;
    if(ground_hit && water_material==3u) {
        float3 incoming=normalize(direction);
        LavaOptics lava=lava_optics(float3(0.0f),incoming,length(point),p);
        float3 molten=sqrt(clamp(lava.radiance,0.0f,1.0f));
        if(lava.share>0.0f) {
            uint colour=trace_mirrors(lava.hit+lava.normal*lava.bias,reflect(incoming,lava.normal),pixel,
                lava.bias,true,scene,p,vertices,materials,palette,texels,panorama,enhanced);
            molten+=rgb(colour)*lava.share;
        }
        output[id]=(rgba(molten)&0x00ffffffu)|0xfe000000u;
        return;
    }
    if(ground_hit && (water_material==1u || water_material==2u)) {
        float3 incoming=normalize(direction),cast=reflect(incoming,normal);
        float bias=max(.05f,length(point)*1e-5f);
        float3 value=float3(0.0f);
        if(p.water_settings.y>0.0f) {
            value=rgb(trace_mirrors(point+normal*bias,cast,pixel,bias,true,
                scene,p,vertices,materials,palette,texels,panorama,enhanced));
            float grazing=pow(1.0f-clamp(dot(-incoming,normal),0.0f,1.0f),5.0f);
            float3 f0=water_material==2u?float3(1.0f,.766f,.336f):float3(1.0f);
            value*=sqrt(p.water_settings.y*(f0+(1.0f-f0)*grazing));
        }
        // A completed enhanced ground surface uses the ground-coverage marker,
        // never the opaque-model marker or the original palette image.
        output[id]=(rgba(value*p.water_settings.z)&0x00ffffffu)|0xfe000000u;
        return;
    }
    if(ground_hit && p.water_settings.z>0.0f && (uint(p.water_settings.w)&15u)==0u) {
        // The rows carry world-to-view rotation; camera translation is in W.
        float3x3 rotation=transpose(float3x3(p.water_row0.xyz,p.water_row1.xyz,p.water_row2.xyz));
        float3 camera=float3(p.water_row0.w,p.water_row1.w,p.water_row2.w);
        float3 world=transpose(rotation)*point+camera;
        float t=p.water_settings.x;
        float footprint=length(point)/max(p.focal_x,1.0f)/max(abs(dot(normalize(direction),p.ground_normal.xyz)),.04f);
        float3 frequencies=float3(.022f,.054f,.024f)*footprint;
        float3 bands=1.0f/(1.0f+frequencies*frequencies*frequencies*frequencies);
        float dx=.055f*cos(world.x*.018f+world.z*.011f-t*.8f)*bands.x
            +.025f*cos(world.x*.047f-world.z*.025f+t*1.2f)*bands.y;
        float dz=.045f*cos(world.z*.022f-world.x*.009f-t*.65f)*bands.z
            -.020f*cos(world.x*.047f-world.z*.025f+t*1.2f)*bands.y;
        normal=normalize(rotation*float3(dx,-1.0f,dz));
        float3 incoming=normalize(direction);
        if(dot(normal,incoming)>0.0f) normal=-normal;
        float bias=max(.05f,distance*1.e-5f);
        float3 through=refract(incoming,normal,.75f),origin=point-normal*bias;
        float3 world_origin=transpose(rotation)*origin+camera;
        float down=(transpose(rotation)*through).y;
        float bed=down>0.0f?(world.y+640.0f-world_origin.y)/down:65536.0f;
        SF_VISIBLE_HIT(submerged,origin,through,bias,min(65536.0f,bed))
        float travel=bed;
        float3 receiver=float3(.28f,.24f,.16f);
        if(submerged.type==intersection_type::triangle) {
            receiver=rgb(palette_hit(submerged.primitive_id,submerged.triangle_barycentric_coord,
                pixel,materials,palette,texels,p.texel_count));
            receiver*=receiver;travel=submerged.distance;
        }
        uint caustics=(uint(p.water_settings.w)>>5u)&3u;
        if(caustics!=0u && travel<65536.0f) {
            float3 receiver_view=origin+through*travel;
            float3 receiver_world=transpose(rotation)*receiver_view+camera;
            float depth=receiver_world.y-world.y;
            float up=1.0f;
            if(submerged.type==intersection_type::triangle) {
                uint first=submerged.primitive_id*3u;
                float3 receiver_normal=normalize(cross(vertices[first+1u].xyz-vertices[first].xyz,
                    vertices[first+2u].xyz-vertices[first].xyz));
                if(dot(receiver_normal,through)>0.0f) receiver_normal=-receiver_normal;
                up=clamp(-(transpose(rotation)*receiver_normal).y,0.0f,1.0f);
            }
            if(depth>0.0f && up>0.0f) {
                WaterCausticSample focus=water_caustic_sample(receiver_world.x,receiver_world.z,t,depth,footprint);
                float3 entry=rotation*(float3(focus.entry_x,world.y,focus.entry_z)-camera);
                float3 segment=entry-receiver_view;float length_to_water=length(segment);
                if(length_to_water>bias*2.0f) {
                    SF_VISIBLE_HIT(underwater,receiver_view,segment/length_to_water,bias,length_to_water-bias)
                    SF_VISIBLE_HIT(overhead,entry,rotation*float3(0,-1,0),bias,65536.0f)
                    if(underwater.type==intersection_type::none && overhead.type==intersection_type::none)
                        receiver*=clamp(1.0f+(focus.irradiance-exp(-depth/1600.0f))*up*float(caustics)/3.0f,.25f,3.0f);
                }
            }
        }
        float brightness=dot(rgb(sample_backdrop(float2(128.0f,112.0f)+256.0f*point.xy/max(point.z,1.0f),p,panorama,palette,enhanced)),float3(.3f,.59f,.11f));
        float3 body=brightness*float3(.2f,.58f,.85f);body*=body;
        float3 radiance=float3(water_transmitted_channel(receiver.r,body.r,travel,.0025f),
            water_transmitted_channel(receiver.g,body.g,travel,.0008f),
            water_transmitted_channel(receiver.b,body.b,travel,.00035f));
        float3 bounce=reflect(incoming,normal);
        uint reflected_word;
        if((uint(p.water_settings.w)&16u)!=0u)
            reflected_word=trace_mirrors(point+normal*bias,bounce,pixel,bias,true,
                scene,p,vertices,materials,palette,texels,panorama,enhanced);
        else {
            SF_VISIBLE_HIT(reflected_hit,point+normal*bias,bounce,bias,65536.0f)
            reflected_word=reflected_hit.type==intersection_type::triangle
                ?palette_hit(reflected_hit.primitive_id,reflected_hit.triangle_barycentric_coord,
                    pixel,materials,palette,texels,p.texel_count):reflected_backdrop(bounce,p,panorama,palette,enhanced);
        }
        float3 reflected_colour=rgb(reflected_word);
        float fresnel=.02f+.98f*pow(1.0f-clamp(dot(-incoming,normal),0.0f,1.0f),5.0f);
        radiance=mix(radiance,reflected_colour*reflected_colour,clamp(fresnel*p.water_settings.y,0.0f,1.0f));
        output[id]=(rgba(sqrt(clamp(radiance,0.0f,1.0f))*p.water_settings.z)&0x00ffffffu)|0xfe000000u;
        return;
    }
    float3 reflected=normalize(reflect(normalize(direction),normal));
    uint rays=p.quality>=3u?4u:p.quality==2u?2u:1u;
    float3 sum=float3(0.0f);
    for(uint sample=0u;sample<rays;++sample) {
        float3 cast=reflected;
        if(sample>0u && p.roughness>0.0f) {
            float3 tangent=normalize(cross(reflected,
                abs(reflected.y)<0.9f?float3(0.0f,1.0f,0.0f):float3(1.0f,0.0f,0.0f)));
            float3 bitangent=cross(reflected,tangent);
            float angle=6.2831853f*hash(id*17u+sample*101u);
            float spread=p.roughness*0.08f*sqrt(hash(id*29u+sample*47u));
            cast=normalize(reflected+spread*(cos(angle)*tangent+sin(angle)*bitangent));
        }
        uint colour;
        if((uint(p.water_settings.w)&16u)!=0u || water_material!=0u)
            colour=trace_mirrors(point,cast,pixel,max(.1f,distance*1e-5f),false,
                scene,p,vertices,materials,palette,texels,panorama,enhanced);
        else {
            SF_VISIBLE_HIT(bounced,point,cast,max(0.1f,distance*1.e-5f),65536.0f)
            colour=bounced.type==intersection_type::triangle
                ?palette_hit(bounced.primitive_id,bounced.triangle_barycentric_coord,
                    pixel,materials,palette,texels,p.texel_count)
                :reflected_backdrop(cast,p,panorama,palette,enhanced);
        }
        sum+=rgb(colour);
    }
    float3 value=sum/float(rays);
    if(p.metallic==2u) value*=float3(1.0f,0.82f,0.34f);
    else if(p.metallic==3u) value*=float3(1.0f,0.59f,0.38f);
    output[id]=rgba(value);
}
)METAL";

[[nodiscard]] Float4 as_float4(Vec3 v) {
    return {float(v.x),float(v.y),float(v.z),0.0F};
}
[[nodiscard]] std::array<Float4,16> light_samples(Vec3 light,unsigned samples,double angular_radius) {
    const auto length=std::sqrt(dot(light,light));
    light=light*(1.0/length);
    const auto reference=std::abs(light.y)<.9?Vec3{0,1,0}:Vec3{1,0,0};
    auto tangent=cross(light,reference);
    tangent=tangent*(1.0/std::sqrt(dot(tangent,tangent)));
    const auto bitangent=cross(light,tangent);
    std::array<Float4,16> result{};
    for(unsigned i=0;i<samples;++i) {
        const auto radius=angular_radius*std::sqrt((i+.5)/samples);
        const auto angle=i*2.399963229728653;
        auto direction=light+tangent*(radius*std::cos(angle))
            +bitangent*(radius*std::sin(angle));
        direction=direction*(1.0/std::sqrt(dot(direction,direction)));
        result[i]=as_float4(direction);
    }
    result[0].w=float(samples);return result;
}
[[nodiscard]] std::string metal_error(NSError* error) {
    return error ? std::string(error.localizedDescription.UTF8String)
                 : std::string("Metal ray-tracing operation failed");
}
[[nodiscard]] bool hardware_ray_tracing(id<MTLDevice> device) {
    // Apple7/8 can expose ray intersections without dedicated RT hardware.
    // Apple identifies Apple9 (A17 Pro/M3) and later as the hardware RT family.
    if(!device || !device.supportsRaytracing) return false;
    if(@available(iOS 17.0, macOS 14.0, *))
        return [device supportsFamily:MTLGPUFamilyApple9];
    return false;
}
} // namespace

struct MetalHardwareRt::Impl {
    SDL_GPUDevice* device{}; // Borrowed from SDL renderer.
    id<MTLDevice> metal;
    id<MTLComputePipelineState> shadow_pipeline;
    id<MTLComputePipelineState> reflection_pipeline;
    SDL_GPUBuffer* output{};
    std::uint32_t output_capacity{};
    SDL_GPUBuffer* reflection_buffer{};
    std::uint32_t reflection_capacity{};
    GpuBackground reflection_backdrop;
    struct InFlight {
        SDL_GPUFence* fence{};
        id<MTLBuffer> cpu_vertices;
        id<MTLBuffer> cpu_materials;
        id<MTLBuffer> enhanced_backdrop;
        BackdropUploadCache enhanced_upload;
        id<MTLBuffer> palette;
        id<MTLBuffer> texels;
        id<MTLBuffer> scratch;
        id<MTLAccelerationStructure> acceleration;
    };
    std::array<InFlight,3> inflight{};
    unsigned serial{};
    GpuShadowOutput shadow{};
    GpuReflectionOutput reflection{};
    std::string status{"Metal hardware ray tracing not initialized"};

    ~Impl() {
        if(!device) return;
        for(auto& frame:inflight) {
            if(frame.fence) {
                SDL_WaitForGPUFences(device,true,&frame.fence,1);
                SDL_ReleaseGPUFence(device,frame.fence);
            }
        }
        if(output) SDL_ReleaseGPUBuffer(device,output);
        if(reflection_buffer) SDL_ReleaseGPUBuffer(device,reflection_buffer);
    }
    void initialize(SDL_GPUDevice* next) {
        if(device==next && shadow_pipeline) return;
        if(device && device!=next) throw std::runtime_error("Metal device changed before release");
        if(!next || !SDL_GetGPUDeviceDriver(next)
            || std::strcmp(SDL_GetGPUDeviceDriver(next),"metal")!=0)
            throw std::runtime_error("Metal GPU renderer is not active");
        metal=(__bridge id<MTLDevice>)SDL_StarfoxMetalDevice(next);
        if(!hardware_ray_tracing(metal))
            throw std::runtime_error("This Metal device has no dedicated ray-tracing hardware");
        NSError* error=nil;
        NSString* source=[[NSString stringWithUTF8String:starfox_metal_native_material_shared]
            stringByAppendingString:[NSString stringWithUTF8String:kIndexedShader]];
        source=[source stringByAppendingString:[NSString stringWithUTF8String:kShadowShader]];
        MTLCompileOptions* options=[MTLCompileOptions new];
        if(@available(iOS 18.0, macOS 15.0, *)) options.mathMode=MTLMathModeSafe;
        else options.fastMathEnabled=NO;
        id<MTLLibrary> library=[metal newLibraryWithSource:source options:options error:&error];
        if(!library) throw std::runtime_error(metal_error(error));
        id<MTLFunction> function=[library newFunctionWithName:@"starfox_hardware_shadow"];
        shadow_pipeline=[metal newComputePipelineStateWithFunction:function error:&error];
        if(!shadow_pipeline) throw std::runtime_error(metal_error(error));
        device=next;
        status="Metal hardware acceleration structures and ray intersector";
    }
    void ensure_reflection_pipeline() {
        if(reflection_pipeline) return;
        NSError* error=nil;
        NSString* source=[[NSString stringWithUTF8String:starfox_metal_water_shared]
            stringByAppendingString:[NSString stringWithUTF8String:starfox_metal_native_material_shared]];
        source=[source stringByAppendingString:[NSString stringWithUTF8String:kIndexedShader]];
        source=[source stringByAppendingString:[NSString stringWithUTF8String:kReflectionShader]];
        MTLCompileOptions* options=[MTLCompileOptions new];
        if(@available(iOS 18.0, macOS 15.0, *)) options.mathMode=MTLMathModeSafe;
        else options.fastMathEnabled=NO;
        id<MTLLibrary> library=[metal newLibraryWithSource:source options:options error:&error];
        if(!library) throw std::runtime_error(metal_error(error));
        id<MTLFunction> function=[library newFunctionWithName:@"starfox_hardware_reflection"];
        reflection_pipeline=[metal newComputePipelineStateWithFunction:function error:&error];
        if(!reflection_pipeline) throw std::runtime_error(metal_error(error));
    }
    void wait_slot(InFlight& slot) {
        if(slot.fence) {
            if(!SDL_WaitForGPUFences(device,true,&slot.fence,1))
                throw std::runtime_error(SDL_GetError());
            SDL_ReleaseGPUFence(device,slot.fence);slot.fence=nullptr;
        }
        slot.cpu_vertices=nil;slot.cpu_materials=nil;
        slot.palette=nil;slot.texels=nil;
        slot.scratch=nil;slot.acceleration=nil;
    }
    void ensure_output(std::uint32_t pixels) {
        if(output && output_capacity>=pixels) return;
        SDL_GPUBufferCreateInfo info{};
        info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        info.size=pixels*4U;
        auto* next=SDL_CreateGPUBuffer(device,&info);
        if(!next) throw std::runtime_error(SDL_GetError());
        if(output) SDL_ReleaseGPUBuffer(device,output);
        output=next;output_capacity=pixels;
    }
    void ensure_reflection_output(std::uint32_t pixels) {
        if(reflection_buffer && reflection_capacity>=pixels) return;
        SDL_GPUBufferCreateInfo info{};
        info.usage=SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ|SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        info.size=pixels*4U;
        auto* next=SDL_CreateGPUBuffer(device,&info);
        if(!next) throw std::runtime_error(SDL_GetError());
        if(reflection_buffer) SDL_ReleaseGPUBuffer(device,reflection_buffer);
        reflection_buffer=next;reflection_capacity=pixels;
    }
};

MetalHardwareRt::MetalHardwareRt():impl_(std::make_unique<Impl>()) {}
MetalHardwareRt::~MetalHardwareRt()=default;
bool MetalHardwareRt::available(void* raw) const noexcept {
    auto* device=static_cast<SDL_GPUDevice*>(raw);
    if(!device || !SDL_GetGPUDeviceDriver(device)
        || std::strcmp(SDL_GetGPUDeviceDriver(device),"metal")!=0) return false;
    auto metal=(__bridge id<MTLDevice>)SDL_StarfoxMetalDevice(device);
    return hardware_ray_tracing(metal);
}
bool MetalHardwareRt::render_shadows(void* raw,const Scene& scene,
    const render::GpuScene::RayGeometryOutput* resident_geometry,
    Camera camera,Vec3 light,std::optional<ReceiverPlane> ground,bool ground_only) {
    impl_->shadow={};
    if(ground_only && !ground) return false;
    try {
        auto* device=static_cast<SDL_GPUDevice*>(raw);
        const auto pixels=std::uint64_t(camera.width)*camera.height;
        const auto length=std::sqrt(dot(light,light));
        if(!pixels || pixels>std::numeric_limits<std::uint32_t>::max()/4U || !std::isfinite(length)
            || length<1.e-10 || camera.focal_length<=0
            || camera.vertical_focal_length()<=0) return false;
        const bool resident=resident_geometry && resident_geometry->complete
            && resident_geometry->device==raw && resident_geometry->buffer
            && resident_geometry->vertex_count>=3
            && resident_geometry->vertex_count%3==0;
        const auto* coverage=resident?resident_geometry->materials:nullptr;
        if(coverage && coverage->encoding!=RayMaterialEncoding::indexed
            && coverage->encoding!=RayMaterialEncoding::native_rgba) {
            impl_->status="Unknown Metal shadow material encoding";
            return false;
        }
        const bool native_rgba=coverage && coverage->encoding==RayMaterialEncoding::native_rgba;
        const bool indexed=coverage && (native_rgba || !coverage->texels.empty());
        const auto record_bytes=resident?std::uint64_t(resident_geometry->vertex_count/3U)*sizeof(render::RayMaterial):0U;
        if(native_rgba && (!resident_geometry->material_offset
            || resident_geometry->vertex_count/3U>0x03ffffffU
            || resident_geometry->material_bytes%4U || resident_geometry->material_bytes<record_bytes)) {
            impl_->status="Invalid native Metal shadow material payload";return false;
        }
        if(indexed && !native_rgba && ((!resident_geometry->material_offset
                && coverage->triangles.size()!=resident_geometry->vertex_count/3U)
            || coverage->texels.size()>16'000'000
            || resident_geometry->material_offset%16U)) {
            impl_->status="Invalid indexed Metal shadow material topology/atlas";return false;
        }
        impl_->initialize(device);
        auto& slot=impl_->inflight[impl_->serial++%impl_->inflight.size()];
        impl_->wait_slot(slot);
        id<MTLBuffer> vertices=nil;
        std::uint32_t vertex_count{};
        if(resident) {
            vertex_count=resident_geometry->vertex_count;
            vertices=(__bridge id<MTLBuffer>)SDL_StarfoxMetalBuffer(
                static_cast<SDL_GPUBuffer*>(resident_geometry->buffer));
        } else {
            if(scene.triangle_count()>std::numeric_limits<std::uint32_t>::max()/3U) return false;
            std::vector<Float4> packed;
            packed.reserve(scene.triangle_count()*3U);
            for(const auto& t:scene.triangles()) {
                packed.push_back(as_float4(t.a));
                packed.push_back(as_float4(t.b));
                packed.push_back(as_float4(t.c));
            }
            vertex_count=std::uint32_t(packed.size());
            if(!vertex_count) return false;
            slot.cpu_vertices=[impl_->metal newBufferWithBytes:packed.data()
                length:packed.size()*sizeof(Float4) options:MTLResourceStorageModeShared];
            vertices=slot.cpu_vertices;
        }
        if(!vertices || std::uint64_t(vertex_count)*sizeof(Float4)>vertices.length) return false;
        id<MTLBuffer> materials=nil;
        NSUInteger material_offset=0;
        std::uint32_t native_material_words=0;
        if(indexed) {
            material_offset=resident_geometry->material_offset;
            const auto material_bytes=native_rgba?std::uint64_t(resident_geometry->material_bytes)
                :std::uint64_t(vertex_count/3U)*sizeof(render::RayMaterial);
            if(material_offset) {
                if(material_offset<std::uint64_t(vertex_count)*sizeof(Float4)
                    || material_offset%16U
                    || std::uint64_t(material_offset)+material_bytes>vertices.length) {
                    impl_->status="Resident Metal shadow records/atlas exceed their buffer";return false;
                }
                materials=vertices;
                if(native_rgba) {
                    const auto words=std::uint64_t(vertices.length-material_offset)/4U;
                    if(words>std::numeric_limits<std::uint32_t>::max()) {
                        impl_->status="Native Metal shadow binding word count overflow";return false;
                    }
                    native_material_words=std::uint32_t(words);
                }
            } else {
                slot.cpu_materials=[impl_->metal newBufferWithBytes:coverage->triangles.data()
                    length:NSUInteger(material_bytes) options:MTLResourceStorageModeShared];
                materials=slot.cpu_materials;
            }
            if(!native_rgba) {
                slot.texels=[impl_->metal newBufferWithBytes:coverage->texels.data()
                    length:coverage->texels.size() options:MTLResourceStorageModeShared];
            }
            if(!materials || (!native_rgba && !slot.texels))
                throw std::runtime_error("Metal shadow cutout allocation failed");
        }
        impl_->ensure_output(std::uint32_t(pixels));
        auto* command=SDL_AcquireGPUCommandBuffer(device);
        if(!command) throw std::runtime_error(SDL_GetError());
        bool submitted=false;
        try {
            if(resident && !SDL_StarfoxMetalTrackBuffer(command,
                static_cast<SDL_GPUBuffer*>(resident_geometry->buffer)))
                throw std::runtime_error("Could not retain Metal ray geometry");
            if(!SDL_StarfoxMetalPrepareWrite(device,command,impl_->output))
                throw std::runtime_error("Could not cycle Metal ray output");
            auto target=(__bridge id<MTLBuffer>)SDL_StarfoxMetalBuffer(impl_->output);
            auto native=(__bridge id<MTLCommandBuffer>)SDL_StarfoxMetalCommandBuffer(command);
            if(!target || !native) throw std::runtime_error("Missing native Metal command resource");
            auto* triangles=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
            triangles.vertexBuffer=vertices;
            triangles.vertexStride=sizeof(Float4);
            triangles.vertexFormat=MTLAttributeFormatFloat3;
            triangles.triangleCount=vertex_count/3U;
            triangles.opaque=indexed?NO:YES;
            auto* descriptor=[MTLPrimitiveAccelerationStructureDescriptor descriptor];
            descriptor.geometryDescriptors=@[triangles];
            auto sizes=[impl_->metal accelerationStructureSizesWithDescriptor:descriptor];
            slot.acceleration=[impl_->metal newAccelerationStructureWithSize:sizes.accelerationStructureSize];
            slot.scratch=[impl_->metal newBufferWithLength:sizes.buildScratchBufferSize
                options:MTLResourceStorageModePrivate];
            if(!slot.acceleration || !slot.scratch)
                throw std::runtime_error("Metal acceleration-structure allocation failed");
            auto builder=[native accelerationStructureCommandEncoder];
            [builder buildAccelerationStructure:slot.acceleration descriptor:descriptor
                scratchBuffer:slot.scratch scratchBufferOffset:0];
            [builder endEncoding];
            Parameters p{};
            p.ground_only=ground_only?1.0F:0.0F;
            p.width=camera.width;p.height=camera.height;
            p.focal_x=float(camera.focal_length);
            p.focal_y=float(camera.vertical_focal_length());
            p.center_x=float(camera.center_x);p.center_y=float(camera.center_y);
            p.has_ground=ground?1.0F:0.0F;
            if(ground) {
                p.ground_point=as_float4(ground->point);
                p.ground_normal=as_float4(ground->normal);
            }
            p.lights=light_samples(light,camera.shadow_samples(),camera.shadow_angular_radius());
            if(native_rgba) p.coverage={vertex_count/3U,resident_geometry->material_bytes,2,native_material_words};
            else if(indexed) p.coverage={vertex_count/3U,std::uint32_t(coverage->texels.size()),1,0};
            auto encoder=[native computeCommandEncoder];
            [encoder setComputePipelineState:impl_->shadow_pipeline];
            [encoder setAccelerationStructure:slot.acceleration atBufferIndex:0];
            [encoder setBuffer:target offset:0 atIndex:1];
            [encoder setBytes:&p length:sizeof(p) atIndex:2];
            // No auxiliary allocation/upload for opaque geometry. Bind the
            // existing vertex buffer as an unused argument on that path.
            [encoder setBuffer:indexed?materials:vertices offset:material_offset atIndex:3];
            // Native RGBA records include their atlas in the tracked resident
            // allocation. The legacy texel argument is unused for that mode.
            [encoder setBuffer:indexed && !native_rgba?slot.texels:vertices offset:0 atIndex:4];
            [encoder useResource:slot.acceleration usage:MTLResourceUsageRead];
            const auto grid=MTLSizeMake(NSUInteger(pixels),1,1);
            const auto group=MTLSizeMake(std::min<NSUInteger>(64,
                impl_->shadow_pipeline.maxTotalThreadsPerThreadgroup),1,1);
            [encoder dispatchThreads:grid threadsPerThreadgroup:group];
            [encoder endEncoding];
            slot.fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
            submitted=true;
            if(!slot.fence) throw std::runtime_error(SDL_GetError());
        } catch(...) {
            if(!submitted) SDL_CancelGPUCommandBuffer(command);
            throw;
        }
        impl_->shadow={device,impl_->output,camera.width,camera.height,0};
        impl_->status=resident
            ?"Metal hardware rays from resident GPU casters"
            :"Metal hardware rays from CPU fallback casters";
        return true;
    } catch(const std::exception& error) {
        impl_->status=error.what();
        return false;
    }
}
GpuShadowOutput MetalHardwareRt::shadow_output() const noexcept {return impl_->shadow;}
bool MetalHardwareRt::render_reflections(void* raw,
    const render::GpuScene::RayGeometryOutput& geometry,
    Camera camera,std::span<const std::uint32_t,256> palette,
    std::uint32_t environment,std::uint8_t quality,float roughness,
    std::uint32_t metallic,std::optional<ReceiverPlane> ground,const RayWater* water,const GpuBackgroundDraw* background,bool ground_only) {
    impl_->reflection={};
    if(ground_only && (!ground || !water)) return false;
    try {
        auto* device=static_cast<SDL_GPUDevice*>(raw);
        const auto pixels=std::uint64_t(camera.width)*camera.height;
        if(geometry.materials && geometry.materials->encoding!=RayMaterialEncoding::indexed) {
            impl_->status="Native calibrated RGBA reflection material ABI not supported by this Metal producer";
            return false;
        }
        if(!quality || quality>3 || !pixels || pixels>std::numeric_limits<std::uint32_t>::max()/4U
            || camera.focal_length<=0 || camera.vertical_focal_length()<=0
            || !std::isfinite(roughness) || roughness<0
            || !geometry.complete || geometry.device!=raw || !geometry.buffer
            || geometry.vertex_count<3 || geometry.vertex_count%3
            || !geometry.materials
            || geometry.materials->encoding!=RayMaterialEncoding::indexed
            || (!geometry.material_offset && geometry.materials->triangles.size()!=geometry.vertex_count/3U)
            || geometry.material_offset%16U
            || geometry.materials->texels.size()>16'000'000) return false;
        if(water) {
            if(!ground || water->material>3 || water->caustics>3
                || !std::isfinite(water->time) || !std::isfinite(water->brightness)
                || water->brightness<0 || water->brightness>1
                || !std::isfinite(water->reflection_strength)
                || water->reflection_strength<0 || water->reflection_strength>1) return false;
            for(float value:water->world_to_view) if(!std::isfinite(value)) return false;
            for(float value:water->camera_position) if(!std::isfinite(value)) return false;
        }
        impl_->initialize(device);
        impl_->ensure_reflection_pipeline();
        auto& slot=impl_->inflight[impl_->serial++%impl_->inflight.size()];
        impl_->wait_slot(slot);
        impl_->ensure_reflection_output(std::uint32_t(pixels));
        auto* command=SDL_AcquireGPUCommandBuffer(device);
        if(!command) throw std::runtime_error(SDL_GetError());
        bool submitted=false;
        try {
            constexpr unsigned backdrop_width=1664,backdrop_height=224,backdrop_origin=(backdrop_width-256)/2;
            SDL_GPUBuffer* panorama_buffer=nullptr;
            if(background && background->ppu && background->settings.layer==2 && (background->ppu->main_screen&2)) {
                auto settings=background->settings;
                settings.priority=TilePriorityPass::all;settings.horizontal_origin=int(backdrop_origin);
                settings.extend_horizontal=true;settings.logical_viewport={backdrop_width,backdrop_height};settings.raster_jitter={};
                const auto panorama=impl_->reflection_backdrop.enqueue(raw,command,*background->ppu,
                    backdrop_width,backdrop_height,1,settings);
                panorama_buffer=static_cast<SDL_GPUBuffer*>(panorama.pixels);
                if(!panorama_buffer || !SDL_StarfoxMetalTrackBuffer(command,panorama_buffer))
                    throw std::runtime_error("Could not prepare Metal reflection backdrop");
            }
            auto* source=static_cast<SDL_GPUBuffer*>(geometry.buffer);
            if(!SDL_StarfoxMetalTrackBuffer(command,source)
                || !SDL_StarfoxMetalPrepareWrite(device,command,impl_->reflection_buffer))
                throw std::runtime_error("Could not retain Metal reflection buffers");
            auto vertices=(__bridge id<MTLBuffer>)SDL_StarfoxMetalBuffer(source);
            auto target=(__bridge id<MTLBuffer>)SDL_StarfoxMetalBuffer(impl_->reflection_buffer);
            auto native=(__bridge id<MTLCommandBuffer>)SDL_StarfoxMetalCommandBuffer(command);
            if(!vertices || !target || !native)
                throw std::runtime_error("Missing native Metal reflection resource");
            const auto vertex_bytes=std::uint64_t(geometry.vertex_count)*sizeof(Float4);
            if(vertex_bytes>vertices.length || (geometry.material_offset
                && (geometry.material_offset<vertex_bytes || std::uint64_t(geometry.material_offset)
                    +std::uint64_t(geometry.vertex_count/3U)*sizeof(render::RayMaterial)>vertices.length)))
                throw std::runtime_error("Resident Metal reflection geometry/materials exceed their buffer");
            auto* triangles=[MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
            triangles.vertexBuffer=vertices;
            triangles.vertexStride=sizeof(Float4);
            triangles.vertexFormat=MTLAttributeFormatFloat3;
            triangles.triangleCount=geometry.vertex_count/3U;
            triangles.opaque=NO;
            auto* descriptor=[MTLPrimitiveAccelerationStructureDescriptor descriptor];
            descriptor.geometryDescriptors=@[triangles];
            const auto sizes=[impl_->metal accelerationStructureSizesWithDescriptor:descriptor];
            slot.acceleration=[impl_->metal newAccelerationStructureWithSize:sizes.accelerationStructureSize];
            slot.scratch=[impl_->metal newBufferWithLength:sizes.buildScratchBufferSize
                options:MTLResourceStorageModePrivate];
            slot.palette=[impl_->metal newBufferWithBytes:palette.data()
                length:palette.size_bytes() options:MTLResourceStorageModeShared];
            const std::uint8_t blank=0;
            const auto& texels=geometry.materials->texels;
            slot.texels=[impl_->metal newBufferWithBytes:texels.empty()?&blank:texels.data()
                length:std::max<std::size_t>(1,texels.size())
                options:MTLResourceStorageModeShared];
            id<MTLBuffer> materials=vertices;
            NSUInteger material_offset=geometry.material_offset;
            if(!material_offset) {
                const auto& cpu=geometry.materials->triangles;
                slot.cpu_materials=[impl_->metal newBufferWithBytes:cpu.data()
                    length:cpu.size()*sizeof(render::RayMaterial)
                    options:MTLResourceStorageModeShared];
                materials=slot.cpu_materials;
            }
            if(!slot.acceleration || !slot.scratch || !slot.palette || !slot.texels
                || !materials)
                throw std::runtime_error("Metal reflection resource allocation failed");
            auto builder=[native accelerationStructureCommandEncoder];
            [builder buildAccelerationStructure:slot.acceleration descriptor:descriptor
                scratchBuffer:slot.scratch scratchBufferOffset:0];
            [builder endEncoding];
            ReflectionParameters p{};
            p.width=camera.width;p.height=camera.height;p.quality=quality;
            p.metallic=metallic;p.focal_x=float(camera.focal_length);
            p.focal_y=float(camera.vertical_focal_length());
            p.center_x=float(camera.center_x);p.center_y=float(camera.center_y);
            p.roughness=roughness;p.environment=environment;
            p.texel_count=std::uint32_t(texels.size());
            p.has_ground=ground?1.0F:0.0F;
            if(panorama_buffer) p.backdrop={backdrop_width,backdrop_height,backdrop_origin,0};
            const auto* enhanced_environment=panorama_buffer?background->settings.reflection_environment:nullptr;
            const auto* enhanced_image=enhanced_environment && enhanced_environment->modes[2]
                && enhanced_environment->backdrop_projection[3]==0?enhanced_environment->backdrop:nullptr;
            if(enhanced_image) {
                if(!enhanced_image->width || !enhanced_image->height
                    || enhanced_image->width>8192 || enhanced_image->height>8192
                    || enhanced_image->pixels.size()!=std::size_t(enhanced_image->width)*enhanced_image->height)
                    throw std::runtime_error("Invalid enhanced Metal reflection sky");
                if(!slot.enhanced_backdrop || !slot.enhanced_upload.matches(*enhanced_image)) {
                    slot.enhanced_backdrop=[impl_->metal newBufferWithBytes:enhanced_image->pixels.data()
                        length:enhanced_image->pixels.size()*sizeof(std::uint32_t) options:MTLResourceStorageModeShared];
                    if(!slot.enhanced_backdrop) throw std::runtime_error("Enhanced Metal reflection sky allocation failed");
                    slot.enhanced_upload.remember(*enhanced_image);
                }
                p.enhanced_size={enhanced_image->width,enhanced_image->height,1,0};
                const auto vector=[](const std::array<float,4>& v){return Float4{v[0],v[1],v[2],v[3]};};
                p.enhanced_motion=vector(enhanced_environment->motion);
                p.enhanced_plane=vector(enhanced_environment->plane);
                p.enhanced_projection=vector(enhanced_environment->backdrop_projection);
                p.enhanced_palette=vector(enhanced_environment->backdrop_palette[0]);
                p.enhanced_keep0=vector(enhanced_environment->backdrop_keep[0]);
                p.enhanced_keep1=vector(enhanced_environment->backdrop_keep[1]);
            }
            if(ground) {
                p.ground_point=as_float4(ground->point);
                p.ground_normal=as_float4(ground->normal);
            }
            p.ground_point.w=ground_only?1.0F:0.0F;
            if(water) {
                p.water_settings={water->time,water->reflection_strength,water->brightness,
                    float(water->material+(water->mirror_models?16:0)+(water->caustics<<5))};
                const auto& r=water->world_to_view;
                p.water_row0={r[0],r[1],r[2],water->camera_position[0]};
                p.water_row1={r[3],r[4],r[5],water->camera_position[1]};
                p.water_row2={r[6],r[7],r[8],water->camera_position[2]};
            }
            auto encoder=[native computeCommandEncoder];
            [encoder setComputePipelineState:impl_->reflection_pipeline];
            [encoder setAccelerationStructure:slot.acceleration atBufferIndex:0];
            [encoder setBuffer:target offset:0 atIndex:1];
            [encoder setBytes:&p length:sizeof(p) atIndex:2];
            [encoder setBuffer:vertices offset:0 atIndex:3];
            [encoder setBuffer:materials offset:material_offset atIndex:4];
            [encoder setBuffer:slot.palette offset:0 atIndex:5];
            [encoder setBuffer:slot.texels offset:0 atIndex:6];
            id<MTLBuffer> panorama=panorama_buffer
                ?(__bridge id<MTLBuffer>)SDL_StarfoxMetalBuffer(panorama_buffer):slot.palette;
            [encoder setBuffer:panorama offset:0 atIndex:7];
            id<MTLBuffer> enhanced=enhanced_image?slot.enhanced_backdrop:slot.palette;
            [encoder setBuffer:enhanced offset:0 atIndex:8];
            [encoder useResource:slot.acceleration usage:MTLResourceUsageRead];
            const auto grid=MTLSizeMake(NSUInteger(pixels),1,1);
            const auto group=MTLSizeMake(std::min<NSUInteger>(64,
                impl_->reflection_pipeline.maxTotalThreadsPerThreadgroup),1,1);
            [encoder dispatchThreads:grid threadsPerThreadgroup:group];
            [encoder endEncoding];
            slot.fence=SDL_SubmitGPUCommandBufferAndAcquireFence(command);
            submitted=true;
            if(!slot.fence) throw std::runtime_error(SDL_GetError());
        } catch(...) {
            if(!submitted) SDL_CancelGPUCommandBuffer(command);
            throw;
        }
        impl_->reflection={device,impl_->reflection_buffer,camera.width,camera.height,
            camera.width*4U};
        impl_->status=quality==1?"Metal hardware reflections LOW (1 ray)"
            :quality==2?"Metal hardware reflections MEDIUM (2 rays)"
            :"Metal hardware reflections HIGH (4 rays)";
        return true;
    } catch(const std::exception& error) {
        impl_->status=error.what();
        return false;
    }
}
GpuReflectionOutput MetalHardwareRt::reflection_output() const noexcept {return impl_->reflection;}
const std::string& MetalHardwareRt::status() const noexcept {return impl_->status;}
void MetalHardwareRt::release_device() noexcept {impl_.reset(new Impl);}
} // namespace starfox::render::shadows
