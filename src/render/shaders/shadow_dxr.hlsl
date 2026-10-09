#include "../../../include/starfox/render/water_transmission.inc"
#include "../../../include/starfox/render/water_caustics.inc"
#include "liquid_optics.hlsli"
#if defined(STARFOX_DXR_STABLE_HITS)
// DXIL rejects a noinline struct-return function's hidden aggregate output.
// This experimental variant inlines the oracle; ordinary PSOs exclude it.
#define SF_NOINLINE
#include "geometry_fp64.hlsli"
#undef SF_NOINLINE
#endif
RaytracingAccelerationStructure scene : register(t0);
ByteAddressBuffer coverage : register(t1);
ByteAddressBuffer triangleVertices : register(t2);
ByteAddressBuffer residentMaterials : register(t3);
ByteAddressBuffer backdropPixels : register(t4);
RWByteAddressBuffer outputMask : register(u0);
cbuffer Settings : register(b0) {
    float4 camera; // width, height, focal length, center x
    float4 options; // center y, ground enabled
    float4 groundPoint;
    float4 groundNormal;
    float4 lights[16];
    float4 primaryRange; // near/far camera depths; z=explicit depth-plane mode.
};

bool ray_position_less(float3 a,float3 b) {
    return a.x!=b.x?a.x<b.x:a.y!=b.y?a.y<b.y:a.z<b.z;
}
float3 ray_face_normal(uint primitive) {
    uint stride=uint(groundNormal.w),vertex=primitive*3*stride;
    float3 a=asfloat(triangleVertices.Load3(vertex)),b=asfloat(triangleVertices.Load3(vertex+stride)),c=asfloat(triangleVertices.Load3(vertex+2*stride));
    if((uint(primaryRange.w)&16u)!=0) {
        // Identical two-sided faces can be selected in either winding by AS
        // traversal. Canonical subtraction order makes their physical normal
        // identical, without changing intersection, shape or precision.
        if(ray_position_less(b,a)) {float3 swap=a;a=b;b=swap;}
        if(ray_position_less(c,b)) {float3 swap=b;b=c;c=swap;}
        if(ray_position_less(b,a)) {float3 swap=a;a=b;b=swap;}
    }
    return normalize(cross(b-a,c-a));
}
#if defined(STARFOX_DXR_STABLE_HITS)
bool ray_same_triangle(uint left,uint right) {
    uint stride=uint(groundNormal.w),l=left*3*stride,r=right*3*stride;
    float3 a=asfloat(triangleVertices.Load3(l)),b=asfloat(triangleVertices.Load3(l+stride)),c=asfloat(triangleVertices.Load3(l+2*stride));
    float3 x=asfloat(triangleVertices.Load3(r)),y=asfloat(triangleVertices.Load3(r+stride)),z=asfloat(triangleVertices.Load3(r+2*stride));
    return (all(a==x)||all(a==y)||all(a==z))
        && (all(b==x)||all(b==y)||all(b==z))
        && (all(c==x)||all(c==y)||all(c==z));
}
struct RayPrecise3 {Sf64 axis[3];};
uint ray_add_words(uint al,uint ah,uint bl,uint bh,out uint high) {
    Sf64 r=sf_add(sf_make(al,ah),sf_make(bl,bh));high=r.hi;return r.lo;
}
uint ray_mul_words(uint al,uint ah,uint bl,uint bh,out uint high) {
    Sf64 r=sf_mul(sf_make(al,ah),sf_make(bl,bh));high=r.hi;return r.lo;
}
uint ray_div_words(uint al,uint ah,uint bl,uint bh,out uint high) {
    Sf64 r=sf_div(sf_make(al,ah),sf_make(bl,bh));high=r.hi;return r.lo;
}
Sf64 ray_add(Sf64 a,Sf64 b) {uint high;uint low=ray_add_words(a.lo,a.hi,b.lo,b.hi,high);return sf_make(low,high);}
Sf64 ray_sub(Sf64 a,Sf64 b) {return ray_add(a,sf_neg(b));}
Sf64 ray_mul(Sf64 a,Sf64 b) {uint high;uint low=ray_mul_words(a.lo,a.hi,b.lo,b.hi,high);return sf_make(low,high);}
Sf64 ray_div(Sf64 a,Sf64 b) {uint high;uint low=ray_div_words(a.lo,a.hi,b.lo,b.hi,high);return sf_make(low,high);}
RayPrecise3 ray_precise(float3 value) {
    RayPrecise3 r;
    [loop] for(uint axis=0;axis<3;++axis) r.axis[axis]=sf_from_float_bits(asuint(value[axis]));
    return r;
}
RayPrecise3 ray_precise_sub(RayPrecise3 a,RayPrecise3 b) {
    // Keep the binary64 operation and per-axis order, without cloning its
    // complete integer implementation at every component's source call site.
    RayPrecise3 r;
    [loop] for(uint axis=0;axis<3;++axis) r.axis[axis]=ray_sub(a.axis[axis],b.axis[axis]);
    return r;
}
RayPrecise3 ray_precise_cross(RayPrecise3 a,RayPrecise3 b) {
    RayPrecise3 r;
    [loop] for(uint axis=0;axis<3;++axis) {
        uint y=(axis+1)%3,z=(axis+2)%3;
        r.axis[axis]=ray_sub(ray_mul(a.axis[y],b.axis[z]),ray_mul(a.axis[z],b.axis[y]));
    }
    return r;
}
Sf64 ray_precise_dot(RayPrecise3 a,RayPrecise3 b) {
    Sf64 result=ray_mul(a.axis[0],b.axis[0]);
    [loop] for(uint axis=1;axis<3;++axis) result=ray_add(result,ray_mul(a.axis[axis],b.axis[axis]));
    return result;
}
Sf64 ray_precise_distance(uint primitive,RayDesc ray) {
    uint stride=uint(groundNormal.w),vertex=primitive*3*stride;
    RayPrecise3 a=ray_precise(asfloat(triangleVertices.Load3(vertex)));
    RayPrecise3 b=ray_precise(asfloat(triangleVertices.Load3(vertex+stride)));
    RayPrecise3 c=ray_precise(asfloat(triangleVertices.Load3(vertex+2*stride)));
    RayPrecise3 normal=ray_precise_cross(ray_precise_sub(b,a),ray_precise_sub(c,a));
    return ray_div(ray_precise_dot(normal,ray_precise_sub(a,ray_precise(ray.Origin))),
        ray_precise_dot(normal,ray_precise(ray.Direction)));
}
bool ray_precise_positive(Sf64 depth) {return sf_valid(depth) && !sf_zero(depth) && (depth.hi&0x80000000u)==0;}
#endif

bool indexed_covered(uint primitive, float2 bary) {
    uint record = 16 + primitive * 40;
    if (primitive >= coverage.Load(0)) return false;
    if (coverage.Load(record + 36) == 0) return true;
    float2 a = asfloat(coverage.Load2(record));
    float2 b = asfloat(coverage.Load2(record + 8));
    float2 c = asfloat(coverage.Load2(record + 16));
    float2 uv = a * (1 - bary.x - bary.y) + b * bary.x + c * bary.y;
    uint2 mask = coverage.Load2(record + 28);
    uint2 xy = uint2(int2(floor(uv))) & mask;
    uint index = coverage.Load(record + 24) + xy.y * (mask.x + 1) + xy.x;
    return (coverage.Load(coverage.Load(8) + index * 4) >> 24) != 0;
}

