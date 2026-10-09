// Exact scale-induced model checkerboard resolve. This pass has no decorative,
// environment, bloom or overlay bindings and does not smooth model silhouettes.
Texture2D<float4> sourceColour : register(t0,space0);
ByteAddressBuffer ownership : register(t1,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> outputColour : register(u0,space1);
cbuffer Settings : register(b0,space2) { uint width,height,scale,packedTags; };
uint tagAt(uint2 p) {
    uint i=p.y*width+p.x;
    return packedTags!=0?(ownership.Load(i*4)>>8)&255u
        :(ownership.Load(i&~3u)>>((i&3u)*8))&255u;
}
uint4 colourAt(uint2 p) { return uint4(sourceColour.Load(int3(p,0))*255+.5); }
[numthreads(8,8,1)]
void main(uint3 dispatchId : SV_DispatchThreadID) {
    uint2 p=dispatchId.xy;
    if(p.x>=width || p.y>=height) return;
    uint4 c=colourAt(p),result=c;
    if(scale>1 && tagAt(p)==0) for(uint attempt=0;attempt<2;++attempt) {
        uint spacing=attempt==0?1:scale;
        if(p.x<spacing || p.y<spacing || p.x+spacing>=width || p.y+spacing>=height) continue;
        uint2 l=p-uint2(spacing,0),r=p+uint2(spacing,0),u=p-uint2(0,spacing),d=p+uint2(0,spacing);
        uint2 ul=p-uint2(spacing,spacing),dr=p+uint2(spacing,spacing);
        uint3 opposite=colourAt(l).rgb;
        if(any(c.rgb!=opposite) && tagAt(l)==0 && tagAt(r)==0 && tagAt(u)==0 && tagAt(d)==0
            && tagAt(ul)==0 && tagAt(dr)==0 && all(opposite==colourAt(r).rgb)
            && all(opposite==colourAt(u).rgb) && all(opposite==colourAt(d).rgb)
            && all(c.rgb==colourAt(ul).rgb) && all(c.rgb==colourAt(dr).rgb)) {
            result.rgb=(c.rgb+opposite+1)/2;
            break;
        }
    }
    outputColour[p]=float4(result)/255;
}
