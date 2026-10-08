// Native resident homogeneous single scattering. Geometry/light coordinates
// are +Y down/+Z forward in cartridge units, exactly like native ray output.
#if defined(STARFOX_FOG_BUILD)
ByteAddressBuffer geometry : register(t0,space0);
RWStructuredBuffer<float4> nodes : register(u0,space1);
cbuffer Build : register(b0,space2) {uint triangles,leaves,levelFirst,levelCount;};
[numthreads(64,1,1)]
void fog_leaves_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=leaves) return;
    float3 low=1.e30,high=-1.e30;
    for(uint primitive=id.x*4;primitive<min((id.x+1)*4,triangles);++primitive) {
        float3 a=asfloat(geometry.Load3(primitive*48));
        float3 b=asfloat(geometry.Load3(primitive*48+16));
        float3 c=asfloat(geometry.Load3(primitive*48+32));
        float3 normal=cross(b-a,c-a);
        if(all(isfinite(a))&&all(isfinite(b))&&all(isfinite(c))&&dot(normal,normal)>1.e-20) {
            float3 nextLow=min(a,min(b,c)),nextHigh=max(a,max(b,c));
            // Conservatively contain float-packed authored triangle edges.
            float3 margin=max(abs(nextLow),abs(nextHigh))*2.e-6+1.e-5;
            low=min(low,nextLow-margin);high=max(high,nextHigh+margin);
        }
    }
    nodes[(leaves+id.x)*2]=float4(low,0);nodes[(leaves+id.x)*2+1]=float4(high,0);
}
[numthreads(64,1,1)]
void fog_merge_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=levelCount) return;uint node=levelFirst+id.x;
    nodes[node*2]=float4(min(nodes[node*4].xyz,nodes[node*4+2].xyz),0);
    nodes[node*2+1]=float4(max(nodes[node*4+1].xyz,nodes[node*4+3].xyz),0);
}
#else
#if defined(STARFOX_FOG_FRAGMENT)
#define FOG_UNIFORM_SPACE space3
#else
#define FOG_UNIFORM_SPACE space2
#endif
cbuffer Settings : register(b0,FOG_UNIFORM_SPACE) {
    uint width,height,triangleCount,leafBase;
    uint materialOffset,materialBytes,flags,samples;
    float4 projection; // Actual eye/sample focal X/Y and pixel centre X/Y.
    float4 medium; // extinction, anisotropy, bounded distance, unused.
    float4 albedo,ambient,sunlight,light;
};
#if defined(STARFOX_FOG_INTEGRATE)
Texture2D<float4> ownership : register(t0,space0);
SamplerState ownershipSampler : register(s0,space0);
Texture2D<float4> surfaces : register(t1,space0);
SamplerState surfaceSampler : register(s1,space0);
ByteAddressBuffer geometry : register(t2,space0);
StructuredBuffer<float4> nodes : register(t3,space0);
RWStructuredBuffer<float4> integral : register(u0,space1);
bool bounds_hit(uint node,float3 origin,float3 ray,float nearT,float farT) {
    float3 low=nodes[node*2].xyz,high=nodes[node*2+1].xyz;
    if(any(low>high)) return false;
    for(uint axis=0;axis<3;++axis) {
        if(abs(ray[axis])<1.e-15) {if(origin[axis]<low[axis]||origin[axis]>high[axis]) return false;}
        else {
            float a=(low[axis]-origin[axis])/ray[axis],b=(high[axis]-origin[axis])/ray[axis];
            nearT=max(nearT,min(a,b));farT=min(farT,max(a,b));if(nearT>farT) return false;
        }
    }
    return true;
}
bool covered(uint primitive,float2 bary) {
    if(materialBytes==0) return true; // Explicit opaque geometry fixture/producer.
    uint at=materialOffset+primitive*64,kind=geometry.Load(at+60);
    if(kind==2) return (geometry.Load(at+32)>>24)==255;
    if(kind!=3 || geometry.Load(at+24)!=1) return false;
    uint textureFlags=geometry.Load(at+28),offset=geometry.Load(at+48);
    uint2 mask=geometry.Load2(at+52);
    bool indexed=(textureFlags&536870912U)!=0;
    if((textureFlags&~(7U|536870912U))!=0||(textureFlags&1U)==0||any(mask>4095)||any(mask&(mask+1))) return false;
    uint pixels=(mask.x+1)*(mask.y+1),words=indexed?256+(pixels+3)/4:pixels;
    if((offset&3U)!=0||offset<triangleCount*64||offset>materialBytes||words>(materialBytes-offset)/4) return false;
    float2 a=asfloat(geometry.Load2(at)),b=asfloat(geometry.Load2(at+8)),c=asfloat(geometry.Load2(at+16));
    if(!all(isfinite(a))||!all(isfinite(b))||!all(isfinite(c))) return false;
    float2 uv=a*(1-bary.x-bary.y)+b*bary.x+c*bary.y;
    if((textureFlags&4U)!=0&&(any(uv<0)||any(uv>=float2(mask+1)))) return false;
    uint2 xy=uint2(int2(floor(uv)))&mask;uint texel=xy.y*(mask.x+1)+xy.x;
    if(indexed) texel=(geometry.Load(materialOffset+offset+1024+(texel/4)*4)>>((texel&3U)*8))&255U;
    return (geometry.Load(materialOffset+offset+texel*4)>>24)==255;
}
bool triangle_hit(uint primitive,float3 origin,float3 direction,float nearT,float farT) {
    float3 a=asfloat(geometry.Load3(primitive*48));
    float3 e1=asfloat(geometry.Load3(primitive*48+16))-a,e2=asfloat(geometry.Load3(primitive*48+32))-a;
    float3 p=cross(direction,e2);float determinant=dot(e1,p);
    if(abs(determinant)<=sqrt(dot(e1,e1)*dot(e2,e2))*1.e-10) return false;
    float inverse=1/determinant;float3 offset=origin-a;float u=dot(offset,p)*inverse;
    if(u< -2.e-6||u>1+2.e-6) return false;
    float3 q=cross(offset,e1);float v=dot(direction,q)*inverse;
    if(v< -2.e-6||u+v>1+2.e-6) return false;
    float t=dot(e2,q)*inverse;
    return isfinite(t)&&t>nearT&&t<=farT&&covered(primitive,float2(u,v));
}
bool blocked(float3 origin,float3 direction) {
    if(triangleCount==0) return false;
    uint stack[32];uint size=1;stack[0]=1;
    while(size) {
        uint node=stack[--size];if(!bounds_hit(node,origin,direction,.01,65536)) continue;
        if(node>=leafBase) {
            uint first=(node-leafBase)*4;
            for(uint i=first;i<min(first+4,triangleCount);++i)
                if(triangle_hit(i,origin,direction,.01,65536)) return true;
        } else {stack[size++]=node*2;stack[size++]=node*2+1;}
    }
    return false;
}
float opacity(float opticalDepth) {
    return opticalDepth<.001?opticalDepth*(1-opticalDepth*(.5-opticalDepth/6)):1-exp(-opticalDepth);
}
[numthreads(8,8,1)]
void fog_integrate_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width||id.y>=height) return;uint index=id.y*width+id.x;
    if(medium.x==0||ownership.Load(int3(id.xy,0)).b==0) {integral[index]=float4(0,0,0,1);return;}
    float3 axial=float3((float2(id.xy)+.5-projection.zw)/projection.xy,1),ray=normalize(axial);
    float z=surfaces.Load(int3(id.xy,0)).w;
    float distance=z>0?min(medium.z,z*length(axial)):medium.z;
    float g=medium.y,phase=(1-g*g)/pow(1+g*g-2*g*clamp(dot(ray,light.xyz),-1,1),1.5);
    float T=exp(-medium.x*distance);float3 scattering=0;
    if(triangleCount==0||all(sunlight.xyz==0))
        scattering=albedo.xyz*(ambient.xyz+sunlight.xyz*phase)*opacity(medium.x*distance);
    else {
        float step=distance/samples,attenuation=exp(-medium.x*step),weight=opacity(medium.x*step),remembered=1;
        for(uint i=0;i<samples;++i) {
            bool occluded=blocked(ray*((i+.5)*step),light.xyz);
            scattering+=albedo.xyz*(ambient.xyz+(occluded?0:sunlight.xyz*phase))*remembered*weight;
            remembered*=attenuation;
        }
    }
    integral[index]=float4(scattering,T);
}
#else
// Graphics bindings use SDL fragment sampler space 2 and uniform space 3.
#include "calibrated_colour.hlsli"
Texture2D<float4> source : register(t0,space2);
SamplerState sourceSampler : register(s0,space2);
Texture2D<float4> ownership : register(t1,space2);
SamplerState ownershipSampler : register(s1,space2);
StructuredBuffer<float4> integral : register(t2,space2);
struct Fullscreen {float4 position:SV_Position;};
float4 fog_fragment_main(Fullscreen input):SV_Target0 {
    uint2 pixel=uint2(input.position.xy);if(pixel.x>=width||pixel.y>=height) discard;
    float4 output=source.Load(int3(pixel,0));
    if(medium.x==0||ownership.Load(int3(pixel,0)).b==0||output.a==0) return output;
    float4 volume=integral[pixel.y*width+pixel.x];
    float3 radiance=(flags&1U)!=0?output.rgb:calibrated_decode_srgb(output.rgb);
    radiance=saturate(radiance*volume.a+volume.rgb);
    output.rgb=(flags&1U)!=0?radiance:calibrated_encode_srgb(radiance);return output;
}
#endif
#endif
