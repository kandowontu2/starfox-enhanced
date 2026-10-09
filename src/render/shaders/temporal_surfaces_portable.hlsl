StructuredBuffer<uint> sourcePixels : register(t0,space0);
StructuredBuffer<float4> sourceSurfaces : register(t1,space0);
StructuredBuffer<float> sourceDepth : register(t2,space0);
StructuredBuffer<uint> protectedPixels : register(t3,space0);
RWStructuredBuffer<uint> targetPixels : register(u0,space1);
RWStructuredBuffer<float4> targetSurfaces : register(u1,space1);
cbuffer Settings : register(b0,space2) {uint width,height;float2 jitter;uint sourceWidth,sourceHeight,pad0,pad1;};
bool eligible(uint value) {
    uint tag=(value>>8)&255;
    return tag==0 || tag==3 || tag==4 || tag==5;
}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint2 p=id.xy;if(p.x>=width || p.y>=height) return;
    uint i=p.y*width+p.x;
    // HUD restoration happens after temporal colour resolution. Its ownership
    // must win here too, even if a jittered model shares the same palette entry.
    if(((protectedPixels[i]>>8)&255)==1) {
        targetPixels[i]=protectedPixels[i]&~0x01ff0000u;targetSurfaces[i]=0;return;
    }
    bool resized=sourceWidth!=width || sourceHeight!=height;
    uint protectedValue=protectedPixels[i];uint protectedTag=(protectedValue>>8)&255;
    if(resized && ((protectedTag==2 && (protectedValue&0x08000000u)==0)
        || (protectedTag==1 && (protectedValue&0x10000000u)!=0))) {
        targetPixels[i]=protectedValue&~0x01ff0000u;targetSurfaces[i]=0;return;
    }
    // Map pixel centres directly onto the original sample grid. Resolving an
    // already nearest-enlarged normal buffer doubles quantization and causes
    // reflections/lighting to crawl over the unjittered neural colour.
    float2 at=clamp((float2(p)+.5)*float2(sourceWidth,sourceHeight)/float2(width,height)-.5+jitter,
        0,float2(sourceWidth-1,sourceHeight-1));
    uint2 base=uint2(clamp((float2(p)+.5)*float2(sourceWidth,sourceHeight)/float2(width,height),
        0,float2(sourceWidth-1,sourceHeight-1)));
    uint selected=base.y*sourceWidth+base.x;
    if(eligible(sourcePixels[selected])) {
        uint2 a=uint2(at);float2 f=frac(at);float nearest=3.402823e38,bestWeight=-1;
        for(uint y=0;y<2;++y) for(uint x=0;x<2;++x) {
            uint2 n=min(a+uint2(x,y),uint2(sourceWidth-1,sourceHeight-1));uint j=n.y*sourceWidth+n.x;
            float weight=(x?f.x:1-f.x)*(y?f.y:1-f.y),z=sourceDepth[j];
            // Choose one contributing foreground surface; averaging normals
            // or palette owners across silhouettes produces false lighting.
            // Match the motion-guide resolve on coplanar surfaces as well:
            // a tiny first tap must not steal a stronger tap's normal/owner.
            if(weight>0 && eligible(sourcePixels[j]) && isfinite(z) && z>0
                && (z<nearest || (z==nearest && weight>bestWeight))) {
                nearest=z;selected=j;bestWeight=weight;
            }
        }
    }
    uint packed=sourcePixels[selected];float4 surface=sourceSurfaces[selected];
    bool valid=eligible(packed) && (packed&0x01000000u)!=0
        && ((packed>>16)&255)==(packed&255) && all(isfinite(surface)) && surface.w>0;
    targetPixels[i]=valid?packed:packed&~0x01ff0000u;
    targetSurfaces[i]=valid?surface:0;
}
