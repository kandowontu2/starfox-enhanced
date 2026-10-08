#include "msaa_coverage.hlsli"
// Ordered flat faces: palette shading is pixel-frequency, not sample-frequency.
// Dithered materials name both palette entries; 0xffffffff means solid color.
#include "msaa_triangle.hlsli"
Texture2D<float4> background : register(t0,space0);
StructuredBuffer<Triangle> triangles : register(t1,space0);
StructuredBuffer<uint> palette : register(t2,space0);
StructuredBuffer<uint2> previousSamples : register(t3,space0);
ByteAddressBuffer texels : register(t4,space0);
StructuredBuffer<uint> layerPixels : register(t5,space0);
StructuredBuffer<uint> faceKinds : register(t6,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> target : register(u0,space1);
RWStructuredBuffer<uint2> sampleOutput : register(u1,space1);
cbuffer Settings : register(b0,space2) {uint width,height,count,samples;uint hasPrevious,retainSamples,texelBytes,packedFaces;uint sceneLayer,faceCount,inPlace,resolveOnly;};
float4 unpackColor(uint c) {return float4(c&255,(c>>8)&255,(c>>16)&255,c>>24)/255.0;}
int msaaWaveShift(int x,uint offset,uint frame) {
    int phase=(int((offset+uint(x))<<16))>>16;
    phase=(int(uint((phase>>1)+int(frame&15U)-1)<<16))>>16;
    static const int sine[16]={0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1};
    return sine[uint(phase)&15U];
}
float4 unpackSample(uint2 c) {return f16tof32(uint4(c.x&65535,c.x>>16,c.y&65535,c.y>>16));}
uint2 packSample(float4 c) {uint4 h=f32tof16(c);return uint2(h.x|(h.y<<16),h.z|(h.w<<16));}
float4 unpackIndexed(uint token) {
    if((token&0x10000U)==0) return 0;
    float4 color=unpackColor(palette[token&255]);
    if(token&0x20000U) color=(color+unpackColor(palette[(token>>8)&255]))*.5;
    color.a=1;return color;
}
float2 polygonVertex(uint first,uint vertex) {
    return vertex==0?triangles[first].a:vertex==1?triangles[first].b:triangles[first+vertex-2].c;
}
int vertexRow(float y) {return y<0?-int(floor(-y+.5)):int(floor(y+.5));}
bool continuationEdges(uint first,uint active,int row) {
    uint size=active+2,minimum=0;
    for(uint v=1;v<size;++v) if(vertexRow(polygonVertex(first,v).y)<vertexRow(polygonVertex(first,minimum).y)) minimum=v;
    int left=vertexRow(polygonVertex(first,minimum).y),right=left;
    for(uint step=1;step<size;++step) {
        int y=vertexRow(polygonVertex(first,(minimum+step)%size).y);
        if(y>row) break;left=y;
    }
    for(uint step=1;step<size;++step) {
        int y=vertexRow(polygonVertex(first,(minimum+size-step)%size).y);
        if(y>row) break;right=y;
    }
    // A right-chain restart clears continuation; a later left-chain restart
    // sets it. The restart rows themselves are filled in the native effect.
    return left>right && left!=row && right!=row;
}
uint sparseWobbleCoverage(uint first,uint active,uint2 pixel,uint sampleCount) {
    uint size=active+2,minimum=0;
    for(uint v=1;v<size;++v) if(vertexRow(polygonVertex(first,v).y)<vertexRow(polygonVertex(first,minimum).y)) minimum=v;
    if(int(pixel.y)<=vertexRow(polygonVertex(first,minimum).y)) return 0;
    for(uint step=1;step<size;++step) {
        int y=vertexRow(polygonVertex(first,(minimum+size-step)%size).y);
        if(y>int(pixel.y)) break;if(y==int(pixel.y)) return 0;
    }
    uint mask=0;
    for(uint s=0;s<sampleCount;++s) {
        float2 at=float2(pixel)+.5+msaaOffsets[sampleCount-2+s];float previousY=at.y-1;
        float left=3.402823e38;bool found=false;
        for(uint v=0;v<size;++v) {
            float2 a=polygonVertex(first,v),b=polygonVertex(first,(v+1)%size);
            if(previousY>=min(a.y,b.y) && previousY<max(a.y,b.y)) {
                left=min(left,a.x+(previousY-a.y)*(b.x-a.x)/(b.y-a.y));found=true;
            }
        }
        if(found && at.x>=left && at.x<left+1) mask|=1U<<s;
    }
    return mask;
}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint2 p=id.xy;if(p.x>=width || p.y>=height) return;
    float4 base=sceneLayer!=0?float4(0,0,0,0):background.Load(int3(p,0));
    uint sampleBase=(p.y*width+p.x)*samples;
    if(resolveOnly!=0) {
        float4 color=0;for(uint s=0;s<samples;++s) color+=unpackIndexed(sampleOutput[sampleBase+s].x);
        target[p]=color/samples;return;
    }
    float4 colors[8];uint tokens[8];
    if(inPlace==0) for(uint s=0;s<samples;++s) {
        tokens[s]=hasPrevious!=0 && sceneLayer!=0?previousSamples[sampleBase+s].x:0;
        colors[s]=sceneLayer!=0?unpackIndexed(tokens[s]):hasPrevious!=0?unpackSample(previousSamples[sampleBase+s]):base;
    }
    uint remaining=(1U<<samples)-1;
    bool fallback=count==0;
    if(sceneLayer!=0) for(uint f=0;f<faceCount;++f) if(faceKinds[f]==2) {fallback=true;break;}
    if(sceneLayer!=0 && fallback) {
        uint packed=layerPixels[p.y*width+p.x];
        if((packed&0x04000000U)!=0) {
            float4 color=unpackColor(palette[packed&255]);
            if((packed&0x80000000U)!=0) color=(color+unpackColor(palette[(packed>>8)&255]))*.5;
            color.a=1; // Native coverage is opaque, including palette zero.
            uint token=0x10000U|(packed&255U)|((packed&0x80000000U)?0x20000U|(packed&0xff00U):0);
            for(uint s=0;s<samples;++s) {colors[s]=color;tokens[s]=token;}
            remaining=0;
        }
    }
    for(uint i=sceneLayer!=0 && fallback?0:count;i>0 && remaining!=0;) {
        if(packedFaces!=0 && i%128==0) {
            uint first=i-128;
            uint active=min(triangles[first].padding.x,128U);
            i=first+active;
            if(active==0) continue;
        }
        Triangle t=triangles[--i];
        if(t.padding.y==3) {
            // The native wave is constant within an output pixel column.
            // Displace coverage, not its shaded color or sample pattern.
            float2 displacement=float2(0,msaaWaveShift(int(p.x),t.scrollX,t.scrollY));
            t.a+=displacement;t.b+=displacement;t.c+=displacement;
        }
        uint coverage=msaaCoverage(t.a,t.b,t.c,p,samples);
        if(t.padding.y==6) {
            int row=int(p.y)-((t.padding.z&1)!=0?msaaWaveShift(int(p.x),t.scrollX,t.scrollY):0);
            if(row>=0 && row<int(t.vMask) && t.uMask!=0 && t.uMask%4==0 && p.x/32<t.uMask/4 &&
                (t.padding.z>>8)==samples && t.textureOffset<texelBytes && t.vMask<=(texelBytes-t.textureOffset)/t.uMask/samples) {
                for(uint s=0;s<samples;++s) {
                    uint address=t.textureOffset+(s*t.vMask+uint(row))*t.uMask+(p.x/32)*4;
                    if(texels.Load(address)&(1U<<(p.x&31))) coverage|=1U<<s;
                }
            }
        }
        if(t.padding.y==1) coverage|=msaaCoverage(t.a,t.c,t.uvA,p,samples);
        if(packedFaces!=0 && t.padding.y==5) {
            uint first=(i/128)*128,active=min(triangles[first].padding.x,128U);
            coverage=sparseWobbleCoverage(first,active,p,samples);i=first;
        }
        if(packedFaces!=0 && (t.padding.y==2 || t.padding.y==4)) {
            uint first=(i/128)*128,active=min(triangles[first].padding.x,128U);
            uint left=0,right=0,filled=0;
            for(uint v=0;v<active;++v) {
                Triangle face=triangles[first+v];float2 inset=float2(1,0);
                filled|=msaaCoverage(face.a,face.b,face.c,p,samples);
                left|=msaaCoverage(face.a+inset,face.b+inset,face.c+inset,p,samples);
                right|=msaaCoverage(face.a-inset,face.b-inset,face.c-inset,p,samples);
            }
            coverage=t.padding.y==2?left&right:continuationEdges(first,active,int(p.y))?filled&~(left&right):filled;
            i=first;
        }
        uint mask=coverage&remaining;
        if(mask==0) continue;
        // Resolve authored material dithering before distribution to samples.
        // Color lookup happens once, regardless of how many samples are covered.
        float4 color=unpackColor(palette[t.color0&255]);
        uint token=0x10000U|(t.color0&255U)|(t.color1!=0xffffffff?0x20000U|((t.color1&255U)<<8):0);
        if(t.color1!=0xffffffff) color=(color+unpackColor(palette[t.color1&255]))*.5;
        if(t.textured!=0) {
            // Pixel-frequency shading, with a covered-sample centroid at an
            // edge so interpolation never extrapolates beyond the primitive.
            float2 at=float2(p)+.5;
            float area=msaaEdge(t.a,t.b,t.c);
            float3 bary=float3(msaaEdge(t.b,t.c,at),msaaEdge(t.c,t.a,at),msaaEdge(t.a,t.b,at))/area;
            if(any(bary<0)) {
                float2 offset=0;uint covered=0;
                for(uint s=0;s<samples;++s) if(coverage&(1U<<s)) {offset+=msaaOffsets[samples-2+s];++covered;}
                at+=offset/covered;
                bary=float3(msaaEdge(t.b,t.c,at),msaaEdge(t.c,t.a,at),msaaEdge(t.a,t.b,at))/area;
            }
            float2 coord=t.uvA*bary.x+t.uvB*bary.y+t.uvC*bary.z;
            if(!all(isfinite(coord)) || any(abs(coord)>16777216.0)) continue;
            // Model sprites are bounded images, not wrapping polygon textures.
            if(t.textured==2 && (any(coord<0) || any(floor(coord)>float2(t.uMask,t.vMask)))) continue;
            uint2 uv=(uint2(int2(floor(coord)))+uint2(t.scrollX,t.scrollY))&uint2(t.uMask,t.vMask);
            // Validate products before arithmetic, not after overflow.
            if(t.uMask==0xffffffff || t.textureOffset>=texelBytes) continue;
            uint remainingBytes=texelBytes-t.textureOffset;
            if(uv.y>remainingBytes/(t.uMask+1)) continue;
            uint row=uv.y*(t.uMask+1);
            if(row>=remainingBytes || uv.x>=remainingBytes-row) continue;
            uint address=t.textureOffset+row+uv.x;
            uint ink=(texels.Load(address&~3U)>>((address&3U)*8))&255;
            if(ink==0) continue;
            color=unpackColor(palette[(t.colorBase+ink)&255]);
            token=0x10000U|((t.colorBase+ink)&255U);
        }
        if(sceneLayer!=0) color.a=1;
        for(uint s=0;s<samples;++s) if((mask&(1U<<s))!=0) {colors[s]=color;tokens[s]=token;}
        remaining&=~mask;
    }
    if(inPlace!=0) {
        if(remaining==((1U<<samples)-1)) return;
        for(uint s=0;s<samples;++s) if(remaining&(1U<<s)) {
            tokens[s]=sampleOutput[sampleBase+s].x;colors[s]=unpackIndexed(tokens[s]);
        }
    }
    float4 resolved=0;for(uint s=0;s<samples;++s) resolved+=colors[s];
    target[p]=resolved/samples;
    if(retainSamples!=0) for(uint s=0;s<samples;++s) sampleOutput[sampleBase+s]=sceneLayer!=0?uint2(tokens[s],0):packSample(colors[s]);
}
