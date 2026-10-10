// Existing accepted-source cell/ring/five-half-point fold guards, applied to
// every possible source cell of the native certified root enclosure. No root
// midpoint, authored CPU guide, old incident RGB or colour permission.
#include "reflection_curved_path_motion.hlsli"
#define STARFOX_SOURCE_WITNESS_LIBRARY 1
#include "reflection_source_witness.hlsl"

#if defined(STARFOX_FOLD_HELPER_NOINLINE)
#define SOURCE_FOLD_BOUNDARY [noinline]
#else
#define SOURCE_FOLD_BOUNDARY
#endif
SOURCE_FOLD_BOUNDARY bool fold_classify(float3 features[4],uint kind,inout bool positive,inout bool negative) {
    const Interval3 dx0=vsub(p3(features[1]),p3(features[0])),dx1=vsub(p3(features[3]),p3(features[2]));
    const Interval3 dy0=vsub(p3(features[2]),p3(features[0])),dy1=vsub(p3(features[3]),p3(features[1]));
    [unroll] for(uint tap=0;tap<4;++tap) {
        Interval3 dx=dx0,dy=dy0;if(tap&2U)dx=dx1;if(tap&1U)dy=dy1;
        Interval orientation;
        if(kind==1)orientation=isub(imul(dx.x,dy.y),imul(dx.y,dy.x));
        else orientation=vdot(p3(features[tap]),vcross(dx,dy));
        // A possibly conflicting sign is a refusal, never rounded into safety.
        // Keep the existing 1e-20 threshold and allow its original neutral case.
        if(intervalFailed || !isfinite(orientation.lo) || !isfinite(orientation.hi))return false;
        positive=positive || orientation.hi>1.e-20;
        negative=negative || orientation.lo<-1.e-20;
    }
    return !(positive && negative);
}
bool fold_cell(int2 cell,WitnessQuery query,out float3 features[4]) {
    bool valid=true;
    [unroll] for(uint tap=0;tap<4;++tap) {
        const WitnessTap old=witness_tap(cell+int2(tap&1U,tap>>1),query);
        valid=valid && old.same;features[tap]=old.feature;
    }
    // As in the original ring guard, depth of noncontributing neighbours does
    // not create evidence; ownership/complete path/feature validity do.
    return valid;
}
SOURCE_FOLD_BOUNDARY bool fold_inner(float2 sample,ReflectionRoughFrame receiver,ReflectionLiquidFrame liquid,
    ReflectionSpecularPlane planes[4],uint4 control,ReflectionSpecularPlane terminal,out float3 feature) {
    feature=0;CurvedVector origin,outgoing;float2 depth,bias;
    if(!reflection_curved_forward_precise(sample,receiver,liquid,planes,control.x,control.y,control.z,true,
        origin,outgoing,depth,bias))return false;
    if(control.w!=1)feature=curved_vec_result(outgoing);
    else {
        if(!reflection_specular_plane_valid(terminal))return false;
        const CurvedVector a=curved_vec_float(terminal.a.xyz),u=curved_vec_sub(curved_vec_float(terminal.b.xyz),a),
            v=curved_vec_sub(curved_vec_float(terminal.c.xyz),a),normal=curved_vec_unit(curved_vec_cross(u,v));
        const float2 den=curved_vec_dot(outgoing,normal);
        if(!curved_dd_less(float2(1.e-12,0),curved_dd_abs(den)))return false;
        const float2 distance=curved_dd_div(curved_vec_dot(curved_vec_sub(a,origin),normal),den);
        if(!curved_dd_less(bias,distance) || !curved_dd_less(distance,float2(65536,0)))return false;
        const CurvedVector hit=curved_vec_add(origin,curved_vec_scale(outgoing,distance));
        if(!reflection_curved_inside(terminal,hit))return false;
        const CurvedVector offset=curved_vec_sub(hit,a);
        const float2 aa=curved_vec_dot(u,u),ab=curved_vec_dot(u,v),bb=curved_vec_dot(v,v),
            ra=curved_vec_dot(offset,u),rb=curved_vec_dot(offset,v);
        const float2 det=curved_dd_sub(curved_dd_mul(aa,bb),curved_dd_mul(ab,ab));
        if(!curved_dd_less(float2(1.e-20,0),det))return false;
        feature=float3(curved_dd_float(curved_dd_div(curved_dd_sub(curved_dd_mul(bb,ra),curved_dd_mul(ab,rb)),det)),
            curved_dd_float(curved_dd_div(curved_dd_sub(curved_dd_mul(aa,rb),curved_dd_mul(ab,ra)),det)),0);
    }
    return all(isfinite(feature));
}
void fold_refuse(uint at,uint reason,uint cells,uint traces) {results.Store4(at,uint4(0,intervalFailed?8:reason,cells,traces));}
[numthreads(64,1,1)]
void feature_folds_main(uint3 id:SV_DispatchThreadID) {
    const uint q=id.x;if(q>=queryCount || q>=64)return;
    const uint rootAt=q*24576,at=rootAt+256,frameAt=q*512;
    [unroll] for(uint n=0;n<4;++n)results.Store4(at+n*16,0);
    intervalFailed=false;
    if(queryCount>64 || witnessStride!=24576 || witnessOffset!=256 || witnessBytes!=64 || witnessReserved
        || (pathStride!=52 && pathStride!=64) || !width || !height || width>16384 || height>16384
        || (lobes!=1 && lobes!=8) || any(frames.Load4(frameAt+496)!=uint4(1,0,0,0)))
        {fold_refuse(at,8,0,0);return;}
    if(results.Load(rootAt)!=1 || results.Load(rootAt+4)!=1 || results.Load(rootAt+12)
        || results.Load(rootAt+192)!=1 || results.Load(rootAt+196)!=31 || results.Load(rootAt+200)
        || any(results.Load4(rootAt+144)!=results.Load4(rootAt+208)))
        {fold_refuse(at,1,0,0);return;}
    const float4 root=asfloat(results.Load4(rootAt+144));results.Store4(at+16,asuint(root));
    if(!all(isfinite(root)) || any(root.xy>root.zw) || any(root.xy<.5) || any(root.zw>float2(width,height)-.5))
        {fold_refuse(at,8,0,0);return;}
    WitnessQuery query;query.primary=queries.Load(q*48);query.mirrors=queries.Load4(q*48+4);
    query.control=queries.Load(q*48+20);query.terminal=queries.Load(q*48+24);query.lobe=queries.Load(q*48+44);
    const uint4 control=frames.Load4(frameAt+480);
    if(query.primary==0xffffffffU || query.lobe>=lobes || control.x>4 || (control.y>>control.x)!=0
        || control.z>1 || control.w<1 || control.w>3 || query.control!=((control.w<<8)|(control.y<<4)|control.x)
        || control.z!=(query.primary==0xfffffffdU?1U:0U)) {fold_refuse(at,8,0,0);return;}
    ReflectionRoughFrame receiver;receiver.a=asfloat(frames.Load4(frameAt));receiver.b=asfloat(frames.Load4(frameAt+16));
    receiver.c=asfloat(frames.Load4(frameAt+32));receiver.projection=asfloat(frames.Load4(frameAt+48));
    receiver.extentClip=asfloat(frames.Load4(frameAt+64));receiver.settings=asfloat(frames.Load4(frameAt+80));
    ReflectionSpecularPlane planes[4];
    [unroll] for(uint hop=0;hop<4;++hop) {
        planes[hop].a=asfloat(frames.Load4(frameAt+96+hop*48));planes[hop].b=asfloat(frames.Load4(frameAt+112+hop*48));
        planes[hop].c=asfloat(frames.Load4(frameAt+128+hop*48));
    }
    ReflectionLiquidFrame liquid;liquid.planePoint=asfloat(frames.Load4(frameAt+288));liquid.planeNormal=asfloat(frames.Load4(frameAt+304));
    liquid.rotation0=asfloat(frames.Load4(frameAt+320));liquid.rotation1=asfloat(frames.Load4(frameAt+336));
    liquid.rotation2=asfloat(frames.Load4(frameAt+352));liquid.projection=asfloat(frames.Load4(frameAt+368));
    liquid.extentClip=asfloat(frames.Load4(frameAt+384));liquid.settings=asfloat(frames.Load4(frameAt+400));
    if(!control.z && !control.y) {
        // The shared forward library validates a liquid frame even for a path
        // with NO liquid receiver/hop. This unused valid frame supplies no
        // optical surface and is never sampled. Real curved frames stay exact.
        liquid.planePoint=0;liquid.planeNormal=float4(0,1,0,0);
        liquid.rotation0=float4(1,0,0,0);liquid.rotation1=float4(0,1,0,0);liquid.rotation2=float4(0,0,1,0);
        liquid.projection=receiver.projection;liquid.extentClip=receiver.extentClip;liquid.settings=0;
    }
    if(control.z || reflection_rough_analytic(receiver))receiver.settings=0;
    ReflectionSpecularPlane terminal;terminal.a=asfloat(frames.Load4(frameAt+432));
    terminal.b=asfloat(frames.Load4(frameAt+448));terminal.c=asfloat(frames.Load4(frameAt+464));
    const Interval x=isub(iv(root.x,root.z),ip(.5)),y=isub(iv(root.y,root.w),ip(.5));
    const int2 first=int2(floor(float2(x.lo,y.lo))),last=int2(floor(float2(x.hi,y.hi)));
    if(intervalFailed || any(first<0) || any(last>=int2(width,height)) || any(last-first>2))
        {fold_refuse(at,7,0,0);return;}
    uint cells=0,traces=0;
    [loop] for(int cy=first.y;cy<=last.y;++cy)[loop] for(int cx=first.x;cx<=last.x;++cx) {
        // The original fold guard runs only at fractional two-axis samples
        // inside a complete cell. Check every such possible cell, not a guide.
        const float xmax=min(x.hi,float(cx+1)),ymax=min(y.hi,float(cy+1));
        const float xmin=max(x.lo,float(cx)),ymin=max(y.lo,float(cy));
        if(xmax<=float(cx) || xmin>=float(cx+1) || ymax<=float(cy) || ymin>=float(cy+1)
            || cx+1>=int(width) || cy+1>=int(height))continue;
        const int2 cell=int2(cx,cy);float3 corners[4];bool positive=false,negative=false;
        if(!fold_cell(cell,query,corners)) {fold_refuse(at,2,cells,traces);return;}
        if(!fold_classify(corners,control.w,positive,negative)) {fold_refuse(at,3,cells,traces);return;}
        [loop] for(int ry=-1;ry<=1;++ry)[loop] for(int rx=-1;rx<=1;++rx) {
            if(!rx && !ry)continue;float3 ring[4];
            if(fold_cell(cell+int2(rx,ry),query,ring) && !fold_classify(ring,control.w,positive,negative))
                {fold_refuse(at,4,cells,traces);return;}
        }
        const uint2 locations[5]={uint2(1,0),uint2(0,1),uint2(1,1),uint2(2,1),uint2(1,2)};float3 inner[5];
        [loop] for(uint i=0;i<5;++i) {
            ++traces;float3 traced;
            if(!fold_inner(float2(cell)+.5+float2(locations[i])*.5,receiver,liquid,planes,control,terminal,traced))
                {fold_refuse(at,5,cells,traces);return;}
            inner[i]=traced;
        }
        const float3 grid[9]={corners[0],inner[0],corners[1],inner[1],inner[2],inner[3],corners[2],inner[4],corners[3]};
        [unroll] for(uint gy=0;gy<2;++gy)[unroll] for(uint gx=0;gx<2;++gx) {
            const uint index=gy*3+gx;float3 quarter[4]={grid[index],grid[index+1],grid[index+3],grid[index+4]};
            if(!fold_classify(quarter,control.w,positive,negative)) {fold_refuse(at,6,cells,traces);return;}
        }
        ++cells;
    }
    if(intervalFailed) {fold_refuse(at,8,cells,traces);return;}
    results.Store4(at,uint4(1,0,cells,traces));
    // Source footprint + fold guard only. Angular, current-colour clamp,
    // producer/consumer and practical cost qualification are still required.
}
