Texture2D<float4> sourceColor : register(t0,space0);
Texture2D<float> sourceDepth : register(t1,space0);
Texture2D<float2> sourceMotion : register(t2,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> targetColor : register(u0,space1);
RWTexture2D<float> targetDepth : register(u1,space1);
RWTexture2D<float2> targetMotion : register(u2,space1);
cbuffer Settings : register(b0,space2) {uint sourceWidth,sourceHeight,targetWidth,targetHeight;};
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=targetWidth || id.y>=targetHeight) return;
    float2 ratio=float2(sourceWidth,sourceHeight)/float2(targetWidth,targetHeight);
    // Determine the footprint exactly. FP32 reciprocal/edge rounding can add
    // a zero-area neighbor, even when source and target widths are equal.
    // Such a neighbor must not donate depth or physical motion. Dimensions are
    // bounded to 8192, so these integer products cannot overflow uint32.
    uint2 sourceSize=uint2(sourceWidth,sourceHeight),targetSize=uint2(targetWidth,targetHeight);
    uint2 lo=id.xy*sourceSize,hi=(id.xy+1)*sourceSize;
    uint2 first=lo/targetSize,last=(hi+targetSize-1)/targetSize;
    float4 color=0;float weight=0,closest=2;float2 motion=asfloat(0xff7fffffu);
    // Area-filter color; depth and motion must come from the same nearest
    // visible sample, never average across foreground/background boundaries.
    for(uint y=first.y;y<last.y;++y) for(uint x=first.x;x<last.x;++x) {
        uint wx=min(hi.x,(x+1)*targetWidth)-max(lo.x,x*targetWidth);
        uint wy=min(hi.y,(y+1)*targetHeight)-max(lo.y,y*targetHeight);
        // Common target-size divisors cancel when normalizing the color sum.
        float w=float(wx)*float(wy);
        color+=sourceColor.Load(int3(x,y,0))*w;weight+=w;
        float z=sourceDepth.Load(int3(x,y,0));
        if(z<closest) {closest=z;motion=sourceMotion.Load(int3(x,y,0));}
    }
    bool valid=all(abs(motion)<1e20);
    targetColor[id.xy]=color/max(weight,1e-8);
    targetDepth[id.xy]=min(closest,1.0);
    targetMotion[id.xy]=valid?motion/ratio:float2(asfloat(0xff7fffffu),asfloat(0xff7fffffu));
}