uint ray_material_word(uint at) {
    return (coverage.Load(12)&2u)?residentMaterials.Load(at):coverage.Load(16+at);
}
uint2 ray_material_pair(uint at) {return uint2(ray_material_word(at),ray_material_word(at+4));}
#include "calibrated_colour.hlsli"
#if defined(STARFOX_DXR_NATIVE_MATERIALS) && !defined(STARFOX_DXR_SHADOW_ONLY)
#define STARFOX_CALIBRATED_PALETTE
#include "../../vr/shaders/scene_colour.hlsli"
#endif
uint reflected_colour_sample(uint primitive,float2 bary,uint2 pixel,bool coverageOnly) {
    if(primitive>=coverage.Load(0)) return 0;
    uint record=primitive*64,index;
    uint kind=ray_material_word(record+60);
#if defined(STARFOX_DXR_NATIVE_MATERIALS)
    if(kind==2) {
        if((coverage.Load(12)&4U)==0) return 0;
        uint scale=ray_material_word(record+28);
        if(ray_material_word(record+24)!=0 || scale>4096) return 0;
        uint divisor=max(scale,1u);
        bool odd=scale!=0 && (((pixel.x/divisor)^(pixel.y/divisor))&1)!=0;
        uint colour=ray_material_word(record+(odd?36:32));
        return (colour>>24)==255?colour:0;
    }
    if(kind==3) {
        if((coverage.Load(12)&4U)==0 || ray_material_word(record+24)!=1) return 0;
        uint flags=ray_material_word(record+28),offset=ray_material_word(record+48);
        uint2 mask=ray_material_pair(record+52);
        uint bytes=coverage.Load(coverage.Load(4)+1152);
        uint records=coverage.Load(0)*64;
        bool indexed=(flags&536870912U)!=0;
        uint pixels=(mask.x+1)*(mask.y+1);
        uint words=indexed?256+(pixels+3)/4:pixels;
        if((flags&~(7U|536870912U))!=0 || (flags&1U)==0 || any(mask>4095) || any(mask&(mask+1))
            || (offset&3U)!=0 || offset<records || offset>bytes || words>(bytes-offset)/4) return 0;
        float2 a=asfloat(ray_material_pair(record)),b=asfloat(ray_material_pair(record+8)),c=asfloat(ray_material_pair(record+16));
        if(!all(isfinite(a)) || !all(isfinite(b)) || !all(isfinite(c))
            || any(abs(a)>65536) || any(abs(b)>65536) || any(abs(c)>65536)) return 0;
        float2 uv=a*(1-bary.x-bary.y)+b*bary.x+c*bary.y;
        if((flags&4U)!=0 && (any(uv<0) || any(uv>=float2(mask+1)))) return 0;
        uint2 xy=uint2(int2(floor(uv)))&mask;
        uint texel=xy.y*(mask.x+1)+xy.x;
        if(indexed) texel=(ray_material_word(offset+1024+(texel/4)*4)>>((texel&3U)*8))&255U;
        uint packed=ray_material_word(offset+texel*4);
        if((packed>>24)!=255) return 0; // Binary cutouts only, never opaque fractional alpha.
        // Ray-query candidates need binary opacity, not every selected colour
        // style. A literal true specializes those copies at shader compilation;
        // shade only the committed hit. This preserves exactly the same texel,
        // UV/cutout/extent validation without duplicating material colour work
        // in each reflected/transmitted/caustic-visibility ray query.
        if(coverageOnly) return packed;
#if defined(STARFOX_DXR_SHADOW_ONLY)
        // Coverage is binary. Colour styling cannot change this texture's
        // opacity, and must not pull every material/effect into a shadow PSO.
        return packed;
#else
        float4 colour=float4(packed&255U,(packed>>8)&255U,(packed>>16)&255U,255)/255.;
        if((flags&2U)!=0) colour.rgb=calibrated_decode_srgb(colour.rgb);
        uint4 effects=uint4(ray_material_pair(record+32),ray_material_pair(record+40));
        colour=scene_styled_colour(colour,effects);
        if((effects.z&1U)!=0) colour.rgb=calibrated_encode_srgb(colour.rgb);
        uint3 rgb=uint3(round(saturate(colour.rgb)*255.));
        return rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xff000000U;
#endif
    }
#endif
    if(kind!=0) return 0;
    uint textured=ray_material_word(record+24);
    if(textured>1) return 0;
    if(textured) {
        float2 a=asfloat(ray_material_pair(record)),b=asfloat(ray_material_pair(record+8)),c=asfloat(ray_material_pair(record+16));
        uint2 mask=ray_material_pair(record+52);uint offset=ray_material_word(record+48);
        uint count=coverage.Load(coverage.Load(4)+1068);
        if(any(mask>4095) || any(mask&(mask+1)) || offset>count || (mask.x+1)*(mask.y+1)>count-offset) return 0;
        if(!all(isfinite(a)) || !all(isfinite(b)) || !all(isfinite(c))
            || any(abs(a)>1e8) || any(abs(b)>1e8) || any(abs(c)>1e8)) return 0;
        uint2 xy=uint2(int2(floor(a*(1-bary.x-bary.y)+b*bary.x+c*bary.y)))&mask;
        uint at=offset+xy.y*(mask.x+1)+xy.x;
        index=coverage.Load(coverage.Load(8)+at*4);
        if(!index) return 0;
        index=(index+ray_material_word(record+40))&255;
    } else {
        bool odd=ray_material_word(record+28)!=0 && ((pixel.x+pixel.y)&1)!=0;
        index=ray_material_word(record+(odd?36:32));
    }
    if(index>255) return 0;
    return coverage.Load(coverage.Load(4)+index*4)|0xff000000u;
}
uint reflected_colour(uint primitive,float2 bary,uint2 pixel) {return reflected_colour_sample(primitive,bary,pixel,false);}
uint coverage_colour(uint primitive,float2 bary,uint2 pixel) {return reflected_colour_sample(primitive,bary,pixel,true);}
struct ReflectionHit {uint primitive;float distance;float2 bary;bool found;};
ReflectionHit reflection_hit(RayDesc ray,uint2 pixel) {
    ReflectionHit hit;hit.primitive=0;hit.distance=ray.TMax;hit.bary=0;hit.found=false;
    {
        RayQuery<RAY_FLAG_NONE> query;
        query.TraceRayInline(scene,RAY_FLAG_FORCE_NON_OPAQUE,255,ray);
        while(query.Proceed()) if(query.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE
            && coverage_colour(query.CandidatePrimitiveIndex(),query.CandidateTriangleBarycentrics(),pixel)!=0)
            query.CommitNonOpaqueTriangleHit();
        if(query.CommittedStatus()!=COMMITTED_TRIANGLE_HIT) return hit;
        hit.primitive=query.CommittedPrimitiveIndex();hit.distance=query.CommittedRayT();
        hit.bary=query.CommittedTriangleBarycentrics();hit.found=true;
    }
#if defined(STARFOX_DXR_STABLE_HITS)
    if((uint(primaryRange.w)&32u)!=0 && hit.distance>0 && isfinite(hit.distance)) {
        // A nearest-hit commit prunes equal-distance faces in traversal order.
        // Enumerate ONLY the one-ULP interval around that exact nearest depth,
        // without committing/pruning its candidates. Native intersection can
        // round distinct surfaces to the same float distance. Refine ONLY such
        // ties with portable binary64 plane depth, then use source order for
        // physically equal hits. Exact vertex copies take a cheap fast path.
        // The original returned distance and ray bounds stay unchanged.
        uint bits=asuint(hit.distance);
        RayDesc tied=ray;tied.TMin=max(ray.TMin,asfloat(bits-1));
        tied.TMax=min(ray.TMax,asfloat(bits+1));
        RayQuery<RAY_FLAG_NONE> ties;
        ties.TraceRayInline(scene,RAY_FLAG_FORCE_NON_OPAQUE,255,tied);
        Sf64 nearest=sf_invalid();bool nearestKnown=false;
        while(ties.Proceed()) if(ties.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE
            && ties.CandidateTriangleRayT()==hit.distance
            && ties.CandidatePrimitiveIndex()!=hit.primitive
            && coverage_colour(ties.CandidatePrimitiveIndex(),ties.CandidateTriangleBarycentrics(),pixel)!=0) {
            uint candidate=ties.CandidatePrimitiveIndex();bool choose=false;
            if(ray_same_triangle(hit.primitive,candidate)) choose=candidate>hit.primitive;
            else {
                if(!nearestKnown) {nearest=ray_precise_distance(hit.primitive,ray);nearestKnown=true;}
                Sf64 depth=ray_precise_distance(candidate,ray);
                if(ray_precise_positive(nearest) && ray_precise_positive(depth)
                    && sf_less(sf_from_float_bits(asuint(ray.TMin)),depth)
                    && sf_less(depth,sf_from_float_bits(asuint(ray.TMax)))) {
                    choose=sf_less(depth,nearest) || (depth.lo==nearest.lo && depth.hi==nearest.hi && candidate>hit.primitive);
                    if(choose) nearest=depth;
                }
            }
            if(choose) {hit.primitive=candidate;hit.bary=ties.CandidateTriangleBarycentrics();}
        }
    }
#endif
    return hit;
}
bool covered(uint primitive,float2 bary) {
#if defined(STARFOX_DXR_NATIVE_MATERIALS)
    return (coverage.Load(12)&4U)!=0?coverage_colour(primitive,bary,uint2(0,0))!=0:indexed_covered(primitive,bary);
#else
    return indexed_covered(primitive,bary);
#endif
}
float3 reflection_rgb(uint packed) {
    return float3(packed&255u,(packed>>8)&255u,(packed>>16)&255u)/255.;
}
float3 reflection_linear(uint packed) {
    float3 value=reflection_rgb(packed);
    uint flags=coverage.Load(12);
    if((flags&32U)!=0) return calibrated_decode_srgb(value);
    return (flags&16U)!=0?value:value*value;
}
uint reflection_pack_linear(float3 radiance) {
    uint flags=coverage.Load(12);
    float3 value=saturate(radiance);
    if((flags&32U)!=0) value=calibrated_encode_srgb(value);
    else if((flags&16U)==0) value=sqrt(value);
    uint3 rgb=uint3(saturate(value)*255+.5);
    return rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xff000000U;
}
float3 conductor_f0(uint source,uint metallic) {
    if(metallic==2) return float3(1,.766,.336);
    if(metallic==3) return float3(.955,.638,.538);
    return reflection_linear(source);
}
uint reflection_transport(uint packed,float3 throughput) {
    // Preserve the original one-bounce/flat byte values when no conductor
    // attenuation was applied; never introduce an unnecessary re-encoding.
    return all(throughput==1)?packed:reflection_pack_linear(reflection_linear(packed)*throughput);
}
void reflection_cube_coordinates(float3 direction,out uint face,out float2 uv) {
    float3 a=abs(direction);
    if(a.x>=a.y && a.x>=a.z) {
        face=direction.x>=0?0:1;uv=float2(direction.x>=0?-direction.z:direction.z,-direction.y)/a.x;
    } else if(a.y>=a.z) {
        face=direction.y>=0?2:3;uv=float2(direction.x,direction.y>=0?direction.z:-direction.z)/a.y;
    } else {
        face=direction.z>=0?4:5;uv=float2(direction.z>=0?direction.x:-direction.x,-direction.y)/a.z;
    }
}
float3 reflection_cube_tap(uint base,uint size,uint face,int2 pixel) {
    // Reproject out-of-face taps onto the neighbour, rather than stretching
    // the last texel. This also keeps rough reflection cones seam-free.
    if(any(pixel<0) || any(pixel>=int(size))) {
        float2 uv=(float2(pixel)+.5)/size*2-1;
        float3 direction;
        if(face==0) direction=float3(1,-uv.y,-uv.x);
        else if(face==1) direction=float3(-1,-uv.y,uv.x);
        else if(face==2) direction=float3(uv.x,1,uv.y);
        else if(face==3) direction=float3(uv.x,-1,-uv.y);
        else if(face==4) direction=float3(uv.x,-uv.y,1);
        else direction=float3(-uv.x,-uv.y,-1);
        reflection_cube_coordinates(direction,face,uv);
        pixel=int2(floor((uv*.5+.5)*size));
    }
    pixel=clamp(pixel,0,int(size)-1);
    uint at=base+(face*size*size+pixel.y*size+pixel.x)*4;
    float3 colour=reflection_rgb((coverage.Load(12)&8U)!=0?residentMaterials.Load(at):coverage.Load(at));
    return (coverage.Load(12)&32U)!=0?calibrated_decode_srgb(colour):colour;
}
uint reflection_background_byte(uint base,uint address) {
    address&=65535u;return (coverage.Load(base+64+(address&~3u))>>((address&3u)*8))&255u;
}
uint reflection_background_word(uint base,uint address) {
    return reflection_background_byte(base,address*2)|(reflection_background_byte(base,address*2+1)<<8);
}
#include "environment_material.hlsli"
uint backdropWord(uint address) {return backdropPixels.Load(address);}
#include "backdrop_sample.hlsli"
uint reflection_background_at(uint base,float sampleX,float sampleY) {
    uint environment=coverage.Load(base+60);
    if(environment!=0) {
        uint3 image=coverage.Load3(environment+1072);
        if(image.x!=0 && image.y!=0) {
            float4 motion=asfloat(coverage.Load4(environment+1040)),plane=asfloat(coverage.Load4(environment+1056));
            float4 projection=asfloat(coverage.Load4(environment+1088));
            float4 keep0=asfloat(coverage.Load4(environment+1104)),keep1=asfloat(coverage.Load4(environment+1120));
            if(backdropCovers(sampleX-128,sampleY,motion.x,plane.x,projection,keep0,keep1)) {
                uint4 modes=coverage.Load4(environment+1024);
                float2 uv=backdropMotion(backdropCoordinates(sampleX-128,sampleY,motion.x,plane.x,plane.z,projection,keep0,keep1),modes.w,motion.w);
                bool moonAtlas=projection.w>=6 && projection.w<=8;
                bool cloudLimb=projection.w==9 && uv.y>=320/512.f;
                float4 response=cloudLimb || (moonAtlas && uv.y>=2)?float4(0,0,0,1):
                    asfloat(coverage.Load4(environment+(projection.w!=0 && projection.w!=6 && projection.w!=8 && sampleY>=motion.x+plane.x*(sampleX-128)?1152:1136)));
                float3 sky=backdropStyle(backdropSample(uv.x,uv.y,image.x,image.y,image.z,projection.w),uv,modes.z,modes.w!=0?motion.w:0.f);
                if(cloudLimb) {
                    uint4 ramp[4];
                    for(uint i=0;i<4;++i) ramp[i]=coverage.Load4(environment+1168+i*16);
                    sky=backdropLimbColour(sky,ramp);
                }
                uint moonRamp=coverage.Load(environment+1192);
                if(moonAtlas && uv.y>=2 && (moonRamp==1 || moonRamp==2)) {
                    uint4 ramp[4];
                    for(uint i=0;i<4;++i) ramp[i]=coverage.Load4(environment+1168+i*16);
                    sky=backdropRampMoonColour(sky,uv,ramp);
                } else if(moonAtlas && uv.y>=2) sky=backdropMoonColour(backdropMoonSurface(sky,uv,keep1),uv,
                    coverage.Load(environment+1172),coverage.Load(environment+1176),coverage.Load(environment+1180),
                    coverage.Load(environment+1184),coverage.Load(environment+1188));
                if(coverage.Load(environment+1168)==2) {
                    uint4 ramp[4];
                    for(uint i=0;i<4;++i)ramp[i]=coverage.Load4(environment+1168+i*16);
                    sky=backdropNebulaColour(sky,ramp);
                } else if(coverage.Load(environment+1168)!=0) {
                    uint limits=coverage.Load(environment+1168);
                    float maximum=((limits>>16)&255)!=0?float((limits>>16)&255):245.f;
                    float minimum=((limits>>16)&255)!=0?float((limits>>8)&255):140.f;
                    float shade=1.f+14.f*(1.f-saturate((dot(sky,float3(.299,.587,.114))-minimum)/max(1.f,maximum-minimum)));
                    uint a=uint(shade),b=min(a+1,15u);
                    uint ca=coverage.Load(environment+1168+a*4),cb=coverage.Load(environment+1168+b*4);
                    sky=lerp(float3(ca&255,(ca>>8)&255,(ca>>16)&255),float3(cb&255,(cb>>8)&255,(cb>>16)&255),shade-float(a));
                }
                float3 colour=sky*response.w+255.f*response.rgb;
                float opacity=moonAtlas && projection.w!=8?backdropMoonOpacity(uv,keep1):1;
                if(opacity<1) {
                    float2 backgroundUV=backdropMotion(backdropCoordinates(sampleX-128,sampleY,motion.x,plane.x,plane.z,projection),modes.w,motion.w);
                    float3 behind=backdropStyle(backdropSample(backgroundUV.x,backgroundUV.y,image.x,image.y,image.z,6.f),backgroundUV,modes.z,modes.w!=0?motion.w:0.f);
                    float4 skyResponse=asfloat(coverage.Load4(environment+1136));
                    colour=lerp(behind*skyResponse.w+255.f*skyResponse.rgb,colour,opacity);
                }
                uint3 rgb=uint3(clamp(colour*plane.w,0.f,255.f)+.5);
                return rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xff000000u;
            }
        }
    }
    // Native pixel art stays point sampled. Photographic reflections above
    // retain subpixel ray coordinates rather than snapping to a 256-wide grid.
    int x=int(floor(sampleX)),y=int(floor(sampleY));
    uint4 layout=coverage.Load4(base);uint edge=coverage.Load(base+16),flags=coverage.Load(base+28);
    uint mapWidth=layout.z*edge,mapHeight=layout.w*edge;
    y=clamp(y,0,223);
    bool outside=x<0 || x>=256;
    // Tunnel art belongs to the forward corridor only. Other directions sample
    // its wall, not another copy of the opening.
    if((flags&16u) && outside) {x=0;y=112;}
    int sx=(flags&2u)?asint(coverage.Load(base+65600+y*4)):asint(coverage.Load(base+20));
    int sy=(flags&4u)?asint(coverage.Load(base+66496+y*4)):asint(coverage.Load(base+24));
    if(flags&8u) {
        if(flags&64u) {
            float2 fit=asfloat(coverage.Load2(base+48));
            float offset=fit.x+fit.y*float(x+(asint(coverage.Load(base+20))&7))/8;
            // Match round-away-from-zero used by the background's roll fit.
            sy=(offset<0?-int(floor(-offset+.5)):int(floor(offset+.5)))&8191;
        } else {
            int column=int(floor(float(x+(sx&7))/8));
            uint word=reflection_background_word(base,0x2fa0u+uint(clamp(column-1,0,31)));
            if(word&0x4000u) sy=int(word&8191u);
        }
    }
    int unwrapped=x+sx;bool outMap=unwrapped<0 || unwrapped>=int(mapWidth);
    uint black=coverage.Load(base+40),palette=coverage.Load(4);
    if((!(flags&1u) && outMap) || (outside && outMap && y<int(coverage.Load(base+36))))
        return coverage.Load(palette+black*4)|0xff000000u;
    uint sourceX=uint(unwrapped)&(mapWidth-1),sourceY=uint(y+sy)&(mapHeight-1);
    uint skySourceMin=coverage.Load(base+44);
    if(outside && y<144 && !(flags&16u) && (flags&8u) && skySourceMin<mapHeight)
        sourceY=max(sourceY,skySourceMin);
    uint tx=sourceX/edge,ty=sourceY/edge;
    uint tile=reflection_background_word(base,layout.x+((ty>>5)*(layout.z>>5)+(tx>>5))*1024+(ty&31)*32+(tx&31));
    uint px=sourceX&(edge-1),py=sourceY&(edge-1);
    if(tile&16384u) px=edge-1-px;if(tile&32768u) py=edge-1-py;
    uint character=((tile&1023u)+(px>>3)+(py>>3)*16)&1023u;
    uint inkAt=layout.y*2+character*32+(py&7)*2,mask=128u>>(px&7),ink=0;
    if(reflection_background_byte(base,inkAt)&mask) ink|=1;
    if(reflection_background_byte(base,inkAt+1)&mask) ink|=2;
    if(reflection_background_byte(base,inkAt+16)&mask) ink|=4;
    if(reflection_background_byte(base,inkAt+17)&mask) ink|=8;
    // Empty ink reveals the scene backdrop, rather than inventing a black
    // surface. Transparency uses source CGRAM, before display palette effects.
    if(!ink) return asuint(groundPoint.w)|0xff000000u;
    uint colour=((tile>>10)&7u)*16+ink;
    int uniqueX=x+int(uint(sx+int(mapWidth/2))&(mapWidth-1))-int(mapWidth/2);
    if(outside) for(uint r=0;r<coverage.Load(base+32);++r) {
        uint at=base+67392+r*32;int4 bounds=asint(coverage.Load4(at));uint3 colours=coverage.Load3(at+16);
        int replacement=asint(coverage.Load(at+28));bool suppressAll=replacement>=0 && (replacement&0x40000000)!=0;
        if(suppressAll) replacement=replacement&~0x40000000;
        if((suppressAll || uniqueX<0 || uniqueX>=int(mapWidth))
            && int(sourceX)>=bounds.x && int(sourceX)<bounds.z && int(sourceY)>=bounds.y && int(sourceY)<bounds.w
            && colour>=colours.x && colour<=colours.y) {
            colour=colours.z;
            if(replacement!=0) {
                uint rx=uint(int(sourceX)+replacement)&(mapWidth-1);
                uint rtx=rx/edge;
                uint rt=reflection_background_word(base,layout.x+((ty>>5)*(layout.z>>5)+(rtx>>5))*1024+(ty&31)*32+(rtx&31));
                uint rpx=rx&(edge-1),rpy=sourceY&(edge-1);
                if(rt&16384u) rpx=edge-1-rpx;if(rt&32768u) rpy=edge-1-rpy;
                uint rc=((rt&1023u)+(rpx>>3)+(rpy>>3)*16)&1023u;
                uint ra=layout.y*2+rc*32+(rpy&7)*2,rm=128u>>(rpx&7),ri=0;
                if(reflection_background_byte(base,ra)&rm) ri|=1;
                if(reflection_background_byte(base,ra+1)&rm) ri|=2;
                if(reflection_background_byte(base,ra+16)&rm) ri|=4;
                if(reflection_background_byte(base,ra+17)&rm) ri|=8;
                if(ri) colour=((rt>>10)&7u)*16u+ri;
            }
            break;
        }
    }
    if((flags&32u) && !(coverage.Load(coverage.Load(base+56)+colour*4)&32767u))
        return asuint(groundPoint.w)|0xff000000u;
    uint rgba=coverage.Load(palette+colour*4);
    uint enhanced=coverage.Load(base+60);
    if(enhanced!=0) {
        uint kind=coverage.Load(enhanced+colour*4);
        if(kind!=0) {
            float3 rgb=float3(rgba&255u,(rgba>>8)&255u,(rgba>>16)&255u);
            uint3 shaded=uint3(environmentColour(rgb,kind,float(x)-128,float(y),
                coverage.Load4(enhanced+1024),asfloat(coverage.Load4(enhanced+1040)),
                asfloat(coverage.Load(enhanced+1056)),asfloat(coverage.Load(enhanced+1068)))+.5);
            rgba=shaded.x|(shaded.y<<8)|(shaded.z<<16);
        }
    }
    return rgba|0xff000000u;
}
uint reflection_background(uint base,float3 direction) {
    // The authored image covers the original camera's angular field, not an
    // entire hemisphere. Continue its surroundings without duplicating unique art.
    return reflection_background_at(base,128+atan2(direction.x,direction.z)*256,
        112+atan2(direction.y,length(direction.xz))*256);
}
uint reflected_environment(float3 direction) {
    uint settings=coverage.Load(4)+1024,size=coverage.Load(settings+8);
    uint background=coverage.Load(settings+28);
    if(background) return reflection_background(background,direction);
    if(size==0) return asuint(groundPoint.w)|0xff000000u;
    direction=float3(dot(direction,asfloat(coverage.Load3(settings+16))),
        dot(direction,asfloat(coverage.Load3(settings+32))),dot(direction,asfloat(coverage.Load3(settings+48))));
    uint face;float2 uv;reflection_cube_coordinates(direction,face,uv);
    float2 at=(uv*.5+.5)*size-.5;
    int2 lo=int2(floor(at));float2 f=frac(at);
    uint base=coverage.Load(settings+12);
    float3 c00=reflection_cube_tap(base,size,face,lo);
    float3 c10=reflection_cube_tap(base,size,face,lo+int2(1,0));
    float3 c01=reflection_cube_tap(base,size,face,lo+int2(0,1));
    float3 c11=reflection_cube_tap(base,size,face,lo+int2(1,1));
    float3 colour;
    if((coverage.Load(12)&16U)!=0) {
        colour=lerp(lerp(c00,c10,f.x),lerp(c01,c11,f.x),f.y);
        if((coverage.Load(12)&32U)!=0) colour=calibrated_encode_srgb(colour);
    } else colour=sqrt(lerp(lerp(c00*c00,c10*c10,f.x),lerp(c01*c01,c11*c11,f.x),f.y));
    uint3 rgb=uint3(saturate(colour)*255+.5);
    return rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xff000000u;
}
bool caustic_blocked(float3 origin,float3 direction,float maximum,float bias,uint2 id) {
    RayDesc ray;ray.Origin=origin;ray.Direction=direction;ray.TMin=bias;ray.TMax=maximum;
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;
    query.TraceRayInline(scene,RAY_FLAG_FORCE_NON_OPAQUE,255,ray);
    while(query.Proceed()) if(query.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE
        && coverage_colour(query.CandidatePrimitiveIndex(),query.CandidateTriangleBarycentrics(),id)!=0)
        query.CommitNonOpaqueTriangleHit();
    return query.CommittedStatus()==COMMITTED_TRIANGLE_HIT;
}
#if defined(STARFOX_DXR_NATIVE_WATER)
struct NativeWaterSample {
    float3 hit,normal,radiance,specular;
    float fresnel,bias;
};
NativeWaterSample native_water_sample(float3 origin,float3 direction,float distance,uint2 id) {
    uint data=coverage.Load(4)+1088;
    float4 settings=asfloat(coverage.Load4(data));
    uint caustics=(uint(settings.w)>>5)&3u;
    float4 r0=asfloat(coverage.Load4(data+16)),r1=asfloat(coverage.Load4(data+32)),r2=asfloat(coverage.Load4(data+48));
    float3x3 rotation=float3x3(r0.xyz,r1.xyz,r2.xyz);
    // The retained matrix is the inverse transpose of the affine model/eye
    // transform: row-vector positions and column-vector normals use it.
    // Directions and world->view points need its inverse transpose instead.
    // Treating these as interchangeable assumes exactly unit Q15 rotations.
    float3 cofactor0=cross(rotation[1],rotation[2]);
    float3x3 forward=float3x3(cofactor0,cross(rotation[2],rotation[0]),cross(rotation[0],rotation[1]))
        /dot(rotation[0],cofactor0);
    float3 offset=float3(r0.w,r1.w,r2.w);
    NativeWaterSample result;
    result.hit=origin+direction*distance;
    float footprint=length(result.hit)/max(camera.z,1.f)/max(abs(dot(direction,groundNormal.xyz)),.04f);
    float t=settings.x;
    LiquidOpticalSample optical=liquid_optical_sample(result.hit,direction,distance,footprint,rotation,offset,t,0);
    float3 position=optical.position;result.hit=optical.hit;result.normal=optical.normal;
    bool entering=optical.entering;
    result.bias=max(.05,distance*1e-5);
    float3 light=normalize(mul(forward,float3(-1,-1,-1)));
    float visibility=caustic_blocked(result.hit+result.normal*result.bias,light,65536,result.bias,id)?0:1;
    // Live authored source colour, converted exactly once to the negotiated
    // native linear domain. Never reinterpret a linear target as gamma-squared.
    float3 authored=asfloat(coverage.Load3(coverage.Load(4)+1156));
    if((coverage.Load(12)&32U)!=0) authored=calibrated_decode_srgb(authored);
    float3 base=dot(authored,float3(.3,.59,.11))*float3(.20,.58,.85);
    result.radiance=base*(.65+.35*visibility*max(0,dot(result.normal,light)));
    float3 transmitted=refract(direction,result.normal,entering?.75:1./.75);
    bool totalInternal=dot(transmitted,transmitted)<1.e-10;
    if(!totalInternal) {
        RayDesc through;through.Origin=result.hit-result.normal*result.bias;through.Direction=transmitted;
        through.TMin=result.bias;through.TMax=65536;
        float3 worldOrigin=mul(through.Origin,rotation)+offset,worldDirection=mul(transmitted,rotation);
        float bottomTravel=worldDirection.y>0?(position.y+640-worldOrigin.y)/worldDirection.y:65536;
        through.TMax=min(through.TMax,max(result.bias,bottomTravel));
        ReflectionHit submerged=reflection_hit(through,id);
        bool found=submerged.found;
        float travel=found?submerged.distance:bottomTravel;
        if(found || (bottomTravel>result.bias && bottomTravel<65536)) {
            float3 receiver=found?reflection_linear(reflected_colour(submerged.primitive,submerged.bary,id))
                :float3(.28,.24,.16)*settings.z;
            float3 receiverView=through.Origin+transmitted*travel;
            float3 receiverWorld=mul(receiverView,rotation)+offset;
            float depth=receiverWorld.y-position.y,up=1;
            if(found) {
                float3 receiverNormal=ray_face_normal(submerged.primitive);
                if(dot(receiverNormal,transmitted)>0) receiverNormal=-receiverNormal;
                up=saturate(-normalize(mul(receiverNormal,forward)).y);
            }
            if(caustics && depth>0 && up>0) {
                WaterCausticSample focus=water_caustic_sample(receiverWorld.x,receiverWorld.z,t,depth,footprint);
                float3 entry=mul(forward,float3(focus.entry_x,position.y,focus.entry_z)-offset);
                float3 segment=entry-receiverView;float path=length(segment);
                if(path>result.bias*2
                    && !caustic_blocked(receiverView,segment/path,path-result.bias,result.bias,id)
                    && !caustic_blocked(entry,normalize(mul(forward,float3(0,-1,0))),65536,result.bias,id))
                    receiver*=clamp(1+(focus.irradiance-exp(-depth/1600))*up*float(caustics)/3,.25,3);
            }
            result.radiance=float3(water_transmitted_channel(receiver.r,result.radiance.r,travel,.0025),
                water_transmitted_channel(receiver.g,result.radiance.g,travel,.0008),
                water_transmitted_channel(receiver.b,result.radiance.b,travel,.00035));
        }
    }
    float grazing=pow(1-saturate(dot(-direction,result.normal)),5);
    result.fresnel=totalInternal?1:saturate((.02+.98*grazing)*settings.y);
    float3 halfway=normalize(light-direction);
    result.specular=float3(1,.95,.82)*pow(saturate(dot(result.normal,halfway)),96)*visibility
        *max(authored.r,max(authored.g,authored.b))*.55;
    return result;
}
#endif
// Generic sharp/flat and model-lobe history PSOs reject curved liquids; native
// water PSOs accept only material0. Keep lava registers out of those paths.
#if (defined(STARFOX_DXR_CURVED_PATH_HISTORY) || !defined(STARFOX_DXR_MODEL_LOBE_HISTORY)) && !defined(STARFOX_DXR_NATIVE_WATER) && (!defined(STARFOX_DXR_REFLECTION_HISTORY) || defined(STARFOX_DXR_LIQUID_HISTORY) || defined(STARFOX_DXR_CURVED_PATH_HISTORY))
#define STARFOX_DXR_SECONDARY_LAVA 1
#endif
#if defined(STARFOX_DXR_SECONDARY_LAVA)
struct SecondaryLavaSample {
    float3 hit,normal,radiance;
    float share,bias;
};
SecondaryLavaSample secondary_lava_sample(float3 origin,float3 direction,float distance) {
    uint data=coverage.Load(4)+1088;
    float4 settings=asfloat(coverage.Load4(data));
    float4 r0=asfloat(coverage.Load4(data+16)),r1=asfloat(coverage.Load4(data+32)),r2=asfloat(coverage.Load4(data+48));
    float3x3 rotation=float3x3(r0.xyz,r1.xyz,r2.xyz);
    float3 hit=origin+direction*distance;
    // Secondary footprints remain eye-to-surface, not last-mirror-to-surface.
    float footprint=length(hit)/max(camera.z,1.f)/max(abs(dot(direction,groundNormal.xyz)),.04f);
    LiquidOpticalSample optical=liquid_optical_sample(hit,direction,distance,footprint,
        rotation,float3(r0.w,r1.w,r2.w),settings.x,3);
    float3 viewer=mul(-direction,rotation);
    LavaColour colour=lava_shade(optical.lava,viewer.x,viewer.y,viewer.z);
    SecondaryLavaSample result;
    result.hit=optical.hit;result.normal=optical.normal;result.bias=max(.05,distance*1e-5);
    result.radiance=float3(colour.r,colour.g,colour.b)*settings.z;
    if((coverage.Load(12)&32U)!=0) result.radiance=calibrated_decode_srgb(result.radiance);
    else if((coverage.Load(12)&16U)==0) result.radiance*=result.radiance;
    result.share=(.035+.40*pow(1-saturate(dot(-direction,result.normal)),5))*settings.y*(.3+.7*optical.lava.crust);
    return result;
}
#endif
uint reflection_accumulate(uint packed,float3 throughput,float3 accumulated) {
    return all(accumulated==0)?reflection_transport(packed,throughput)
        :reflection_pack_linear(accumulated+reflection_linear(packed)*throughput);
}
#if defined(STARFOX_DXR_MODEL_PATH_HISTORY)
#include "reflection_path_record.hlsli"
#endif
uint trace_reflected_ray(RayDesc ray,uint2 id,out ReflectionHit terminal,bool escapingSourceGround
#if defined(STARFOX_DXR_MODEL_PATH_HISTORY)
    ,out ReflectionPathRecord path
#endif
) {
    terminal.primitive=0;terminal.distance=0;terminal.bary=0;terminal.found=false;
#if defined(STARFOX_DXR_MODEL_PATH_HISTORY)
    path=reflection_path_empty();
#endif
    uint waterFlags=uint(asfloat(coverage.Load(coverage.Load(4)+1100)));
    bool modelTransport=(coverage.Load(12)&64U)!=0;
    uint metallic=modelTransport?coverage.Load(coverage.Load(4)+1028):0;
    float3 throughput=1;
    float3 accumulated=0;
    // Bounded specular transport: never resolve a mirror hit to the original
    // palette. At the budget limit use the enhanced environment, not stale art.
    [loop] for(uint bounce=0;bounce<4;++bounce) {
    float groundDistance=ray.TMax;bool groundHit=false;
    // Lava's displaced origin can lie below its analytic reference plane.
    // An outward first ray leaves the REAL wave; the undisplaced plane is not
    // another opaque surface in front of it. Model rays and later bounces keep
    // their ordinary ground intersection, including inward liquid rays.
    if(options.y!=0 && !(escapingSourceGround && bounce==0 && dot(ray.Direction,groundNormal.xyz)>0)) {
        float denominator=dot(ray.Direction,groundNormal.xyz);
        if(abs(denominator)>1e-8) {
            float t=dot(groundPoint.xyz-ray.Origin,groundNormal.xyz)/denominator;
            if(t>ray.TMin && t<ray.TMax) {groundDistance=t;groundHit=true;ray.TMax=t;}
        }
    }
    ReflectionHit secondary=reflection_hit(ray,id);
    if(secondary.found) {
        uint primitive=secondary.primitive;
        uint source=reflected_colour(primitive,secondary.bary,id);
        if(!modelTransport && (waterFlags&16u)==0) {
            if(bounce==0) terminal=secondary;
#if defined(STARFOX_DXR_MODEL_PATH_HISTORY)
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
            path.base=accumulated;
#endif
            reflection_path_terminal(path,1,bounce,primitive,float3(secondary.bary,0),source,throughput);
#endif
            return reflection_accumulate(source,throughput,accumulated);
        }
#if defined(STARFOX_DXR_MODEL_PATH_HISTORY)
        path.mirrors[bounce]=primitive;
#endif
        float3 normal=ray_face_normal(primitive);
        if(dot(normal,ray.Direction)>0) normal=-normal;
        if(metallic) {
            float3 f0=conductor_f0(source,metallic);
            float grazing=pow(1-saturate(dot(-ray.Direction,normal)),5);
            throughput*=f0+(1-f0)*grazing;
        }
        float distance=secondary.distance,bias=max(.05,distance*1e-5);
        ray.Origin+=ray.Direction*distance+normal*bias;
        ray.Direction=reflect(ray.Direction,normal);ray.TMin=bias;ray.TMax=65536;
        continue;
    }
    if(groundHit) {
        float3 hitPosition=ray.Origin+ray.Direction*groundDistance;
#if defined(STARFOX_DXR_NATIVE_WATER)
        if((coverage.Load(12)&128U)!=0 && (waterFlags&15u)==0) {
            // Resolve the SAME native liquid on secondary model rays. Its
            // refracted radiance is accumulated; the specular ray continues in
            // this bounded loop, rather than returning the original flat floor.
            NativeWaterSample water=native_water_sample(ray.Origin,ray.Direction,groundDistance,id);
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
            path.mirrors[bounce]=0xfffffffdU;
            path.control|=1U<<(bounce+4);
#endif
            accumulated+=throughput*(water.radiance*(1-water.fresnel)+water.specular);
            throughput*=water.fresnel;
            if(all(throughput<1.e-5)) {
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
                path.base=accumulated;
                reflection_path_terminal(path,4,bounce+1,0xffffffffU,0,0,throughput);
#endif
                return reflection_pack_linear(accumulated);
            }
            ray.Origin=water.hit+water.normal*water.bias;ray.Direction=reflect(ray.Direction,water.normal);
            ray.TMin=water.bias;ray.TMax=65536;
            continue;
        }
#endif
#if defined(STARFOX_DXR_SECONDARY_LAVA)
        if((waterFlags&15u)==3u) {
            // Lava is self-emissive, with the SAME continuous displaced waves
            // and current time/material as the primary surface. Do not return
            // authored flat ground or gate this on the water-only colour flag.
            SecondaryLavaSample lava=secondary_lava_sample(ray.Origin,ray.Direction,groundDistance);
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
            path.mirrors[bounce]=0xfffffffdU;
            path.control|=1U<<(bounce+4);
#endif
            accumulated+=throughput*lava.radiance;
            throughput*=lava.share;
            if(all(throughput<1.e-5)) {
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
                path.base=accumulated;
                reflection_path_terminal(path,4,bounce+1,0xffffffffU,0,0,throughput);
#endif
                return reflection_pack_linear(accumulated);
            }
            ray.Origin=lava.hit+lava.normal*lava.bias;ray.Direction=reflect(ray.Direction,lava.normal);
            ray.TMin=lava.bias;ray.TMax=65536;
            continue;
        }
#endif
        if((waterFlags&15u)==1u || (waterFlags&15u)==2u) {
#if defined(STARFOX_DXR_SCENE_PATH_HISTORY)
            path.mirrors[bounce]=0xfffffffeU; // Actual analytic mirror/gold receiver.
#endif
            float3 normal=normalize(groundNormal.xyz);
            if(dot(normal,ray.Direction)>0) normal=-normal;
            if((waterFlags&15u)==2u) {
                float grazing=pow(1-saturate(dot(-ray.Direction,normal)),5);
                throughput*=float3(1,.766,.336)+(1-float3(1,.766,.336))*grazing;
            }
            float bias=max(.05,groundDistance*1e-5);
            ray.Origin=hitPosition+normal*bias;
            ray.Direction=reflect(ray.Direction,normal);ray.TMin=bias;ray.TMax=65536;
            continue;
        }
        hitPosition.x+=asfloat(coverage.Load(coverage.Load(4)+1084));
        uint background=coverage.Load(coverage.Load(4)+1052);
        // A physical floor remains opaque even without a tiled material.
        // Zero is the missing-background sentinel, not a tile-data address.
        if(background==0) return reflection_accumulate(asuint(groundPoint.w)|0xff000000u,throughput,accumulated);
        // Project the finite hit onto the authored ground, not the secondary
        // ray's direction. Nearby receivers now show positional parallax.
        if(hitPosition.z>1) return reflection_accumulate(reflection_background_at(background,
            128+256*hitPosition.x/hitPosition.z,112+256*hitPosition.y/hitPosition.z),throughput,accumulated);
        return reflection_accumulate(reflection_background(background,hitPosition),throughput,accumulated);
    }
    uint environment=reflected_environment(ray.Direction);
#if defined(STARFOX_DXR_MODEL_PATH_HISTORY)
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    path.base=accumulated;
#endif
    reflection_path_terminal(path,2,bounce,0xffffffffU,ray.Direction,environment,throughput);
#endif
    return reflection_accumulate(environment,throughput,accumulated);
    }
    uint environment=reflected_environment(ray.Direction);
#if defined(STARFOX_DXR_MODEL_PATH_HISTORY)
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    path.base=accumulated;
#endif
    reflection_path_terminal(path,3,4,0xffffffffU,ray.Direction,environment,throughput);
#endif
    return reflection_accumulate(environment,throughput,accumulated);
}
#if defined(STARFOX_DXR_MODEL_PATH_HISTORY)
uint trace_reflected_ray(RayDesc ray,uint2 id,out ReflectionHit terminal,bool escapingSourceGround) {
    ReflectionPathRecord unused;return trace_reflected_ray(ray,id,terminal,escapingSourceGround,unused);
}
#endif
uint trace_reflected_ray(RayDesc ray,uint2 id,out ReflectionHit terminal) {
    return trace_reflected_ray(ray,id,terminal,false);
}
uint trace_reflected_ray(RayDesc ray,uint2 id) {
    ReflectionHit unused;return trace_reflected_ray(ray,id,unused);
}
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
#include "reflection_hit_motion.hlsli"
#if defined(STARFOX_DXR_MODEL_LOBE_HISTORY)
#include "reflection_rough_hit_motion.hlsli"
#include "reflection_lobe_record.hlsli"
#endif
float4 reflected_hit_motion(uint receiver,ReflectionHit secondary) {
    if((coverage.Load(12)&1024U)==0 || !secondary.found) return 0;
    uint settings=coverage.Load(4)+1168;
    uint4 layout=coverage.Load4(settings);
    uint primary=layout.x+receiver*48,hit=layout.x+secondary.primitive*48;
    return reflection_hit_motion(asfloat(triangleVertices.Load4(primary)),asfloat(triangleVertices.Load4(primary+16)),
        asfloat(triangleVertices.Load4(primary+32)),asfloat(triangleVertices.Load4(hit)),asfloat(triangleVertices.Load4(hit+16)),
        asfloat(triangleVertices.Load4(hit+32)),secondary.bary,asfloat(coverage.Load4(settings+16)),float2(layout.zw),
        asfloat(coverage.Load2(settings+32)));
}
float4 reflected_ground_motion(ReflectionHit secondary) {
    if((coverage.Load(12)&4096U)==0 || !secondary.found) return 0;
    uint settings=coverage.Load(4)+1168;uint4 layout=coverage.Load4(settings);
    uint hit=layout.x+secondary.primitive*48;
    return reflection_ground_hit_motion(asfloat(coverage.Load3(settings+48)),asfloat(coverage.Load3(settings+64)),
        asfloat(triangleVertices.Load4(hit)),asfloat(triangleVertices.Load4(hit+16)),asfloat(triangleVertices.Load4(hit+32)),
        secondary.bary,asfloat(coverage.Load4(settings+16)),float2(layout.zw),asfloat(coverage.Load2(settings+32)));
}
#if defined(STARFOX_DXR_MODEL_LOBE_HISTORY)
#if defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
uint curved_receiver_prefix() {
    uint flags=coverage.Load(12);
    return (flags&256U)!=0?24:(flags&512U)!=0?20:4;
}
uint trace_curved_liquid_primary(uint2 id,float3 direction,float distance,bool visible);
#endif
uint trace_model_lobes(uint2 id) {
    uint width=uint(camera.x),height=uint(camera.y),count=width*height,index=id.y*width+id.x;
    uint settings=coverage.Load(4),lobes=coverage.Load(settings+1212);
#if defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
    uint primaryPrefix=curved_receiver_prefix(),recordPrefix=primaryPrefix+40;
    outputMask.Store4(count*(primaryPrefix+24)+index*16,0);
    if((coverage.Load(12)&256U)!=0) outputMask.Store(count*4+index*4,0);
    if((coverage.Load(12)&768U)!=0)
        outputMask.Store4(count*((coverage.Load(12)&256U)!=0?8:4)+index*16,0);
#else
    const uint primaryPrefix=4,recordPrefix=28;
#endif
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    const uint pathStride=64;
#else
    const uint pathStride=52;
#endif
    // All records are initialized, including sky/miss/protected receivers.
    outputMask.Store(count*primaryPrefix+index*4,0xffffffffU);
    outputMask.Store(count*(primaryPrefix+4)+index*4,0);
    outputMask.Store4(count*(primaryPrefix+8)+index*16,0);
    [loop] for(uint lobe=0;lobe<lobes;++lobe) {
#if defined(STARFOX_DXR_MODEL_PATH_HISTORY)
        reflection_path_store(outputMask,count*recordPrefix+(index*lobes+lobe)*pathStride,reflection_path_empty());
#else
        outputMask.Store3(count*recordPrefix+(index*lobes+lobe)*12,uint3(0xffffffffU,0,0));
#endif
    }
    float3 direction=normalize(float3((id.x+.5-camera.w)/camera.z,(id.y+.5-options.x)/options.z,1));
    RayDesc ray;ray.Origin=0;ray.Direction=direction;
    ray.TMin=primaryRange.z!=0?primaryRange.x/direction.z:.05;
    ray.TMax=primaryRange.z!=0?primaryRange.y/direction.z:65536;
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    // Actual nearer-liquid ownership, never a model behind the receiver.
    float denominator=dot(direction,groundNormal.xyz);
    float floorDistance=abs(denominator)>1.e-8?dot(groundPoint.xyz,groundNormal.xyz)/denominator:0;
    bool floorHit=options.y!=0 && floorDistance>ray.TMin && floorDistance<ray.TMax;
#if defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
    floorHit=floorHit && (asfloat(coverage.Load(settings+1096))!=0 || (coverage.Load(12)&128U)!=0);
#endif
    if(floorHit) ray.TMax=floorDistance;
#endif
    ReflectionHit primary=reflection_hit(ray,id);
#if defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
    uint liquidColour=0;
    if(floorHit && (!primary.found || (coverage.Load(12)&256U)!=0))
        liquidColour=trace_curved_liquid_primary(id,direction,floorDistance,!primary.found);
    if(!primary.found) return liquidColour;
    outputMask.Store4(count*(primaryPrefix+24)+index*16,asuint(float4(0,0,0,1)));
#else
    if(!primary.found) return 0;
#endif
    float3 normal=ray_face_normal(primary.primitive);
    if(dot(normal,direction)>0) normal=-normal;
    float bias=max(.01,primary.distance*1e-5);
    ray.Origin=direction*primary.distance+normal*bias;
    float3 reflected=reflect(direction,normal);
    ray.TMin=bias;ray.TMax=65536;
    float roughness=asfloat(coverage.Load(settings+1024));uint metallic=coverage.Load(settings+1028);
    float3 response=1;
    if(metallic) {
        float3 f0=conductor_f0(reflected_colour(primary.primitive,primary.bary,id),metallic);
        float grazing=pow(1-saturate(dot(-direction,normal)),5);
        response=f0+(1-f0)*grazing;
    }
    outputMask.Store(count*primaryPrefix+index*4,primary.primitive);
    outputMask.Store(count*(primaryPrefix+4)+index*4,asuint(primary.distance*direction.z));
    outputMask.Store4(count*(primaryPrefix+8)+index*16,asuint(float4(response,1)));
    float3 sum=0;
    [loop] for(uint lobe=0;lobe<lobes;++lobe) {
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
        // Match the ordinary sharp dielectric fast path exactly: it reflects
        // the native ray without another normalize. Re-normalizing a nearly
        // unit float ray moves animated liquid samples across RGB boundaries.
        ray.Direction=roughness==0 && metallic==0?reflected:reflection_rough_direction(reflected,normal,roughness,lobe);
#else
        ray.Direction=reflection_rough_direction(reflected,normal,roughness,lobe);
#endif
        ReflectionHit terminal;
#if defined(STARFOX_DXR_MODEL_PATH_HISTORY)
        ReflectionPathRecord path;
        uint incoming=trace_reflected_ray(ray,id,terminal,false,path);
        reflection_path_store(outputMask,count*recordPrefix+(index*lobes+lobe)*pathStride,path);
#else
        uint incoming=trace_reflected_ray(ray,id,terminal);
#endif
        sum+=reflection_linear(incoming);
#if !defined(STARFOX_DXR_MODEL_PATH_HISTORY)
        outputMask.Store3(count*recordPrefix+(index*lobes+lobe)*12,
            uint3(terminal.found?terminal.primitive:0xffffffffU,reflection_lobe_bary_pack(terminal.bary),incoming));
#endif
    }
    // No material response follows a secondary hit into the next frame.
    return reflection_pack_linear((sum/lobes)*response);
}
#endif
#if defined(STARFOX_DXR_LIQUID_HISTORY)
void reflected_liquid_witness(ReflectionHit terminal,float depth,out float4 motion,
    out uint4 identity,out float4 witness) {
    motion=0;identity=0xffffffffU;witness=0;
    if(!terminal.found) return;
    // The ordered liquid-motion compute pass fills only this zeroed guide.
    // Keep inverse-wave iteration out of the RT/material producer's registers.
    identity.xy=uint2(0xfffffffeU,terminal.primitive);witness=float4(terminal.bary,depth,1);
    uint mapping=coverage.Load(coverage.Load(4)+1208);
    if(mapping!=0 && (coverage.Load(12)&8192U)!=0)
        identity.zw=uint2(0xfffffffeU,triangleVertices.Load(mapping+terminal.primitive*4));
}
#endif
#endif
#if defined(STARFOX_DXR_NATIVE_WATER)
uint trace_native_water(uint2 id,float3 direction,float distance,out float4 surface
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
    ,out ReflectionHit terminal,out uint incomingColour,out float4 baseLight,out float4 response
#endif
#if defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
    ,out ReflectionPathRecord path
#endif
) {
#if defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
        path=reflection_path_empty();
#endif
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
        terminal.found=false;terminal.primitive=0;terminal.distance=0;terminal.bary=0;
        incomingColour=0;baseLight=0;response=0;
#endif
        NativeWaterSample water=native_water_sample(float3(0,0,0),direction,distance,id);
        surface=float4(water.normal,water.hit.z);
        float3 radiance=water.radiance*(1-water.fresnel)+water.specular;
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
        baseLight=float4(radiance,1);response=float4(water.fresnel.xxx,0);
#endif
        if(water.fresnel>0) {
            RayDesc reflected;reflected.Origin=water.hit+water.normal*water.bias;
            reflected.Direction=reflect(direction,water.normal);reflected.TMin=water.bias;reflected.TMax=65536;
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
#if defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
            incomingColour=trace_reflected_ray(reflected,id,terminal,false,path);
#else
            incomingColour=trace_reflected_ray(reflected,id,terminal);
#endif
            radiance+=reflection_linear(incomingColour)*water.fresnel;
#else
            radiance+=reflection_linear(trace_reflected_ray(reflected,id))*water.fresnel;
#endif
        }
        // 253 identifies the physically nearer translucent water receiver even
        // where primary raster saw a submerged model. UI remains excluded by
        // the independent receiver ownership; source alpha is never changed.
        return (reflection_pack_linear(radiance)&0x00ffffffU)|0xfd000000U;
}
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
#if defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
uint trace_native_water(uint2 id,float3 direction,float distance,out float4 surface,
    out ReflectionHit terminal,out uint incomingColour,out float4 baseLight,out float4 response) {
    ReflectionPathRecord unused;
    return trace_native_water(id,direction,distance,surface,terminal,incomingColour,baseLight,response,unused);
}
#endif
uint trace_native_water(uint2 id,float3 direction,float distance,out float4 surface) {
    ReflectionHit terminal;uint incoming;float4 baseLight,response;
    return trace_native_water(id,direction,distance,surface,terminal,incoming,baseLight,response);
}
#endif
#endif
uint trace_water(uint2 id,float3 direction,float distance
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
    ,out ReflectionHit terminal,out uint incomingColour,out float4 baseLight,out float4 response
#endif
#if defined(STARFOX_DXR_SCENE_PATH_HISTORY) || defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
    ,out ReflectionPathRecord path
#endif
) {
#if defined(STARFOX_DXR_SCENE_PATH_HISTORY) || defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
    path=reflection_path_empty();
#endif
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
    terminal.found=false;terminal.primitive=0;terminal.distance=0;terminal.bary=0;
    incomingColour=0;baseLight=0;response=0;
#endif
#if defined(STARFOX_DXR_NATIVE_WATER)
    float4 unused;
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
#if defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
    return trace_native_water(id,direction,distance,unused,terminal,incomingColour,baseLight,response,path);
#else
    return trace_native_water(id,direction,distance,unused,terminal,incomingColour,baseLight,response);
#endif
#else
    return trace_native_water(id,direction,distance,unused);
#endif
#else
    uint data=coverage.Load(4)+1088;
    float4 settings=asfloat(coverage.Load4(data));
    uint caustics=(uint(settings.w)>>5)&3u;
    settings.w=float(uint(settings.w)&15u);
#if defined(STARFOX_DXR_LIQUID_HISTORY)
    settings.w=3; // This non-water PSO is validated exclusively for native lava.
#endif
    float4 r0=asfloat(coverage.Load4(data+16)),r1=asfloat(coverage.Load4(data+32)),r2=asfloat(coverage.Load4(data+48));
    float3x3 rotation=float3x3(r0.xyz,r1.xyz,r2.xyz);
    float3 hit=direction*distance;
    float3 position=mul(hit,rotation)+float3(r0.w,r1.w,r2.w);
    float t=settings.x;
    float footprint=distance/max(camera.z,1.f)/max(abs(dot(direction,groundNormal.xyz)),.04f);
#if defined(STARFOX_DXR_REFLECTION_HISTORY) && !defined(STARFOX_DXR_LIQUID_HISTORY) && !defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
    // This producer is explicitly restricted to native mirror/gold floors by
    // both API validators. Curved liquids have separate optional PSOs; do not
    // carry their wave/transmission registers through a flat mirror.
    float3 normal=normalize(mul(rotation,float3(0,-1,0)));
    if(dot(normal,direction)>0) normal=-normal;
#else
    // Long waves plus smaller crossing ripples; anchored in world coordinates,
    // not screen pixels. No stochastic sampling or per-frame noise.
    LiquidOpticalSample optical=liquid_optical_sample(hit,direction,distance,footprint,rotation,
        float3(r0.w,r1.w,r2.w),t,uint(settings.w));
    hit=optical.hit;position=optical.position;
    LavaSample liquid=optical.lava;float3 normal=optical.normal;
    if(settings.w==3) {
        float3 viewer=mul(-direction,rotation);
        LavaColour colour=lava_shade(liquid,viewer.x,viewer.y,viewer.z);
        float3 molten=float3(colour.r,colour.g,colour.b)*settings.z;
#if defined(STARFOX_DXR_NATIVE_MATERIALS)
        // The shared surface recipe is authored colour, just like the raster
        // lava. Separate/transport it in the negotiated linear domain, then
        // encode exactly once. Otherwise sRGB panels brighten emission twice.
        if((coverage.Load(12)&32U)!=0) molten=calibrated_decode_srgb(molten);
#endif
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
        baseLight=float4(molten,1);
#endif
        if(settings.y>0) {
            float bias=max(.05,distance*1e-5);
            RayDesc reflected;reflected.Origin=hit+normal*bias;
            reflected.TMin=bias;reflected.TMax=65536;
            reflected.Direction=reflect(direction,normal);
            float fresnel=.035f+.40f*pow(1-saturate(dot(-direction,normal)),5);
            float share=fresnel*settings.y*(.3f+.7f*liquid.crust);
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
#if defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
            incomingColour=trace_reflected_ray(reflected,id,terminal,true,path);
#else
            incomingColour=trace_reflected_ray(reflected,id,terminal,true);
#endif
            response=float4(share.xxx,0);
            uint reflectedWord=incomingColour;
#else
            ReflectionHit unused;uint reflectedWord=trace_reflected_ray(reflected,id,unused,true);
#endif
#if defined(STARFOX_DXR_NATIVE_MATERIALS)
            molten+=reflection_linear(reflectedWord)*share;
#else
            molten+=reflection_rgb(reflectedWord)*share;
#endif
        }
        // Shade the continuous surface directly. Packing slopes then rounding
        // a second screen-space warp caused visible whole-pixel jumps.
#if defined(STARFOX_DXR_NATIVE_MATERIALS)
        return (reflection_pack_linear(molten)&0x00ffffffU)|0xfe000000U;
#else
        uint3 rgb=uint3(saturate(molten)*255+.5);
        return rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xfe000000u;
#endif
    }
#endif
    float bias=max(.05,distance*1e-5);
    RayDesc ray;ray.Origin=hit+normal*bias;ray.TMin=bias;ray.TMax=65536;
    float3 light=normalize(mul(rotation,float3(-1,-1,-1)));
    ray.Direction=light;
    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> shadow;
    shadow.TraceRayInline(scene,RAY_FLAG_FORCE_NON_OPAQUE,255,ray);
    while(shadow.Proceed()) if(shadow.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE
        && coverage_colour(shadow.CandidatePrimitiveIndex(),shadow.CandidateTriangleBarycentrics(),id)!=0)
        shadow.CommitNonOpaqueTriangleHit();
    float visibility=shadow.CommittedStatus()==COMMITTED_TRIANGLE_HIT?0:1;
    uint background=coverage.Load(coverage.Load(4)+1052);
    float3 authored=reflection_rgb(background?reflection_background_at(background,
        128+256*(hit.x+asfloat(coverage.Load(coverage.Load(4)+1084)))/hit.z,
        112+256*hit.y/hit.z):asuint(groundPoint.w));
    float luminance=dot(authored,float3(.3,.59,.11));
    float3 base=luminance*float3(.20,.58,.85);
    if(settings.w==1) base=luminance*float3(.8,.82,.85);
    if(settings.w==2) base=luminance*float3(1,.875,.58);
    float diffuse=.65+.35*visibility*max(0,dot(normal,light));
    float3 radiance=base*base*diffuse;
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
    const bool nativeMetal=true;
#else
    bool nativeMetal=(coverage.Load(12)&4U)!=0 && (settings.w==1 || settings.w==2);
#endif
    if(nativeMetal) radiance=((coverage.Load(12)&32U)!=0?calibrated_decode_srgb(base):base)*diffuse;
#if !defined(STARFOX_DXR_REFLECTION_HISTORY)
    if(settings.w==0) {
        // Trace UNDER the water plane rather than alpha-blending a screenshot.
        // This ray deliberately ignores the analytic water receiver itself.
        float3 transmitted=refract(direction,normal,.75);
        RayDesc through;through.Origin=hit-normal*bias;through.Direction=transmitted;
        through.TMin=bias;through.TMax=65536;
        float3 worldOrigin=mul(through.Origin,rotation)+float3(r0.w,r1.w,r2.w);
        float3 worldDirection=mul(transmitted,rotation);
        float bottomTravel=worldDirection.y>0?(position.y+640-worldOrigin.y)/worldDirection.y:65536;
        through.TMax=min(through.TMax,bottomTravel);
        ReflectionHit submerged=reflection_hit(through,id);
        if(submerged.found) {
            float3 receiver=reflection_rgb(reflected_colour(submerged.primitive,submerged.bary,id));
            receiver*=receiver;
            float travel=submerged.distance;
            if(caustics!=0) {
                float3 receiverView=through.Origin+transmitted*travel;
                float3 receiverWorld=mul(receiverView,rotation)+float3(r0.w,r1.w,r2.w);
                float depth=receiverWorld.y-position.y;
                if(depth>0) {
                    WaterCausticSample focus=water_caustic_sample(receiverWorld.x,receiverWorld.z,t,depth,footprint);
                    float3 entry=mul(rotation,float3(focus.entry_x,position.y,focus.entry_z)-float3(r0.w,r1.w,r2.w));
                    float3 segment=entry-receiverView;float lengthToWater=length(segment);
                    float3 receiverNormal=ray_face_normal(submerged.primitive);
                    if(dot(receiverNormal,transmitted)>0)receiverNormal=-receiverNormal;
                    float up=saturate(-mul(receiverNormal,rotation).y);
                    if(lengthToWater>bias*2 && up>0
                        && !caustic_blocked(receiverView,segment/lengthToWater,lengthToWater-bias,bias,id)
                        && !caustic_blocked(entry,mul(rotation,float3(0,-1,0)),65536,bias,id))
                        receiver*=clamp(1+(focus.irradiance-exp(-depth/1600))*up*float(caustics)/3,.25,3);
                }
            }
            radiance=float3(water_transmitted_channel(receiver.r,radiance.r,travel,.0025),
                water_transmitted_channel(receiver.g,radiance.g,travel,.0008),
                water_transmitted_channel(receiver.b,radiance.b,travel,.00035));
        }
        else if(bottomTravel>bias && bottomTravel<65536) {
            float3 receiverView=through.Origin+transmitted*bottomTravel;
            float3 receiverWorld=mul(receiverView,rotation)+float3(r0.w,r1.w,r2.w);
            // A quiet sandy bed supplies a real refracted receiver on stages
            // whose original ground was only an infinite coloured plane.
            float3 receiver=float3(.28,.24,.16);
            if(caustics!=0) {
                WaterCausticSample focus=water_caustic_sample(receiverWorld.x,receiverWorld.z,t,640,footprint);
                float3 entry=mul(rotation,float3(focus.entry_x,position.y,focus.entry_z)-float3(r0.w,r1.w,r2.w));
                float3 segment=entry-receiverView;float lengthToWater=length(segment);
                if(lengthToWater>bias*2
                    && !caustic_blocked(receiverView,segment/lengthToWater,lengthToWater-bias,bias,id)
                    && !caustic_blocked(entry,mul(rotation,float3(0,-1,0)),65536,bias,id))
                    receiver*=clamp(1+(focus.irradiance-exp(-640.f/1600))*float(caustics)/3,.25,3);
            }
            radiance=float3(water_transmitted_channel(receiver.r,radiance.r,bottomTravel,.0025),
                water_transmitted_channel(receiver.g,radiance.g,bottomTravel,.0008),
                water_transmitted_channel(receiver.b,radiance.b,bottomTravel,.00035));
        }
    }
#endif
    float grazing=pow(1-saturate(dot(-direction,normal)),5);
    float fresnel=settings.w==1 || nativeMetal?1:settings.w!=0?.95:.02+.98*grazing;
    if(settings.y>0) {
        ray.Direction=reflect(direction,normal);
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
#if defined(STARFOX_DXR_SCENE_PATH_HISTORY) || defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
        uint reflectedWord=trace_reflected_ray(ray,id,terminal,false,path);
#else
        uint reflectedWord=trace_reflected_ray(ray,id,terminal);
#endif
#else
        uint reflectedWord=trace_reflected_ray(ray,id);
#endif
        float3 reflected=reflection_rgb(reflectedWord);
        float3 reflectance=settings.w==2?float3(1,.766,.336)+(1-float3(1,.766,.336))*grazing:float3(1,1,1);
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
        if(nativeMetal) {
            incomingColour=reflectedWord;
            baseLight=float4(radiance*(1-saturate(fresnel*settings.y)),1);
            response=float4(reflectance*saturate(fresnel*settings.y),0);
        }
#endif
        radiance=lerp(radiance,(nativeMetal?reflection_linear(reflectedWord):reflected*reflected)*reflectance,saturate(fresnel*settings.y));
    }
    float3 halfway=normalize(light-direction);
    float specular=pow(saturate(dot(normal,halfway)),96)*visibility;
    radiance+=float3(1,.95,.82)*specular*max(authored.r,max(authored.g,authored.b))*.55;
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
    if(baseLight.w==1) baseLight.rgb+=float3(1,.95,.82)*specular*max(authored.r,max(authored.g,authored.b))*.55;
#endif
    if(nativeMetal) return (reflection_pack_linear(radiance)&0x00ffffffU)|0xfe000000U;
    uint3 rgb=uint3(sqrt(saturate(radiance))*255+.5);
    // 254 denotes a shaded water receiver; 255 remains a model reflection.
    return rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xfe000000u;
#endif
}
#if defined(STARFOX_DXR_SCENE_PATH_HISTORY) || defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
uint trace_water(uint2 id,float3 direction,float distance,out ReflectionHit terminal,
    out uint incomingColour,out float4 baseLight,out float4 response) {
    ReflectionPathRecord unused;return trace_water(id,direction,distance,terminal,incomingColour,baseLight,response,unused);
}
#endif
#if defined(STARFOX_DXR_CURVED_RECEIVER_HISTORY)
uint trace_curved_liquid_primary(uint2 id,float3 direction,float distance,bool visible) {
    uint count=uint(camera.x)*uint(camera.y),index=id.y*uint(camera.x)+id.x;
    uint prefix=curved_receiver_prefix(),lobes=coverage.Load(coverage.Load(4)+1212);
    ReflectionHit terminal;uint incoming;float4 baseLight,response,surface=0;ReflectionPathRecord path;
#if defined(STARFOX_DXR_NATIVE_WATER)
    uint colour=trace_native_water(id,direction,distance,surface,terminal,incoming,baseLight,response,path);
#else
    uint colour=trace_water(id,direction,distance,terminal,incoming,baseLight,response,path);
#endif
    if((coverage.Load(12)&256U)!=0) outputMask.Store(count*4+index*4,colour);
    if(visible) {
        if((coverage.Load(12)&768U)!=0)
            outputMask.Store4(count*((coverage.Load(12)&256U)!=0?8:4)+index*16,asuint(surface));
        outputMask.Store(count*prefix+index*4,0xfffffffdU);
        outputMask.Store(count*(prefix+4)+index*4,asuint(distance*direction.z));
        outputMask.Store4(count*(prefix+8)+index*16,asuint(float4(response.rgb,baseLight.w)));
        outputMask.Store4(count*(prefix+24)+index*16,asuint(baseLight));
        // A sharp floor keeps the same physical ray even when models use
        // eight rough lobes. Preserve current transmission/emission separately.
        [loop] for(uint l=0;l<lobes;++l)
            reflection_path_store(outputMask,count*(prefix+40)+(index*lobes+l)*64,path);
    }
    return colour;
}
#endif
#if defined(STARFOX_DXR_SCENE_PATH_HISTORY)
// Full model/analytic-floor producer. Prefix: RGBA, primary/depth, CURRENT
// response and additive direct/base light. No primary ground reads a model
// behind the nearer plane; every secondary ground bounce has an explicit ID.
uint trace_scene_paths(uint2 id) {
    uint count=uint(camera.x)*uint(camera.y),index=id.y*uint(camera.x)+id.x;
    uint settings=coverage.Load(4),lobes=coverage.Load(settings+1212);
    outputMask.Store(count*4+index*4,0xffffffffU);outputMask.Store(count*8+index*4,0);
    outputMask.Store4(count*12+index*16,0);outputMask.Store4(count*28+index*16,0);
    [loop] for(uint l=0;l<lobes;++l)
        reflection_path_store(outputMask,count*44+(index*lobes+l)*52,reflection_path_empty());
    float3 direction=normalize(float3((id.x+.5-camera.w)/camera.z,(id.y+.5-options.x)/options.z,1));
    float depthScale=primaryRange.z!=0?1/direction.z:1;
    RayDesc ray;ray.Origin=0;ray.Direction=direction;
    ray.TMin=min(3.402823466e38,primaryRange.x*depthScale);ray.TMax=min(3.402823466e38,primaryRange.y*depthScale);
    float denominator=dot(direction,groundNormal.xyz);
    float floorDistance=abs(denominator)>1.e-8?dot(groundPoint.xyz,groundNormal.xyz)/denominator:0;
    bool floorHit=options.y!=0 && floorDistance>ray.TMin && floorDistance<ray.TMax;
    if(floorHit) ray.TMax=floorDistance;
    ReflectionHit primary=reflection_hit(ray,id);
    if(!primary.found) {
        if(!floorHit) return 0;
        ReflectionHit terminal;uint incoming;float4 baseLight,response;ReflectionPathRecord path;
        uint colour=trace_water(id,direction,floorDistance,terminal,incoming,baseLight,response,path);
        outputMask.Store(count*4+index*4,0xfffffffeU);
        outputMask.Store(count*8+index*4,asuint(floorDistance*direction.z));
        outputMask.Store4(count*12+index*16,asuint(float4(response.rgb,baseLight.w)));
        outputMask.Store4(count*28+index*16,asuint(baseLight));
        // The floor is sharp regardless of the MODEL rough quadrature count.
        // Duplicate its same physical ray, not eight invented rough floor rays.
        [loop] for(uint l=0;l<lobes;++l) reflection_path_store(outputMask,count*44+(index*lobes+l)*52,path);
        return colour;
    }
    float3 normal=ray_face_normal(primary.primitive);if(dot(normal,direction)>0) normal=-normal;
    float bias=max(.01,primary.distance*1e-5);ray.Origin=direction*primary.distance+normal*bias;
    float3 reflected=reflect(direction,normal);ray.TMin=bias;ray.TMax=65536;
    float rough=asfloat(coverage.Load(settings+1024));uint metallic=coverage.Load(settings+1028);float3 response=1;
    if(metallic) {
        float3 f0=conductor_f0(reflected_colour(primary.primitive,primary.bary,id),metallic);
        response=f0+(1-f0)*pow(1-saturate(dot(-direction,normal)),5);
    }
    outputMask.Store(count*4+index*4,primary.primitive);
    outputMask.Store(count*8+index*4,asuint(primary.distance*direction.z));
    outputMask.Store4(count*12+index*16,asuint(float4(response,1)));
    outputMask.Store4(count*28+index*16,asuint(float4(0,0,0,1)));
    float3 sum=0;
    [loop] for(uint l=0;l<lobes;++l) {
        ray.Direction=reflection_rough_direction(reflected,normal,rough,l);ReflectionHit terminal;ReflectionPathRecord path;
        uint colour=trace_reflected_ray(ray,id,terminal,false,path);
        reflection_path_store(outputMask,count*44+(index*lobes+l)*52,path);sum+=reflection_linear(colour);
    }
    return reflection_pack_linear(sum/lobes*response);
}
#endif
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
uint trace_water(uint2 id,float3 direction,float distance) {
    ReflectionHit terminal;uint incoming;float4 baseLight,response;
    return trace_water(id,direction,distance,terminal,incoming,baseLight,response);
}
#endif
uint trace_reflection(uint2 id,out uint worldColour,out float4 surface,out float4 hitMotion,out uint4 hitIdentity,out float4 hitWitness,
    out uint incomingColour,out float4 baseLight,out float4 response) {
    worldColour=0;surface=0;hitMotion=0;hitIdentity=0xffffffffU;hitWitness=0;incomingColour=0;baseLight=0;response=0;
    float3 direction=normalize(float3((id.x+.5-camera.w)/camera.z,(id.y+.5-options.x)/options.z,1));
    // Legacy reflection bounds are radial. A calibrated depth plane must be
    // converted for this normalized ray, including asymmetric FOV corners.
    float depthScale=primaryRange.z!=0?1/direction.z:1;
    RayDesc ray;ray.Origin=0;ray.Direction=direction;
    ray.TMin=min(3.402823466e38,primaryRange.x*depthScale);
    ray.TMax=min(3.402823466e38,primaryRange.y*depthScale);
    bool water=false;
#if !defined(STARFOX_DXR_MODELS_ONLY)
    if(options.y!=0 && (asfloat(coverage.Load(coverage.Load(4)+1096))!=0 || (coverage.Load(12)&128U)!=0)) {
        float denominator=dot(direction,groundNormal.xyz);
        float distance=abs(denominator)>1e-8?dot(groundPoint.xyz,groundNormal.xyz)/denominator:0;
        if(distance>ray.TMin && distance<ray.TMax) {ray.TMax=distance;water=true;}
    }
#if defined(STARFOX_DXR_NATIVE_WATER)
    bool worldLayers=(coverage.Load(12)&256U)!=0;
    float4 waterSurface=0;
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
    ReflectionHit worldTerminal=(ReflectionHit)0;uint worldIncoming=0;float4 worldBase=0,worldResponse=0;
    if(water && worldLayers) worldColour=trace_native_water(id,direction,ray.TMax,waterSurface,
        worldTerminal,worldIncoming,worldBase,worldResponse);
#else
    if(water && worldLayers) worldColour=trace_native_water(id,direction,ray.TMax,waterSurface);
#endif
    if ((uint(options.w)&2u)!=0) {
        if(!water) return 0;
        if(worldLayers) {surface=waterSurface;return worldColour;}
        return trace_native_water(id,direction,ray.TMax,surface);
    }
#else
    if ((uint(options.w)&2u)!=0) return water?trace_water(id,direction,ray.TMax):0;
#endif
#else
    if ((uint(options.w)&2u)!=0) return 0;
#endif
    ReflectionHit primary=reflection_hit(ray,id);
    if(!primary.found) {
        if((uint(primaryRange.w)&15u)!=0) return 0; // Opt-in raw hit diagnostic, not presentation colour.
#if defined(STARFOX_DXR_MODELS_ONLY)
        return 0;
#else
#if defined(STARFOX_DXR_NATIVE_WATER)
        if(!water) return 0;
#if defined(STARFOX_DXR_LIQUID_HISTORY)
        ReflectionHit terminal;uint colour;
        if(worldLayers) {
            surface=waterSurface;colour=worldColour;terminal=worldTerminal;
            incomingColour=worldIncoming;baseLight=worldBase;response=worldResponse;
        } else colour=trace_native_water(id,direction,ray.TMax,surface,terminal,incomingColour,baseLight,response);
        reflected_liquid_witness(terminal,ray.TMax*direction.z,hitMotion,hitIdentity,hitWitness);
        return colour;
#else
        if(worldLayers) {surface=waterSurface;return worldColour;}
        return trace_native_water(id,direction,ray.TMax,surface);
#endif
#else
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
        if(water && (coverage.Load(12)&2048U)!=0) {
            ReflectionHit terminal;
            uint colour=trace_water(id,direction,ray.TMax,terminal,incomingColour,baseLight,response);
#if defined(STARFOX_DXR_LIQUID_HISTORY)
            reflected_liquid_witness(terminal,ray.TMax*direction.z,hitMotion,hitIdentity,hitWitness);
#else
            if(terminal.found) {
                hitMotion=reflected_ground_motion(terminal);
                hitIdentity.xy=uint2(0xfffffffeU,terminal.primitive);
                hitWitness=float4(terminal.bary,ray.TMax*direction.z,1);
                uint mapping=coverage.Load(coverage.Load(4)+1208);
                if(mapping!=0 && (coverage.Load(12)&4096U)!=0)
                    hitIdentity.zw=uint2(0xfffffffeU,triangleVertices.Load(mapping+terminal.primitive*4));
            }
#endif
            return colour;
        }
#endif
        return water?trace_water(id,direction,ray.TMax):0;
#endif
#endif
    }
    uint primitive=primary.primitive;
    uint diagnostic=uint(primaryRange.w)&15u;
    if(diagnostic==1) return primitive+1;
    if(diagnostic==2) return asuint(primary.distance);
    float3 normal=ray_face_normal(primitive);
    if(dot(normal,direction)>0) normal=-normal;
    if(diagnostic>=3) return asuint(normal[diagnostic-3]);
    float distance=primary.distance;
    float bias=max(.01,distance*1e-5);
    ray.Origin=direction*distance+normal*bias;
    ray.Direction=reflect(direction,normal);ray.TMin=bias;ray.TMax=65536;
    uint settings=coverage.Load(4)+1024;
    float roughness=asfloat(coverage.Load(settings));
    uint metallic=coverage.Load(settings+4);
    if(roughness==0 && !metallic) {
        ReflectionHit terminal;
        uint colour=trace_reflected_ray(ray,id,terminal);
        incomingColour=colour;baseLight=float4(0,0,0,1);response=float4(1,1,1,0);
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
        hitMotion=reflected_hit_motion(primitive,terminal);
        if(terminal.found) {
            hitIdentity.xy=uint2(primitive,terminal.primitive);
            hitWitness=float4(terminal.bary,distance*direction.z,1);
            uint mapping=coverage.Load(coverage.Load(4)+1208);
            if(mapping!=0) hitIdentity.zw=uint2(triangleVertices.Load(mapping+primitive*4),
                triangleVertices.Load(mapping+terminal.primitive*4));
        }
#endif
        return colour;
    }
    float3 reflected=ray.Direction;
    float3 tangent=normalize(cross(reflected,abs(reflected.y)<.95?float3(0,1,0):float3(1,0,0)));
    float3 bitangent=cross(reflected,tangent),sum=0;
    // Fixed quadrature: no frame/pixel noise or temporal accumulation required.
    const float2 taps[8]={float2(.5,0),float2(-.5,0),float2(0,.5),float2(0,-.5),
        float2(.612,.612),float2(-.612,.612),float2(.612,-.612),float2(-.612,-.612)};
    uint samples=roughness>0?8:1;
    for(uint sample=0;sample<samples;++sample) {
        ray.Direction=normalize(reflected+roughness*roughness*(tangent*taps[sample].x+bitangent*taps[sample].y));
        if(dot(ray.Direction,normal)<=0) ray.Direction=reflected;
        sum+=reflection_linear(trace_reflected_ray(ray,id));
    }
    float3 radiance=sum/samples;
    if(metallic) {
        float3 f0=conductor_f0(reflected_colour(primitive,primary.bary,id),metallic);
        // Linear-light conductor reflectance; the scene remains ray traced.
        float grazing=pow(1-saturate(dot(-direction,normal)),5);
        radiance*=f0+(1-f0)*grazing;
    }
    return reflection_pack_linear(radiance);
}

