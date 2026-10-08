// Preserve GpuClip's fractional vertices. Never run these through integer
// scanline endpoints: those have already lost subpixel coverage information.
struct Material {
    int4 bounds;uint even,odd,dither,tag;
    uint4 texture;int4 uv;float4 surface;
    uint hasSurface,textured,scrollX,scrollY;
};
#include "msaa_triangle.hlsli"
StructuredBuffer<int4> clipped : register(t0,space0);
StructuredBuffer<Material> materials : register(t1,space0);
StructuredBuffer<uint> order : register(t2,space0);
StructuredBuffer<uint2> orderResults : register(t3,space0);
RWStructuredBuffer<Triangle> triangles : register(u0,space1);
// 0 = empty/rejected by clipping; 1 = emitted; 2 = needs its own evaluator.
RWStructuredBuffer<uint> kinds : register(u1,space1);
cbuffer Settings : register(b0,space2) {
    uint count,polygonCount,fractional,ordered;
    float2 scale;uint orderFirst,orderTree;
    uint lineThickness,maskOffset,maskStride,maskHeight;
    uint maskSamples;uint3 maskPadding;
};
float2 position(uint at) {
    int2 p=clipped[at].xy;
    return (fractional!=0?asfloat(p):float2(p))*scale;
}
float2 uv(uint at) {int2 p=clipped[at].zw;return fractional!=0?asfloat(p):float2(p);}
void emitLine(uint first,uint active,float2 a,float2 b,Material material) {
    if(a.y>b.y || (a.y==b.y && a.x>b.x)) {float2 swap=a;a=b;b=swap;}
    float2 delta=b-a;float lengthSquared=dot(delta,delta);
    float2 direction=lengthSquared>0?delta*rsqrt(lengthSquared):float2(1,0);
    float halfWidth=.5*min(scale.x,scale.y)*lineThickness;
    float2 normal=float2(-direction.y,direction.x)*halfWidth;
    float2 start=a-direction*halfWidth,last=b+direction*halfWidth;
    Triangle t=(Triangle)0;t.a=start-normal;t.b=last-normal;t.c=last+normal;
    t.color0=material.even&255;t.color1=material.dither!=0?material.odd&255:0xffffffff;t.padding.x=active;
    // A boundary stroke uses one packet: UV A holds its fourth corner.
    t.uvA=start+normal;t.padding.y=1;triangles[first]=t;
}
[numthreads(32,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint slot=id.x;if(slot>=count) return;
    Triangle empty=(Triangle)0;
    for(uint i=0;i<128;++i) triangles[slot*128+i]=empty;
    kinds[slot]=0;
    if(ordered==2) {
        uint2 result=orderResults[orderTree];
        if(result.y!=0 || result.x>count) {kinds[slot]=2;return;}
        if(slot>=result.x) return;
    }
    uint polygon=ordered!=0?order[orderFirst+slot]:slot;
    if(polygon>=polygonCount) {kinds[slot]=2;return;}
    uint base=polygon*129;int4 header=clipped[base];
    if(header.y!=0 || header.x==0) return;
    Material material=materials[polygon];
    if(header.x>=3 && header.x<=128 && material.textured==0 && (material.scrollY&262144U)!=0) {
        if(maskHeight==0 || maskStride==0 || (maskSamples!=2 && maskSamples!=4 && maskSamples!=8)) {kinds[slot]=2;return;}
        Triangle t=(Triangle)0;t.color0=material.even&255;
        t.color1=material.dither!=0?material.odd&255:0xffffffff;
        t.textureOffset=maskOffset+slot*maskHeight*maskStride*(maskSamples+1)+maskHeight*maskStride;
        t.uMask=maskStride;t.vMask=maskHeight;t.padding.x=1;t.padding.y=6;
        t.padding.z=((material.scrollY&131072U)!=0?1U:0U)|(maskSamples<<8);
        t.scrollX=uint(material.uv.z);t.scrollY=uint(material.uv.w);
        triangles[slot*128]=t;kinds[slot]=1;return;
    }
    if(header.x==1 && header.z==2 && material.textured==1) {
        int4 raw=clipped[base+1];
        float3 centre=fractional!=0?asfloat(raw.xyz):float3(raw.xyz);
        if(!all(isfinite(centre)) || material.texture.y==0xffffffffU) {kinds[slot]=2;return;}
        int depth=int(uint(int(centre.z>=0?floor(centre.z+.5):ceil(centre.z-.5)))&65535U);
        uint sourceWidth=material.texture.y+1;
        if(sourceWidth>65536) {kinds[slot]=2;return;}
        int increment=clamp((depth*(sourceWidth==64?128:256))>>8,1,32767);
        int extent=int(sourceWidth)*128/increment;
        float2 lower=centre.xy-extent,upper=centre.xy+extent+1;
        float2 uvLower=float(sourceWidth)/2+(lower-centre.xy)*float(increment)/256;
        float2 uvUpper=float(sourceWidth)/2+(upper-centre.xy)*float(increment)/256;
        Triangle t=(Triangle)0;t.a=lower*scale;t.b=float2(upper.x,lower.y)*scale;t.c=upper*scale;
        t.uvA=uvLower;t.uvB=float2(uvUpper.x,uvLower.y);t.uvC=uvUpper;
        t.color0=material.even&255;t.color1=0xffffffff;
        t.textureOffset=material.texture.x;t.uMask=material.texture.y;t.vMask=material.texture.z;t.colorBase=material.texture.w;
        t.textured=2;t.padding.x=2;
        triangles[slot*128]=t;
        t.b=t.c;t.uvB=t.uvC;t.c=float2(lower.x,upper.y)*scale;t.uvC=float2(uvLower.x,uvUpper.y);
        triangles[slot*128+1]=t;kinds[slot]=1;return;
    }
    if(header.x==2 && header.z==1 && material.textured==0 && material.scrollY==0) {
        float2 a=position(base+1),b=position(base+2);
        if(!all(isfinite(a)) || !all(isfinite(b))) {kinds[slot]=2;return;}
        // Canonical endpoint order makes reversed source lines cover exactly
        // the same samples. Square caps retain endpoint visibility; a zero-
        // length segment remains a thickness-sized square, not a lost laser.
        if(a.y>b.y || (a.y==b.y && a.x>b.x)) {float2 swap=a;a=b;b=swap;}
        float2 delta=b-a;float lengthSquared=dot(delta,delta);
        float2 direction=lengthSquared>0?delta*rsqrt(lengthSquared):float2(1,0);
        float halfWidth=.5*min(scale.x,scale.y)*lineThickness;
        float2 normal=float2(-direction.y,direction.x)*halfWidth;
        float2 first=a-direction*halfWidth,last=b+direction*halfWidth;
        Triangle t=(Triangle)0;t.a=first-normal;t.b=last-normal;t.c=last+normal;
        t.color0=material.even&255;t.color1=material.dither!=0?material.odd&255:0xffffffff;t.padding.x=2;
        triangles[slot*128]=t;t.b=t.c;t.c=first+normal;triangles[slot*128+1]=t;
        kinds[slot]=1;return;
    }
    // Special lines, sprites and screen-space deformations cannot
    // masquerade as filled flat faces. Report them for explicit dispatch.
    if(header.x<3 || header.x>128 || material.textured>1 || (material.textured==0 && (material.scrollY&~196615U)!=0)) {
        kinds[slot]=2;return;
    }
    float2 a=position(base+1);
    if(!all(isfinite(a))) {kinds[slot]=2;return;}
    for(uint v=1;v<uint(header.x);++v)
        if(!all(isfinite(position(base+1+v))) || (material.textured!=0 && !all(isfinite(uv(base+1+v))))) {kinds[slot]=2;return;}
    if(material.textured!=0 && !all(isfinite(uv(base+1)))) {kinds[slot]=2;return;}
    if(material.textured==0 && (material.scrollY&2U)!=0 && (material.scrollY&65536U)==0) {
        // Emit only the actual boundary, with square-cap joins shared with
        // native line geometry. Do not expose the filled polygon's fan edges.
        for(uint v=0;v<uint(header.x);++v)
            emitLine(slot*128+v,uint(header.x),position(base+1+v),position(base+1+(v+1)%uint(header.x)),material);
        kinds[slot]=1;return;
    }
    for(uint v=1;v+1<uint(header.x);++v) {
        Triangle t=(Triangle)0;t.a=a;t.b=position(base+1+v);t.c=position(base+2+v);
        t.color0=material.even&255;
        t.color1=material.dither!=0?material.odd&255:0xffffffff;
        t.uvA=uv(base+1);t.uvB=uv(base+1+v);t.uvC=uv(base+2+v);
        t.textureOffset=material.texture.x;t.uMask=material.texture.y;t.vMask=material.texture.z;t.colorBase=material.texture.w;
        t.scrollX=material.scrollX;t.scrollY=material.scrollY;t.textured=material.textured;
        t.padding.x=uint(header.x)-2;
        // Cel mode removes one stored pixel from both horizontal boundaries.
        // The resolve intersects translated polygon coverage, not fan edges.
        if(material.textured==0 && (material.scrollY&1U)!=0) t.padding.y=2;
        if(material.textured==0 && (material.scrollY&6U)==4U) t.padding.y=4;
        if(material.textured==0 && (material.scrollY&131072U)!=0) {
            t.padding.y=3;t.scrollX=uint(material.uv.z);t.scrollY=uint(material.uv.w);
        }
        if(material.textured==0 && (material.scrollY&65536U)!=0) t.padding.y=5;
        triangles[slot*128+v-1]=t;
    }
    kinds[slot]=1;
}
