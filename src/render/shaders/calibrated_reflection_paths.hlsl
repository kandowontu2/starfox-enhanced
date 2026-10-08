// Complete planar-path incident history. The inverse optical guide alone is
// never permission to reuse a colour. Every old bilinear tap must have the
// matched primary, ordered mirrors, terminal kind/ID, depth and feature footprint.
#include "calibrated_colour.hlsli"
#include "calibrated_ray_history.hlsli"
#include "reflection_specular_path_motion.hlsli"
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
#ifndef STARFOX_CURVED_COLOUR_ROOT
#define STARFOX_CURVED_COLOUR_ROOT 1
#endif
#include "reflection_curved_path_motion.hlsli"
#define HISTORY_CURVED_PARAMETERS ,ReflectionPathRecord currentRecord
#define HISTORY_CURVED_ARGUMENTS ,r
#else
#define HISTORY_CURVED_PARAMETERS
#define HISTORY_CURVED_ARGUMENTS
#endif
#if defined(STARFOX_CURVED_CACHE_TRACE)
#define HISTORY_TRACE_PARAMETERS ,out uint refusal,out uint refusedTap,out float3 refusedDelta,out float3 refusedFootprint
#define HISTORY_TRACE_INIT refusal=0;refusedTap=0xffffffffU;refusedDelta=refusedFootprint=0
#define HISTORY_REFUSE(reason,tap,delta,footprint) {refusal=reason;refusedTap=tap;refusedDelta=delta;refusedFootprint=footprint;return false;}
#else
#define HISTORY_TRACE_PARAMETERS
#define HISTORY_TRACE_INIT
#define HISTORY_REFUSE(reason,tap,delta,footprint) return false
#endif
#include "reflection_path_record.hlsli"
Texture2D<float4> ownership:register(t0,space0);SamplerState ownerSampler:register(s0,space0);
ByteAddressBuffer current:register(t1,space0),history:register(t2,space0),geometry:register(t3,space0);
RWByteAddressBuffer resolved:register(u0,space1);
cbuffer Settings:register(b0,space2) {
    uint width,height,previousWidth,previousHeight;
    uint lobes,flags,previousVertices,previousMapping;
    uint triangles,geometryBytes;float weight,roughness;
    float4 projection,clip;
    uint oldTriangles,scenePaths,primaryPrefix,curvedReceivers;
    float4 currentCube[3],previousCube[3];
    float4 currentPoint,currentNormal,previousPoint,previousNormal;
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    float4 oldLiquid[8];
    float4 currentLiquidRotation[3],currentProjection;
#endif
};
uint receiver_prefix() {
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    return primaryPrefix;
#else
    return 4;
#endif
}
uint record_prefix() {
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    return curvedReceivers!=0?primaryPrefix+40:28;
#else
    return scenePaths!=0?44:28;
#endif
}
uint path_stride() {
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    return 64;
#else
    return 52;
#endif
}
bool primary_analytic(uint primary) {
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    return curvedReceivers!=0 && primary==0xfffffffdU;
#else
    return primary==0xfffffffeU;
#endif
}
float4 current_base(uint count,uint index) {
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    return curvedReceivers!=0?asfloat(current.Load4(count*(primaryPrefix+24)+index*16)):float4(0,0,0,1);
#else
    return scenePaths!=0?asfloat(current.Load4(count*28+index*16)):float4(0,0,0,1);
#endif
}
float3 incident_colour(uint word) {
    float3 rgb=float3(word&255U,(word>>8)&255U,(word>>16)&255U)/255.;
    return (flags&2U)!=0?calibrated_decode_srgb(rgb):rgb;
}
uint incident_pack(float3 rgb,uint alpha) {
    rgb=(flags&2U)!=0?calibrated_encode_srgb(saturate(rgb)):saturate(rgb);
    uint3 bytes=uint3(floor(rgb*255.+.5));return bytes.x|(bytes.y<<8)|(bytes.z<<16)|alpha;
}
ReflectionPathRecord path_record(ByteAddressBuffer image,uint count,uint index,uint lobe) {
    return reflection_path_load(image,count*record_prefix()+(index*lobes+lobe)*path_stride());
}
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
ReflectionLiquidFrame old_liquid_frame() {
    ReflectionLiquidFrame f;f.planePoint=oldLiquid[0];f.planeNormal=oldLiquid[1];
    f.rotation0=oldLiquid[2];f.rotation1=oldLiquid[3];f.rotation2=oldLiquid[4];
    f.projection=oldLiquid[5];f.extentClip=oldLiquid[6];f.settings=oldLiquid[7];return f;
}
#endif
bool ground_valid(bool old) {
    float4 position=old?previousPoint:currentPoint,normal=old?previousNormal:currentNormal;
    float length2=dot(normal.xyz,normal.xyz);
    return scenePaths==1 && position.w==1 && normal.w==0 && all(isfinite(position)) && all(isfinite(normal))
        && all(abs(position.xyz)<=1.e12) && all(abs(normal.xyz)<=1.e12) && isfinite(length2) && length2>1.e-20;
}
bool path_valid(ReflectionPathRecord r,uint bound,bool old) {
    uint hops=r.control&7U,kind=r.control>>8;
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    uint mask=(r.control>>4)&15U;
    if(hops>4 || kind<1 || kind>4 || r.control!=((kind<<8)|(mask<<4)|hops) || (mask>>hops)!=0
        || (kind==3 && hops!=4) || ((kind==1 || kind==2) && hops==4) || (kind!=4 && (r.incoming>>24)!=255)
        || !all(isfinite(r.feature)) || !all(isfinite(r.response)) || !all(isfinite(r.base))
        || any(r.response<0) || any(r.response>1) || any(r.base<0) || any(r.base>1.e12)) return false;
    [unroll] for(uint h=0;h<4;++h) {
        if(h<hops) {
            if((mask&(1U<<h))!=0) {if(r.mirrors[h]!=0xfffffffdU) return false;}
            else if(r.mirrors[h]>=bound) return false;
        } else if(r.mirrors[h]!=0xffffffffU) return false;
    }
    if(kind==4) return hops>0 && r.terminal==0xffffffffU && all(r.feature==0) && all(r.response<1.e-5) && r.incoming==0;
#else
    if(hops>4 || kind<1 || kind>3 || r.control!=((kind<<8)|hops)
        || (kind!=3 && hops==4) || (kind==3 && hops!=4) || (r.incoming>>24)!=255
        || !all(isfinite(r.feature)) || !all(isfinite(r.response))
        || any(r.response<0) || any(r.response>1)) return false;
    [unroll] for(uint h=0;h<4;++h)
        if(h<hops?(r.mirrors[h]==0xfffffffeU?!ground_valid(old):r.mirrors[h]>=bound):r.mirrors[h]!=0xffffffffU) return false;
#endif
    if(kind==1) return r.terminal<bound && r.feature.z==0 && all(r.feature.xy>=0) && dot(r.feature.xy,1.xx)<=1;
    return r.terminal==0xffffffffU && abs(dot(r.feature,r.feature)-1)<.0001;
}
bool same_path(ReflectionPathRecord a,ReflectionPathRecord b) {
    return a.control==b.control && a.terminal==b.terminal && all(a.mirrors==b.mirrors);
}
bool receiver_valid(uint2 pixel,uint word,uint primary,float depth,float4 response,float4 base) {
    float4 owner=ownership.Load(int3(pixel,0));
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    bool floor=primary_analytic(primary);
    bool validBase=curvedReceivers==0 || (base.w==1 && all(isfinite(base)) && all(base.rgb>=0) && all(base.rgb<=1.e12));
    bool validKind=floor?(word>>24)==(oldLiquid[7].y==0?253U:254U):primary<triangles && (word>>24)==255U;
#else
    bool floor=primary==0xfffffffeU && ground_valid(false);
    bool validBase=scenePaths==0 || (base.w==1 && all(isfinite(base)) && all(base.rgb>=0) && all(base.rgb<=1.e12));
    bool validKind=floor?(word>>24)==254U:primary<triangles && (word>>24)==255U;
#endif
    return owner.a>.5 && calibrated_secondary_radiance(word,owner,true)
        && validKind
        && isfinite(depth) && depth>0 && response.w==1
        && validBase
        && all(isfinite(response)) && all(response.rgb>=0) && all(response.rgb<=1) && any(response.rgb>0);
}
ReflectionSpecularPlane old_plane(uint primitive) {
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    if(primitive==0xfffffffdU) {ReflectionSpecularPlane p;p.a=p.b=p.c=0;return p;}
#endif
    if(primitive==0xfffffffeU) {
        ReflectionSpecularPlane p;p.a=float4(previousPoint.xyz,2);p.b=float4(previousNormal.xyz,2);p.c=float4(0,0,0,2);return p;
    }
    uint at=previousVertices+primitive*48;ReflectionSpecularPlane p;
    p.a=asfloat(geometry.Load4(at));p.b=asfloat(geometry.Load4(at+16));p.c=asfloat(geometry.Load4(at+32));
    // Only the explicit ground identity may construct an analytic W=2
    // plane. Malformed finite geometry cannot opt itself out of face bounds.
    if(p.a.w!=1 || p.b.w!=1 || p.c.w!=1) p.a=p.b=p.c=0;
    return p;
}
float3 old_direction(float3 currentDirection) {
    float3 cube=float3(dot(currentCube[0].xyz,currentDirection),dot(currentCube[1].xyz,currentDirection),
        dot(currentCube[2].xyz,currentDirection));
    return normalize(cube.x*previousCube[0].xyz+cube.y*previousCube[1].xyz+cube.z*previousCube[2].xyz);
}
bool matched_path(ReflectionPathRecord r,out ReflectionPathRecord matched) {
    matched=r;uint hops=r.control&7U;
    [loop] for(uint h=0;h<hops;++h) {
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
        if((r.control&(1U<<(h+4)))!=0) continue;
#endif
        if(r.mirrors[h]==0xfffffffeU) {if(!ground_valid(true)) return false;continue;}
        matched.mirrors[h]=geometry.Load(previousMapping+r.mirrors[h]*4);
        if(matched.mirrors[h]>=oldTriangles) return false;
    }
    if((r.control>>8)==1) {
        matched.terminal=geometry.Load(previousMapping+r.terminal*4);
        if(matched.terminal>=oldTriangles) return false;
    } else matched.feature=old_direction(r.feature);
    return true;
}
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
bool curved_subcell_features(uint primary,ReflectionPathRecord r,uint lobe,int2 cell,out float3 inner[5]) {
    ReflectionSpecularPlane receiver=old_plane(primary),planes[4];uint hops=r.control&7U;
    [unroll] for(uint h=0;h<4;++h) {planes[h].a=planes[h].b=planes[h].c=0;}
    [loop] for(uint h=0;h<hops;++h) if((r.control&(1U<<(h+4)))==0) planes[h]=old_plane(r.mirrors[h]);
    ReflectionRoughFrame frame;frame.a=receiver.a;frame.b=receiver.b;frame.c=receiver.c;
    frame.projection=projection;frame.extentClip=float4(previousWidth,previousHeight,clip.xy);
    frame.settings=primary_analytic(primary)?float4(0,0,0,0):float4(roughness,lobe,0,0);
    ReflectionSpecularPlane terminal=old_plane((r.control>>8)==1?r.terminal:0);
    const uint2 locations[5]={uint2(1,0),uint2(0,1),uint2(1,1),uint2(2,1),uint2(1,2)};
    [loop] for(uint i=0;i<5;++i) {
        float2 sample=float2(cell)+.5+float2(locations[i])*.5;
        CurvedVector origin,outgoing;float2 depth,bias;
        if(!reflection_curved_forward_precise(sample,frame,old_liquid_frame(),planes,hops,(r.control>>4)&15U,
            uint(primary_analytic(primary)),true,origin,outgoing,depth,bias)) return false;
        if((r.control>>8)!=1) inner[i]=curved_vec_result(outgoing);
        else {
            if(!reflection_specular_plane_valid(terminal)) return false;
            CurvedVector a=curved_vec_float(terminal.a.xyz),u=curved_vec_sub(curved_vec_float(terminal.b.xyz),a),v=curved_vec_sub(curved_vec_float(terminal.c.xyz),a);
            CurvedVector normal=curved_vec_unit(curved_vec_cross(u,v));float2 den=curved_vec_dot(outgoing,normal);
            if(!curved_dd_less(float2(1.e-12,0),curved_dd_abs(den)))return false;
            float2 distance=curved_dd_div(curved_vec_dot(curved_vec_sub(a,origin),normal),den);
            if(!curved_dd_less(bias,distance) || !curved_dd_less(distance,float2(65536,0)))return false;
            CurvedVector hitPoint=curved_vec_add(origin,curved_vec_scale(outgoing,distance));
            if(!reflection_curved_inside(terminal,hitPoint))return false;
            CurvedVector offset=curved_vec_sub(hitPoint,a);
            float2 aa=curved_vec_dot(u,u),ab=curved_vec_dot(u,v),bb=curved_vec_dot(v,v),ra=curved_vec_dot(offset,u),rb=curved_vec_dot(offset,v);
            float2 det=curved_dd_sub(curved_dd_mul(aa,bb),curved_dd_mul(ab,ab));
            if(!curved_dd_less(float2(1.e-20,0),det))return false;
            inner[i]=float3(curved_dd_float(curved_dd_div(curved_dd_sub(curved_dd_mul(bb,ra),curved_dd_mul(ab,rb)),det)),
                curved_dd_float(curved_dd_div(curved_dd_sub(curved_dd_mul(aa,rb),curved_dd_mul(ab,ra)),det)),0);
        }
        if(!all(isfinite(inner[i])))return false;
    }
    return true;
}
#endif
bool previous_sample(uint primary,ReflectionPathRecord matched,uint lobe,float4 guide,out float3 prior HISTORY_CURVED_PARAMETERS HISTORY_TRACE_PARAMETERS) {
    HISTORY_TRACE_INIT;
    prior=0;
    if((flags&1U)==0 || weight==0 || guide.w!=1 || !all(isfinite(guide)) || guide.z<=0) HISTORY_REFUSE(1,0xffffffffU,0,0);
    uint matchedPrimary=primary_analytic(primary)?primary:geometry.Load(previousMapping+primary*4);
    if(!primary_analytic(primary) && matchedPrimary>=oldTriangles) HISTORY_REFUSE(2,0xffffffffU,0,0);
    float2 at=guide.xy-.5;
    if(any(at<0) || any(at>float2(previousWidth-1,previousHeight-1))) HISTORY_REFUSE(3,0xffffffffU,0,0);
    int2 base=int2(floor(at));float2 fraction=at-float2(base);uint count=previousWidth*previousHeight;
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    if(all(fraction>0) && all(fraction<1) && all(base+1<int2(previousWidth,previousHeight))) {
        // A bilinear angular/barycentric footprint must not fold across the
        // selected cell. The inverse may have a valid nearby branch, but that
        // alone is not permission to interpolate across a liquid caustic fold.
        float3 features[4];
        [unroll] for(uint tap=0;tap<4;++tap) {
            uint index=(base.y+int(tap>>1))*previousWidth+base.x+int(tap&1U);
            ReflectionPathRecord r=path_record(history,count,index,lobe);
            if(history.Load(count*receiver_prefix()+index*4)!=matchedPrimary || !path_valid(r,oldTriangles,true) || !same_path(r,matched)) HISTORY_REFUSE(4,index,0,0);
            features[tap]=r.feature;
        }
        float3 dx0=features[1]-features[0],dx1=features[3]-features[2];
        float3 dy0=features[2]-features[0],dy1=features[3]-features[1];
        bool positive=false,negative=false;
        [unroll] for(uint tap=0;tap<4;++tap) {
            float3 dx=(tap&2U)!=0?dx1:dx0,dy=(tap&1U)!=0?dy1:dy0;
            float orientation=(matched.control>>8)==1?dx.x*dy.y-dx.y*dy.x:dot(features[tap],cross(dx,dy));
            if(!isfinite(orientation)) HISTORY_REFUSE(5,tap,0,0);
            positive=positive || orientation>1.e-20;negative=negative || orientation<-1.e-20;
        }
        if(positive && negative) HISTORY_REFUSE(6,0xffffffffU,0,0);
        // Two individually regular cells can sit on opposite sides of a
        // narrow optical fold. A nearby inverse may select either branch;
        // a regular selected cell alone is not an unambiguous color source.
        // Classify the one-cell ring using ONLY accepted records with the
        // same complete path. Missing/unrelated cells supply no evidence.
        [loop] for(int y=-1;y<=1;++y) [loop] for(int x=-1;x<=1;++x) {
            if(x==0 && y==0) continue;
            int2 cell=base+int2(x,y);
            if(any(cell<0) || any(cell+1>=int2(previousWidth,previousHeight))) continue;
            float3 ring[4];bool valid=true;
            [unroll] for(uint tap=0;tap<4;++tap) {
                uint index=(cell.y+int(tap>>1))*previousWidth+cell.x+int(tap&1U);
                ReflectionPathRecord old=path_record(history,count,index,lobe);
                valid=valid && history.Load(count*receiver_prefix()+index*4)==matchedPrimary
                    && path_valid(old,oldTriangles,true) && same_path(old,matched);
                ring[tap]=old.feature;
            }
            if(!valid) continue;
            float3 dx0=ring[1]-ring[0],dx1=ring[3]-ring[2];
            float3 dy0=ring[2]-ring[0],dy1=ring[3]-ring[1];
            [unroll] for(uint tap=0;tap<4;++tap) {
                float3 dx=(tap&2U)!=0?dx1:dx0,dy=(tap&1U)!=0?dy1:dy0;
                float orientation=(matched.control>>8)==1?dx.x*dy.y-dx.y*dy.x:dot(ring[tap],cross(dx,dy));
                if(!isfinite(orientation)) HISTORY_REFUSE(5,tap,0,0);
                positive=positive || orientation>1.e-20;negative=negative || orientation<-1.e-20;
            }
        }
        if(positive && negative) HISTORY_REFUSE(12,0xffffffffU,0,0);
        // Coarse chords can miss a caustic folded INSIDE a bilinear cell.
        // Retrace its five half-pixel points with the accepted old optics and
        // check four quarter-cells. No old color is fetched from these points.
        float3 inner[5];
        if(!curved_subcell_features(primary,currentRecord,lobe,base,inner)) HISTORY_REFUSE(11,0xffffffffU,0,0);
        float3 grid[9]={features[0],inner[0],features[1],inner[1],inner[2],inner[3],features[2],inner[4],features[3]};
        [unroll] for(uint y=0;y<2;++y) [unroll] for(uint x=0;x<2;++x) {
            uint a=y*3+x;float3 dx0=grid[a+1]-grid[a],dx1=grid[a+4]-grid[a+3];
            float3 dy0=grid[a+3]-grid[a],dy1=grid[a+4]-grid[a+1];
            [unroll] for(uint tap=0;tap<4;++tap) {
                float3 dx=(tap&2U)!=0?dx1:dx0,dy=(tap&1U)!=0?dy1:dy0;
                uint at=a+(tap&1U)+(tap>>1)*3;
                float orientation=(matched.control>>8)==1?dx.x*dy.y-dx.y*dy.x:dot(grid[at],cross(dx,dy));
                if(!isfinite(orientation)) HISTORY_REFUSE(5,at,0,0);
                positive=positive || orientation>1.e-20;negative=negative || orientation<-1.e-20;
            }
        }
        if(positive && negative) HISTORY_REFUSE(10,0xffffffffU,0,0);
    }
#endif
    float inverseDepth=0;
    // Keep the four bilinear taps in bounded loops. Native D3D12 traces
    // observed a zero footprint in the nested-unrolled early-return form,
    // while a separate GPU fetch validated the same neighbours and bounds.
    // This changes code generation, not path eligibility or tolerances.
    [loop] for(uint dy=0;dy<2;++dy) [loop] for(uint dx=0;dx<2;++dx) {
        float share=(dx?fraction.x:1-fraction.x)*(dy?fraction.y:1-fraction.y);if(share<=0) continue;
        int2 tap=min(base+int2(dx,dy),int2(previousWidth-1,previousHeight-1));uint index=tap.y*previousWidth+tap.x;
        float depth=asfloat(history.Load(count*(receiver_prefix()+4)+index*4));ReflectionPathRecord old=path_record(history,count,index,lobe);
        if(history.Load(count*receiver_prefix()+index*4)!=matchedPrimary || !path_valid(old,oldTriangles,true) || !same_path(old,matched)
            || !isfinite(depth) || depth<=0) HISTORY_REFUSE(7,index,0,0);
        // Forward Z changes rapidly across a receding plane, including a
        // tilted finite model face, not only a floor. Its reciprocal is
        // affine in the accepted eye grid for the same planar primitive.
        // Compare depth AT the optical guide, not each neighbour's different
        // ray. Every nonzero tap still has to match the entire ordered path.
        // Curved liquid receivers retain their existing interpolation and
        // tolerance. This does not turn a missing/different tap into coverage.
        inverseDepth+=share/depth;
        // A full ordered-path feature footprint, not a terminal-ID-only colour
        // lookup. Even tiny nonzero bilinear shares must pass visibility.
        float3 footprint=2.e-6;
        [unroll] for(int axis=0;axis<2;++axis) {
            float3 gradient=0;
            [unroll] for(int side=-1;side<=1;side+=2) {
                int2 next=tap;next[axis]+=side;
                if(any(next<0) || any(next>=int2(previousWidth,previousHeight))) continue;
                uint n=next.y*previousWidth+next.x;ReflectionPathRecord adjacent=path_record(history,count,n,lobe);
                if(history.Load(count*receiver_prefix()+n*4)==matchedPrimary && path_valid(adjacent,oldTriangles,true) && same_path(adjacent,matched))
                    gradient=max(gradient,abs(adjacent.feature-old.feature)*1.5);
            }
            footprint+=gradient*abs(at[axis]-float(tap[axis]));
        }
        if(any(abs(old.feature-matched.feature)>min(footprint,(matched.control>>8)==1?.05:.02))) HISTORY_REFUSE(8,index,abs(old.feature-matched.feature),min(footprint,(matched.control>>8)==1?.05:.02));
        prior+=incident_colour(old.incoming)*share;
    }
    if(!isfinite(inverseDepth) || inverseDepth<=0
        || abs(1/inverseDepth-guide.z)>max(.01,guide.z*.005)) HISTORY_REFUSE(9,0xffffffffU,0,0);
    return true;
}
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
float2 curved_receiver_seed(uint primary,uint2 pixel,float depth,ReflectionSpecularPlane oldReceiver) {
    float3 hit=float3((float2(pixel)+.5-currentProjection.zw)/currentProjection.xy,1)*depth;
    if(primary_analytic(primary)) return reflection_liquid_receiver_seed(old_liquid_frame(),hit,
        currentLiquidRotation[0],currentLiquidRotation[1],currentLiquidRotation[2]);
    // Current/accepted positions share an EXPLICIT primitive correspondence.
    // Transport barycentrics only as an initializer, never as an optical guide.
    uint at=primary*48;float3 a=asfloat(geometry.Load3(at)),b=asfloat(geometry.Load3(at+16)),c=asfloat(geometry.Load3(at+32));
    float3 u=b-a,v=c-a,r=hit-a;
    float aa=dot(u,u),ab=dot(u,v),bb=dot(v,v),ra=dot(r,u),rb=dot(r,v),det=aa*bb-ab*ab;
    if(!isfinite(det) || det<=1.e-20) return -1;
    float2 bary=float2(bb*ra-ab*rb,aa*rb-ab*ra)/det;
    float3 prior=oldReceiver.a.xyz*(1-bary.x-bary.y)+oldReceiver.b.xyz*bary.x+oldReceiver.c.xyz*bary.y;
    if(!all(isfinite(prior)) || prior.z<=0) return -1;
    return projection.xy*prior.xy/prior.z+projection.zw;
}
#endif
#if !defined(STARFOX_CURVED_CACHE_TRACE)
[numthreads(8,8,1)]
void reflection_paths_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    uint count=width*height,index=id.y*width+id.x,word=current.Load(index*4),prefix=receiver_prefix(),primary=current.Load(count*prefix+index*4);
    float depth=asfloat(current.Load(count*(prefix+4)+index*4));float4 response=asfloat(current.Load4(count*(prefix+8)+index*16));
    float4 base=current_base(count,index);
    bool eligible=receiver_valid(id.xy,word,primary,depth,response,base);
    // A malformed/unsupported lobe cannot contaminate a partially recomposed
    // pixel. Preserve its original prefix and refuse reuse for that pixel.
    [loop] for(uint l=0;l<lobes;++l) eligible=eligible && path_valid(path_record(current,count,index,l),triangles,false);
    // Canonical hidden-world and surface guides belong to CURRENT primary
    // optics. Never filter/copy them using the reflected secondary guide.
    resolved.Store(index*4,word);
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    if(prefix==24) resolved.Store(count*4+index*4,current.Load(count*4+index*4));
    if(prefix>=20) {
        uint surface=count*(prefix==24?8:4)+index*16;
        resolved.Store4(surface,current.Load4(surface));
    }
