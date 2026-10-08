// Fixture-only dispatcher. The implementation under test is the production
// optical helper, not a duplicate shader formula or downloaded ray oracle.
ByteAddressBuffer cases : register(t0,space0);
RWByteAddressBuffer results : register(u0,space1);
cbuffer Settings : register(b0,space2) {uint count;uint3 reserved;};
static uint diagnosticCase;
// A trace exists only in this fixture shader, never in native gameplay.
void liquid_solve_trace(uint iteration,uint slot,float4 value) {
    results.Store4(diagnosticCase*832+64+(iteration*4+slot)*16,asuint(value));
}
#define STARFOX_LIQUID_SOLVE_TRACE(iteration,slot,value) liquid_solve_trace(iteration,slot,value)
#include "../../src/render/shaders/reflection_liquid_hit_motion.hlsli"
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    diagnosticCase=id.x;
    for(uint trace=0;trace<48;++trace) results.Store4(id.x*832+64+trace*16,uint4(0,0,0,0));
    uint at=id.x*208;
    ReflectionLiquidFrame old;
    old.planePoint=asfloat(cases.Load4(at));old.planeNormal=asfloat(cases.Load4(at+16));
    old.rotation0=asfloat(cases.Load4(at+32));old.rotation1=asfloat(cases.Load4(at+48));old.rotation2=asfloat(cases.Load4(at+64));
    old.projection=asfloat(cases.Load4(at+80));old.extentClip=asfloat(cases.Load4(at+96));old.settings=asfloat(cases.Load4(at+112));
    float4 a=asfloat(cases.Load4(at+128)),b=asfloat(cases.Load4(at+144)),c=asfloat(cases.Load4(at+160));
    float4 target=asfloat(cases.Load4(at+176));
    float2 source=asfloat(cases.Load2(at+192));
    float4 motion=reserved.x!=0?reflection_liquid_hit_motion(old,a,b,c,target.xy,source+(reserved.x==2?float2(3.75,-2.25):0))
        :reflection_liquid_hit_motion(old,a,b,c,target.xy);
    results.Store4(id.x*832,asuint(motion));
    // A direct forward-normal/height check at the independent source pixel
    // guards drift from the real native optical equations, even on rejection.
    float3 hit,normal;float depth,bias;
    bool valid=reflection_liquid_frame_valid(old) && reflection_liquid_ray(asfloat(cases.Load2(at+192)),old,hit,normal,depth,bias);
    results.Store4(id.x*832+16,asuint(valid?float4(hit,bias):float4(0,0,0,0)));
    results.Store4(id.x*832+32,asuint(valid?float4(normal,depth):float4(0,0,0,0)));
    float3 currentHit=float3(31.25+.125*(id.x%7),94.75,320.5+(id.x%5)*7.25);
    float2 transported=reflection_liquid_receiver_seed(old,currentHit,float4(.9998,.019998,0,7.25),
        float4(-.019998,.9998,0,-11.5),float4(0,0,1.0004,19.75));
    results.Store4(id.x*832+48,asuint(float4(transported,0,0)));
}
