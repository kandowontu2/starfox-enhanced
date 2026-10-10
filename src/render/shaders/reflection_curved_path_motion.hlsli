// Ordered finite/analytic/animated-liquid inverse optics. This is a geometric
// guide only: the caller must still match EVERY retained old hop and terminal,
// old coverage/depth/feature footprint, and keep all material/base light CURRENT.
// Do not enable colour reuse by substituting an analytic plane for a liquid.
// The exceptional restart budget is diagnostic-stage geometry, not a promise
// of production frame cost or approval to remove native history exclusions.
#ifndef STARFOX_REFLECTION_CURVED_PATH_MOTION
#define STARFOX_REFLECTION_CURVED_PATH_MOTION
#include "reflection_specular_path_motion.hlsli"
#include "reflection_liquid_hit_motion.hlsli"
#include "reflection_curved_precision.hlsli"

bool reflection_curved_frame_valid(ReflectionRoughFrame receiver,ReflectionLiquidFrame liquid,
    ReflectionSpecularPlane planes[4],uint hops,uint liquidMask,uint liquidReceiver) {
    if(hops>4 || liquidReceiver>1 || (liquidMask>>hops)!=0
        || !reflection_liquid_frame_valid(liquid) || any(liquid.settings.zw!=0)
        || any(receiver.projection!=liquid.projection) || any(receiver.extentClip!=liquid.extentClip)) return false;
    // The native floor ray is sharp even when the model uses eight rough lobes.
    if(liquidReceiver!=0) {
        if(any(receiver.settings!=0)) return false;
    } else if(!reflection_rough_frame_valid(receiver)) return false;
    [loop] for(uint hop=0;hop<hops;++hop)
        if((liquidMask&(1U<<hop))==0 && !reflection_specular_plane_valid(planes[hop])) return false;
    return true;
}

// The secondary ray has an arbitrary origin. Its screen footprint is measured
// from the eye to the analytic hit, as in native_water_sample, NOT from the last
// mirror. The lava displacement, time, affine normal transform and origin bias
// are evaluated anew at this hop. No primary depth clipping on secondary rays.
bool reflection_curved_liquid_hop(ReflectionLiquidFrame liquid,inout CurvedVector origin,
    inout CurvedVector outgoing,inout float2 bias) {
    CurvedVector planeNormal=curved_vec_float(liquid.planeNormal.xyz);
    float2 denominator=curved_vec_dot(outgoing,planeNormal);
    if(!curved_dd_less(float2(1.e-8,0),curved_dd_abs(denominator))) return false;
    float2 distance=curved_dd_div(curved_vec_dot(curved_vec_sub(curved_vec_float(liquid.planePoint.xyz),origin),planeNormal),denominator);
    if(!curved_dd_less(bias,distance) || !curved_dd_less(distance,float2(65536,0))) return false;
    CurvedVector hit=curved_vec_add(origin,curved_vec_scale(outgoing,distance)),normal;
    float2 footprint=curved_dd_div(curved_dd_div(curved_vec_length(hit),float2(max(liquid.projection.x,1.f),0)),curved_dd_max(curved_dd_abs(denominator),curved_dd_ratio(4,100)));
    if(!curved_liquid_sample(liquid,outgoing,distance,footprint,hit,normal)) return false;
    bias=curved_dd_max(curved_dd_ratio(5,100),curved_dd_mul(distance,curved_dd_ratio(1,100000)));
    origin=curved_vec_add(hit,curved_vec_scale(normal,bias));outgoing=curved_vec_reflect(outgoing,normal);
    return all(isfinite(curved_vec_result(origin))) && all(isfinite(curved_vec_result(outgoing))) && all(isfinite(bias));
}

