struct Fullscreen {float4 position:SV_Position;};
cbuffer Settings:register(b0,space3) {uint width,height,count,index;};
#if defined(STARFOX_MSAA_EXTRACT)
Texture2DMS<float4> color:register(t0,space2);
Texture2DMS<float4> receiver:register(t1,space2);
Texture2DMS<float4> surfaces:register(t2,space2);
Texture2DMS<float4> motion:register(t3,space2);
struct Sample {float4 color:SV_Target0;float4 receiver:SV_Target1;};
struct SurfaceSample {float4 color:SV_Target0;float4 receiver:SV_Target1;float4 surface:SV_Target2;};
struct MotionSample {float4 color:SV_Target0;float4 receiver:SV_Target1;float4 surface:SV_Target2;float4 motion:SV_Target3;};
Sample msaa_extract_fragment_main(Fullscreen input) {
    uint2 p=uint2(input.position.xy);Sample output;
    output.color=color.Load(p,index);output.receiver=receiver.Load(p,index);return output;
}
SurfaceSample msaa_extract_surface_fragment_main(Fullscreen input) {
    uint2 p=uint2(input.position.xy);SurfaceSample output;
    output.color=color.Load(p,index);output.receiver=receiver.Load(p,index);output.surface=surfaces.Load(p,index);return output;
}
MotionSample msaa_extract_motion_fragment_main(Fullscreen input) {
    uint2 p=uint2(input.position.xy);MotionSample output;
    output.color=color.Load(p,index);output.receiver=receiver.Load(p,index);
    output.surface=surfaces.Load(p,index);output.motion=motion.Load(p,index);return output;
}
#else
Texture2D<float4> sample0:register(t0,space2);
Texture2D<float4> sample1:register(t1,space2);
Texture2D<float4> sample2:register(t2,space2);
Texture2D<float4> sample3:register(t3,space2);
Texture2D<float4> sample4:register(t4,space2);
Texture2D<float4> sample5:register(t5,space2);
Texture2D<float4> sample6:register(t6,space2);
Texture2D<float4> sample7:register(t7,space2);
#if defined(STARFOX_MSAA_NATIVE_INK)
Texture2D<float4> nativeInk:register(t8,space2);
Texture2D<float4> nativeOwnership:register(t9,space2);
#endif
float4 msaa_resolve_fragment_main(Fullscreen input):SV_Target0 {
    int3 p=int3(input.position.xy,0);
#if defined(STARFOX_MSAA_NATIVE_INK)
    // Native centre ownership, never an averaged label or colour heuristic.
    if(nativeOwnership.Load(p).b==0) return nativeInk.Load(p);
#endif
    float4 value=sample0.Load(p)+sample1.Load(p);
    if(count>=4) value+=sample2.Load(p)+sample3.Load(p);
    if(count==8) value+=sample4.Load(p)+sample5.Load(p)+sample6.Load(p)+sample7.Load(p);
    return value/float(count);
}
#endif
