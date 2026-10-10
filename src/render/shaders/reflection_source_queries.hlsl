// Generate accepted-frame feature queries from CURRENT native GPU records and
// the submitted primitive correspondence. No RGB, inverse, root or CPU query.
#include "reflection_source_feature.hlsli"
#define STARFOX_CURVED_SCALAR_ONLY 1
#include "reflection_curved_precision.hlsli"
ByteAddressBuffer current:register(t0,space0),geometry:register(t1,space0);
RWByteAddressBuffer queries:register(u0,space1);
cbuffer Mapping:register(b0,space2) {
    uint currentWidth,currentHeight,primaryPrefix,recordPrefix;
    uint pathStride,currentTriangles,oldTriangles,previousMapping;
    uint geometryBytes,lobes,queryFirst,queryCount;
    uint pathFlags,mapUnused0,mapUnused1,mapUnused2;
    float4 currentCube[3],previousCube[3];
};
bool mapped_primitive(uint primitive,out uint mapped) {
    mapped=0xffffffffU;
    if(primitive>=currentTriangles || previousMapping>geometryBytes || primitive>=(geometryBytes-previousMapping)/4)return false;
    mapped=geometry.Load(previousMapping+primitive*4);return mapped<oldTriangles;
}
float3 accepted_direction(float3 direction) {
    float2 cube[3],old[3];
    [unroll] for(uint r=0;r<3;++r) {
        cube[r]=0;
        [unroll] for(uint c=0;c<3;++c)cube[r]=curved_dd_add(cube[r],curved_dd_mul(float2(currentCube[r][c],0),float2(direction[c],0)));
    }
    [unroll] for(uint c=0;c<3;++c) {
        old[c]=0;
        [unroll] for(uint r=0;r<3;++r)old[c]=curved_dd_add(old[c],curved_dd_mul(float2(previousCube[r][c],0),cube[r]));
    }
    float2 length2=0;
    [unroll] for(uint c=0;c<3;++c)length2=curved_dd_add(length2,curved_dd_mul(old[c],old[c]));
    if(!all(isfinite(length2)) || !curved_dd_less(float2(1.e-20,0),length2))return 0;
    const float2 length=curved_dd_sqrt(length2);
    return float3(curved_dd_float(curved_dd_div(old[0],length)),curved_dd_float(curved_dd_div(old[1],length)),
        curved_dd_float(curved_dd_div(old[2],length)));
}
[numthreads(64,1,1)]
void feature_mapping_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=queryCount)return;
    const uint sampleIndex=queryFirst+id.x,p=sampleIndex/lobes,lobe=sampleIndex%lobes,count=currentWidth*currentHeight;
    const uint outAt=id.x*48,primary=current.Load(count*primaryPrefix+p*4);
    const float depth=asfloat(current.Load(count*(primaryPrefix+4)+p*4));
    const uint at=count*recordPrefix+sampleIndex*pathStride;
    uint4 mirrors=current.Load4(at);const uint control=current.Load(at+16);uint terminal=current.Load(at+20);
    float3 feature=asfloat(current.Load3(at+24));uint matchedPrimary=0xffffffffU;
    const uint hops=control&7U,mask=(control>>4)&15U,kind=control>>8;
    bool valid=primary!=0xffffffffU && isfinite(depth) && depth>0 && control_valid(control) && feature_valid(feature,kind)
        && control==((kind<<8)|(mask<<4)|hops) && (kind==3?hops==4:hops<4)
        && ((pathFlags&2U)!=0 || mask==0);
    if(primary==0xfffffffdU) {matchedPrimary=primary;valid=valid && (pathFlags&4U)!=0;}
    else if(primary==0xfffffffeU) {matchedPrimary=primary;valid=valid && (pathFlags&1U)!=0;}
    else valid=valid && mapped_primitive(primary,matchedPrimary);
    [unroll] for(uint h=0;h<4;++h) {
        if(h>=hops)valid=valid && mirrors[h]==0xffffffffU;
        else if((mask&(1U<<h))!=0)valid=valid && mirrors[h]==0xfffffffdU;
        else if(mirrors[h]==0xfffffffeU)valid=valid && (pathFlags&1U)!=0;
        else {uint mapped;const bool exists=mapped_primitive(mirrors[h],mapped);mirrors[h]=mapped;valid=valid && exists;}
    }
    if(kind==1) {uint mapped;const bool exists=mapped_primitive(terminal,mapped);terminal=mapped;valid=valid && exists;}
    else {
        valid=valid && terminal==0xffffffffU;feature=accepted_direction(feature);valid=valid && feature_valid(feature,kind);
    }
    // An invalid primary forces whole-query refusal in the shared traversal
    // and support stages; never expose a partially mapped usable path.
    queries.Store(outAt,valid?matchedPrimary:0xffffffffU);queries.Store4(outAt+4,mirrors);
    queries.Store2(outAt+20,uint2(control,terminal));queries.Store3(outAt+28,asuint(feature));
    queries.Store2(outAt+40,uint2(0,lobe));
}
