// Accepted finite specular path optics, including the native per-hop origin
// bias. This does NOT establish old visibility: callers must retain and match
// the complete ordered path, not only its last triangle, before reusing light.
// The fixed rough quadrature can have multiple valid roots. A geometric guide
// alone is never a cached-colour or visibility witness for any of those roots.
// Curved-liquid hops require their own forward surface and are not planes.
#ifndef STARFOX_REFLECTION_SPECULAR_PATH_MOTION
#define STARFOX_REFLECTION_SPECULAR_PATH_MOTION
#include "reflection_rough_hit_motion.hlsli"
struct ReflectionSpecularPlane {float4 a,b,c;};
bool reflection_specular_analytic(ReflectionSpecularPlane p) {return p.a.w==2 && p.b.w==2 && p.c.w==2;}
float3 reflection_specular_normal(ReflectionSpecularPlane p) {
    return normalize(reflection_specular_analytic(p)?p.b.xyz:cross(p.b.xyz-p.a.xyz,p.c.xyz-p.a.xyz));
}
bool reflection_specular_plane_valid(ReflectionSpecularPlane plane) {
    bool analytic=reflection_specular_analytic(plane);
    if((!analytic && (plane.a.w!=1 || plane.b.w!=1 || plane.c.w!=1)) || !all(isfinite(plane.a))
        || !all(isfinite(plane.b)) || !all(isfinite(plane.c)) || any(abs(plane.a.xyz)>1.e12)
        || any(abs(plane.b.xyz)>1.e12) || any(abs(plane.c.xyz)>1.e12)) return false;
    if(analytic && any(plane.c.xyz!=0)) return false;
    float3 n=analytic?plane.b.xyz:cross(plane.b.xyz-plane.a.xyz,plane.c.xyz-plane.a.xyz);
    float length2=dot(n,n);
    return all(isfinite(n)) && isfinite(length2) && length2>1.e-20;
}
bool reflection_specular_inside(ReflectionSpecularPlane plane,float3 position) {
    if(reflection_specular_analytic(plane)) return true;
    float3 u=plane.b.xyz-plane.a.xyz,v=plane.c.xyz-plane.a.xyz,r=position-plane.a.xyz;
    float aa=dot(u,u),ab=dot(u,v),bb=dot(v,v),ra=dot(r,u),rb=dot(r,v),det=aa*bb-ab*ab;
    if(!isfinite(det) || det<=1.e-20) return false;
    float2 bary=float2(bb*ra-ab*rb,aa*rb-ab*ra)/det;
    return all(isfinite(bary)) && all(bary>=-.00001) && bary.x+bary.y<=1.00001;
}
bool reflection_specular_forward(float2 pixel,ReflectionRoughFrame old,
    ReflectionSpecularPlane planes[4],uint hops,bool finiteFaces,
    out float3 origin,out float3 outgoing,out float depth,out float bias) {
    origin=outgoing=0;depth=bias=0;
    if(hops>4 || !reflection_rough_frame_valid(old)
        || !reflection_rough_ray(pixel,old,origin,outgoing,depth,bias)) return false;
    if(finiteFaces && !reflection_rough_finite_receiver(old,pixel,depth)) return false;
    [loop] for(uint hop=0;hop<hops;++hop) {
        ReflectionSpecularPlane plane=planes[hop];
        if(!reflection_specular_plane_valid(plane)) return false;
        float3 normal=reflection_specular_normal(plane);
        if(dot(normal,outgoing)>0) normal=-normal;
        float denominator=dot(outgoing,normal);
        if(!isfinite(denominator) || abs(denominator)<=1.e-12) return false;
        float distance=dot(plane.a.xyz-origin,normal)/denominator;
        // These are secondary path units, not primary forward-Z clipping.
        if(!isfinite(distance) || distance<=bias || distance>=65536) return false;
        float3 position=origin+outgoing*distance;
        if(!all(isfinite(position)) || (finiteFaces && !reflection_specular_inside(plane,position))) return false;
        bias=max(.05,distance*1.e-5);
        origin=position+normal*bias;outgoing=reflect(outgoing,normal);
        if(!all(isfinite(origin)) || !all(isfinite(outgoing))) return false;
    }
    return true;
}
bool reflection_specular_error(float2 pixel,ReflectionRoughFrame old,
    ReflectionSpecularPlane planes[4],uint hops,float4 terminal,
    out float2 error,out float depth) {
    error=0;depth=0;float3 origin,outgoing;float bias;
    if(!reflection_specular_forward(pixel,old,planes,hops,false,origin,outgoing,depth,bias)) return false;
    float3 travel=terminal.w==0?terminal.xyz:terminal.xyz-origin;
    float length2=dot(travel,travel);
    if(!isfinite(length2) || length2<=(terminal.w==0?1.e-20:bias*bias)) return false;
    travel*=rsqrt(length2);
    if(dot(travel,outgoing)<=0) return false;
    float3 tangent=normalize(cross(outgoing,abs(outgoing.y)<.95?float3(0,1,0):float3(1,0,0)));
    float3 bitangent=cross(outgoing,tangent);
    error=float2(dot(travel,tangent),dot(travel,bitangent))*max(old.projection.x,old.projection.y);
    return all(isfinite(error));
}
float4 reflection_specular_result(float2 pixel,ReflectionRoughFrame old,
    ReflectionSpecularPlane planes[4],uint hops,float4 terminal) {
    float3 origin,outgoing;float depth,bias;
    if(!reflection_specular_forward(pixel,old,planes,hops,true,origin,outgoing,depth,bias)) return 0;
    if(terminal.w==1) {
        float3 magnitude=max(abs(origin),abs(terminal.xyz));
        float scale=max(1.f,max(magnitude.x,max(magnitude.y,magnitude.z)));
        float travel=length(terminal.xyz-origin);
        // Keep the final segment's float-origin uncertainty separate from the
        // solver residual. An almost coincident endpoint is not a usable guide.
        if(!isfinite(travel) || scale*1.e-6*max(old.projection.x,old.projection.y)>travel*.012) return 0;
    }
    return float4(pixel,depth,1);
}
float4 reflection_specular_path_motion(ReflectionRoughFrame old,
    ReflectionSpecularPlane planes[4],uint hops,float4 terminal) {
    if(hops>4 || !reflection_rough_frame_valid(old) || !all(isfinite(terminal))
        || (terminal.w!=0 && terminal.w!=1) || any(abs(terminal.xyz)>1.e12)
        || (terminal.w==0 && dot(terminal.xyz,terminal.xyz)<=1.e-20)) return 0;
    [loop] for(uint hop=0;hop<hops;++hop) if(!reflection_specular_plane_valid(planes[hop])) return 0;
    // Reverse the ideal planar path only to choose a seed. The solve below
    // follows the actual finite path, rough first lobe and every origin bias.
    float3 virtualTerminal=terminal.xyz;
    [loop] for(uint reverse=hops;reverse>0;--reverse) {
        ReflectionSpecularPlane plane=planes[reverse-1];
        float3 normal=reflection_specular_normal(plane);
        virtualTerminal-=2*normal*dot(virtualTerminal-(terminal.w==1?plane.a.xyz:float3(0,0,0)),normal);
    }
    float3 normal=reflection_rough_normal(old);
    virtualTerminal-=2*normal*dot(virtualTerminal-(terminal.w==1?old.a.xyz:float3(0,0,0)),normal);
    float2 pixel=old.extentClip.xy*.5;
    if(all(isfinite(virtualTerminal)) && virtualTerminal.z>0)
        pixel=clamp(old.projection.xy*virtualTerminal.xy/virtualTerminal.z+old.projection.zw,
            0,old.extentClip.xy-.001);
    const float derivativeStep=.25;
    const float trust=max(8.f,min(old.extentClip.x,old.extentClip.y)*.125f);
    [loop] for(uint iteration=0;iteration<16;++iteration) {
        float2 error;float depth;
        if(!reflection_specular_error(pixel,old,planes,hops,terminal,error,depth)) return 0;
        float residual=max(abs(error.x),abs(error.y));
        if(residual<=.002) return reflection_specular_result(pixel,old,planes,hops,terminal);
        float2 ex,ey;float unused;
        float dx=pixel.x+derivativeStep<old.extentClip.x?derivativeStep:-derivativeStep;
        float dy=pixel.y+derivativeStep<old.extentClip.y?derivativeStep:-derivativeStep;
        if(!reflection_specular_error(pixel+float2(dx,0),old,planes,hops,terminal,ex,unused)
            || !reflection_specular_error(pixel+float2(0,dy),old,planes,hops,terminal,ey,unused)) return 0;
        float2 jx=(ex-error)/dx,jy=(ey-error)/dy;
        float determinant=jx.x*jy.y-jy.x*jx.y;
        if(!isfinite(determinant) || abs(determinant)<1.e-8) return 0;
        float2 step=float2(jy.y*error.x-jy.x*error.y,-jx.y*error.x+jx.x*error.y)/determinant;
        if(!all(isfinite(step))) return 0;
        step*=min(1.f,trust/max(max(abs(step.x),abs(step.y)),1.e-12));
        bool advanced=false;
        [loop] for(uint backtrack=0;backtrack<6;++backtrack) {
            float2 next=pixel-step,trial;float trialDepth;
            if(reflection_specular_error(next,old,planes,hops,terminal,trial,trialDepth)
                && max(abs(trial.x),abs(trial.y))<residual) {pixel=next;advanced=true;break;}
            step*=.5;
        }
        if(!advanced) return 0;
    }
    float2 error;float depth;
    if(reflection_specular_error(pixel,old,planes,hops,terminal,error,depth) && max(abs(error.x),abs(error.y))<=.002)
        return reflection_specular_result(pixel,old,planes,hops,terminal);
    return 0;
}
#endif
