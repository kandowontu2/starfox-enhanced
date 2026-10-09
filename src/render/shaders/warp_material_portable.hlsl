// Decode ordered random descriptors. Material output remains occurrence-based.
StructuredBuffer<uint> descriptors : register(t0,space0);
StructuredBuffer<uint> ordered : register(t1,space0);
StructuredBuffer<int4> normals : register(t2,space0);
ByteAddressBuffer diffuse : register(t3,space0);
ByteAddressBuffer depthColours : register(t4,space0);
StructuredBuffer<uint> textureLookup : register(t5,space0);
RWStructuredBuffer<uint4> materials : register(u0,space1);
cbuffer Settings : register(b0,space2) {
    uint count,faceCount,depthBand,flags;
    int4 light;
    uint4 shadeCounts;
    uint colourBase,overrideColour,forcedColour,padding;
};
uint byteAt(ByteAddressBuffer data,uint index) {
    return (data.Load(index&~3U)>>((index&3U)*8U))&255U;
}
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    uint face=ordered[id.x],word=descriptors[id.x];
    // Invalid/hidden occurrences never become material-bearing draws.
    if(face>=faceCount || word>65535U){materials[id.x]=uint4(0,0,0,0xfffffffeU);return;}
    uint even=0,odd=0,texture=0xffffffffU;
    if((flags&1U)!=0) even=odd=(overrideColour-colourBase)&255U;
    else if((flags&2U)!=0){even=forcedColour&15U;odd=(forcedColour>>4U)&15U;}
    else if((word&49152U)==16384U){even=odd=15;if((flags&16U)==0)texture=textureLookup[word];}
    else if((word&49152U)==49152U)even=odd=word&15U;
    else {
        uint band=min(depthBand,3U),material=word>>8U,value=word&255U;
        if(material<62U && (flags&4U)!=0 && material<shadeCounts[band]) {
            int intensity=clamp((normals[face].x*light.x+normals[face].y*light.y+normals[face].z*light.z)>>10,6,15);
            value=byteAt(diffuse,(band*62U+material)*10U+uint(intensity-6));
        } else if(material==62U && (flags&8U)!=0) value=byteAt(depthColours,band*32U+(value&31U));
        even=value&15U;odd=value>>4U;
    }
    materials[id.x]=uint4((even+colourBase)&255U,(odd+colourBase)&255U,uint(even!=odd),texture);
}
