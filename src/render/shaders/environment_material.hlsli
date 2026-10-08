#include "../../../include/starfox/render/lava_surface.inc"
float environmentHash(int a,int b) {
    uint n=uint(a)*1597334677u ^ uint(b)*3812015801u;
    n^=n>>16;n*=2246822519u;n^=n>>13;return float(n&65535u)/65535.f;
}
float environment_noise(float x,float y) {
    int ix=int(floor(x)),iy=int(floor(y));
    float fx=x-float(ix),fy=y-float(iy);fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
    float a=environmentHash(ix,iy),b=environmentHash(ix+1,iy),c=environmentHash(ix,iy+1),d=environmentHash(ix+1,iy+1);
    return (a+(b-a)*fx)*(1-fy)+(c+(d-c)*fx)*fy;
}
float2 backdropMotion(float2 uv,uint motion,float time) {
    float sky=clamp((.72f-uv.y)/.20f,0.f,1.f);
    if(motion==1) uv.x+=time*.006f*sky;
    if(motion==2) {float u=uv.x;uv.x+=sin(uv.y*6+time*.15f)*.012f*sky;uv.y+=cos(u*9+time*.1f)*.008f*sky;}
    return uv;
}
float3 backdropStyle(float3 c,float2 uv,uint style,float time) {
    float sky=clamp((.72f-uv.y)/.20f,0.f,1.f);
    if(style==2) {
        float wisps=pow(max(0.f,sin(uv.y*95+environment_noise(uv.x*12,uv.y*4)*7)),12.f)*sky*.13f;
        c+=(255-c)*wisps;
    }
    if(style==3) {
        float ribbon=pow(max(0.f,sin(uv.x*21+sin(uv.y*9+time*.08f)*2)),8.f)*sky;
        c=c*float3(.20f,.28f,.40f)+ribbon*float3(30,145,90);
    }
    if(style==4) {float light=c.r*.3f+c.g*.59f+c.b*.11f;c=float3(light*1.12f+12*sky,light*.68f,light*.50f+10*sky);}
    return clamp(c,0.f,255.f);
}
float3 environmentColour(float3 c,uint kind,float x,float y,uint4 m,float4 v,float slope,float brightness) {
    v.x+=slope*x;
    const float3 authored=c;
    bool is_lava=false;
    const float t=v[3];float detail=0;float3 tint=float3(1,1,1);
    if(kind<=5 && m[0]) {
        if(m[0]>1) kind=m[0]-1;
        is_lava=kind==9;
        const float distance=max(kind==9?16.f:4.f,y-v[0]);
        float u=x*24.f/distance+v[1]*.015625f,z=2048.f/distance+v[2]*.015625f;
        if(m[1]==1) u+=sin(z*.17f+t)*.6f;
        if(m[1]==2) z+=sin(u*.2f+t*1.5f)*.8f;
        // Fine detail fades at the horizon instead of aliasing into shimmer.
        const float near=clamp(distance/100.f,0.f,1.f);
        // Preserve native gradients on basic ground; relief supplies shape.
        if(kind==5) {
            const float wave=sin(z*.42f+sin(u*.09f)*2-t*.8f);
            const float glint=pow(max(0.f,sin(u*.035f+wave*.23f)),16.f);
            const float b=sin(u*2.7f+z*1.3f)*sin(z*3.1f-u*.9f);
            detail=wave*.10f+b*.025f*near+glint*.30f;tint=float3(.86f,1.06f,1.18f);
        }
        if(kind==8) {
            float broad=environment_noise(u*.10f,z*.10f)*2-1;
            detail=broad*.025f;
        }
        if(kind==9) {
            float footprint=max(384.f/distance,(32768.f+abs(x)*384.f)/(distance*distance));
            LavaSample surface=lava_surface(x*384.f/distance+v[1],32768.f/distance+v[2],t,footprint);
            LavaColour molten=lava_shade(surface,-x/256.f,-distance/256.f,-1.f);
            c=float3(molten.r,molten.g,molten.b)*(255.f*saturate(brightness));
        }
        { // Auto and an explicit selection share the same material response.
            const float light=(c[0]*.3f+c[1]*.59f+c[2]*.11f);
            if(m[0]>1 && kind==1) c=float3(light*.70f,light*1.20f,light*.56f);
            if(m[0]>1 && kind==2) c=float3(light*1.25f,light*.95f,light*.67f);
            if(m[0]>1 && kind==3) c=float3(light*1.30f,light*1.15f,light*.75f);
            if(m[0]>1 && kind==4) c=float3(light*.88f,light*.93f,light*.98f);
            if(kind==5) c=float3(light*.46f,light*.95f,light*1.50f);
            if(kind==6) c=float3(light*1.15f,light*1.18f,light*1.22f);
            if(kind==7) c=float3(light*1.55f,light*1.15f,light*.45f);
            if(kind==8) c=float3(light*1.27f,light*.58f,light*.43f);
        }
        if(kind==2 && m[0]>1) {
            const float light=c[0]*.3f+c[1]*.59f+c[2]*.11f;
            c=c*.2f+light*float3(.864f,.768f,.64f);
        }
        if(m[1]==3) detail+=sin(t*1.5f+z*.1f)*.08f;
        const float fade=clamp(distance/32.f,0.f,1.f);
        detail*=fade;
        for(uint i=0;i<3;++i) tint[i]=1+(tint[i]-1)*fade;
    } else if(kind==6 && m[2]) {
        float u=x*.018f,w=(y-slope*x)*.03f;
        if(m[3]==1) u+=t*.04f;
        if(m[3]==2) {u+=sin(w+t*.15f)*.5f;w+=cos(u+t*.1f)*.3f;}
        const float cloud=environment_noise(u,w)*.65f+environment_noise(u*2.7f,w*2.7f)*.35f;
        if(m[2]==1) detail=max(0.f,cloud-.35f)*.65f;
        if(m[2]==2) detail=max(0.f,sin(w*2+sin(u)*2))*.08f;
        if(m[2]==3) {detail=pow(max(0.f,sin(u*1.7f+sin(w*.4f)*2)),6.f)*.20f;tint=float3(.85f,1.12f,1.14f);}
        if(m[2]==4) {detail=cloud*.035f;tint=float3(1.23f,.92f,.85f);}
    } else return c;
    // Multiplicative radiance preserves fades, black and authored brightness.
    for(uint i=0;i<3;++i) c[i]=clamp(c[i]*(tint[i]+detail),0.f,255.f);
    if(is_lava) {
        // Ownership is already limited to the authored ground class. Keep a
        // narrow anti-aliased seam at the actual sloped horizon, then cover
        // the whole floor instead of leaving a half-screen red underlay.
        float edge=saturate((y-v[0])/8.f);
        edge=edge*edge*(3.f-2.f*edge);
        c=lerp(authored,c,edge);
    }
    return c;
}
