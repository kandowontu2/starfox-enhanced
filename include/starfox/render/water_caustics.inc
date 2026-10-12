// Scalar C++/HLSL-compatible reference. The normal field matches ray-water.
// Coordinates and receiver depth are game-world units, time is seconds.
#ifndef STARFOX_WATER_CAUSTICS_INC
#define STARFOX_WATER_CAUSTICS_INC
#ifndef WC_FN
#define WC_FN
#endif
struct WaterLightLanding {float x,z;};
WC_FN float water_light_band(float footprint,float frequency) {
    float f=footprint*frequency;return 1.f/(1.f+f*f*f*f);
}
WC_FN WaterLightLanding water_light_landing(float x,float z,float time,float depth,float footprint) {
    float a=x*.018f+z*.011f-time*.8f;
    float b=x*.047f-z*.025f+time*1.2f;
    float c=z*.022f-x*.009f-time*.65f;
    float nx=.055f*cos(a)*water_light_band(footprint,.022f)+.025f*cos(b)*water_light_band(footprint,.054f);
    float nz=.045f*cos(c)*water_light_band(footprint,.024f)-.020f*cos(b)*water_light_band(footprint,.054f);
    float inverse=1.f/sqrt(1.f+nx*nx+nz*nz);
    // Snell refraction from air into water (n=1.333...), vertical sunlight.
    float eta=.75f;
    float k=eta*inverse-sqrt(1.f-eta*eta*(1.f-inverse*inverse));
    float down=eta-k*inverse;
    WaterLightLanding result;
    // Store displacement, not absolute landing coordinates. Differencing large
    // world positions would lose the small Jacobian terms to float cancellation.
    result.x=depth*k*nx*inverse/down;
    result.z=depth*k*nz*inverse/down;
    return result;
}
struct WaterLightDifferential {float x,z,xx,xz,zx,zz;};
WC_FN WaterLightDifferential water_light_differential(float x,float z,float time,float depth,float footprint) {
    float a=x*.018f+z*.011f-time*.8f,b=x*.047f-z*.025f+time*1.2f,c=z*.022f-x*.009f-time*.65f;
    float fa=water_light_band(footprint,.022f),fb=water_light_band(footprint,.054f),fc=water_light_band(footprint,.024f);
    float nx=.055f*cos(a)*fa+.025f*cos(b)*fb,nz=.045f*cos(c)*fc-.020f*cos(b)*fb;
    float sa=sin(a)*fa,sb=sin(b)*fb,sc=sin(c)*fc;
    float nx_x=-.055f*.018f*sa-.025f*.047f*sb,nx_z=-.055f*.011f*sa+.025f*.025f*sb;
    float nz_x=.045f*.009f*sc+.020f*.047f*sb,nz_z=-.045f*.022f*sc-.020f*.025f*sb;
    // Closed-form horizontal Snell displacement: depth * F(|slope|^2) * slope.
    float eta=.75f,s=nx*nx+nz*nz,A=sqrt(1.f+(1.f-eta*eta)*s),den=eta*s+A;
    float F=(eta-A)/den,Ap=(1.f-eta*eta)/(2.f*A);
    float derivative=eta*(A-eta-Ap*(s+1.f))/(den*den);
    float xx=F+2.f*derivative*nx*nx,xz=2.f*derivative*nx*nz,zz=F+2.f*derivative*nz*nz;
    WaterLightDifferential result;
    result.x=depth*F*nx;result.z=depth*F*nz;
    result.xx=depth*(xx*nx_x+xz*nz_x);result.xz=depth*(xx*nx_z+xz*nz_z);
    result.zx=depth*(xz*nx_x+zz*nz_x);result.zz=depth*(xz*nx_z+zz*nz_z);
    return result;
}
struct WaterCausticSample {float irradiance,entry_x,entry_z;};
WC_FN WaterCausticSample water_caustic_sample(float x,float z,float time,float depth,float footprint) {
    WaterCausticSample result;
    result.irradiance=0.f;result.entry_x=x;result.entry_z=z;
    if(depth<=0.f) return result; // Never illuminate dry/above-water geometry.
    footprint=max(0.f,footprint);
    // Invert the refracted light landing map: receive light at THIS world
    // position rather than drawing the wave's curvature directly on the floor.
    float sx=x,sz=z;
    float jxx=1.f,jxz=0.f,jzx=0.f,jzz=1.f;
    for(int iteration=0;iteration<4;++iteration) {
        WaterLightDifferential p=water_light_differential(sx,sz,time,depth,footprint);
        jxx=1.f+p.xx;jzx=p.zx;jxz=p.xz;jzz=1.f+p.zz;
        float determinant=jxx*jzz-jxz*jzx;
        if(abs(determinant)<.08f) break;
        float ex=sx-x+p.x,ez=sz-z+p.z;
        sx-=min(24.f,max(-24.f,(jzz*ex-jxz*ez)/determinant));
        sz-=min(24.f,max(-24.f,(-jzx*ex+jxx*ez)/determinant));
    }
    // Finite pixel footprints broaden singular focus lines rather than letting
    // them shimmer or become infinite. Beer attenuation limits deep receivers.
    float determinant=abs(jxx*jzz-jxz*jzx);
    float irradiance=min(6.f,1.f/max(.1666667f,determinant));
    result.irradiance=irradiance*exp(-depth/1600.f);
    result.entry_x=sx;result.entry_z=sz;
    return result;
}
WC_FN float water_caustic_irradiance(float x,float z,float time,float depth,float footprint) {
    return water_caustic_sample(x,z,time,depth,footprint).irradiance;
}
#endif
