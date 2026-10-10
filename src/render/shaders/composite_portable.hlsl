#include "raster_jitter.hlsli"
#include "native_pixel.hlsli"
Texture2D<float4> nativeMsaa : register(t0,space0);
Texture2D<float4> lateMsaa : register(t1,space0);
Texture2D<float4> backgroundMsaa : register(t2,space0);
StructuredBuffer<uint> cpuPixels : register(t3,space0);
StructuredBuffer<uint> nativePixels : register(t4,space0);
StructuredBuffer<float4> nativeSurfaces : register(t5,space0);
StructuredBuffer<uint> palette : register(t6,space0);
StructuredBuffer<uint> latePixels : register(t7,space0);
StructuredBuffer<uint> backgroundPixels : register(t8,space0);
StructuredBuffer<float> nativeDepth : register(t9,space0);
StructuredBuffer<float4> nativeMotion : register(t10,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> rgba : register(u0,space1);
RWStructuredBuffer<uint> pixels : register(u1,space1);
RWStructuredBuffer<float4> surfaces : register(u2,space1);
RWStructuredBuffer<uint> edgeColours : register(u3,space1);
RWStructuredBuffer<float> geometryDepth : register(u4,space1);
RWStructuredBuffer<float4> motion : register(u5,space1);
cbuffer Settings : register(b0,space2) {
    uint width,height,sourceWidth,sourceHeight;
    uint scale,sourceScale,mosaic,hasSurfaces;
    int offsetX,offsetY,clipLeft,clipTop;
    int clipRight,clipBottom,originX,originY;
    uint hasLate,hasBackground,phase,marginOrigin;
    uint marginWidth,repairMargins,hasDepth,hasMotion;
    uint worldOnly,pad0,pad1,matchRightMargin;
    uint sourceReferenceWidth,sourceReferenceHeight,outputWidth,outputHeight;
    uint lateWidth,lateHeight,backgroundWidth,backgroundHeight;
    uint uniformCpuEnabled,uniformCpuValue,stripeCpuEnabled,stripeCpuValue;
    uint stripeLeft0,stripeRight0,stripeLeft1,stripeRight1;
    uint hasNativeMsaa,hasLateMsaa,hasBackgroundMsaa,msaaPadding;
};
float4 paletteColor(uint value) {
    uint color=palette[value&255];uint3 rgb=uint3(color&255,(color>>8)&255,(color>>16)&255);
    if(sourceScale>1 && (value&0x80000000U)!=0) {
        uint other=palette[(value>>8)&255];rgb=(rgb+uint3(other&255,(other>>8)&255,(other>>16)&255)+1)/2;
    }
    return float4(rgb,color>>24)/255.0;
}
float4 over(float4 front,float4 back) {return front+back*(1-front.a);}
uint cpuAt(uint index) {
    if(uniformCpuEnabled) return uniformCpuValue;
    if(stripeCpuEnabled) {
        uint x=index%width;
        return ((x>=stripeLeft0 && x<stripeRight0) || (x>=stripeLeft1 && x<stripeRight1))
            ?stripeCpuValue:uniformCpuValue;
    }
    return cpuPixels[index];
}
uint2 sourcePixel(int2 source,uint2 sub) {
    return (uint2(source)*sourceScale+sub)*uint2(sourceWidth,sourceHeight)
        /uint2(sourceReferenceWidth,sourceReferenceHeight);
}
uint2 sourceSample(int2 source,uint2 sub,uint2 position,uint2 extent) {
    if(mosaic>1 || (sourceReferenceWidth==sourceWidth && sourceReferenceHeight==sourceHeight))
        return sourcePixel(source,sub);
    // A resized native scene already lives on its own sample grid. Going
    // through integer reference pixels first duplicates/skips samples and
    // shifts colour, depth and motion relative to the projection jitter.
    precise float2 logical=(float2(position)+0.5)*float2(width,height)/float2(extent)/float(scale);
    precise float2 native=(logical-float2(offsetX,offsetY))*float(sourceScale)
        *float2(sourceWidth,sourceHeight)/float2(sourceReferenceWidth,sourceReferenceHeight);
    return uint2(clamp(floor(native),0.0,float2(sourceWidth-1,sourceHeight-1)));
}
uint cpuIndex(uint2 position,uint2 extent) {
    float2 jitter=asfloat(uint2(pad0,pad1))*float2(extent)/float2(width,height);
    uint2 at=uint2(clamp(int2(jitterFloor(position.x,width,extent.x,jitter.x,any(jitter!=0)),
        jitterFloor(position.y,height,extent.y,jitter.y,any(jitter!=0))),0,int2(width-1,height-1)));
    return at.y*width+at.x;
}
int mosaicAt(int at,int origin) {
    int value=at-origin,rem=value%int(mosaic);
    if(rem<0) rem+=int(mosaic);
    return at-rem;
}
uint backgroundAt(uint2 position,uint2 extent) {
    uint2 p=position*uint2(backgroundWidth,backgroundHeight)/extent;
    return backgroundPixels[p.y*backgroundWidth+p.x];
}
uint composedValue(uint2 at,uint cpu,uint2 sampleAt,uint2 sampleExtent,uint2 nativeSample) {
    uint i=at.y*width+at.x;
    uint value=cpu&65535;
    if(worldOnly && ((value>>8)&255u)==1u) value=0;
    if(hasBackground && !(cpu&0xc0000000u)) {
        uint background=backgroundAt(sampleAt,sampleExtent);
        if(background&0x04000000u) value=background&0x1800ffffu;
    }
    int2 logical=int2(at/scale),source=logical-int2(offsetX,offsetY);
    if(marginOrigin && repairMargins && !(cpu&0xc0000000u)
        && (logical.x<int(marginOrigin) || logical.x>=int(marginOrigin+marginWidth))
        && !(backgroundAt(at/scale*scale,uint2(width,height))&255u))
        value=edgeColours[0]|(1u<<8);
    uint2 sub=min(sourceScale-1,((at%scale)*2+1)*sourceScale/(scale*2));
    if(all(source>=0) && all(source<int2(sourceReferenceWidth,sourceReferenceHeight)/int(sourceScale))
        && logical.x>=clipLeft && logical.y>=clipTop && logical.x<clipRight && logical.y<clipBottom
        && !(cpu&0x80000000u)) {
        if(mosaic>1) source=int2(mosaicAt(logical.x,originX),mosaicAt(logical.y,originY))-int2(offsetX,offsetY);
        if(all(source>=0) && all(source<int2(sourceReferenceWidth,sourceReferenceHeight)/int(sourceScale))) {
            uint2 p=mosaic>1?sourceSample(source,sub,sampleAt,sampleExtent):nativeSample;
            uint native=nativePixels[p.y*sourceWidth+p.x];
            if(((native&255u) || (sourceScale>1 && (native&0x80000000u))) && (!worldOnly || nativeTag(native)!=1u || (native&0x10000000u))) value=native&0x9800ffffu;
        }
    }
    return value;
}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
#if !defined(STARFOX_COMPOSITE_PIXEL_ONLY)
#if defined(STARFOX_COMPOSITE_EDGE_ONLY)
    if(true) {
#else
    if(!phase) {
#endif
        if(id.y || id.x>1) return;
        if(repairMargins) {edgeColours[id.x]=backgroundAt(uint2(marginOrigin*scale,0),uint2(width,height))&255u;return;}
        uint counts[256];for(uint c=0;c<256;++c) counts[c]=0;
        uint x=(marginOrigin+(id.x?marginWidth-1:0))*scale;
        for(uint y=0;y<height/scale;++y) {
            uint2 at=uint2(x,y*scale);
            uint ci=uniformCpuEnabled?0:cpuIndex(at,uint2(width,height));
            int2 source=int2(at/scale)-int2(offsetX,offsetY);
            uint2 sub=min(sourceScale-1,((at%scale)*2+1)*sourceScale/(scale*2));
            uint2 nativeSample=0;
            if(all(source>=0) && all(source<int2(sourceReferenceWidth,sourceReferenceHeight)/int(sourceScale)))
                nativeSample=sourceSample(source,sub,at,uint2(width,height));
            ++counts[composedValue(at,cpuAt(ci),at,uint2(width,height),nativeSample)&255u];
        }
        uint best=0;for(uint c=1;c<256;++c) if(counts[c]>counts[best]) best=c;
        edgeColours[id.x]=best;return;
    }
#endif
#if !defined(STARFOX_COMPOSITE_EDGE_ONLY)
    if(id.x>=outputWidth || id.y>=outputHeight) return;
    uint2 outputAt=id.xy;
    id.xy=id.xy*uint2(width,height)/uint2(outputWidth,outputHeight);
    uint ci=uniformCpuEnabled?0:cpuIndex(outputAt,uint2(outputWidth,outputHeight));
    uint cpu=cpuAt(ci);
    int2 logical=int2(id.xy/scale),source=logical-int2(offsetX,offsetY);
    uint2 sub=min(sourceScale-1,((id.xy%scale)*2+1)*sourceScale/(scale*2));
    bool inSource=all(source>=0) && all(source<int2(sourceReferenceWidth,sourceReferenceHeight)/int(sourceScale));
    uint2 nativeSample=0;
    if(inSource) nativeSample=sourceSample(source,sub,outputAt,uint2(outputWidth,outputHeight));
    uint i=id.y*width+id.x;
    uint value=composedValue(id.xy,cpu,outputAt,uint2(outputWidth,outputHeight),nativeSample);
    float4 normal=0;uint flags=0;
    // Emissive beams (bit 29 from scene_portable) stay unshadowed when the
    // final pixel is still that beam pixel; later overrides clear flags.
    if(inSource && !(cpu&0x80000000u)) {
        uint native=nativePixels[nativeSample.y*sourceWidth+nativeSample.x];
        if((native&0x20000000u) && (native&0xffffu)==(value&0xffffu)) flags|=0x20000000u;
    }
    float depth=0;float4 temporal=0;
    // Depth, motion and surface ownership all inspect the same unmosaicked
    // native sample. Resolve its potentially fractional projection only once.
    uint nativeIndex=0,nativeMeta=0;
    if(inSource && (hasDepth || hasMotion || hasSurfaces)) {
        nativeIndex=nativeSample.y*sourceWidth+nativeSample.x;
        nativeMeta=nativePixels[nativeIndex];
    }
    // Temporal guides follow visible colour ownership, unlike lighting's
    // historical unmosaicked metadata. Screen-space mosaic has no trustworthy
    // pinhole correspondence and is intentionally left unknown.
    if(inSource && mosaic==1 && logical.x>=clipLeft && logical.x<clipRight
        && logical.y>=clipTop && logical.y<clipBottom && !(cpu&0x80000000u)) {
        if(((nativeMeta&255u) || (sourceScale>1 && (nativeMeta&0x80000000u))) && (!worldOnly || nativeTag(nativeMeta)!=1u || (nativeMeta&0x10000000u))) {
            if(hasDepth) depth=nativeDepth[nativeIndex];
            if(hasMotion) {
                temporal=nativeMotion[nativeIndex];
                if(sourceReferenceWidth==sourceWidth && sourceReferenceHeight==sourceHeight
                    && outputWidth==width && outputHeight==height)
                    temporal.xy*=float(scale)/float(sourceScale);
                else {
                    precise float2 sourceRatio=float2(sourceReferenceWidth,sourceReferenceHeight)/float2(sourceWidth,sourceHeight);
                    precise float2 outputRatio=float2(outputWidth,outputHeight)/float2(width,height);
                    precise float2 motionScale=(float(scale)/float(sourceScale))*sourceRatio*outputRatio;
                    temporal.xy*=motionScale;
                }
            }
        }
    }
    // GAMEOVER's background fade excludes whole source cells containing
    // foreground at their top-left sample, independently of surface normals.
    uint2 topLeft=inSource?sourcePixel(source,uint2(0,0)):uint2(0,0);
    if(inSource && (nativePixels[topLeft.y*sourceWidth+topLeft.x]&255))
        flags|=0x02000000u;
    // Metadata follows the original, unmosaicked surface projection. Keeping
    // its palette owner handles mosaic remapping. CPU foreground coverage is
    // authoritative: HUD pixels can share the model's palette index and must
    // not inherit its normals merely because the indexed colours coincide.
    if(hasSurfaces && inSource && !(cpu&0x80000000u)
        && logical.x>=clipLeft && logical.x<clipRight && logical.y>=clipTop && logical.y<clipBottom) {
        flags|=nativeMeta&0x01ff0000;
        normal=nativeSurfaces[nativeIndex];
    }
    if(marginOrigin && !repairMargins && (logical.x<int(marginOrigin) || logical.x>=int(marginOrigin+marginWidth))) {
        value=edgeColours[matchRightMargin?1:(logical.x<int(marginOrigin)?0:1)]|(1u<<8);
        flags&=0x02000000u;normal=0;
        depth=0;temporal=0;
    }
    if(hasLate) {
        uint2 p=outputAt*uint2(lateWidth,lateHeight)/uint2(outputWidth,outputHeight);
        uint late=latePixels[p.y*lateWidth+p.x];
        if((late&0x04000000u) && (!worldOnly || nativeTag(late)!=1u || (late&0x10000000u))) {
            value=late&0x9800ffffu;
            flags&=0x02000000u;normal=0;
            depth=0;temporal=0;
        }
    }
    if(cpu&0x20000000u) {
        value=cpu&65535u;flags&=0x02000000u;normal=0;
        depth=0;temporal=0;
    }
    uint outputIndex=outputAt.y*outputWidth+outputAt.x;
    pixels[outputIndex]=nativeIndexAndTag(value)|(value&0x18000000u)|flags;surfaces[outputIndex]=normal;
    if(hasDepth) geometryDepth[outputIndex]=depth;
    if(hasMotion) motion[outputIndex]=temporal;
    uint colour=palette[value&255];
    uint3 rgb=uint3(colour&255,(colour>>8)&255,(colour>>16)&255);
    if(sourceScale>1 && (value&0x80000000u)) {
        uint other=palette[(value>>8)&255];
        rgb=(rgb+uint3(other&255,(other>>8)&255,(other>>16)&255)+1)/2;
    }
    float4 resolved=float4(rgb,colour>>24)/255.0;
    if(hasNativeMsaa || hasLateMsaa || hasBackgroundMsaa) {
        uint base=cpu&65535;
        if(worldOnly && nativeTag(base)==1) base=0;
        resolved=paletteColor(base);
        if(hasBackground && !(cpu&0xc0000000u)) {
            uint2 at=outputAt*uint2(backgroundWidth,backgroundHeight)/uint2(outputWidth,outputHeight);
            if(hasBackgroundMsaa) resolved=over(backgroundMsaa.Load(int3(at,0)),resolved);
            else {uint bg=backgroundPixels[at.y*backgroundWidth+at.x];if(bg&0x04000000U) resolved=paletteColor(bg&0x1800ffffU);}
        }
        if(marginOrigin && repairMargins && !(cpu&0xc0000000u)
            && (logical.x<int(marginOrigin) || logical.x>=int(marginOrigin+marginWidth))
            && !(backgroundAt(id.xy/scale*scale,uint2(width,height))&255u)) resolved=paletteColor(edgeColours[0]);
        if(inSource && logical.x>=clipLeft && logical.y>=clipTop && logical.x<clipRight && logical.y<clipBottom && !(cpu&0x80000000U)) {
            int2 sampleSource=source;
            if(mosaic>1) sampleSource=int2(mosaicAt(logical.x,originX),mosaicAt(logical.y,originY))-int2(offsetX,offsetY);
            if(all(sampleSource>=0) && all(sampleSource<int2(sourceReferenceWidth,sourceReferenceHeight)/int(sourceScale))) {
                uint2 at=mosaic>1?sourceSample(sampleSource,sub,outputAt,uint2(outputWidth,outputHeight)):nativeSample;
                uint ink=nativePixels[at.y*sourceWidth+at.x];
                if(!worldOnly || nativeTag(ink)!=1 || (ink&0x10000000U)) {
                    if(hasNativeMsaa) resolved=over(nativeMsaa.Load(int3(at,0)),resolved);
                    else if((ink&255U) || (sourceScale>1 && (ink&0x80000000U))) resolved=paletteColor(ink);
                }
            }
        }
        if(marginOrigin && !repairMargins && (logical.x<int(marginOrigin) || logical.x>=int(marginOrigin+marginWidth)))
            resolved=paletteColor(edgeColours[matchRightMargin?1:(logical.x<int(marginOrigin)?0:1)]);
        if(hasLate) {
            uint2 at=outputAt*uint2(lateWidth,lateHeight)/uint2(outputWidth,outputHeight);
            uint ink=latePixels[at.y*lateWidth+at.x];
            if(!worldOnly || nativeTag(ink)!=1 || (ink&0x10000000U)) {
                if(hasLateMsaa) resolved=over(lateMsaa.Load(int3(at,0)),resolved);
                else if(ink&0x04000000U) resolved=paletteColor(ink);
            }
        }
        if(cpu&0x20000000U) resolved=paletteColor(cpu&65535);
    }
    rgba[outputAt]=resolved;
#endif
}