CurvedVector reflection_curved_normal(ReflectionSpecularPlane plane) {
    CurvedVector a=curved_vec_float(plane.a.xyz),b=curved_vec_float(plane.b.xyz),c=curved_vec_float(plane.c.xyz);
    if(reflection_specular_analytic(plane)) return curved_vec_unit(b);
    return curved_vec_unit(curved_vec_cross(curved_vec_sub(b,a),curved_vec_sub(c,a)));
}
bool reflection_curved_inside(ReflectionSpecularPlane plane,CurvedVector position) {
    if(reflection_specular_analytic(plane)) return true;
    CurvedVector a=curved_vec_float(plane.a.xyz),u=curved_vec_sub(curved_vec_float(plane.b.xyz),a),
        v=curved_vec_sub(curved_vec_float(plane.c.xyz),a),r=curved_vec_sub(position,a);
    float2 aa=curved_vec_dot(u,u),ab=curved_vec_dot(u,v),bb=curved_vec_dot(v,v),ra=curved_vec_dot(r,u),rb=curved_vec_dot(r,v);
    float2 determinant=curved_dd_sub(curved_dd_mul(aa,bb),curved_dd_mul(ab,ab));
    if(!curved_dd_less(float2(1.e-20,0),determinant)) return false;
    float2 x=curved_dd_div(curved_dd_sub(curved_dd_mul(bb,ra),curved_dd_mul(ab,rb)),determinant);
    float2 y=curved_dd_div(curved_dd_sub(curved_dd_mul(aa,rb),curved_dd_mul(ab,ra)),determinant);
    float2 tolerance=curved_dd_ratio(1,100000);
    return all(isfinite(x)) && all(isfinite(y)) && !curved_dd_less(x,-tolerance) && !curved_dd_less(y,-tolerance)
        && !curved_dd_less(curved_dd_add(float2(1,0),tolerance),curved_dd_add(x,y));
}
bool reflection_curved_forward_precise(float2 pixel,ReflectionRoughFrame receiver,ReflectionLiquidFrame liquid,
    ReflectionSpecularPlane planes[4],uint hops,uint liquidMask,uint liquidReceiver,bool finiteFaces,
    out CurvedVector origin,out CurvedVector outgoing,out float2 depth,out float2 bias) {
    origin=outgoing=curved_vec_float(0);depth=bias=0;
    if(!reflection_curved_frame_valid(receiver,liquid,planes,hops,liquidMask,liquidReceiver)
        || !all(isfinite(pixel)) || any(pixel<0) || any(pixel>=receiver.extentClip.xy)) return false;
    CurvedVector incident=curved_vec_unit(curved_vec(
        curved_dd_div(curved_dd_sub(float2(pixel.x,0),float2(receiver.projection.z,0)),float2(receiver.projection.x,0)),
        curved_dd_div(curved_dd_sub(float2(pixel.y,0),float2(receiver.projection.w,0)),float2(receiver.projection.y,0)),float2(1,0)));
    ReflectionSpecularPlane primary;primary.a=receiver.a;primary.b=receiver.b;primary.c=receiver.c;
    CurvedVector normal=curved_vec_float(liquid.planeNormal.xyz);
    if(liquidReceiver==0) normal=reflection_curved_normal(primary);
    if(liquidReceiver==0 && curved_dd_less(0,curved_vec_dot(normal,incident))) normal=curved_vec_scale(normal,float2(-1,0));
    float2 denominator=curved_vec_dot(incident,normal);
    if(!curved_dd_less(float2(1.e-12,0),curved_dd_abs(denominator))) return false;
    CurvedVector planePoint=curved_vec_float(liquidReceiver!=0?liquid.planePoint.xyz:receiver.a.xyz);
    float2 distance=curved_dd_div(curved_vec_dot(planePoint,normal),denominator);
    depth=curved_dd_mul(distance,incident.z);
    if(!curved_dd_less(0,distance) || curved_dd_less(depth,float2(receiver.extentClip.z,0)) || curved_dd_less(float2(receiver.extentClip.w,0),depth)) return false;
    CurvedVector hit=curved_vec_scale(incident,distance);
    if(liquidReceiver!=0) {
        float2 footprint=curved_dd_div(curved_dd_div(distance,float2(max(liquid.projection.x,1.f),0)),curved_dd_max(curved_dd_abs(denominator),curved_dd_ratio(4,100)));
        if(!curved_liquid_sample(liquid,incident,distance,footprint,hit,normal)) return false;
        bias=curved_dd_max(curved_dd_ratio(5,100),curved_dd_mul(distance,curved_dd_ratio(1,100000)));
        origin=curved_vec_add(hit,curved_vec_scale(normal,bias));outgoing=curved_vec_reflect(incident,normal);
    } else {
        if(finiteFaces && !reflection_curved_inside(primary,hit)) return false;
        bias=curved_dd_max(curved_dd_ratio(reflection_rough_analytic(receiver)?5:1,100),curved_dd_mul(distance,curved_dd_ratio(1,100000)));
        origin=curved_vec_add(hit,curved_vec_scale(normal,bias));
        CurvedVector reflected=curved_vec_reflect(incident,normal);
        CurvedVector tangent=curved_vec_unit(curved_vec_cross(reflected,curved_vec_float(abs(curved_dd_float(reflected.y))<.95?float3(0,1,0):float3(1,0,0)))),bitangent=curved_vec_cross(reflected,tangent);
        // Decimal quadrature offsets match the authored native recipe, not a
        // new sampling distribution; receiver roughness remains its stored float.
        const int2 taps[8]={int2(500,0),int2(-500,0),int2(0,500),int2(0,-500),int2(612,612),int2(-612,612),int2(612,-612),int2(-612,-612)};
        int2 tap=taps[uint(receiver.settings.y)];float2 rough=curved_dd_mul(float2(receiver.settings.x,0),float2(receiver.settings.x,0));
        outgoing=curved_vec_unit(curved_vec_add(reflected,curved_vec_scale(curved_vec_add(curved_vec_scale(tangent,curved_dd_ratio(tap.x,1000)),curved_vec_scale(bitangent,curved_dd_ratio(tap.y,1000))),rough)));
        if(!curved_dd_less(0,curved_vec_dot(outgoing,normal))) outgoing=reflected;
    }
    [loop] for(uint hop=0;hop<hops;++hop) {
        if((liquidMask&(1U<<hop))!=0) {
            if(!reflection_curved_liquid_hop(liquid,origin,outgoing,bias)) return false;
            continue;
        }
        ReflectionSpecularPlane plane=planes[hop];normal=reflection_curved_normal(plane);
        if(curved_dd_less(0,curved_vec_dot(normal,outgoing))) normal=curved_vec_scale(normal,float2(-1,0));
        denominator=curved_vec_dot(outgoing,normal);
        if(!curved_dd_less(float2(1.e-12,0),curved_dd_abs(denominator))) return false;
        distance=curved_dd_div(curved_vec_dot(curved_vec_sub(curved_vec_float(plane.a.xyz),origin),normal),denominator);
        if(!curved_dd_less(bias,distance) || !curved_dd_less(distance,float2(65536,0))) return false;
        hit=curved_vec_add(origin,curved_vec_scale(outgoing,distance));
        if(!all(isfinite(curved_vec_result(hit))) || (finiteFaces && !reflection_curved_inside(plane,hit))) return false;
        bias=curved_dd_max(curved_dd_ratio(5,100),curved_dd_mul(distance,curved_dd_ratio(1,100000)));
        origin=curved_vec_add(hit,curved_vec_scale(normal,bias));outgoing=curved_vec_reflect(outgoing,normal);
    }
    return all(isfinite(curved_vec_result(origin))) && all(isfinite(curved_vec_result(outgoing))) && all(isfinite(depth)) && all(isfinite(bias));
}
bool reflection_curved_forward(float2 pixel,ReflectionRoughFrame receiver,ReflectionLiquidFrame liquid,
    ReflectionSpecularPlane planes[4],uint hops,uint liquidMask,uint liquidReceiver,bool finiteFaces,
    out float3 origin,out float3 outgoing,out float depth,out float bias) {
    CurvedVector preciseOrigin,preciseOutgoing;float2 preciseDepth,preciseBias;
    bool valid=reflection_curved_forward_precise(pixel,receiver,liquid,planes,hops,liquidMask,liquidReceiver,finiteFaces,preciseOrigin,preciseOutgoing,preciseDepth,preciseBias);
    origin=curved_vec_result(preciseOrigin);outgoing=curved_vec_result(preciseOutgoing);
    depth=curved_dd_float(preciseDepth);bias=curved_dd_float(preciseBias);return valid;
}

