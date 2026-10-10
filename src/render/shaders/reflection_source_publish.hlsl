// Copy ONLY whole certified pixel packets to a separate owner-owned image.
// CURRENT is never bound writable; the image starts as an exact CURRENT copy.
ByteAddressBuffer packets:register(t0,space0);
RWByteAddressBuffer image:register(u0,space1);
#if defined(STARFOX_SOURCE_ADMISSION_DIAGNOSTICS)
// Test-only discrete admission evidence. Never read by a colour/optical stage.
RWByteAddressBuffer admission:register(u1,space1);
#endif
cbuffer PublishSettings:register(b0,space2) {
    uint publishWidth,publishHeight,publishRecords,publishPathStride;
    uint publishFirst,publishCount,publishLobes,publishReserved;
};
[numthreads(64,1,1)]
void feature_publish_main(uint3 id:SV_DispatchThreadID) {
    if((publishLobes!=1 && publishLobes!=8) || !publishWidth || !publishHeight
        || publishWidth>16384 || publishHeight>16384 || publishReserved
        || (publishPathStride!=52 && publishPathStride!=64) || publishRecords>64
        || !publishCount || publishCount>64 || publishCount%publishLobes || publishFirst%publishLobes)return;
    const uint first=id.x*publishLobes;if(first>=publishCount)return;
    const uint record=publishFirst+first,pixel=record/publishLobes,total=publishWidth*publishHeight;
    if(record<publishFirst || pixel>=total)return;
    const uint packet=first*24576+512;const uint4 header=packets.Load4(packet);
    const uint canonical=packets.Load(packet+16),mask=header.w;
    if(header.x!=1 || header.y || header.z!=pixel || !mask || mask>=(1U<<publishLobes))return;
    const uint type=image.Load(pixel*4)&0xff000000U;
    if((canonical&0xff000000U)!=type)return;
    // Validate the ENTIRE group before any canonical/incident write. A later
    // malformed lobe must not publish an earlier lobe or a half-updated pixel.
    [loop] for(uint l=0;l<publishLobes;++l) {
        const uint at=(first+l)*24576+512;
        if(any(packets.Load4(at)!=header) || packets.Load(at+16)!=canonical || packets.Load(at+28)
            || packets.Load(at+44) || any(packets.Load4(at+48))
            || any(packets.Load3(at+32)!=packets.Load3(packet+32)))return;
        const uint path=total*publishRecords+(record+l)*publishPathStride;
        const uint incoming=packets.Load(at+20);
        if(mask&(1U<<l)) {if(incoming>>24!=255)return;}
        else if(incoming!=image.Load(path+36))return;
    }
    image.Store(pixel*4,canonical);
    [loop] for(uint l=0;l<publishLobes;++l) {
        const uint path=total*publishRecords+(record+l)*publishPathStride;
        image.Store(path+36,packets.Load((first+l)*24576+532));
    }
#if defined(STARFOX_SOURCE_ADMISSION_DIAGNOSTICS)
    admission.Store(pixel*4,mask);
#endif
    // No material/path/geometry/index field or refused packet is rewritten.
}
