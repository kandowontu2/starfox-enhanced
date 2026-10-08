// Native motion is previous-minus-current in stored pixel coordinates.
Texture2D<float4> currentColor : register(t0,space0);
Texture2D<float4> historyColor : register(t1,space0);
Texture2D<float2> historyMeta : register(t2,space0);
StructuredBuffer<float> cameraDepth : register(t3,space0);
StructuredBuffer<float4> sourceMotion : register(t4,space0);
StructuredBuffer<uint> packedPixels : register(t5,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> targetColor : register(u0,space1);
RWTexture2D<float2> targetMeta : register(u1,space1);
cbuffer Settings : register(b0,space2) {
    uint width,height,resetHistory,padding;
    float historyWeight;float3 pad;
    float4 projection; // focal XY, centre XY, stored pixels
    float4 previousZ; // current camera point -> previous camera Z
    float4 jitter; // current XY, previous XY; native motion excludes jitter
};
bool eligible(uint i) {
    uint tag=(packedPixels[i]>>8)&255;
    return tag==0 || tag==3 || tag==4 || tag==5;
}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint2 p=id.xy;if(p.x>=width || p.y>=height) return;
    // History lives on the jittered raster grid. Presentation must not: undo
    // the current projection offset after resolving, without feeding this
    // additional reconstruction filter back into the temporal history.
    if(padding!=0) {
        float4 original=currentColor.Load(int3(p,0));
        if(!eligible(p.y*width+p.x)) {targetColor[p]=original;return;}
        float2 at=clamp(float2(p)+jitter.xy,float2(0,0),float2(width-1,height-1));
        uint2 a=uint2(at);float2 f=frac(at);float4 result=0;bool geometry=false;
        for(uint y=0;y<2;++y) for(uint x=0;x<2;++x) {
            uint2 n=min(a+uint2(x,y),uint2(width-1,height-1));
            uint index=n.y*width+n.x;
            float w=(x?f.x:1-f.x)*(y?f.y:1-f.y);
            geometry=geometry || (w>0 && eligible(index) && cameraDepth[index]>0);
            result+=(eligible(index)?currentColor.Load(int3(n,0)):original)*w;
        }
        targetColor[p]=geometry?result:original;return;
    }
    uint i=p.y*width+p.x;float4 current=currentColor.Load(int3(p,0));
    float z=cameraDepth[i];bool owns=eligible(i) && z>0 && isfinite(z);
    targetMeta[p]=float2(owns?z:0,owns?1:0);
    targetColor[p]=current;
    if(resetHistory!=0 || !owns) return;
    float4 motion=sourceMotion[i];
    if(motion.w<.5 || !all(isfinite(motion.xy))) return;
    float2 at=float2(p)+motion.xy+jitter.zw-jitter.xy;
    if(any(at<0) || at.x>width-1 || at.y>height-1) return;
    float3 position=float3((float2(p)+.5-jitter.xy-projection.zw)/projection.xy*z,z);
    float expectedZ=dot(float4(position,1),previousZ);
    if(!(expectedZ>0) || !isfinite(expectedZ)) return;
    uint2 a=uint2(at);float2 f=frac(at);float3 prior=0;float sum=0;
    for(uint y=0;y<2;++y) for(uint x=0;x<2;++x) {
        uint2 n=min(a+uint2(x,y),uint2(width-1,height-1));
        float2 meta=historyMeta.Load(int3(n,0));
        float w=(x?f.x:1-f.x)*(y?f.y:1-f.y);
        if(meta.y<.5 || !isfinite(meta.x) || abs(meta.x-expectedZ)>max(.01,expectedZ*.01)) continue;
        prior+=historyColor.Load(int3(n,0)).rgb*w;sum+=w;
    }
    if(sum<.75) return;
    float3 lo=1,hi=0;
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) {
        uint2 n=uint2(clamp(int2(p)+int2(x,y),int2(0,0),int2(width-1,height-1)));
        if(!eligible(n.y*width+n.x)) continue;
        float3 v=currentColor.Load(int3(n,0)).rgb;lo=min(lo,v);hi=max(hi,v);
    }
    targetColor[p]=float4(lerp(current.rgb,clamp(prior/sum,lo,hi),clamp(historyWeight,0,.95)),current.a);
}
