// Shared CPU/HLSL animated screen-space effects. Coordinates are native pixels.
struct SpecialFxSample {float dx,dy,r,g,b,gain,keep;};
float fx_fract(float x) {return x-floor(x);}
float fx_noise(float x,float y) {
    unsigned int n=(unsigned int)((int)floor(x))*1664525u+(unsigned int)((int)floor(y))*1013904223u;
    n^=n>>16;n*=2246822519u;n^=n>>13;
    return float(n&65535u)/65535.f;
}
SpecialFxSample special_fx_sample(unsigned int effect,float x,float y,float cx,float cy,float t,float edge) {
    SpecialFxSample s;s.dx=0;s.dy=0;s.r=0;s.g=0;s.b=0;s.gain=0;s.keep=1;
    float px=x-cx,py=y-cy;
    float radius=sqrt(px*px+py*py)+.001f;
    if(effect==84) { // Hex shield lattice and a travelling recharge wave.
        float row=floor(y/9.f),hx=fx_fract((x+row*5.f)/10.f),hy=fx_fract(y/9.f);
        float hex=abs(hx-.5f)+abs(hy-.5f)*.58f;
        float lattice=1.f-FX_MIN(1.f,abs(hex-.48f)*28.f);
        s.gain=.45f*lattice+.65f*edge+.35f*FX_MAX(0.f,1.f-abs(fx_fract(y/96.f-t*.3f)-.5f)*18.f);
        s.r=.1f;s.g=.75f;s.b=1.f;
    } else if(effect==85) { // Branching arcs, continuously animated (no random frame flicker).
        float bolt=x+7.f*sin(y*.19f+t*3.f)+3.f*sin(y*.61f-t*5.f);
        float d=abs(fx_fract(bolt/38.f)-.5f)*38.f;
        float branch=abs(fx_fract((bolt+y*.7f)/61.f+t*.12f)-.5f)*61.f;
        s.gain=FX_MAX(0.f,1.f-FX_MIN(d,branch)*.65f)*(.65f+.35f*sin(t*4.f))+edge*.3f;
        s.r=.5f;s.g=.65f;s.b=1.f;
    } else if(effect==86) { // Radial flight streaks, strongest away from the vanishing point.
        float angle=atan2(py,px),lane=floor(angle*70.f);
        float stripe=FX_MAX(0.f,1.f-abs(fx_fract(angle*70.f)-.5f)*14.f);
        float travel=FX_MAX(0.f,1.f-abs(fx_fract(radius/90.f-t*1.3f+fx_noise(lane,0.f))-.5f)*5.f);
        s.gain=stripe*travel*FX_MIN(1.f,radius/65.f);s.r=.5f;s.g=.8f;s.b=1.f;
    } else if(effect==87) { // Reversible cellular disintegration with glowing borders.
        float threshold=.5f+.48f*sin(t*.9f),n=fx_noise(floor(x/3.f),floor(y/3.f));
        s.keep=n>threshold?1.f:.12f;
        s.gain=FX_MAX(0.f,1.f-abs(n-threshold)*20.f);s.r=1.f;s.g=.5f;s.b=.08f;
    } else if(effect==88) {
        float sweep_height=FX_MAX(1.f,cy*2.f);
        float band=abs(fx_fract(y/sweep_height-t*.22f)-.5f)*sweep_height;
        s.gain=FX_MAX(0.f,1.f-band/5.f)+edge*FX_MAX(0.f,1.f-band/30.f);
        s.r=.1f;s.g=1.f;s.b=.35f;
    } else if(effect==89) { // Angular crystal branches spread and recede.
        float branches=FX_MIN(abs(sin(x*.24f+y*.14f)),abs(sin(x*.24f-y*.14f)));
        float growth=.5f+.5f*sin(t*.45f-y*.012f);
        s.gain=FX_MAX(0.f,1.f-branches*13.f)*growth*.8f+edge*growth*.5f;
        s.r=.65f;s.g=.9f;s.b=1.f;
    } else if(effect==90) {
        s.dx=(sin(y*.16f-t*3.f)+.5f*sin(y*.39f+t*1.7f))*3.f;
        s.dy=sin(x*.12f+t*2.f)*1.5f;
    } else if(effect==91) {
        float ring=exp(-abs(radius-48.f-8.f*sin(t*.6f))*.07f);
        s.dx=px/radius*ring*12.f;s.dy=py/radius*ring*12.f;
        s.gain=ring*.08f;s.r=.45f;s.g=.3f;s.b=1.f;
    }
    return s;
}
