// Source-footprint proof over a certified WHOLE-source root enclosure. Actual
// accepted GPU bank only. This is not a colour consumer or subcell-fold proof.
#include "reflection_source_index_settings.hlsli"
ByteAddressBuffer frames:register(t0,space0),queries:register(t1,space0),source:register(t2,space0);
RWByteAddressBuffer results:register(u0,space1);
cbuffer WitnessSettings:register(b1,space2) {uint witnessStride,witnessOffset,witnessBytes,witnessReserved;};
#include "reflection_source_optical_interval.hlsli"
#include "reflection_source_optical_target.hlsli"
struct WitnessQuery {uint primary;uint4 mirrors;uint control,terminal,lobe;};
struct WitnessTap {float depth;float3 feature;bool same,visible;};
bool witness_feature(float3 value,uint kind) {
    if(!all(isfinite(value)))return false;
    if(kind==1)return value.z==0 && all(value.xy>=0) && value.x+value.y<=1;
    // Actual source guard's unit-vector gate, NOT candidate feature_valid's
    // wider arithmetic enclosure. No source acceptance tolerance is enlarged.
    const Interval length=vdot(p3(value),p3(value));
    return !intervalFailed && (kind==2 || kind==3) && iabs(isub(length,ip(1))).hi<.0001;
}
WitnessTap witness_tap(int2 pixel,WitnessQuery q) {
    WitnessTap tap;tap.depth=0;tap.feature=0;tap.same=tap.visible=false;
    if(any(pixel<0) || any(pixel>=int2(width,height)))return tap;
    const uint p=uint(pixel.y)*width+uint(pixel.x),count=width*height;
    if(source.Load(count*primaryPrefix+p*4)!=q.primary)return tap;
    const uint at=count*recordPrefix+(p*lobes+q.lobe)*pathStride;
    if(any(source.Load4(at)!=q.mirrors) || source.Load(at+16)!=q.control || source.Load(at+20)!=q.terminal)return tap;
    tap.feature=asfloat(source.Load3(at+24));const float3 response=asfloat(source.Load3(at+40));
    if(!witness_feature(tap.feature,q.control>>8) || source.Load(at+36)>>24!=255
        || !all(isfinite(response)) || any(response<0) || any(response>1))return tap;
    if(pathStride==64) {const float3 base=asfloat(source.Load3(at+52));
        if(!all(isfinite(base)) || any(base<0) || any(base>1.e12))return tap;}
    tap.same=true;tap.depth=asfloat(source.Load(count*(primaryPrefix+4)+p*4));
    tap.visible=isfinite(tap.depth) && tap.depth>0;return tap;
}
Interval3 witness_component_max(Interval3 a,Interval3 b) {return i3(imax(a.x,b.x),imax(a.y,b.y),imax(a.z,b.z));}
Interval3 witness_absolute(Interval3 a) {return i3(iabs(a.x),iabs(a.y),iabs(a.z));}
Interval witness_select(bool pick,Interval a,Interval b) {if(pick)return a;return b;}
// Primary Z is exactly the nominal receiver-plane intersection, BEFORE liquid
// normals affect the outgoing path. The unnormalised plane ratio cancels the
// eye-ray length/orientation algebraically; this is NOT flat liquid optics.
void witness_plane(uint at,out Interval3 planePoint,out Interval3 normal) {
    const uint curved=frames.Load(at+488);
    if(curved) {planePoint=p3(asfloat(frames.Load4(at+288)).xyz);normal=p3(asfloat(frames.Load4(at+304)).xyz);}
    else {const float4 a=asfloat(frames.Load4(at)),b=asfloat(frames.Load4(at+16)),c=asfloat(frames.Load4(at+32));planePoint=p3(a.xyz);
        if(all(float3(a.w,b.w,c.w)==2))normal=p3(b.xyz);
        else normal=vcross(vsub(p3(b.xyz),planePoint),vsub(p3(c.xyz),planePoint));}
}
Interval witness_depth(uint at,float4 root) {
    const float4 projection=asfloat(frames.Load4(at+48));Interval3 planePoint,normal;witness_plane(at,planePoint,normal);
    const Interval3 direction=i3(idiv(isub(iv(root.x,root.z),ip(projection.z)),ip(projection.x)),
        idiv(isub(iv(root.y,root.w),ip(projection.w)),ip(projection.y)),ip(1));
    return idiv(vdot(planePoint,normal),vdot(direction,normal));
}
void witness_refuse(uint outAt,uint bits,uint reason,uint cells) {
    results.Store4(outAt,uint4(0,bits,intervalFailed?6:reason,cells));
}
#if !defined(STARFOX_SOURCE_WITNESS_LIBRARY)
[numthreads(64,1,1)]
void feature_witness_main(uint3 id:SV_DispatchThreadID) {
    const uint q=id.x;if(q>=queryCount || q>=64)return;
    const uint rootAt=q*24576,outAt=rootAt+192,frameAt=q*512;
    [unroll] for(uint n=0;n<4;++n)results.Store4(outAt+n*16,0);
    intervalFailed=false;
    if(queryCount>64 || witnessStride!=24576 || witnessOffset!=192 || witnessBytes!=64 || witnessReserved
        || (pathStride!=52 && pathStride!=64) || !width || !height || width>16384 || height>16384
        || (lobes!=1 && lobes!=8) || any(frames.Load4(frameAt+496)!=uint4(1,0,0,0)))
        {witness_refuse(outAt,0,8,0);return;}
    if(any(results.Load4(rootAt)!=uint4(1,1,results.Load(rootAt+8),0)))
        {witness_refuse(outAt,0,1,0);return;}
    const float4 root=asfloat(results.Load4(rootAt+144));results.Store4(outAt+16,asuint(root));
    if(!all(isfinite(root)) || any(root.xy>root.zw) || any(root.xy<.5) || any(root.zw>float2(width,height)-.5))
        {witness_refuse(outAt,1,2,0);return;}
    WitnessQuery query;query.primary=queries.Load(q*48);query.mirrors=queries.Load4(q*48+4);
    query.control=queries.Load(q*48+20);query.terminal=queries.Load(q*48+24);query.lobe=queries.Load(q*48+44);
    if(query.primary==0xffffffffU || query.lobe>=lobes || query.lobe>7)
        {witness_refuse(outAt,1,8,0);return;}
    const float4 feature=asfloat(frames.Load4(frameAt+416));
    Interval3 target;if(feature.w!=0)target=p3(feature.xyz);else target=native_target(frameAt,feature);
    const Interval opticalDepth=witness_depth(frameAt,root);const float4 extent=asfloat(frames.Load4(frameAt+64));
    if(intervalFailed || opticalDepth.lo<=0 || opticalDepth.lo<extent.z || opticalDepth.hi>extent.w)
        {witness_refuse(outAt,3,4,0);return;}
    results.Store2(outAt+32,asuint(float2(opticalDepth.lo,opticalDepth.hi)));
    // All possible source cells touched by the enclosure, including BOTH sides
    // of an integer/shared boundary. A bounded refusal is not colour permission.
    const Interval x=isub(iv(root.x,root.z),ip(.5)),y=isub(iv(root.y,root.w),ip(.5));
    const int2 first=int2(floor(float2(x.lo,y.lo))),last=int2(floor(float2(x.hi,y.hi)));
    if(any(first<0) || any(last>=int2(width,height))) {witness_refuse(outAt,3,2,0);return;}
    if(any(last-first>2)) {witness_refuse(outAt,3,7,0);return;}
    uint cells=0;
    [loop] for(int cy=first.y;cy<=last.y;++cy)[loop] for(int cx=first.x;cx<=last.x;++cx) {
        const Interval cellX=iv(max(x.lo,float(cx)),min(x.hi,float(cx+1))),cellY=iv(max(y.lo,float(cy)),min(y.hi,float(cy+1)));
        const Interval fx=iclamp(isub(cellX,ip(float(cx))),ip(0),ip(1)),fy=iclamp(isub(cellY,ip(float(cy))),ip(0),ip(1));
        Interval reciprocal=ip(0),depths[4];bool allVisible=true;
        [loop] for(uint dy=0;dy<2;++dy)[loop] for(uint dx=0;dx<2;++dx) {
            const uint corner=dy*2+dx;depths[corner]=ip(0);
            const int2 pixel=min(int2(cx,cy)+int2(dx,dy),int2(width-1,height-1));
            const WitnessTap tap=witness_tap(pixel,query);allVisible=allVisible && tap.visible;
            if(tap.visible)depths[corner]=idiv(ip(1),ip(tap.depth));
            const Interval share=imul(witness_select(dx!=0,fx,isub(ip(1),fx)),witness_select(dy!=0,fy,isub(ip(1),fy)));
            if(share.hi<=0)continue;
            if(!tap.visible) {witness_refuse(outAt,3,3,cells);return;}
            reciprocal=iadd(reciprocal,imul(share,depths[corner]));
            Interval3 footprint=p3(float3(2.e-6,2.e-6,2.e-6));
            [unroll] for(uint axis=0;axis<2;++axis) {
                Interval3 gradient=p3(0);
                [unroll] for(int side=-1;side<=1;side+=2) {
                    const WitnessTap neighbour=witness_tap(pixel+int2(axis==0?side:0,axis==1?side:0),query);
                    // As in the actual guard, noncontributing neighbour depth
                    // does NOT gate a matching path's feature gradient.
                    if(neighbour.same)gradient=witness_component_max(gradient,
                        vmul(witness_absolute(vsub(p3(neighbour.feature),p3(tap.feature))),ip(1.5)));
                }
                const Interval distance=iabs(isub(witness_select(axis==0,cellX,cellY),ip(float(pixel[axis]))));
                footprint=vadd(footprint,vmul(gradient,distance));
            }
            const Interval3 delta=witness_absolute(vsub(p3(tap.feature),target));
            const Interval cap=ip((query.control>>8)==1?.05:.02);
            if(intervalFailed || delta.x.hi>min(footprint.x.lo,cap.lo) || delta.y.hi>min(footprint.y.lo,cap.lo)
                || delta.z.hi>min(footprint.z.lo,cap.lo)) {witness_refuse(outAt,7,5,cells);return;}
        }
        if(allVisible) {
            // Horner form preserves correlation between the four bilinear
            // weights. All arithmetic is checked/outward, not approximate.
            const Interval low=iadd(depths[0],imul(fx,isub(depths[1],depths[0])));
            const Interval high=iadd(depths[2],imul(fx,isub(depths[3],depths[2])));
            reciprocal=iadd(low,imul(fy,isub(high,low)));
        }
        const Interval interpolated=idiv(ip(1),reciprocal);
        Interval difference=iabs(isub(interpolated,opticalDepth));
        if(allVisible) {
            // The primary plane's reciprocal Z is affine in the SAME cell
            // coordinates. Subtract its coefficients before enclosing the
            // polynomial: independent depth intervals lose that correlation.
            // This exact algebra changes no .01/.005 visibility threshold.
            Interval3 planePoint,normal;witness_plane(frameAt,planePoint,normal);const Interval numerator=vdot(planePoint,normal);
            const float4 projection=asfloat(frames.Load4(frameAt+48));
            const Interval3 direction=i3(idiv(isub(ip(float(cx)+.5),ip(projection.z)),ip(projection.x)),
                idiv(isub(ip(float(cy)+.5),ip(projection.w)),ip(projection.y)),ip(1));
            const Interval zero=idiv(vdot(direction,normal),numerator);
            const Interval stepX=idiv(normal.x,imul(numerator,ip(projection.x))),stepY=idiv(normal.y,imul(numerator,ip(projection.y)));
            const Interval d0=isub(depths[0],zero),dX=isub(isub(depths[1],depths[0]),stepX),dY=isub(isub(depths[2],depths[0]),stepY);
            const Interval dXY=iadd(isub(isub(depths[3],depths[1]),depths[2]),depths[0]);
            const Interval delta=iadd(iadd(d0,imul(fx,dX)),imul(fy,iadd(dY,imul(fx,dXY))));
            difference=idiv(iabs(delta),imul(reciprocal,idiv(ip(1),opticalDepth)));
        }
        const Interval tolerance=imax(ip(.01),imul(opticalDepth,ip(.005)));
        if(intervalFailed || reciprocal.lo<=0 || difference.hi>tolerance.lo)
            {witness_refuse(outAt,23,4,cells);return;}
        ++cells;
    }
    if(intervalFailed || !cells){witness_refuse(outAt,0,6,cells);return;}
    results.Store4(outAt,uint4(1,31,0,cells));
    // No old incident RGB evaluated/output, angular/source fold certificate or
    // current-neighbour colour clamp. These remain separate required gates.
}
#endif
