#ifndef STARFOX_REFLECTION_LOBE_RECORD
#define STARFOX_REFLECTION_LOBE_RECORD
uint reflection_lobe_bary_pack(float2 bary) {
    uint2 value=uint2(floor(saturate(bary)*65535+.5));
    value.y=min(value.y,65535U-value.x); // Rounded edge remains inside its triangle.
    return value.x|(value.y<<16);
}
float2 reflection_lobe_bary(uint word) {return float2(word&65535U,word>>16)/65535.;}
#endif
