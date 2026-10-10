#ifndef STARFOX_CALIBRATED_COLOUR_HLSLI
#define STARFOX_CALIBRATED_COLOUR_HLSLI
// Palette-only styles use the authored 8-bit colour domain, as on the flat
// renderer. XR sRGB targets receive linear inputs, so encode before the
// integer equations and decode afterwards. Do not style alpha or native UI.
float3 calibrated_encode_srgb(float3 rgb) {
    return float3(rgb.r<=.0031308?rgb.r*12.92:1.055*pow(rgb.r,1./2.4)-.055,
                  rgb.g<=.0031308?rgb.g*12.92:1.055*pow(rgb.g,1./2.4)-.055,
                  rgb.b<=.0031308?rgb.b*12.92:1.055*pow(rgb.b,1./2.4)-.055);
}
float3 calibrated_decode_srgb(float3 encoded) {
    return float3(encoded.r<=.04045?encoded.r/12.92:pow((encoded.r+.055)/1.055,2.4),
                  encoded.g<=.04045?encoded.g/12.92:pow((encoded.g+.055)/1.055,2.4),
                  encoded.b<=.04045?encoded.b/12.92:pow((encoded.b+.055)/1.055,2.4));
}
bool calibrated_palette_colour(float4 colour,uint4 effects,out float4 output) {
    output=colour;
    if(effects.x==0 || effects.y==0) return true;
    bool srgb=(effects.z&1U)!=0;
    int3 rgb=int3(round(saturate(srgb?calibrated_encode_srgb(colour.rgb):colour.rgb)*255.));
    int light=(rgb.r*77+rgb.g*150+rgb.b*29)/256;
    int3 value=rgb;
    switch(effects.x) {
    case 4: value=light.xxx;break;
    case 8: value=min(255,int3(dot(rgb,int3(101,197,48)),dot(rgb,int3(89,176,43)),dot(rgb,int3(70,137,34)))/256);break;
    case 9: {
        const int3 palette[5]={int3(8,5,40),int3(65,20,150),int3(220,30,70),int3(255,150,15),int3(255,255,210)};
        int band=min(light/64,3),fraction=light-band*64;
        value=(palette[band]*(64-fraction)+palette[band+1]*fraction)/64;break;
    }
    case 11: value=96+rgb*5/8;break;
    case 14: value=min(255,((rgb+25)/51)*51);break;
    case 15: value=(int3(8,24,65)*(255-light)+int3(224,250,246)*light)/255;break;
    case 16: value=min(255,(rgb*rgb*(765-2*rgb)/65025)*int3(106,100,87)/100+int3(9,4,6));break;
    case 17: value=255-rgb;break;
    case 18: value=int3(rgb.r<128?rgb.r*2:(255-rgb.r)*2,rgb.g<128?rgb.g*2:(255-rgb.g)*2,rgb.b<128?rgb.b*2:(255-rgb.b)*2);break;
    case 19: value=light*int3(255,176,32)/255;break;
    case 20: value=light*int3(55,255,130)/255;break;
    case 21: value=(int3(8,22,70)*(255-light)+int3(224,246,240)*light)/255;break;
    case 22: value=(int3(30,10,8)*(255-light)+int3(255,190,120)*light)/255;break;
    case 23: value=(int3(35,12,65)*(255-light)+int3(250,215,255)*light)/255;break;
    case 24: {
        const int3 palette[4]={int3(0,0,0),int3(0,170,170),int3(170,0,170),int3(255,255,255)};
        value=palette[min(3,light/64)];break;
    }
    case 31: value=(rgb+2*((int3(0,64,80)*(255-light)+int3(255,180,92)*light)/255))/3;break;
    case 32: {
        const int3 palette[4]={int3(15,56,15),int3(48,98,48),int3(139,172,15),int3(155,188,15)};
        value=palette[min(3,light/64)];break;
    }
    case 36: {
        int3 overlay=light<128?2*rgb*light/255:255-2*(255-rgb)*(255-light)/255;
        value=clamp((((overlay+3*light)/4)-112)*7/4+112,0,255);break;
    }
    case 38: {
        int red=max(0,rgb.r-rgb.b),ink=(255-light)*3/4;
        value=clamp(int3(250,237,208)-ink*int3(205,155,65)/255-red*int3(0,110,120)/255,0,255);break;
    }
    case 64: value=(int3(14,17,54)*(255-light)+int3(255,189,101)*light)/255;break;
    case 65: value=light<128?(int3(10,48,71)*(128-light)+int3(194,58,112)*light)/128
        :(int3(194,58,112)*(255-light)+int3(255,239,193)*(light-128))/127;break;
    case 69: {
        int3 phase=(light*3+rgb.r-rgb.g+int3(0,111,222)+768)%384;
        value=clamp(36+max(0,255-abs(phase-192)*3)*3/4,0,255);break;
    }
    case 71: {
        int grey=clamp((light-95)*2+80,0,255);
        bool red=rgb.r>rgb.g*6/5 && rgb.r>rgb.b*6/5 && rgb.r>90;
        value=red?int3(max(grey,rgb.r),grey/5,grey/5):grey.xxx;break;
    }
    case 72: {
        int energy=max(max(rgb.r,rgb.g),rgb.b);
        value=clamp(int3(12,5,34)+energy*int3(130,65,215)/255+max(0,energy-150)*int3(80,170,40)/105,0,255);break;
    }
    case 73: value=light%32<4?int3(239,229,170):clamp(int3(18,46,34)+(light/32)*int3(23,19,12),0,255);break;
    // Outlines, neighbourhood sampling, temporal effects and materials need
    // their own passes; a palette tint is not an implementation of those.
    default: return false;
    }
    int intensity=int(min(effects.y,100U));
    float3 encoded=float3((rgb*(100-intensity)+value*intensity+50)/100)/255.;
    output=float4(srgb?calibrated_decode_srgb(encoded):encoded,colour.a);
    return true;
}
#endif
