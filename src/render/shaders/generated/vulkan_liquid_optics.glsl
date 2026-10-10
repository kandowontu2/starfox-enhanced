// Generated from liquid_optics.hlsli; do not edit.
// Shared current/accepted analytic-liquid geometry. Lighting/transmission are
// deliberately outside this helper: only the reflected optical path moves.
#ifndef STARFOX_LIQUID_OPTICS
#define STARFOX_LIQUID_OPTICS
#include "../../../../include/starfox/render/water_caustics.inc"
#include "../../../../include/starfox/render/lava_surface.inc"
struct LiquidOpticalSample {
    vec3 hit,position,normal;
    LavaSample lava;
    bool entering;
};
LiquidOpticalSample liquid_optical_sample(vec3 hit,vec3 direction,float distance,
    float footprint,mat3 rotation,vec3 offset,float time,uint material) {
    LiquidOpticalSample result;
    result.hit=hit;result.position=(hit*rotation)+offset;result.lava=LavaSample(0.,0.,0.,0.,0.,0.);
    vec3 p=result.position;
    float dx=.055*cos(p.x*.018+p.z*.011-time*.8)*water_light_band(footprint,.022)
        +.025*cos(p.x*.047-p.z*.025+time*1.2)*water_light_band(footprint,.054);
    float dz=.045*cos(p.z*.022-p.x*.009-time*.65)*water_light_band(footprint,.024)
        -.020*cos(p.x*.047-p.z*.025+time*1.2)*water_light_band(footprint,.054);
    if(material==3) {
        result.lava=lava_surface(p.x,p.z,time,footprint);
        vec3 travel=(direction*rotation);
        float shift=clamp(-result.lava.height/max(travel.y,.12f),-distance*.2f,distance*.2f);
        result.position+=travel*shift;result.hit+=direction*shift;
        result.lava=lava_surface(result.position.x,result.position.z,time,footprint);
        dx=result.lava.dx;dz=result.lava.dz;
    }
    if(material==1 || material==2) {dx=0;dz=0;}
    result.normal=normalize((rotation*vec3(dx,-1,dz)));
    result.entering=dot(result.normal,direction)<0;
    if(dot(result.normal,direction)>0) result.normal=-result.normal;
    return result;
}
#endif
