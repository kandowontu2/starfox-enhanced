// Complete CURRENT-material pixel recomposition, staged only. Never write a
// displayed/native bank or let one batch pollute later CURRENT neighbours.
#include "reflection_source_index_settings.hlsli"
#include "calibrated_colour.hlsli"
ByteAddressBuffer current:register(t0,space0),queries:register(t1,space0);
RWByteAddressBuffer results:register(u0,space1);
cbuffer ComposeSettings:register(b1,space2) {
    uint currentWidth,currentHeight,currentPrefix,currentRecords;
    uint currentTriangles,composeFirst,composeStride,composeOffset;
    uint composeBytes,composeFlags,composeCount,composeReserved;
    float composeWeight;uint composeLobes,composePathStride,composePad;
};
float3 compose_linear(uint rgba) {
    const float3 encoded=float3(rgba&255U,(rgba>>8)&255U,(rgba>>16)&255U)/255.;
    return (composeFlags&1U)?calibrated_decode_srgb(encoded):encoded;
}
uint compose_pack(float3 rgb,uint alpha) {
    rgb=(composeFlags&1U)?calibrated_encode_srgb(saturate(rgb)):saturate(rgb);
    const uint3 bytes=uint3(floor(rgb*255.+.5));return bytes.x|(bytes.y<<8)|(bytes.z<<16)|alpha;
}
void compose_status(uint first,uint count,uint valid,uint reason,uint pixel,uint mask) {
    [loop] for(uint l=0;l<count;++l)results.Store4((first+l)*24576+512,uint4(valid,reason,pixel,mask));
}
[numthreads(64,1,1)]
void feature_compose_main(uint3 id:SV_DispatchThreadID) {
    // One thread owns a complete pixel. No atomic half-pixel/lobe publication.
    if(composeLobes!=1 && composeLobes!=8)return;
    const uint first=id.x*composeLobes;if(first>=composeCount || first>=64)return;
    const uint count=min(composeLobes,min(composeCount-first,64-first));
    [loop] for(uint l=0;l<count;++l) {
        const uint at=(first+l)*24576+512;[unroll] for(uint n=0;n<4;++n)results.Store4(at+n*16,0);
    }
    if(composeCount!=queryCount || composeCount>64 || count!=composeLobes || composeCount%composeLobes
        || composeFirst%composeLobes || composeStride!=24576 || composeOffset!=512 || composeBytes!=64
        || composeReserved || composePad || composeFlags>15 || composeLobes!=lobes || composePathStride!=pathStride
        || (pathStride!=52 && pathStride!=64) || !currentWidth || !currentHeight
        || currentWidth>16384 || currentHeight>16384 || !isfinite(composeWeight) || composeWeight<=0 || composeWeight>.95
        || (composeFlags&2U && (pathStride!=52 || currentPrefix!=4 || currentRecords!=44))
        || (composeFlags&4U && (pathStride!=64 || currentRecords!=currentPrefix+40))
        || (!(composeFlags&6U) && (currentPrefix!=4 || currentRecords!=28)))
        {compose_status(first,count,0,2,0xffffffffU,0);return;}
    const uint record=composeFirst+first,pixel=record/composeLobes,total=currentWidth*currentHeight;
    if(record<composeFirst || pixel>=total) {compose_status(first,count,0,2,0xffffffffU,0);return;}
    const uint word=current.Load(pixel*4),primary=current.Load(total*currentPrefix+pixel*4);
    // Refusal payload is exact CURRENT, including its original alpha/type.
    [loop] for(uint l=0;l<count;++l) {
        const uint path=total*currentRecords+(record+l)*pathStride,at=(first+l)*24576+512;
        results.Store4(at+16,uint4(word,current.Load(path+36),0,0));
    }
    const bool curved=(composeFlags&4U)!=0,scene=(composeFlags&2U)!=0;
    const bool floor=curved?primary==0xfffffffdU:scene && primary==0xfffffffeU;
    const float4 response=asfloat(current.Load4(total*(currentPrefix+8)+pixel*16));
    float4 base=float4(0,0,0,1);if(curved || scene)base=asfloat(current.Load4(total*(currentPrefix+24)+pixel*16));
    if(primary==0xffffffffU || (floor?word>>24!=(curved && !(composeFlags&8U)?253U:254U):primary>=currentTriangles || word>>24!=255U)
        || response.w!=1 || !all(isfinite(response)) || any(response.rgb<0) || any(response.rgb>1)
        || base.w!=1 || !all(isfinite(base)) || any(base.rgb<0) || any(base.rgb>1.e12))
        {compose_status(first,count,0,3,pixel,0);return;}
    uint incomingWords[8],transportWords[8];float3 sum=0;uint changed=0;
    [loop] for(uint l=0;l<count;++l) {
        const uint root=(first+l)*24576,path=total*currentRecords+(record+l)*pathStride;
        const uint rgba=current.Load(path+36),control=current.Load(path+16);
        const float3 throughput=asfloat(current.Load3(path+40));
        float3 lobeBase=0;if(pathStride==64)lobeBase=asfloat(current.Load3(path+52));
        if(!all(isfinite(throughput)) || any(throughput<0) || any(throughput>1)
            || !all(isfinite(lobeBase)) || any(lobeBase<0) || any(lobeBase>1.e12))
            {compose_status(first,count,0,3,pixel,0);return;}
        float3 incoming=compose_linear(rgba);incomingWords[l]=rgba;
        // A refused lobe keeps freshly traced CURRENT, but all material
        // responses/base contributions still participate in the full sum.
        // Old colour is authorized ONLY by ALL SAME-batch predecessor proofs.
        const uint4 colour=results.Load4(root+384);
        const bool certified=colour.x==1 && colour.y==0 && colour.z>0 && colour.z<=9 && colour.w==0
            && results.Load(root)==1 && results.Load(root+4)==1 && !results.Load(root+12)
            && all(results.Load4(root+192)==uint4(1,31,0,results.Load(root+204))) && results.Load(root+204)>0
            && results.Load(root+256)==1 && !results.Load(root+260) && all(results.Load4(root+320)==uint4(1,0,0,0));
        if(certified) {
            const float4 admitted=asfloat(results.Load4(root+464));
            const float weight=asfloat(results.Load(root+412));
            if(queries.Load((first+l)*48+44)!=l || queries.Load((first+l)*48+20)!=control
                || admitted.w!=0 || !all(isfinite(admitted)) || any(admitted.rgb<0) || any(admitted.rgb>1)
                || weight!=composeWeight || rgba>>24!=255)
                {compose_status(first,count,0,4,pixel,0);return;}
            incoming=admitted.rgb;incomingWords[l]=compose_pack(incoming,rgba&0xff000000U);changed|=1U<<l;
        }
        const float3 transported=incoming*throughput+lobeBase;
        if(!all(isfinite(transported))) {compose_status(first,count,0,4,pixel,0);return;}
        // Native RT quantizes transported lobes BEFORE averaging. Incident
        // packing above does not replace the unrounded incoming used here.
        transportWords[l]=compose_pack(transported,0xff000000U);sum+=compose_linear(transportWords[l]);
    }
    if(!changed) {compose_status(first,count,0,1,pixel,0);return;}
    const float3 composed=base.rgb+(sum/composeLobes)*response.rgb;
    if(!all(isfinite(composed))) {compose_status(first,count,0,4,pixel,0);return;}
    const uint canonical=compose_pack(composed,word&0xff000000U);
    [loop] for(uint l=0;l<count;++l) {
        const uint at=(first+l)*24576+512;
        results.Store4(at+16,uint4(canonical,incomingWords[l],transportWords[l],0));
        results.Store4(at+32,asuint(float4(composed,0)));
    }
    compose_status(first,count,1,0,pixel,changed);
    // Only a complete packet was written, never CURRENT/source/native pixels.
    // Actual frame publication must preserve this fresh bank for later batches.
}
