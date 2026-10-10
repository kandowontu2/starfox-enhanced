// Certified per-lobe incident colour, not a displayed/partially composed pixel.
// Fresh owner-owned CURRENT, immutable accepted source and SAME-batch proofs.
#include "reflection_source_index_settings.hlsli"
#include "calibrated_colour.hlsli"
ByteAddressBuffer current:register(t0,space0),source:register(t1,space0),queries:register(t2,space0);
RWByteAddressBuffer results:register(u0,space1);
cbuffer ColourSettings:register(b1,space2) {
    uint currentWidth,currentHeight,currentPrefix,currentRecords;
    uint currentTriangles,colourFirst,colourStride,colourOffset;
    uint colourBytes,colourFlags,colourCount,colourReserved;
    float colourWeight;uint colourLobes,colourPathStride,colourPad;
};
struct ColourPath {uint4 mirrors;uint control,terminal,rgba;float3 feature,response,base;};
ColourPath colour_path(ByteAddressBuffer image,uint at) {
    ColourPath p;p.mirrors=image.Load4(at);p.control=image.Load(at+16);p.terminal=image.Load(at+20);
    p.feature=asfloat(image.Load3(at+24));p.rgba=image.Load(at+36);p.response=asfloat(image.Load3(at+40));
    p.base=pathStride==64?asfloat(image.Load3(at+52)):0;return p;
}
bool colour_same(ColourPath a,ColourPath b) {
    return all(a.mirrors==b.mirrors) && a.control==b.control && a.terminal==b.terminal;
}
bool colour_current_path(ColourPath p) {
    const uint hops=p.control&7U,kind=p.control>>8,mask=pathStride==64?(p.control>>4)&15U:0;
    if(hops>4 || kind<1 || kind>3 || p.control!=((kind<<8)|(mask<<4)|hops)
        || (mask>>hops)!=0 || (kind==3?hops!=4:hops==4) || p.rgba>>24!=255
        || !all(isfinite(p.feature)) || !all(isfinite(p.response)) || !all(isfinite(p.base))
        || any(p.response<0) || any(p.response>1) || any(p.base<0) || any(p.base>1.e12))return false;
    [unroll] for(uint h=0;h<4;++h) {
        if(h>=hops) {if(p.mirrors[h]!=0xffffffffU)return false;}
        else if(mask&(1U<<h)) {if(p.mirrors[h]!=0xfffffffdU)return false;}
        else if(p.mirrors[h]>=currentTriangles
            && !(pathStride==52 && (colourFlags&2U) && p.mirrors[h]==0xfffffffeU))return false;
    }
    if(kind==1)return p.terminal<currentTriangles && p.feature.z==0 && all(p.feature.xy>=0) && dot(p.feature.xy,1.xx)<=1;
    return p.terminal==0xffffffffU && abs(dot(p.feature,p.feature)-1)<.0001;
}
float3 colour_linear(uint rgba) {
    const float3 encoded=float3(rgba&255U,(rgba>>8)&255U,(rgba>>16)&255U)/255.;
    return (colourFlags&1U)?calibrated_decode_srgb(encoded):encoded;
}
bool colour_receiver(uint p,out uint primary) {
    const uint count=currentWidth*currentHeight;primary=current.Load(count*currentPrefix+p*4);
    if(primary==0xffffffffU)return false; // Protected/ineligible fresh capture.
    const uint kind=current.Load(p*4)>>24;
    const bool curved=(colourFlags&4U)!=0,scene=(colourFlags&2U)!=0;
    const bool floor=curved?primary==0xfffffffdU:scene && primary==0xfffffffeU;
    if(floor?kind!=(curved && !(colourFlags&8U)?253U:254U):primary>=currentTriangles || kind!=255U)return false;
    const float depth=asfloat(current.Load(count*(currentPrefix+4)+p*4));
    const float4 response=asfloat(current.Load4(count*(currentPrefix+8)+p*16));
    if(!isfinite(depth) || depth<=0 || response.w!=1 || !all(isfinite(response))
        || any(response.rgb<0) || any(response.rgb>1) || !any(response.rgb>0))return false;
    if(curved || scene) {
        const float4 base=asfloat(current.Load4(count*(currentPrefix+24)+p*16));
        if(base.w!=1 || !all(isfinite(base)) || any(base.rgb<0) || any(base.rgb>1.e12))return false;
    }
    return true;
}
void colour_refuse(uint at,uint reason) {results.Store4(at,uint4(0,reason,0,0));}
[numthreads(64,1,1)]
void feature_colour_main(uint3 id:SV_DispatchThreadID) {
    const uint q=id.x;if(q>=queryCount || q>=64)return;
    const uint root=q*24576,at=root+384;
    [unroll] for(uint n=0;n<6;++n)results.Store4(at+n*16,0);
    if(queryCount>64 || colourCount!=queryCount || colourStride!=24576 || colourOffset!=384 || colourBytes!=96
        || colourReserved || colourPad || colourFlags>15 || colourLobes!=lobes || colourPathStride!=pathStride
        || (pathStride!=52 && pathStride!=64) || (lobes!=1 && lobes!=8)
        || !width || !height || !currentWidth || !currentHeight
        || width>16384 || height>16384 || currentWidth>16384 || currentHeight>16384
        || !isfinite(colourWeight) || colourWeight<0 || colourWeight>.95
        || (colourFlags&2U && (pathStride!=52 || currentPrefix!=4 || currentRecords!=44))
        || (colourFlags&4U && (pathStride!=64 || currentRecords!=currentPrefix+40))
        || (!(colourFlags&6U) && (currentPrefix!=4 || currentRecords!=28)))
        {colour_refuse(at,2);return;}
    // ALL predecessor proofs, not progress bits or a guide alone. No old RGB is
    // read before these SAME-batch immutable certificates authorize its source.
    if(results.Load(root)!=1 || results.Load(root+4)!=1 || results.Load(root+12)
        || any(results.Load4(root+192)!=uint4(1,31,0,results.Load(root+204)))
        || !results.Load(root+204) || results.Load(root+256)!=1 || results.Load(root+260)
        || any(results.Load4(root+320)!=uint4(1,0,0,0))) {colour_refuse(at,1);return;}
    const uint record=colourFirst+q,pixel=record/lobes,lobe=record%lobes;
    if(record<colourFirst || pixel>=currentWidth*currentHeight || queries.Load(q*48+44)!=lobe)
        {colour_refuse(at,2);return;}
    uint primary;if(!colour_receiver(pixel,primary)) {colour_refuse(at,3);return;}
    const ColourPath here=colour_path(current,currentWidth*currentHeight*currentRecords+record*pathStride);
    if(!colour_current_path(here) || here.control!=queries.Load(q*48+20)) {colour_refuse(at,3);return;}
    const float4 guide=asfloat(results.Load4(root+352)),enclosure=asfloat(results.Load4(root+336));
    if(guide.w!=1 || !all(isfinite(guide)) || !all(isfinite(enclosure)) || any(guide.xy<enclosure.xy)
        || any(guide.xy>enclosure.zw) || any(guide.xy<.5) || any(guide.xy>float2(width,height)-.5))
        {colour_refuse(at,2);return;}
    const float3 incoming=colour_linear(here.rgba);float3 low=incoming,high=incoming;
    uint neighbours=0;const int2 centre=int2(pixel%currentWidth,pixel/currentWidth);
    [unroll] for(int dy=-1;dy<=1;++dy)[unroll] for(int dx=-1;dx<=1;++dx) {
        const int2 xy=centre+int2(dx,dy);if(any(xy<0) || any(xy>=int2(currentWidth,currentHeight)))continue;
        const uint p=uint(xy.y)*currentWidth+uint(xy.x);uint neighbourPrimary;
        if(!colour_receiver(p,neighbourPrimary) || neighbourPrimary!=primary)continue;
        const ColourPath neighbour=colour_path(current,currentWidth*currentHeight*currentRecords+(p*lobes+lobe)*pathStride);
        if(!colour_current_path(neighbour) || !colour_same(neighbour,here))continue;
        const float3 value=colour_linear(neighbour.rgba);if(!all(isfinite(value))) {colour_refuse(at,6);return;}
        low=min(low,value);high=max(high,value);++neighbours;
    }
    if(!neighbours || !all(isfinite(incoming))) {colour_refuse(at,6);return;}
    const float2 coordinate=guide.xy-.5;const int2 first=int2(floor(coordinate));
    const float2 f=coordinate-float2(first);float3 prior=0;float sum=0;
    const uint oldPrimary=queries.Load(q*48),control=queries.Load(q*48+20),terminal=queries.Load(q*48+24);
    const uint4 mirrors=queries.Load4(q*48+4);
    [unroll] for(uint dy=0;dy<2;++dy)[unroll] for(uint dx=0;dx<2;++dx) {
        const float share=(dx?f.x:1-f.x)*(dy?f.y:1-f.y);if(share<=0)continue;
        const int2 xy=min(first+int2(dx,dy),int2(width-1,height-1));
        if(any(xy<0)) {colour_refuse(at,4);return;}
        const uint p=uint(xy.y)*width+uint(xy.x),oldAt=width*height*recordPrefix+(p*lobes+lobe)*pathStride;
        if(source.Load(width*height*primaryPrefix+p*4)!=oldPrimary || any(source.Load4(oldAt)!=mirrors)
            || source.Load(oldAt+16)!=control || source.Load(oldAt+20)!=terminal || source.Load(oldAt+36)>>24!=255)
            {colour_refuse(at,4);return;}
        const float3 rgb=colour_linear(source.Load(oldAt+36));
        if(!all(isfinite(rgb))) {colour_refuse(at,5);return;}
        prior+=rgb*share;sum+=share;
    }
    if(!all(isfinite(prior)) || !isfinite(sum) || abs(sum-1)>.00001) {colour_refuse(at,5);return;}
    const float3 admitted=lerp(incoming,clamp(prior,low,high),colourWeight);
    if(!all(isfinite(admitted))) {colour_refuse(at,5);return;}
    results.Store4(at+16,asuint(float4(incoming,colourWeight)));results.Store4(at+32,asuint(float4(low,0)));
    results.Store4(at+48,asuint(float4(high,0)));results.Store4(at+64,asuint(float4(prior,0)));
    results.Store4(at+80,asuint(float4(admitted,0)));results.Store4(at,uint4(1,0,neighbours,0));
    // CURRENT per-lobe base/throughput/primary response and canonical pixel
    // remain untouched. Whole-pixel/all-lobe recomposition is a separate gate.
}