#endif
    resolved.Store(count*prefix+index*4,eligible?primary:0xffffffffU);
    resolved.Store(count*(prefix+4)+index*4,asuint(depth));resolved.Store4(count*(prefix+8)+index*16,asuint(response));
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    if(curvedReceivers!=0) resolved.Store4(count*(prefix+24)+index*16,asuint(base));
#else
    if(scenePaths!=0) resolved.Store4(count*28+index*16,asuint(base));
#endif
    float3 sum=0;bool changed=false;
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
    float4 liquidGuide=0;ReflectionPathRecord liquidGuidePath=reflection_path_empty();bool liquidGuideReady=false;
#endif
    [loop] for(uint lobe=0;lobe<lobes;++lobe) {
        ReflectionPathRecord r=path_record(current,count,index,lobe),saved=r;float3 incoming=incident_colour(r.incoming);
        if(eligible && (flags&1U)!=0 && weight>0
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
            && (r.control>>8)!=4
#endif
        ) {
            ReflectionPathRecord matched;
            if(matched_path(r,matched)) {
                ReflectionSpecularPlane receiver=old_plane(primary),planes[4];uint hops=r.control&7U;
                [unroll] for(uint h=0;h<4;++h) {planes[h].a=planes[h].b=planes[h].c=0;}
                [loop] for(uint h=0;h<hops;++h)
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
                    if((r.control&(1U<<(h+4)))==0)
#endif
                    planes[h]=old_plane(r.mirrors[h]);
                ReflectionRoughFrame frame;
                frame.a=receiver.a;frame.b=receiver.b;frame.c=receiver.c;
                frame.projection=projection;frame.extentClip=float4(previousWidth,previousHeight,clip.xy);
                frame.settings=primary_analytic(primary)?float4(0,0,0,0):float4(roughness,lobe,0,0);
                float4 terminal=float4(matched.feature,0);
                if((r.control>>8)==1) {
                    ReflectionSpecularPlane hit=old_plane(r.terminal);
                    if(!reflection_specular_plane_valid(hit)) terminal=0;
                    else {
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
                        // Decode the actual stored barycentrics without large
                        // opposing-vertex cancellation BEFORE the precise ray
                        // solve. Three rounded products can move a near-caustic
                        // finite feature even when the inverse itself is strict.
                        float2 a=curved_dd_sub(curved_dd_sub(float2(1,0),float2(r.feature.x,0)),float2(r.feature.y,0));
                        CurvedVector endpoint=curved_vec_add(curved_vec_add(curved_vec_scale(curved_vec_float(hit.a.xyz),a),
                            curved_vec_scale(curved_vec_float(hit.b.xyz),float2(r.feature.x,0))),
                            curved_vec_scale(curved_vec_float(hit.c.xyz),float2(r.feature.y,0)));
                        terminal=float4(curved_vec_result(endpoint),1);
#else
                        terminal=float4(hit.a.xyz*(1-r.feature.x-r.feature.y)+hit.b.xyz*r.feature.x+hit.c.xyz*r.feature.y,1);
#endif
                    }
                }
                float4 guide;
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
                // The sharp liquid ignores the finite receiver plane. Invalid
                // mapped finite geometry still cannot become an analytic face.
                // Native liquid primaries duplicate ONE sharp physical path
                // for an eight-lobe MODEL allocation. Solve that identical
                // geometry once, but validate EACH lobe's old visibility/RGB.
                if(primary_analytic(primary) && liquidGuideReady && same_path(r,liquidGuidePath) && all(r.feature==liquidGuidePath.feature)) guide=liquidGuide;
                else guide=reflection_curved_path_motion(frame,old_liquid_frame(),planes,hops,(r.control>>4)&15U,
                    uint(primary_analytic(primary)),terminal,curved_receiver_seed(primary,id.xy,depth,receiver));
                if(primary_analytic(primary) && !liquidGuideReady) {liquidGuideReady=true;liquidGuide=guide;liquidGuidePath=r;}
#else
                guide=reflection_specular_path_motion(frame,planes,hops,terminal);
#endif
                float3 prior;
                if(previous_sample(primary,matched,lobe,guide,prior HISTORY_CURVED_ARGUMENTS)) {
                    float3 low=incoming,high=incoming;
                    [unroll] for(int dy=-1;dy<=1;++dy) [unroll] for(int dx=-1;dx<=1;++dx) {
                        int2 tap=clamp(int2(id.xy)+int2(dx,dy),int2(0,0),int2(width-1,height-1));uint n=tap.y*width+tap.x;
                        ReflectionPathRecord adjacent=path_record(current,count,n,lobe);uint p=current.Load(count*prefix+n*4);
                        float4 adjacentBase=current_base(count,n);
                        if(p!=primary || !path_valid(adjacent,triangles,false) || !same_path(adjacent,r)
                            || !receiver_valid(uint2(tap),current.Load(n*4),p,asfloat(current.Load(count*(prefix+4)+n*4)),
                                asfloat(current.Load4(count*(prefix+8)+n*16)),adjacentBase)) continue;
                        float3 rgb=incident_colour(adjacent.incoming);low=min(low,rgb);high=max(high,rgb);
                    }
                    incoming=lerp(incoming,clamp(prior,low,high),weight);
                    saved.incoming=incident_pack(incoming,r.incoming&0xff000000U);changed=true;
                }
            }
        }
        // Native RT quantizes each transported lobe BEFORE quadrature average.
        // Retain that order; never blend primary or per-hop material response.
        sum+=incident_colour(incident_pack(incoming*r.response
#if defined(STARFOX_DXR_CURVED_PATH_HISTORY)
            +r.base
#endif
            ,0xff000000U));
#if !defined(STARFOX_DXR_CURVED_PATH_HISTORY)
        if(!eligible) saved=reflection_path_empty();
#endif
        // Ordered liquid paths retain all CURRENT per-hop geometry/light,
        // even behind protected ink or an unsupported current receiver. The
        // invalid primary written above already rejects every old footprint;
        // erasing CURRENT records is neither needed nor permission to reuse.
        reflection_path_store(resolved,count*record_prefix()+(index*lobes+lobe)*path_stride(),saved);
    }
    if(changed) resolved.Store(index*4,incident_pack(base.rgb+sum/lobes*response.rgb,word&0xff000000U));
}
#else
// Component-only entry point. Separate output/bindings supplied by the tool;
// never allocated, enqueued or accepted by a game color-history owner.
// Seven float/uint4 words: guide, terminal, decision, delta, cap, solve, state.
[numthreads(8,8,1)]
void reflection_curved_trace_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    uint count=width*height,index=id.y*width+id.x,prefix=receiver_prefix(),word=current.Load(index*4);
    uint primary=current.Load(count*prefix+index*4);
    float depth=asfloat(current.Load(count*(prefix+4)+index*4));
    float4 response=asfloat(current.Load4(count*(prefix+8)+index*16));
    bool eligible=receiver_valid(id.xy,word,primary,depth,response,current_base(count,index));
    [loop] for(uint l=0;l<lobes;++l) eligible=eligible && path_valid(path_record(current,count,index,l),triangles,false);
    [loop] for(uint lobe=0;lobe<lobes;++lobe) {
        ReflectionPathRecord r=path_record(current,count,index,lobe),matched;
        float4 guide=0,terminal=0;uint refusal=20,refusedTap=0xffffffffU;float3 delta=0,footprint=0;
        reflectionCurvedSolveTrace=0;reflectionCurvedDecisionTrace=0;
        if(eligible && (flags&1U)!=0 && weight>0 && (r.control>>8)!=4) {
            refusal=21;
            if(matched_path(r,matched)) {
                ReflectionSpecularPlane receiver=old_plane(primary),planes[4];uint hops=r.control&7U;
                [unroll] for(uint h=0;h<4;++h) {planes[h].a=planes[h].b=planes[h].c=0;}
                [loop] for(uint h=0;h<hops;++h) if((r.control&(1U<<(h+4)))==0) planes[h]=old_plane(r.mirrors[h]);
                ReflectionRoughFrame frame;frame.a=receiver.a;frame.b=receiver.b;frame.c=receiver.c;
                frame.projection=projection;frame.extentClip=float4(previousWidth,previousHeight,clip.xy);
                frame.settings=primary_analytic(primary)?float4(0,0,0,0):float4(roughness,lobe,0,0);
                terminal=float4(matched.feature,0);
                if((r.control>>8)==1) {
                    ReflectionSpecularPlane hit=old_plane(r.terminal);
                    if(!reflection_specular_plane_valid(hit)) terminal=0;
                    else {
                        float2 a=curved_dd_sub(curved_dd_sub(float2(1,0),float2(r.feature.x,0)),float2(r.feature.y,0));
                        CurvedVector endpoint=curved_vec_add(curved_vec_add(curved_vec_scale(curved_vec_float(hit.a.xyz),a),
                            curved_vec_scale(curved_vec_float(hit.b.xyz),float2(r.feature.x,0))),
                            curved_vec_scale(curved_vec_float(hit.c.xyz),float2(r.feature.y,0)));
                        terminal=float4(curved_vec_result(endpoint),1);
                    }
                }
                guide=reflection_curved_path_motion(frame,old_liquid_frame(),planes,hops,(r.control>>4)&15U,
                    uint(primary_analytic(primary)),terminal,curved_receiver_seed(primary,id.xy,depth,receiver));
                float3 prior;bool used=previous_sample(primary,matched,lobe,guide,prior,r,refusal,refusedTap,delta,footprint);
                if(used) refusal=0;
            }
        }
        // Trace-only, second-pass neighbour classification. Do not change
        // eligibility, colour reuse or the production footprint calculation.
        // Previously unused fourth lanes preserve the 112-byte trace ABI.
        uint neighbourChecks=0;float checkedFootprint=0;
        if(refusal==8 && refusedTap<count) {
            int2 tap=int2(refusedTap%previousWidth,refusedTap/previousWidth);
            ReflectionPathRecord old=path_record(history,count,refusedTap,lobe);
            uint matchedPrimary=primary_analytic(primary)?primary:geometry.Load(previousMapping+primary*4);
            const int2 offsets[4]={int2(-1,0),int2(1,0),int2(0,-1),int2(0,1)};
            float3 gx=0,gy=0;
            [unroll] for(uint n=0;n<4;++n) {
                int2 next=tap+offsets[n];uint bits=0;
                if(all(next>=0) && all(next<int2(previousWidth,previousHeight))) {
                    bits=1;
                    uint p=next.y*previousWidth+next.x;ReflectionPathRecord adjacent=path_record(history,count,p,lobe);
                    bool samePrimary=history.Load(count*receiver_prefix()+p*4)==matchedPrimary;
                    bool valid=path_valid(adjacent,oldTriangles,true),same=same_path(adjacent,matched);
                    bits|=(samePrimary?2U:0U)|(valid?4U:0U)|(same?8U:0U);
                    float3 gradient=abs(adjacent.feature-old.feature)*1.5;
                    bits|=any(gradient>0)?16U:0U;
                    if(samePrimary && valid && same) {
                        if(n<2)gx=max(gx,gradient);else gy=max(gy,gradient);
                    }
                }
                neighbourChecks|=bits<<(n*8);
            }
            float3 check=2.e-6+gx*abs(guide.x-.5-float(tap.x))+gy*abs(guide.y-.5-float(tap.y));
            checkedFootprint=min(check.x,(matched.control>>8)==1?.05:.02);
        }
        uint at=(index*lobes+lobe)*112;
        resolved.Store4(at,asuint(guide));resolved.Store4(at+16,asuint(terminal));
        resolved.Store4(at+32,uint4(refusal,refusedTap,r.control,primary));
        resolved.Store4(at+48,uint4(asuint(delta),neighbourChecks));resolved.Store4(at+64,asuint(float4(footprint,checkedFootprint)));
        resolved.Store4(at+80,asuint(reflectionCurvedSolveTrace));resolved.Store4(at+96,reflectionCurvedDecisionTrace);
    }
}
#endif
