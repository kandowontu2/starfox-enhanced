// RT-OFF liquid appearance uses the desktop water's broad crossing wave,
// ripples and glints, evaluated at a retained source-world point for each eye.
// This is not transmission/reflection transport; that remains the RT finish.
float calibrated_water_band(float footprint,float frequency) {
    float f=footprint*frequency;return 1./(1.+f*f*f*f);
}
float3 calibrated_water_ripples(float3 colour,float2 world,float seconds,uint motion,
    float distance,float footprint) {
    float2 uv=world/64.;
    if(motion==1) uv.x+=sin(uv.y*.17+seconds)*.6;
    if(motion==2) uv.y+=sin(uv.x*.2+seconds*1.5)*.8;
    // Derivative footprint is in source units; the authored waves use /64.
    // Attenuate unresolved ripples, not the live palette ramp or broad tint.
    float sampleWidth=footprint/64.;
    float wave=sin(uv.y*.42+sin(uv.x*.09)*2-seconds*.8)
        *calibrated_water_band(sampleWidth,.46);
    float glint=pow(max(0.,sin(uv.x*.035+wave*.23)),16.)
        *calibrated_water_band(sampleWidth,.40);
    float ripple=sin(uv.x*2.7+uv.y*1.3)*sin(uv.y*3.1-uv.x*.9)
        *calibrated_water_band(sampleWidth,6.);
    float detail=wave*.10+ripple*.025*saturate(distance/100.)+glint*.30;
    if(motion==3) detail+=sin(seconds*1.5+uv.y*.1)*.08;
    float fade=saturate(distance/32.);
    return saturate(colour*(lerp(float3(1,1,1),float3(.86,1.06,1.18),fade)+detail*fade));
}
