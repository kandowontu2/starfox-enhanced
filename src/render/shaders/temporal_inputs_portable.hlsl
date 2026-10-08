cbuffer Settings : register(b0, space2) {
    uint width,height,reset,padding;
    float nearPlane,farPlane,pad1,pad2;
    uint groundEnabled,groundPreviousValid;
    float2 groundJitter;
    float4 groundPlane,projection,previousProjection;
    float4 previousRow0,previousRow1,previousRow2,previousRow3;
};
StructuredBuffer<float> cameraDepth : register(t0, space0);
StructuredBuffer<float4> sourceMotion : register(t1, space0);
StructuredBuffer<uint> groundCoverage : register(t2, space0);
RWTexture2D<float> projectedDepth : register(u0, space1);
RWTexture2D<float2> pixelMotion : register(u1, space1);
RWTexture2D<float> exposure : register(u2, space1);
bool finiteValue(float v) {return (asuint(v)&0x7f800000u)!=0x7f800000u;}
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    if(id.x==0 && id.y==0) exposure[uint2(0,0)]=1.0;
    uint index=id.y*width+id.x;
    float z=cameraDepth[index];float4 m=0;
    if(reset==0) m=sourceMotion[index];
    bool validDepth=finiteValue(z) && z>=nearPlane && z<=farPlane;
    // Caller supplies visible terrain ownership, after occlusion. Never infer
    // terrain from a background colour/tag or overwrite known model geometry.
    bool terrainCovered=false;
    if(groundEnabled!=0) terrainCovered=groundEnabled==2?(groundCoverage[index]&0x08000000u)!=0:groundCoverage[index]==1;
    if(!validDepth && terrainCovered) {
        float2 pixel=float2(id.xy)+0.5-groundJitter;
        float3 ray=float3((pixel-projection.zw)/projection.xy,1);
        float denominator=dot(groundPlane.xyz,ray);
        float candidate=abs(denominator)>1e-7?-groundPlane.w/denominator:0;
        if(finiteValue(candidate) && candidate>=nearPlane && candidate<=farPlane) {
            z=candidate;validDepth=true;m=0;
            if(reset==0 && groundPreviousValid!=0) {
                float3 p=ray*z;
                float3 previous=p.x*previousRow0.xyz+p.y*previousRow1.xyz+p.z*previousRow2.xyz+previousRow3.xyz;
                if(finiteValue(previous.z) && previous.z>=nearPlane && previous.z<=farPlane)
                    m=float4(previous.xy/previous.z*previousProjection.xy+previousProjection.zw-pixel,z,1);
            }
        }
    }
    // Rearrange the perspective expression to reduce near-plane cancellation.
    projectedDepth[id.xy]=validDepth?saturate((1.0-nearPlane/z)/(1.0-nearPlane/farPlane)):1.0;
    bool validMotion=validDepth && reset==0 && m.w==1.0 && finiteValue(m.x) && finiteValue(m.y)
        && finiteValue(m.z) && abs(m.z-z)<=max(0.001,abs(z)*0.00001);
    // Only the explicitly frozen source scene supplies this guarantee. Do not
    // infer it from missing model identity, paused presentation or zero camera
    // velocity during gameplay. Jitter remains separate from physical motion.
    pixelMotion[id.xy]=padding!=0 && reset==0?float2(0,0)
        :validMotion?m.xy:float2(asfloat(0xff7fffffu),asfloat(0xff7fffffu));
}