bool reflection_curved_error(float2 pixel,ReflectionRoughFrame receiver,ReflectionLiquidFrame liquid,
    ReflectionSpecularPlane planes[4],uint hops,uint liquidMask,uint liquidReceiver,float4 terminal,
    out float2 error,out float depth) {
    error=0;depth=0;CurvedVector origin,outgoing;float2 preciseDepth,bias;
    if(!reflection_curved_forward_precise(pixel,receiver,liquid,planes,hops,liquidMask,liquidReceiver,false,
        origin,outgoing,preciseDepth,bias)) return false;
    depth=curved_dd_float(preciseDepth);
    CurvedVector travel=curved_vec_float(terminal.xyz);
    if(terminal.w!=0) travel=curved_vec_sub(travel,origin);
    float2 length2=curved_vec_dot(travel,travel);
    if(!all(isfinite(length2)) || !curved_dd_less(terminal.w==0?float2(1.e-20,0):curved_dd_mul(bias,bias),length2)) return false;
    travel=curved_vec_unit(travel);
    if(!curved_dd_less(0,curved_vec_dot(travel,outgoing))) return false;
    CurvedVector tangent=curved_vec_unit(curved_vec_cross(outgoing,curved_vec_float(abs(curved_dd_float(outgoing.y))<.95?float3(0,1,0):float3(1,0,0))));
    float2 focal=float2(max(receiver.projection.x,receiver.projection.y),0);
    error=float2(curved_dd_float(curved_dd_mul(curved_vec_dot(travel,tangent),focal)),curved_dd_float(curved_dd_mul(curved_vec_dot(travel,curved_vec_cross(outgoing,tangent)),focal)));
    return all(isfinite(error));
}

