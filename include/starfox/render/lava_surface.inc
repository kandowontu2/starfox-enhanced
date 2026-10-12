// Shared scalar surface math for C++, HLSL and GLSL. Units are game-world units.
// Filter each frequency by its projected pixel footprint before shading it.
#ifndef STARFOX_LAVA_SURFACE_INC
#define STARFOX_LAVA_SURFACE_INC
#ifndef LAVA_FN
#define LAVA_FN
#define LAVA_UINT uint
#endif
LAVA_FN float lava_sat(float x) { return min(max(x,0.f),1.f); }
LAVA_FN float lava_smooth(float a,float b,float x) {
    float f=lava_sat((x-a)/(b-a));return f*f*(3.f-2.f*f);
}
LAVA_FN float lava_hash(int x,int z) {
    LAVA_UINT n=LAVA_UINT(x)*1597334677u ^ LAVA_UINT(z)*3812015801u;
    n^=n>>16;n*=2246822519u;n^=n>>13;return float(n&65535u)/65535.f;
}
LAVA_FN float lava_noise(float x,float z) {
    int ix=int(floor(x)),iz=int(floor(z));
    float fx=x-float(ix),fz=z-float(iz);
    fx=fx*fx*(3.f-2.f*fx);fz=fz*fz*(3.f-2.f*fz);
    float a=lava_hash(ix,iz),b=lava_hash(ix+1,iz);
    float c=lava_hash(ix,iz+1),d=lava_hash(ix+1,iz+1);
    return (a+(b-a)*fx)*(1.f-fz)+(c+(d-c)*fx)*fz;
}
LAVA_FN float lava_band(float footprint,float frequency) {
    float f=footprint*frequency;return 1.f/(1.f+f*f*f*f);
}
struct LavaSample { float height,dx,dz,heat,crust,bubble; };
LAVA_FN LavaSample lava_surface(float x,float z,float t,float footprint) {
    // The same broad, crossing wavelengths as water, with slower viscous flow.
    float bend=x*.006f-z*.008f+t*.11f;
    float bend_filter=lava_band(footprint,.010f);
    float a=x*.018f+z*.011f-t*.45f+.65f*sin(bend)*bend_filter;
    float b=x*.047f-z*.025f+t*.60f;
    float c=z*.022f-x*.009f-t*.32f;
    float fa=lava_band(footprint,.022f),fb=lava_band(footprint,.054f);
    float fc=lava_band(footprint,.024f);
    LavaSample s;
    s.height=9.f*sin(a)*fa+2.5f*sin(b)*fb+5.f*sin(c)*fc;
    s.dx=9.f*(.018f+.0039f*cos(bend)*bend_filter)*cos(a)*fa+.1175f*cos(b)*fb-.045f*cos(c)*fc;
    s.dz=9.f*(.011f-.0052f*cos(bend)*bend_filter)*cos(a)*fa-.0625f*cos(b)*fb+.110f*cos(c)*fc;
    float ripple=x*.173f+z*.129f-t*.73f;
    float fr=lava_band(footprint,.216f);
    s.height+=.38f*sin(ripple)*fr;
    s.dx+=.06574f*cos(ripple)*fr;
    s.dz+=.04902f*cos(ripple)*fr;
    // Crust drifts on the liquid; it does not regenerate at each simulation tick.
    // Fine crust travels more slowly than the swell: rapid advection of thin
    // hot cracks looks like flashing, especially near the camera.
    float flowx=x+t*.9f+9.f*sin(z*.022f-x*.009f-t*.096f)*fc;
    float flowz=z-t*1.5f+12.f*sin(x*.018f+z*.011f-t*.135f)*fa;
    // Advected domain distortion breaks up parallel orange/black bands into
    // irregular cooling plates and flowing channels, without per-frame noise.
    float warp=lava_noise(flowx*.012f,flowz*.012f)-.5f;
    float coarse=.5f+(lava_noise(flowx*.085f+warp*1.8f,flowz*.085f-warp*1.3f)-.5f)*lava_band(footprint,.255f);
    float fine=(lava_noise(flowx*.29f+17.f,flowz*.29f-9.f)-.5f)*lava_band(footprint,.87f);
    float plate=coarse+fine*.20f;
    s.crust=lava_smooth(.36f,.58f,plate)*.97f;
    // Temperature follows freshly exposed channels rather than the sinusoidal
    // wave height. Hot narrow seams sit beside cooler deep-red molten folds.
    float seam=1.f-lava_smooth(.014f,.060f,abs(plate-.40f));
    s.heat=lava_sat(.50f+seam*.32f+fine*.18f+warp*.15f);
    s.bubble=0.f;
    // Sparse domes grow and collapse continuously. The lifetime envelope is
    // zero, with zero velocity, on both sides of every cycle boundary.
    float cx=floor(x/128.f),cz=floor(z/128.f);
    float seed=lava_hash(int(cx),int(cz));
    if(seed>.64f && footprint<24.f) {
        float bx=x/128.f-cx-(.28f+.44f*lava_hash(int(cx)+19,int(cz)));
        float bz=z/128.f-cz-(.28f+.44f*lava_hash(int(cx),int(cz)+29));
        float phase=t*.14f+seed*7.f;phase-=floor(phase);
        float life=sin(phase*3.14159265f);life*=life;
        float radius=.055f+.14f*phase;
        float dome=lava_sat(1.f-(bx*bx+bz*bz)/(radius*radius));
        float amplitude=10.f*life*lava_band(footprint,.18f);
        s.height+=amplitude*dome*dome*dome;
        float derivative=-6.f*amplitude*dome*dome/(128.f*radius*radius);
        s.dx+=derivative*bx;s.dz+=derivative*bz;
        s.bubble=life*dome*dome*lava_band(footprint,.18f);
        // Hot annulus as a dome opens/collapses. Its envelope and spatial
        // edges are smooth so neither the cell nor the lifetime can flash.
        float rim=life*lava_smooth(.02f,.18f,dome)*(1.f-lava_smooth(.25f,.55f,dome))*lava_band(footprint,.18f);
        s.heat=lava_sat(s.heat+s.bubble*.25f);
        s.heat=lava_sat(s.heat+rim*.22f);
        s.crust*=1.f-max(s.bubble,rim*.85f);
    }
    return s;
}
struct LavaColour { float r,g,b; };
LAVA_FN LavaColour lava_shade(LavaSample s,float vx,float vy,float vz) {
    float nl=1.f/sqrt(1.f+s.dx*s.dx+s.dz*s.dz);
    float nx=s.dx*nl,ny=-nl,nz=s.dz*nl;
    float vl=1.f/sqrt(max(.0001f,vx*vx+vy*vy+vz*vz));
    float hx=vx*vl-.57735f,hy=vy*vl-.57735f,hz=vz*vl-.57735f;
    float hl=1.f/sqrt(max(.0001f,hx*hx+hy*hy+hz*hz));
    float spec=pow(lava_sat((nx*hx+ny*hy+nz*hz)*hl),32.f);
    float light=.35f+.65f*lava_sat(-(nx+ny+nz)*.57735f);
    float heat=s.heat*s.heat,crust=s.crust;
    // Molten rock emits its own light; diffuse lighting must not turn it
    // brown on a wave's unlit side. Only the cooling crust receives diffuse
    // shading. Keep the highlight warm and restrained, not chrome-like.
    float shine=spec*(.10f+.16f*crust);
    LavaColour c;
    c.r=(.80f+.20f*s.heat)*(1.f-crust)+.065f*crust*light+shine;
    c.g=(.022f+.78f*heat*heat)*(1.f-crust)+.019f*crust*light+shine*.48f;
    c.b=(.002f+.12f*heat*heat*heat)*(1.f-crust)+.012f*crust*light+shine*.16f;
    return c;
}
#endif
