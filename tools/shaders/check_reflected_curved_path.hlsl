#include "../../src/render/shaders/reflection_curved_path_motion.hlsli"
#ifndef STARFOX_CURVED_WITNESS_PHASE
#define STARFOX_CURVED_WITNESS_PHASE 0
#endif
#ifndef STARFOX_CURVED_REFERENCE_WITNESS
#define STARFOX_CURVED_REFERENCE_WITNESS 0
#endif
ByteAddressBuffer cases:register(t0,space0);
RWByteAddressBuffer results:register(u0,space1);
cbuffer Settings:register(b0,space2) {uint count,first,batchEnd,reserved;};
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    id.x+=first;
    if(id.x>=count || id.x>=batchEnd) return;
    uint at=id.x*480;
    ReflectionRoughFrame receiver;
    receiver.a=asfloat(cases.Load4(at));receiver.b=asfloat(cases.Load4(at+16));receiver.c=asfloat(cases.Load4(at+32));
    receiver.projection=asfloat(cases.Load4(at+48));receiver.extentClip=asfloat(cases.Load4(at+64));
    receiver.settings=asfloat(cases.Load4(at+80));
    ReflectionSpecularPlane planes[4];
    [unroll] for(uint hop=0;hop<4;++hop) {
        planes[hop].a=asfloat(cases.Load4(at+96+hop*48));
        planes[hop].b=asfloat(cases.Load4(at+112+hop*48));
        planes[hop].c=asfloat(cases.Load4(at+128+hop*48));
    }
    ReflectionLiquidFrame liquid;
    liquid.planePoint=asfloat(cases.Load4(at+288));liquid.planeNormal=asfloat(cases.Load4(at+304));
    liquid.rotation0=asfloat(cases.Load4(at+320));liquid.rotation1=asfloat(cases.Load4(at+336));
    liquid.rotation2=asfloat(cases.Load4(at+352));liquid.projection=asfloat(cases.Load4(at+368));
    liquid.extentClip=asfloat(cases.Load4(at+384));liquid.settings=asfloat(cases.Load4(at+400));
    float4 terminal=asfloat(cases.Load4(at+416)),source=asfloat(cases.Load4(at+432));
    uint4 control=cases.Load4(at+464);
#if !STARFOX_CURVED_WITNESS_PHASE
    // Production-like guide work does not contain repeated diagnostic traces.
    // The next pass independently fills all existing checker witness outputs.
    float4 motion=reflection_curved_path_motion(receiver,liquid,planes,control.x,control.y,control.z,
        terminal,asfloat(cases.Load2(at+448)));
    results.Store4(id.x*224,asuint(motion));
#else
    float4 motion=asfloat(results.Load4(id.x*224));
#if STARFOX_CURVED_REFERENCE_WITNESS
    // Retain the preceding checker for explicit output equivalence tests.
    float3 origin,outgoing;float depth,bias;
    bool valid=reflection_curved_forward(source.xy,receiver,liquid,planes,control.x,control.y,control.z,true,
        origin,outgoing,depth,bias);
    results.Store4(id.x*224+16,asuint(valid?float4(origin,bias):float4(0,0,0,0)));
    results.Store4(id.x*224+32,asuint(valid?float4(outgoing,depth):float4(0,0,0,0)));
    float2 error;float unused;
    bool reconstructed=motion.w==1 && reflection_curved_error(motion.xy,receiver,liquid,planes,
        control.x,control.y,control.z,terminal,error,unused);
    results.Store4(id.x*224+48,asuint(reconstructed?float4(error,unused,1):float4(0,0,0,0)));
    [loop] for(uint h=0;h<=4;++h) {
        origin=outgoing=0;depth=bias=0;
        if(h<=control.x) reflection_curved_forward(source.xy,receiver,liquid,planes,h,
            control.y&((1U<<h)-1U),control.z,true,origin,outgoing,depth,bias);
        results.Store4(id.x*224+64+h*32,asuint(float4(origin,bias)));
        results.Store4(id.x*224+80+h*32,asuint(float4(outgoing,depth)));
    }
#else
    // The source, reconstructed residual and partial paths share ONE actual
    // wave-forward site. Some drivers inline the full compensated optics at
    // every call; separate diagnostic calls made witness PSO creation costly.
    // This changes neither the guide nor its search/gates, and retains all
    // 224 bytes, including partial outputs from an unsuccessful forward trace.
    [loop] for(uint phase=0;phase<7;++phase) {
        bool residual=phase==1;
        uint partial=phase>=2?phase-2:0;
        uint hops=phase>=2?partial:control.x;
        uint mask=phase>=2?control.y&((1U<<partial)-1U):control.y;
        bool evaluate=residual?motion.w==1:phase<2 || partial<=control.x;
        CurvedVector origin=curved_vec_float(0),outgoing=curved_vec_float(0);
        float2 depth=0,bias=0;
        bool valid=false;
        if(evaluate) valid=reflection_curved_forward_precise(residual?motion.xy:source.xy,
            receiver,liquid,planes,hops,mask,control.z,!residual,origin,outgoing,depth,bias);
        if(phase==0) {
            results.Store4(id.x*224+16,asuint(valid?float4(curved_vec_result(origin),curved_dd_float(bias)):float4(0,0,0,0)));
            results.Store4(id.x*224+32,asuint(valid?float4(curved_vec_result(outgoing),curved_dd_float(depth)):float4(0,0,0,0)));
        } else if(residual) {
            float2 error=0;
            if(valid) {
                // Exact residual equations from reflection_curved_error; use
                // the precise forward ray, not its rounded float3 witness.
                CurvedVector travel=curved_vec_float(terminal.xyz);
                if(terminal.w!=0) travel=curved_vec_sub(travel,origin);
                float2 length2=curved_vec_dot(travel,travel);
                valid=all(isfinite(length2)) && curved_dd_less(terminal.w==0?float2(1.e-20,0):curved_dd_mul(bias,bias),length2);
                if(valid) {
                    travel=curved_vec_unit(travel);
                    valid=curved_dd_less(0,curved_vec_dot(travel,outgoing));
                    if(valid) {
                        CurvedVector tangent=curved_vec_unit(curved_vec_cross(outgoing,
                            curved_vec_float(abs(curved_dd_float(outgoing.y))<.95?float3(0,1,0):float3(1,0,0))));
                        float2 focal=float2(max(receiver.projection.x,receiver.projection.y),0);
                        error=float2(curved_dd_float(curved_dd_mul(curved_vec_dot(travel,tangent),focal)),
                            curved_dd_float(curved_dd_mul(curved_vec_dot(travel,curved_vec_cross(outgoing,tangent)),focal)));
                        valid=all(isfinite(error));
                    }
                }
            }
            results.Store4(id.x*224+48,asuint(valid?float4(error,curved_dd_float(depth),1):float4(0,0,0,0)));
        } else {
            results.Store4(id.x*224+64+partial*32,asuint(float4(curved_vec_result(origin),curved_dd_float(bias))));
            results.Store4(id.x*224+80+partial*32,asuint(float4(curved_vec_result(outgoing),curved_dd_float(depth))));
        }
    }
#endif
#endif
}