uint trace_pixel(uint2 id) {
    float3 ray = float3((id.x + .5 - camera.w) / camera.z,
        (id.y + .5 - options.x) / options.z, 1);
    float depth = primaryRange.y;
    bool receiverFound = false;
    if (options.y != 0) {
        float denominator = dot(ray, groundNormal.xyz);
        if (abs(denominator) > 1e-10) {
            float groundDepth = dot(groundPoint.xyz, groundNormal.xyz) / denominator;
            if (groundDepth > primaryRange.x && groundDepth < depth) {
                depth = groundDepth;
                receiverFound = true;
            }
        }
    }
    // Underlay receivers must not stop on the model hiding the ground.
    if ((uint(options.w)&2u)==0) {
        RayDesc receiverRay;
        receiverRay.Origin = 0;
        receiverRay.Direction = ray;
        receiverRay.TMin = primaryRange.x;
        receiverRay.TMax = depth;
        RayQuery<RAY_FLAG_NONE> receiver;
        receiver.TraceRayInline(scene, (uint(options.w)&1u) != 0 ? RAY_FLAG_FORCE_NON_OPAQUE : RAY_FLAG_FORCE_OPAQUE, 255, receiverRay);
        while (receiver.Proceed()) {
            if (receiver.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE && covered(receiver.CandidatePrimitiveIndex(), receiver.CandidateTriangleBarycentrics()))
                receiver.CommitNonOpaqueTriangleHit();
        }
        if (receiver.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
            depth = receiver.CommittedRayT();
            receiverFound = true;
        }
    }
    uint blocked = 0;
    if (receiverFound) {
        RayDesc shadowRay;
        shadowRay.Origin = ray * depth;
        shadowRay.TMin = max(.1, depth * 1e-5);
        shadowRay.TMax = 65536;
        for (uint sample = 0; sample < uint(lights[0].w); ++sample) {
            shadowRay.Direction = lights[sample].xyz;
            RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> shadow;
            shadow.TraceRayInline(scene, (uint(options.w)&1u) != 0 ? RAY_FLAG_FORCE_NON_OPAQUE : RAY_FLAG_FORCE_OPAQUE, 255, shadowRay);
            while (shadow.Proceed()) {
                if (shadow.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE && covered(shadow.CandidatePrimitiveIndex(), shadow.CandidateTriangleBarycentrics()))
                    shadow.CommitNonOpaqueTriangleHit();
            }
            if (shadow.CommittedStatus() == COMMITTED_TRIANGLE_HIT) ++blocked;
        }
    }
    return 160 * blocked / uint(lights[0].w);
}

