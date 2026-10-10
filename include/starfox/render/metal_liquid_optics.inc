// Generated from liquid_optics.hlsli; syntax adaptation only.
// Shared current/accepted analytic-liquid geometry. Lighting/transmission are
// deliberately outside this helper: only the reflected optical path moves.
#ifndef STARFOX_LIQUID_OPTICS
#define STARFOX_LIQUID_OPTICS
// Shared scalar helper is embedded earlier.
// Shared scalar helper is embedded earlier.
struct LiquidOpticalSample {
    float3 hit,position,normal;
    LavaSample lava;
    bool entering;
};
LiquidOpticalSample liquid_optical_sample(float3 hit,float3 direction,float distance,
    float footprint,float3x3 rotation,float3 offset,float time,uint material) {
    LiquidOpticalSample result;
    result.hit=hit;result.position=hit*rotation+offset;result.lava=LavaSample{0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
    float3 p=result.position;
    float dx=.055f*cos(p.x*.018f+p.z*.011f-time*.8f)*water_light_band(footprint,.022f)
        +.025f*cos(p.x*.047f-p.z*.025f+time*1.2f)*water_light_band(footprint,.054f);
    float dz=.045f*cos(p.z*.022f-p.x*.009f-time*.65f)*water_light_band(footprint,.024f)
        -.020f*cos(p.x*.047f-p.z*.025f+time*1.2f)*water_light_band(footprint,.054f);
    if(material==3) {
        result.lava=lava_surface(p.x,p.z,time,footprint);
        float3 travel=direction*rotation;
        float shift=clamp(-result.lava.height/max(travel.y,.12f),-distance*.2f,distance*.2f);
        result.position+=travel*shift;result.hit+=direction*shift;
        result.lava=lava_surface(result.position.x,result.position.z,time,footprint);
        dx=result.lava.dx;dz=result.lava.dz;
    }
    if(material==1 || material==2) {dx=0;dz=0;}
    result.normal=normalize(rotation*float3(dx,-1,dz));
    result.entering=dot(result.normal,direction)<0;
    if(dot(result.normal,direction)>0) result.normal=-result.normal;
    return result;
}
#endif
