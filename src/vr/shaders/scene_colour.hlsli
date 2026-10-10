// Shared model colour evaluation for rasterization and native ray materials.
// The calibrated wrapper defines STARFOX_CALIBRATED_PALETTE and its integer
// palette equations; headset rendering keeps its established approximations.
float4 scene_styled_colour(float4 colour,uint4 effects) {
#if defined(STARFOX_CALIBRATED_PALETTE)
    float4 palette_colour;
    if(calibrated_palette_colour(colour,effects,palette_colour)) return palette_colour;
#endif
    uint style=effects.x;
    if(style==0 || effects.y==0) return colour;
    float3 rgb=colour.rgb,result=rgb;
    float light=dot(rgb,float3(77,150,29))/256;
    if(style==1) {
        float peak=max(max(rgb.r,rgb.g),max(rgb.b,1./255));
        result=rgb*(min(1.,floor(peak*5.+.5)/5.)/peak);
    }
    else if(style==4) result=light.xxx;
    else if(style==8) result=saturate(float3(dot(rgb,float3(101,197,48)),
        dot(rgb,float3(89,176,43)),dot(rgb,float3(70,137,34)))/256);
    else if(style==9) {
        const float3 palette[5]={float3(8,5,40),float3(65,20,150),float3(220,30,70),
            float3(255,150,15),float3(255,255,210)};
        float value=light*255/64;uint band=min(uint(value),3U);
        result=lerp(palette[band],palette[band+1],value-band)/255;
    } else if(style==10) result=saturate(float3(light/7,24./255+light*1.2,light/4));
    else if(style==11) result=96./255+rgb*5/8;
    else if(style==13) result=lerp(float3(55,8,100),float3(70,250,245),light)/255;
    else if(style==14) result=floor(saturate(rgb)*5+.5)/5;
    else if(style==15) result=lerp(float3(8,24,65),float3(224,250,246),light)/255;
    else if(style==16) {
        float3 contrast=rgb*rgb*(3-2*rgb);
        result=saturate(contrast*float3(1.06,1.,.87)+float3(.035,.015,.025));
    }
    return float4(lerp(rgb,result,min(effects.y,100U)/100.),colour.a);
}