// One byte per pixel, with four-byte row alignment for ByteAddressBuffer.
// Keep one ray workload per lane; shared packing avoids serializing four
// pixels' ray queries and does not assume a hardware wave/lane ordering.
groupshared uint shadowValues[64];
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID, uint local : SV_GroupIndex) {
    uint width = (uint)camera.x;
    uint height = (uint)camera.y;
    bool valid = id.x < width && id.y < height;
#if defined(STARFOX_DXR_REFLECTION_ONLY)
    if(valid) {
#if defined(STARFOX_DXR_SCENE_PATH_HISTORY)
        outputMask.Store((id.y*width+id.x)*4,trace_scene_paths(id.xy));return;
#endif
#if defined(STARFOX_DXR_MODEL_LOBE_HISTORY)
        // The host selects this model-only PSO only for a validated compact
        // allocation. Keep the extra lobe loop out of legacy/liquid PSOs.
        outputMask.Store((id.y*width+id.x)*4,trace_model_lobes(id.xy));return;
#endif
        uint world,incoming;float4 surface,hitMotion,hitWitness,baseLight,response;uint4 hitIdentity;
        outputMask.Store((id.y*width+id.x)*4,trace_reflection(id.xy,world,surface,hitMotion,hitIdentity,hitWitness,incoming,baseLight,response));
#if defined(STARFOX_DXR_REFLECTION_HISTORY)
        if((coverage.Load(12)&1024U)!=0)
        {
            uint index=id.y*width+id.x,offset=coverage.Load(coverage.Load(4)+1172);
            outputMask.Store4(offset+index*16,asuint(hitMotion));
            outputMask.Store4(offset+width*height*16+index*16,hitIdentity);
            outputMask.Store4(offset+width*height*32+index*16,asuint(hitWitness));
            if((coverage.Load(12)&2048U)!=0) {
                uint count=width*height;
                outputMask.Store(offset+count*48+index*4,incoming);
                outputMask.Store4(offset+count*52+index*16,asuint(baseLight));
                outputMask.Store4(offset+count*68+index*16,asuint(response));
            }
        }
#endif
#if defined(STARFOX_DXR_NATIVE_WATER)
        if((coverage.Load(12)&768U)!=0) {
            uint pixels=width*(uint)camera.y,index=id.y*width+id.x;
            bool worldLayers=(coverage.Load(12)&256U)!=0;
            if(worldLayers) outputMask.Store(pixels*4+index*4,world);
            outputMask.Store4(pixels*(worldLayers?8:4)+index*16,asuint(surface));
        }
#endif
    }
#else
#if !defined(STARFOX_DXR_SHADOW_ONLY)
    if((coverage.Load(12)&1u)!=0) {
        if(valid) {
            uint world,incoming;float4 surface,hitMotion,hitWitness,baseLight,response;uint4 hitIdentity;
            outputMask.Store((id.y*width+id.x)*4,trace_reflection(id.xy,world,surface,hitMotion,hitIdentity,hitWitness,incoming,baseLight,response));
        }
        return;
    }
#endif
    shadowValues[local] = valid ? trace_pixel(id.xy) : 0;
    GroupMemoryBarrierWithGroupSync();
    if ((local & 3) == 0 && valid) {
        uint packed = shadowValues[local] | (shadowValues[local+1] << 8)
            | (shadowValues[local+2] << 16) | (shadowValues[local+3] << 24);
        outputMask.Store(id.y * ((width+3)&~3U) + id.x, packed);
    }
#endif
}