float4 reflection_curved_result(float2 pixel,ReflectionRoughFrame receiver,ReflectionLiquidFrame liquid,
    ReflectionSpecularPlane planes[4],uint hops,uint liquidMask,uint liquidReceiver,float4 terminal) {
    float3 origin,outgoing;float depth,bias;
    if(!reflection_curved_forward(pixel,receiver,liquid,planes,hops,liquidMask,liquidReceiver,true,
        origin,outgoing,depth,bias)) return 0;
    if(terminal.w==1) {
        float3 magnitude=max(abs(origin),abs(terminal.xyz));
        float scale=max(1.f,max(magnitude.x,max(magnitude.y,magnitude.z)));
        float distance=length(terminal.xyz-origin);
        if(!isfinite(distance) || scale*1.e-6*max(receiver.projection.x,receiver.projection.y)>distance*.012) return 0;
    }
    return float4(pixel,depth,1);
}

#ifndef STARFOX_CURVED_REFERENCE_SOLVER
#define STARFOX_CURVED_REFERENCE_SOLVER 0
#endif
// Geometric guides tolerate a small angular residual; incident-color reuse
// needs a stricter root near caustics. Callers may tighten, never relax, the
// accepted residual without changing search bounds or forward visibility.
#ifndef STARFOX_CURVED_COLOUR_ROOT
#define STARFOX_CURVED_COLOUR_ROOT 0
#endif
#if STARFOX_CURVED_COLOUR_ROOT
#define STARFOX_CURVED_PATH_RESIDUAL .000001
#else
#define STARFOX_CURVED_PATH_RESIDUAL .002
#endif
#if defined(STARFOX_CURVED_CACHE_TRACE)
static float4 reflectionCurvedSolveTrace=0;
static uint4 reflectionCurvedDecisionTrace=0;
#endif
#if STARFOX_CURVED_REFERENCE_SOLVER
// Retained only for opt-in diagnostic comparison. Ordinary compilation excludes
// this repeated-call form: some drivers inline the entire wave at every call.
float4 reflection_curved_path_motion(ReflectionRoughFrame receiver,ReflectionLiquidFrame liquid,
    ReflectionSpecularPlane planes[4],uint hops,uint liquidMask,uint liquidReceiver,float4 terminal,float2 receiverSeed) {
    if(!reflection_curved_frame_valid(receiver,liquid,planes,hops,liquidMask,liquidReceiver)
        || !all(isfinite(terminal)) || (terminal.w!=0 && terminal.w!=1)
        || any(abs(terminal.xyz)>1.e12) || (terminal.w==0 && dot(terminal.xyz,terminal.xyz)<=1.e-20)) return 0;
    // Reverse PLANES only for a starting guess. Every iteration below evaluates
    // the accepted old water/lava at each actual liquid hop, including bias.
    // A transported receiver seed is likewise never a substitute for the solve.
    float3 virtualTerminal=terminal.xyz;
    [loop] for(uint reverse=hops;reverse>0;--reverse) {
        uint hop=reverse-1;bool curved=(liquidMask&(1U<<hop))!=0;
        float3 normal=curved?normalize(liquid.planeNormal.xyz):reflection_specular_normal(planes[hop]);
        float3 planePosition=curved?liquid.planePoint.xyz:planes[hop].a.xyz;
        virtualTerminal-=2*normal*dot(virtualTerminal-(terminal.w==1?planePosition:float3(0,0,0)),normal);
    }
    float3 normal=liquidReceiver!=0?normalize(liquid.planeNormal.xyz):reflection_rough_normal(receiver);
    float3 receiverPosition=liquidReceiver!=0?liquid.planePoint.xyz:receiver.a.xyz;
    virtualTerminal-=2*normal*dot(virtualTerminal-(terminal.w==1?receiverPosition:float3(0,0,0)),normal);
    float2 pixel=receiver.extentClip.xy*.5;
    if(all(isfinite(virtualTerminal)) && virtualTerminal.z>0)
        pixel=clamp(receiver.projection.xy*virtualTerminal.xy/virtualTerminal.z+receiver.projection.zw,
            0,receiver.extentClip.xy-.001);
    // Repeated lava has narrow caustic branches. Only that exceptional path
    // uses central derivatives and bounded restarts; ordinary guides retain
    // their single solve. A successful first solve never incurs restart work.
    const bool repeatedLava=liquidReceiver!=0 && liquid.settings.y==3 && hops==4;
    const bool centralDerivative=repeatedLava || STARFOX_CURVED_COLOUR_ROOT!=0;
    const float derivativeStep=centralDerivative?.00390625:.03125;
    const float trust=max(8.f,min(receiver.extentClip.x,receiver.extentClip.y)*.0625f);
    const int2 offsets[8]={int2(-1,-1),int2(0,-1),int2(1,-1),int2(-1,0),
        int2(1,0),int2(-1,1),int2(0,1),int2(1,1)};
    float4 bestResult=0;float bestDistance=1.e30;
    float2 flatSeed=pixel;
    [loop] for(uint attempt=0;attempt<(repeatedLava?25U:1U);++attempt) {
        float2 start=receiverSeed;
        if(attempt!=0) {
            uint index=attempt-1;float radius=index<8?.5:(index<16?1.f:2.f);
            start+=float2(offsets[index%8])*radius;
        }
        float2 seedError;float seedDepth;
        bool seeded=all(isfinite(start)) && reflection_curved_error(start,receiver,liquid,planes,hops,
            liquidMask,liquidReceiver,terminal,seedError,seedDepth);
        pixel=seeded?start:flatSeed;
        if(!seeded && liquidReceiver!=0 && hops==4 && all(isfinite(start))) {
            // Recover an invalid transported crossing locally, without an
            // expected-source seed, exact-root snap or flat-wave substitution.
            float best=1.e30;
            [loop] for(uint ring=0;ring<3;++ring) {
                float radius=ring==0?.75:(ring==1?2.f:4.f);
                [loop] for(uint j=0;j<8;++j) {
                    float2 candidate=start+float2(offsets[j])*radius,trial;float trialDepth;
                    if(reflection_curved_error(candidate,receiver,liquid,planes,hops,liquidMask,liquidReceiver,terminal,trial,trialDepth)) {
                        float residual=max(abs(trial.x),abs(trial.y));
                        if(residual<best) {best=residual;pixel=candidate;}
                    }
                }
            }
        }
        float4 result=0;
        [loop] for(uint iteration=0;iteration<32;++iteration) {
            float2 error;float depth;
            if(!reflection_curved_error(pixel,receiver,liquid,planes,hops,liquidMask,liquidReceiver,terminal,error,depth)) break;
            float residual=max(abs(error.x),abs(error.y));
            if(residual<=STARFOX_CURVED_PATH_RESIDUAL) {
                result=reflection_curved_result(pixel,receiver,liquid,planes,hops,liquidMask,liquidReceiver,terminal);break;
            }
            float dx=pixel.x+derivativeStep<receiver.extentClip.x?derivativeStep:-derivativeStep;
            float dy=pixel.y+derivativeStep<receiver.extentClip.y?derivativeStep:-derivativeStep;
            float2 ex,ey,mx,my;float unused;
            bool px=reflection_curved_error(pixel+float2(dx,0),receiver,liquid,planes,hops,liquidMask,liquidReceiver,terminal,ex,unused);
            bool py=reflection_curved_error(pixel+float2(0,dy),receiver,liquid,planes,hops,liquidMask,liquidReceiver,terminal,ey,unused);
            bool nx=false,ny=false;
            if(centralDerivative) {
                nx=reflection_curved_error(pixel-float2(dx,0),receiver,liquid,planes,hops,liquidMask,liquidReceiver,terminal,mx,unused);
                ny=reflection_curved_error(pixel-float2(0,dy),receiver,liquid,planes,hops,liquidMask,liquidReceiver,terminal,my,unused);
            }
            if((!px && !nx) || (!py && !ny)) break;
            float2 jx=px?(nx?(ex-mx)/(2*dx):(ex-error)/dx):(error-mx)/dx;
            float2 jy=py?(ny?(ey-my)/(2*dy):(ey-error)/dy):(error-my)/dy;
            float determinant=jx.x*jy.y-jy.x*jx.y;
            if(!isfinite(determinant) || abs(determinant)<1.e-8) break;
            float2 step=float2(jy.y*error.x-jy.x*error.y,-jx.y*error.x+jx.x*error.y)/determinant;
            if(!all(isfinite(step))) break;
#if STARFOX_CURVED_COLOUR_ROOT
            // Pixel coordinates are binary32. An angular-only 1e-6 gate can
            // demand a root between representable pixels. Accept a converged
            // screen correction instead, still inside the original optical
            // residual gate and far below one incident-color sampling quantum.
            if(residual<=.002 && all(abs(step)<=.000244140625)) {
                result=reflection_curved_result(pixel,receiver,liquid,planes,hops,liquidMask,liquidReceiver,terminal);break;
            }
#endif
            step*=min(1.f,trust/max(max(abs(step.x),abs(step.y)),1.e-12));
            bool advanced=false;
            [loop] for(uint backtrack=0;backtrack<8;++backtrack) {
                float2 next=pixel-step,trial;float trialDepth;
                if(reflection_curved_error(next,receiver,liquid,planes,hops,liquidMask,liquidReceiver,terminal,trial,trialDepth)
                    && max(abs(trial.x),abs(trial.y))<residual) {pixel=next;advanced=true;break;}
                step*=.5;
            }
            if(!advanced) break;
        }
        if(result.w==0) {
            float2 error;float depth;
            if(reflection_curved_error(pixel,receiver,liquid,planes,hops,liquidMask,liquidReceiver,terminal,error,depth)
                && max(abs(error.x),abs(error.y))<=STARFOX_CURVED_PATH_RESIDUAL)
                result=reflection_curved_result(pixel,receiver,liquid,planes,hops,liquidMask,liquidReceiver,terminal);
        }
        if(result.w!=0) {
            if(attempt==0 || !repeatedLava) return result;
            // Prefer the valid branch nearest the transported receiver. Full
            // old per-hop witnesses still decide reuse; this is only a guide.
            float2 distance=abs(result.xy-receiverSeed);
            float measure=max(distance.x,distance.y);
            if(measure<bestDistance) {bestDistance=measure;bestResult=result;}
        }
    }
    return bestResult;
}
#else
float4 reflection_curved_path_motion(ReflectionRoughFrame receiver,ReflectionLiquidFrame liquid,
    ReflectionSpecularPlane planes[4],uint hops,uint liquidMask,uint liquidReceiver,float4 terminal,float2 receiverSeed) {
    if(!reflection_curved_frame_valid(receiver,liquid,planes,hops,liquidMask,liquidReceiver)
        || !all(isfinite(terminal)) || (terminal.w!=0 && terminal.w!=1)
        || any(abs(terminal.xyz)>1.e12) || (terminal.w==0 && dot(terminal.xyz,terminal.xyz)<=1.e-20)) return 0;
    float3 virtualTerminal=terminal.xyz;
    [loop] for(uint reverse=hops;reverse>0;--reverse) {
        uint hop=reverse-1;bool curved=(liquidMask&(1U<<hop))!=0;
        float3 normal=curved?normalize(liquid.planeNormal.xyz):reflection_specular_normal(planes[hop]);
        float3 planePosition=curved?liquid.planePoint.xyz:planes[hop].a.xyz;
        virtualTerminal-=2*normal*dot(virtualTerminal-(terminal.w==1?planePosition:float3(0,0,0)),normal);
    }
    float3 normal=liquidReceiver!=0?normalize(liquid.planeNormal.xyz):reflection_rough_normal(receiver);
    float3 receiverPosition=liquidReceiver!=0?liquid.planePoint.xyz:receiver.a.xyz;
    virtualTerminal-=2*normal*dot(virtualTerminal-(terminal.w==1?receiverPosition:float3(0,0,0)),normal);
    float2 flatSeed=receiver.extentClip.xy*.5;
    if(all(isfinite(virtualTerminal)) && virtualTerminal.z>0)
        flatSeed=clamp(receiver.projection.xy*virtualTerminal.xy/virtualTerminal.z+receiver.projection.zw,
            0,receiver.extentClip.xy-.001);
    const bool repeatedLava=liquidReceiver!=0 && liquid.settings.y==3 && hops==4;
    const bool centralDerivative=repeatedLava || STARFOX_CURVED_COLOUR_ROOT!=0;
    const float derivativeStep=centralDerivative?.00390625:.03125;
    const float trust=max(8.f,min(receiver.extentClip.x,receiver.extentClip.y)*.0625f);
    const int2 offsets[8]={int2(-1,-1),int2(0,-1),int2(1,-1),int2(-1,0),
        int2(1,0),int2(-1,1),int2(0,1),int2(1,1)};
    float4 bestResult=0;float bestDistance=1.e30;

    // A request/consume loop gives the compiler ONE residual evaluation site.
    // It follows the same seed, 24 recovery samples, 32 Newton iterations,
    // derivative samples, eight backtracks and 25 exceptional attempts as the
    // reference. Nothing is approximated or supplied by an expected-source root.
    // Finite-face acceptance remains a separate full ordered forward retrace.
    const uint SEED=0,RECOVER=1,BASE=2,PX=3,PY=4,NX=5,NY=6,BACKTRACK=7,FINAL=8;
    [loop] for(uint attempt=0;attempt<(repeatedLava?25U:1U);++attempt) {
        float2 start=receiverSeed;
        if(attempt!=0) {
            uint index=attempt-1;float radius=index<8?.5:(index<16?1.f:2.f);
            start+=float2(offsets[index%8])*radius;
        }
        float2 pixel=flatSeed,sample=start,error=0,ex=0,ey=0,mx=0,my=0,step=0;
        float residual=0,dx=0,dy=0,recoveryBest=1.e30;
        bool px=false,py=false,nx=false,ny=false,accept=false;
        uint state=SEED,recovery=0,iteration=0,backtrack=0;
        [loop] while(true) {
            float2 trial;float unused;
            bool valid=all(isfinite(sample)) && reflection_curved_error(sample,receiver,liquid,planes,
                hops,liquidMask,liquidReceiver,terminal,trial,unused);
            bool derivatives=false,finished=false;
            if(state==SEED) {
                if(valid) {pixel=start;state=BASE;sample=pixel;}
                else if(liquidReceiver!=0 && hops==4 && all(isfinite(start))) {
                    state=RECOVER;sample=start+float2(offsets[0])*.75;
                } else {state=BASE;sample=pixel;}
            } else if(state==RECOVER) {
                if(valid) {
                    float measure=max(abs(trial.x),abs(trial.y));
                    if(measure<recoveryBest) {recoveryBest=measure;pixel=sample;}
                }
                ++recovery;
                if(recovery==24) {state=BASE;sample=pixel;}
                else {
                    float radius=recovery<8?.75:(recovery<16?2.f:4.f);
                    sample=start+float2(offsets[recovery%8])*radius;
                }
            } else if(state==BASE || state==FINAL) {
                if(!valid) finished=true;
                else {
                    error=trial;residual=max(abs(error.x),abs(error.y));
                    if(residual<=STARFOX_CURVED_PATH_RESIDUAL) {accept=true;finished=true;}
                    else if(state==FINAL) finished=true;
                    else {
                        dx=pixel.x+derivativeStep<receiver.extentClip.x?derivativeStep:-derivativeStep;
                        dy=pixel.y+derivativeStep<receiver.extentClip.y?derivativeStep:-derivativeStep;
                        px=py=nx=ny=false;state=PX;sample=pixel+float2(dx,0);
                    }
                }
            } else if(state==PX) {
                px=valid;ex=trial;state=PY;sample=pixel+float2(0,dy);
            } else if(state==PY) {
                py=valid;ey=trial;
                if(centralDerivative) {state=NX;sample=pixel-float2(dx,0);}
                else derivatives=true;
            } else if(state==NX) {
                nx=valid;mx=trial;state=NY;sample=pixel-float2(0,dy);
            } else if(state==NY) {
                ny=valid;my=trial;derivatives=true;
            } else if(state==BACKTRACK) {
                if(valid && max(abs(trial.x),abs(trial.y))<residual) {
                    pixel=sample;++iteration;state=iteration==32?FINAL:BASE;sample=pixel;
                } else {
                    ++backtrack;
                    if(backtrack==8) finished=true;
                    else {step*=.5;sample=pixel-step;}
                }
            } else finished=true;
            if(derivatives) {
                if((!px && !nx) || (!py && !ny)) finished=true;
                else {
                    float2 jx=px?(nx?(ex-mx)/(2*dx):(ex-error)/dx):(error-mx)/dx;
                    float2 jy=py?(ny?(ey-my)/(2*dy):(ey-error)/dy):(error-my)/dy;
                    float determinant=jx.x*jy.y-jy.x*jx.y;
                    if(!isfinite(determinant) || abs(determinant)<1.e-8) finished=true;
                    else {
                        step=float2(jy.y*error.x-jy.x*error.y,-jx.y*error.x+jx.x*error.y)/determinant;
                        if(!all(isfinite(step))) finished=true;
                        else {
#if STARFOX_CURVED_COLOUR_ROOT
                            if(residual<=.002 && all(abs(step)<=.000244140625)) {accept=true;finished=true;}
                            else
#endif
                            {
                            step*=min(1.f,trust/max(max(abs(step.x),abs(step.y)),1.e-12));
                            backtrack=0;state=BACKTRACK;sample=pixel-step;
                            }
                        }
                    }
                }
            }
            // A failed derivative/backtrack already knows that the unchanged
            // base pixel exceeds the residual gate. Re-evaluating it would
            // duplicate exactly the same deterministic rejected sample.
            if(finished) break;
        }
#if defined(STARFOX_CURVED_CACHE_TRACE)
        reflectionCurvedSolveTrace=float4(pixel,error);
        reflectionCurvedDecisionTrace=uint4(state,iteration,backtrack,uint(accept));
#endif
        float4 result=accept?reflection_curved_result(pixel,receiver,liquid,planes,hops,
            liquidMask,liquidReceiver,terminal):float4(0,0,0,0);
        if(result.w!=0) {
            if(attempt==0 || !repeatedLava) return result;
            float2 distance=abs(result.xy-receiverSeed);float measure=max(distance.x,distance.y);
            if(measure<bestDistance) {bestDistance=measure;bestResult=result;}
        }
    }
    return bestResult;
}
#endif // STARFOX_CURVED_REFERENCE_SOLVER
#endif
