// Construct optical frames from the exact submitted GPU old-vertex bank and
// CURRENT raw terminal features. No host endpoint, source root or RGB input.
#include "reflection_source_feature.hlsli"
#include "reflection_specular_path_motion.hlsli"
ByteAddressBuffer queries:register(t0,space0),geometry:register(t1,space0),current:register(t2,space0);
RWByteAddressBuffer frames:register(u0,space1);
cbuffer FrameSettings:register(b0,space2) {
    uint oldTriangles,previousVertices,previousMapping,geometryBytes;
    uint currentWidth,currentHeight,recordPrefix,pathStride;
    uint lobes,queryFirst,queryCount,pathFlags;
    float acceptedRoughness;uint frameUnused0,frameUnused1,frameUnused2;
    float4 projection,extentClip,oldPoint,oldNormal;
    float4 oldLiquid[8];
};
bool old_plane(uint primitive,out ReflectionSpecularPlane plane) {
    plane.a=plane.b=plane.c=0;
    if(primitive==0xfffffffeU) {
        if((pathFlags&1U)==0 || oldPoint.w!=1 || oldNormal.w!=0)return false;
        plane.a=float4(oldPoint.xyz,2);plane.b=float4(oldNormal.xyz,2);plane.c=float4(0,0,0,2);
    } else {
        // Do not read mapping/other payload as accepted vertices, even if the
        // overall allocation is large enough. Every primitive is explicit.
        if(primitive>=oldTriangles || previousVertices>previousMapping || previousMapping>geometryBytes
            || primitive>=(previousMapping-previousVertices)/48)return false;
        const uint at=previousVertices+primitive*48;
        plane.a=asfloat(geometry.Load4(at));plane.b=asfloat(geometry.Load4(at+16));plane.c=asfloat(geometry.Load4(at+32));
    }
    return reflection_specular_plane_valid(plane);
}
void store_plane(uint at,ReflectionSpecularPlane plane) {
    frames.Store4(at,asuint(plane.a));frames.Store4(at+16,asuint(plane.b));frames.Store4(at+32,asuint(plane.c));
}
[numthreads(64,1,1)]
void feature_frames_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=queryCount)return;
    const uint output=id.x*512,q=id.x*48;
    [unroll] for(uint n=0;n<32;++n)frames.Store4(output+n*16,0);
    const uint primary=queries.Load(q),control=queries.Load(q+20),terminal=queries.Load(q+24),lobe=queries.Load(q+44);
    const uint hops=control&7U,mask=(control>>4)&15U,kind=control>>8;
    if(primary==0xffffffffU || lobe>=lobes || !control_valid(control)
        || control!=((kind<<8)|(mask<<4)|hops) || (kind==3?hops!=4:hops==4))return;
    const uint sampleIndex=queryFirst+id.x,count=currentWidth*currentHeight;
    const float3 rawFeature=asfloat(current.Load3(count*recordPrefix+sampleIndex*pathStride+24));
    if(!feature_valid(rawFeature,kind))return;
    ReflectionSpecularPlane plane;
    const bool liquidPrimary=primary==0xfffffffdU;
    if(liquidPrimary) {if((pathFlags&4U)==0)return;}
    else {if(!old_plane(primary,plane))return;store_plane(output,plane);}
    frames.Store4(output+48,asuint(projection));frames.Store4(output+64,asuint(extentClip));
    frames.Store4(output+80,asuint(float4(acceptedRoughness,lobe,0,0)));
    const uint4 mirrors=queries.Load4(q+4);
    [unroll] for(uint h=0;h<4;++h) {
        if(h>=hops){if(mirrors[h]!=0xffffffffU)return;}
        else if(mask&(1U<<h)){if((pathFlags&2U)==0 || mirrors[h]!=0xfffffffdU)return;}
        else {if(!old_plane(mirrors[h],plane))return;store_plane(output+96+h*48,plane);}
    }
    if(kind==1){if(terminal>=oldTriangles || !old_plane(terminal,plane))return;store_plane(output+432,plane);}
    else if(terminal!=0xffffffffU)return;
    [unroll] for(uint n=0;n<8;++n)frames.Store4(output+288+n*16,asuint(oldLiquid[n]));
    // Store raw finite barycentrics or CURRENT environment direction. The
    // consumer forms an outward interval endpoint instead of a rounded point.
    frames.Store4(output+416,asuint(float4(rawFeature,kind==1?1:0)));
    frames.Store4(output+480,uint4(hops,mask,liquidPrimary?1U:0U,kind));
    frames.Store4(output+496,uint4(1,0,0,0)); // All-or-nothing frame validity.
}
