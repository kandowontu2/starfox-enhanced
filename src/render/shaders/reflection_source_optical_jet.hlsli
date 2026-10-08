// Interval automatic differentiation of the complete canonical optical map.
// This is a local physical-map enclosure, NOT a global source/colour certificate.
// Continuous min/max/clamp branches use derivative hulls (locally Lipschitz).
// Hard hash/phase/axis/orientation branches must be stable over the WHOLE box.
// Arithmetic uncertainty is unresolved; no finite-difference GPU derivatives.
#include "reflection_source_optical_interval.hlsli"
struct OpticalJet {Interval v,dx,dy;};
struct OpticalJet3 {OpticalJet x,y,z;};
static bool jetBranchKnown=true;
// Optional diagnostic payload. Unread in the full verifier/player, so these
// stores can be removed there. This never changes an arithmetic verdict.
static uint jetFailureSite=0;
static float4 jetFailureArguments=0;
void jet_record_failure(bool alreadyFailed,uint site,Interval a,Interval b) {
    if(!alreadyFailed && intervalFailed && !jetFailureSite) {
        jetFailureSite=site;jetFailureArguments=float4(a.lo,a.hi,b.lo,b.hi);
    }
}
OpticalJet jet(Interval v,Interval dx,Interval dy) {OpticalJet j;j.v=v;j.dx=dx;j.dy=dy;return j;}
OpticalJet jconstant(Interval v) {return jet(v,ip(0),ip(0));}
OpticalJet jp(float v) {return jconstant(ip(v));}
OpticalJet jc(float v) {return jconstant(ic(v));}
OpticalJet jr(float n,float d) {
    const bool was=intervalFailed;const Interval value=iratio(n,d);
    jet_record_failure(was,6,ip(n),ip(d));return jconstant(value);
}
// Exact algebraic identities keep constants' derivatives exactly zero. They
// must not acquire artificial minimum-normal bounds and then enter a tiny
// denominator's checked quotient. Nonzero arithmetic still uses the same
// outward/checking helpers; no tolerance or native estimate is substituted.
bool jet_zero(Interval a) {return a.lo==0 && a.hi==0;}
Interval jet_add_derivative(Interval a,Interval b) {if(jet_zero(a))return b;if(jet_zero(b))return a;return iadd(a,b);}
Interval jet_mul_derivative(Interval a,Interval b) {if(jet_zero(a) || jet_zero(b))return ip(0);return imul(a,b);}
Interval jet_div_derivative(Interval a,Interval b) {
    if(jet_zero(a) && !izero(b))return ip(0);
    const bool was=intervalFailed;const Interval value=idiv(a,b);jet_record_failure(was,2,a,b);return value;
}
OpticalJet ja(OpticalJet a,OpticalJet b) {return jet(iadd(a.v,b.v),jet_add_derivative(a.dx,b.dx),jet_add_derivative(a.dy,b.dy));}
OpticalJet jn(OpticalJet a) {return jet(ineg(a.v),ineg(a.dx),ineg(a.dy));}
OpticalJet js(OpticalJet a,OpticalJet b) {return ja(a,jn(b));}
OpticalJet jm(OpticalJet a,OpticalJet b) {return jet(imul(a.v,b.v),
    jet_add_derivative(jet_mul_derivative(a.dx,b.v),jet_mul_derivative(a.v,b.dx)),
    jet_add_derivative(jet_mul_derivative(a.dy,b.v),jet_mul_derivative(a.v,b.dy)));}
OpticalJet jq(OpticalJet a) {return jet(isquare(a.v),jet_mul_derivative(imul(ip(2),a.v),a.dx),jet_mul_derivative(imul(ip(2),a.v),a.dy));}
OpticalJet jd(OpticalJet a,OpticalJet b) {
    const bool was=intervalFailed;const Interval value=idiv(a.v,b.v);jet_record_failure(was,1,a.v,b.v);
    return jet(value,jet_div_derivative(jet_add_derivative(a.dx,ineg(jet_mul_derivative(value,b.dx))),b.v),
        jet_div_derivative(jet_add_derivative(a.dy,ineg(jet_mul_derivative(value,b.dy))),b.v));
}
OpticalJet jroot(OpticalJet a) {
    const bool was=intervalFailed;const Interval value=isqrt(a.v);jet_record_failure(was,3,a.v,ip(0));
    if(value.lo<=0)jetBranchKnown=false;
    const Interval denominator=imul(ip(2),value);const bool derivativeWas=intervalFailed;
    const Interval derivative=idiv(ip(1),denominator);jet_record_failure(derivativeWas,4,ip(1),denominator);
    return jet(value,jet_mul_derivative(derivative,a.dx),jet_mul_derivative(derivative,a.dy));
}
OpticalJet jhull(OpticalJet a,OpticalJet b) {return jet(ihull(a.v,b.v),ihull(a.dx,b.dx),ihull(a.dy,b.dy));}
OpticalJet jmax(OpticalJet a,OpticalJet b) {
    if(a.v.lo>b.v.hi)return a;if(b.v.lo>a.v.hi)return b;
    OpticalJet r=jhull(a,b);r.v=imax(a.v,b.v);return r;
}
OpticalJet jmin(OpticalJet a,OpticalJet b) {
    if(a.v.hi<b.v.lo)return a;if(b.v.hi<a.v.lo)return b;
    OpticalJet r=jhull(a,b);r.v=imin(a.v,b.v);return r;
}
OpticalJet jclamp(OpticalJet a,OpticalJet lo,OpticalJet hi) {return jmin(jmax(a,lo),hi);}
OpticalJet jabs(OpticalJet a) {
    if(a.v.lo>=0)return a;if(a.v.hi<=0)return jn(a);
    OpticalJet r=jhull(a,jn(a));r.v=iabs(a.v);return r;
}
OpticalJet jsin(OpticalJet a) {const Interval derivative=icos(a.v);return jet(isin(a.v),jet_mul_derivative(derivative,a.dx),jet_mul_derivative(derivative,a.dy));}
OpticalJet jcos(OpticalJet a) {const Interval derivative=ineg(isin(a.v));return jet(icos(a.v),jet_mul_derivative(derivative,a.dx),jet_mul_derivative(derivative,a.dy));}
OpticalJet3 j3(OpticalJet x,OpticalJet y,OpticalJet z) {OpticalJet3 r;r.x=x;r.y=y;r.z=z;return r;}
OpticalJet3 jp3(float3 a) {return j3(jp(a.x),jp(a.y),jp(a.z));}
OpticalJet3 jconstant3(Interval3 a) {return j3(jconstant(a.x),jconstant(a.y),jconstant(a.z));}
OpticalJet3 jadd3(OpticalJet3 a,OpticalJet3 b) {return j3(ja(a.x,b.x),ja(a.y,b.y),ja(a.z,b.z));}
OpticalJet3 jsub3(OpticalJet3 a,OpticalJet3 b) {return jadd3(a,j3(jn(b.x),jn(b.y),jn(b.z)));}
OpticalJet3 jscale(OpticalJet3 a,OpticalJet b) {return j3(jm(a.x,b),jm(a.y,b),jm(a.z,b));}
OpticalJet jdot(OpticalJet3 a,OpticalJet3 b) {return ja(ja(jm(a.x,b.x),jm(a.y,b.y)),jm(a.z,b.z));}
OpticalJet3 jcross(OpticalJet3 a,OpticalJet3 b) {return j3(js(jm(a.y,b.z),jm(a.z,b.y)),js(jm(a.z,b.x),jm(a.x,b.z)),js(jm(a.x,b.y),jm(a.y,b.x)));}
OpticalJet jlength(OpticalJet3 a) {return jroot(ja(ja(jq(a.x),jq(a.y)),jq(a.z)));}
OpticalJet3 junit(OpticalJet3 a) {return jscale(a,jd(jp(1),jlength(a)));}
OpticalJet3 jreflect(OpticalJet3 a,OpticalJet3 n) {return jsub3(a,jscale(n,jm(jp(2),jdot(a,n))));}
OpticalJet3 jrow(OpticalJet3 a,ReflectionLiquidFrame f) {return jadd3(jadd3(jscale(jp3(f.rotation0.xyz),a.x),jscale(jp3(f.rotation1.xyz),a.y)),jscale(jp3(f.rotation2.xyz),a.z));}
OpticalJet3 jcol(OpticalJet3 a,ReflectionLiquidFrame f) {return j3(jdot(jp3(f.rotation0.xyz),a),jdot(jp3(f.rotation1.xyz),a),jdot(jp3(f.rotation2.xyz),a));}
OpticalJet jband(OpticalJet f,OpticalJet frequency) {const OpticalJet s=jq(jm(f,frequency));return jd(jp(1),ja(jp(1),jq(s)));}
OpticalJet jhash(int x,int z) {
    const bool was=intervalFailed;const Interval value=ihash(x,z);
    jet_record_failure(was,7,ip(x),ip(z));return jconstant(value);
}
 OpticalJet3 jlava(OpticalJet x,OpticalJet z,OpticalJet t,OpticalJet footprint) {
    const OpticalJet bend=ja(js(jm(x,jr(6,1000)),jm(z,jr(8,1000))),jm(t,jr(11,100))),filter=jband(footprint,jr(10,1000));
    const OpticalJet a=ja(js(ja(jm(x,jr(18,1000)),jm(z,jr(11,1000))),jm(t,jr(45,100))),jm(jm(jr(65,100),jsin(bend)),filter));
    const OpticalJet b=ja(js(jm(x,jr(47,1000)),jm(z,jr(25,1000))),jm(t,jr(60,100)));
    const OpticalJet c=js(js(jm(z,jr(22,1000)),jm(x,jr(9,1000))),jm(t,jr(32,100)));
    const OpticalJet fa=jband(footprint,jr(22,1000)),fb=jband(footprint,jr(54,1000)),fc=jband(footprint,jr(24,1000));
    OpticalJet h=ja(ja(jm(jm(jp(9),jsin(a)),fa),jm(jm(jp(2.5),jsin(b)),fb)),jm(jm(jp(5),jsin(c)),fc));
    const OpticalJet cb=jm(jcos(bend),filter);
    OpticalJet dx=js(ja(jm(jm(jm(jp(9),ja(jr(18,1000),jm(jr(39,10000),cb))),jcos(a)),fa),jm(jm(jr(1175,10000),jcos(b)),fb)),jm(jm(jr(45,1000),jcos(c)),fc));
    OpticalJet dz=ja(js(jm(jm(jm(jp(9),js(jr(11,1000),jm(jr(52,10000),cb))),jcos(a)),fa),jm(jm(jr(625,10000),jcos(b)),fb)),jm(jm(jr(110,1000),jcos(c)),fc));
    const OpticalJet ripple=js(ja(jm(x,jr(173,1000)),jm(z,jr(129,1000))),jm(t,jr(73,100))),fr=jband(footprint,jr(216,1000));
    h=ja(h,jm(jm(jr(38,100),jsin(ripple)),fr));dx=ja(dx,jm(jm(jr(6574,100000),jcos(ripple)),fr));dz=ja(dz,jm(jm(jr(4902,100000),jcos(ripple)),fr));
    if(footprint.v.lo<24) {
        if(footprint.v.hi>=24){jetBranchKnown=false;return j3(h,dx,dz);}
        const OpticalJet xx=jd(x,jp(128)),zz=jd(z,jp(128));
        if(floor(xx.v.lo)!=floor(xx.v.hi) || floor(zz.v.lo)!=floor(zz.v.hi)) {
            jetBranchKnown=false;return j3(h,dx,dz);
        } else {
            const int cx=int(floor(xx.v.lo)),cz=int(floor(zz.v.lo));const OpticalJet seed=jhash(cx,cz);
            const Interval threshold=iratio(64,100);
            if(seed.v.lo<=threshold.hi && seed.v.hi>threshold.lo){jetBranchKnown=false;return j3(h,dx,dz);}
            if(seed.v.lo>threshold.hi) {
                const OpticalJet bx=js(js(xx,jp(cx)),ja(jr(28,100),jm(jr(44,100),jhash(cx+19,cz))));
                const OpticalJet bz=js(js(zz,jp(cz)),ja(jr(28,100),jm(jr(44,100),jhash(cx,cz+29))));
                OpticalJet phase=ja(jm(t,jr(14,100)),jm(seed,jp(7)));
                if(floor(phase.v.lo)==floor(phase.v.hi))phase=js(phase,jp(floor(phase.v.lo)));else {jetBranchKnown=false;return j3(h,dx,dz);}
                const OpticalJet life=jq(jsin(jm(phase,jc(3.14159265)))),radius=ja(jr(55,1000),jm(jr(14,100),phase)),rr=jq(radius);
                // A sum of real squares cannot be negative. At a bubble
                // centre, outward addition of two zero lower bounds produced
                // -FLT_MIN; checking that impossible negative endpoint against
                // the small radius exhausted the native quotient bracket.
                // Intersect ONLY the value with the exact nonnegative range.
                // Keep the analytic derivative intervals unchanged (no clamp
                // derivative, epsilon, estimate acceptance or tolerance change).
                OpticalJet radialSquared=ja(jq(bx),jq(bz));radialSquared.v.lo=max(0,radialSquared.v.lo);
                const OpticalJet dome=jclamp(js(jp(1),jd(radialSquared,rr)),jp(0),jp(1));
                const OpticalJet amplitude=jm(jm(jp(10),life),jband(footprint,jr(18,100))),dd=jq(dome);
                OpticalJet dh=jm(jm(amplitude,dd),dome),derivative=jd(jm(jm(jp(-6),amplitude),dd),jm(jp(128),rr));
                OpticalJet ddx=jm(derivative,bx),ddz=jm(derivative,bz);
                h=ja(h,dh);dx=ja(dx,ddx);dz=ja(dz,ddz);
            }
        }
    }
    return j3(h,dx,dz);
}

OpticalJet3 jet_oriented(OpticalJet3 n,OpticalJet3 direction) {
    const OpticalJet d=jdot(n,direction);
    if(d.v.lo>0)return jscale(n,jp(-1));
    if(d.v.hi<0)return n;
    jetBranchKnown=false;return n;
}
OpticalJet3 jet_plane_normal(ReflectionSpecularPlane plane) {
    const Interval3 normal=plane_normal(plane);
    // A finite triangle whose three raw coordinates on one axis are EXACTLY
    // equal has a cross product parallel to that axis. Its normalized normal
    // is exactly +/-unit(axis), not three independent uncertain quotients.
    // Keep the original checked normalization/sign proof, and use this exact
    // correlation only when nondegeneracy was certified. A one-ULP tilt,
    // analytic plane, failed arithmetic or general face keeps the full bounds.
    // Liquid normals never enter here; their displaced wave map is unchanged.
    if(!intervalFailed && !reflection_specular_analytic(plane)) {
        [unroll] for(uint axis=0;axis<3;++axis) {
            if(plane.a[axis]!=plane.b[axis] || plane.a[axis]!=plane.c[axis])continue;
            Interval component=normal.z;if(axis==0)component=normal.x;else if(axis==1)component=normal.y;
            if(component.lo<=0 && component.hi>=0)continue;
            float3 exact=0;exact[axis]=component.lo>0?1:-1;
            return jp3(exact);
        }
    }
    return jconstant3(normal);
}
 OpticalJet3 jet_liquid_normal(ReflectionLiquidFrame f,OpticalJet3 direction,OpticalJet distance,OpticalJet footprint,inout OpticalJet3 hit) {
    OpticalJet3 p=jadd3(jrow(hit,f),jp3(float3(f.rotation0.w,f.rotation1.w,f.rotation2.w)));const OpticalJet t=jp(f.settings.x);OpticalJet dx,dz;
    if(f.settings.y==3) {
        // One static lava call site: evaluate displacement, then the displaced
        // normal. Keep the loop to avoid cloning this large interval program
        // into both sites inside a driver's pipeline compiler.
        dx=dz=jp(0);const OpticalJet3 travel=jrow(direction,f);
        [loop] for(uint evaluation=0;evaluation<2;++evaluation) {
            const OpticalJet3 wave=jlava(p.x,p.z,t,footprint);
            if(!evaluation) {
                const OpticalJet limit=jm(distance,jr(2,10)),shift=jclamp(jd(jn(wave.x),jmax(travel.y,jr(12,100))),jn(limit),limit);
                p=jadd3(p,jscale(travel,shift));hit=jadd3(hit,jscale(direction,shift));
            } else {dx=wave.y;dz=wave.z;}
        }
    } else {
        const OpticalJet a=js(ja(jm(p.x,jr(18,1000)),jm(p.z,jr(11,1000))),jm(t,jr(8,10)));
        const OpticalJet b=ja(js(jm(p.x,jr(47,1000)),jm(p.z,jr(25,1000))),jm(t,jr(12,10)));
        const OpticalJet c=js(js(jm(p.z,jr(22,1000)),jm(p.x,jr(9,1000))),jm(t,jr(65,100)));
        dx=ja(jm(jm(jr(55,1000),jcos(a)),jband(footprint,jr(22,1000))),jm(jm(jr(25,1000),jcos(b)),jband(footprint,jr(54,1000))));
        dz=js(jm(jm(jr(45,1000),jcos(c)),jband(footprint,jr(24,1000))),jm(jm(jr(20,1000),jcos(b)),jband(footprint,jr(54,1000))));
    }
    return jet_oriented(junit(jcol(j3(dx,jp(-1),dz),f)),direction);
}

bool jet_inside_face(ReflectionSpecularPlane plane,OpticalJet3 hit) {
    if(reflection_specular_analytic(plane))return true;
    const Interval3 u=vsub(p3(plane.b.xyz),p3(plane.a.xyz)),v=vsub(p3(plane.c.xyz),p3(plane.a.xyz));
    const Interval3 r=vsub(i3(hit.x.v,hit.y.v,hit.z.v),p3(plane.a.xyz));
    const Interval aa=vdot(u,u),ab=vdot(u,v),bb=vdot(v,v),ra=vdot(r,u),rb=vdot(r,v);
    const Interval det=isub(imul(aa,bb),isquare(ab));
    if(det.lo<=0)return false;
    const Interval x=idiv(isub(imul(bb,ra),imul(ab,rb)),det),y=idiv(isub(imul(aa,rb),imul(ab,ra)),det);
    return x.lo>=ic(-.00001).hi && y.lo>=ic(-.00001).hi && iadd(x,y).hi<=ic(1.00001).lo;
}
struct OpticalJetRay {OpticalJet3 origin,outgoing;OpticalJet depth,bias;};
bool optical_jet_forward(float4 box,ReflectionRoughFrame receiver,ReflectionLiquidFrame liquid,
    ReflectionSpecularPlane planes[4],uint4 control,out OpticalJetRay ray) {
    ray.origin=ray.outgoing=jp3(0);ray.depth=ray.bias=jp(0);
    if(!all(isfinite(box)) || any(box.xy>box.zw) || any(box.xy<0)
        || box.z>=receiver.extentClip.x || box.w>=receiver.extentClip.y || control.x>4 || (control.y>>control.x)!=0)return false;
    const OpticalJet x=jet(iv(box.x,box.z),ip(1),ip(0)),y=jet(iv(box.y,box.w),ip(0),ip(1));
    OpticalJet3 outgoing=junit(j3(jd(js(x,jp(receiver.projection.z)),jp(receiver.projection.x)),
        jd(js(y,jp(receiver.projection.w)),jp(receiver.projection.y)),jp(1)));
    OpticalJet3 origin=jp3(0);OpticalJet bias=jp(0);
    [loop] for(uint stage=0;stage<=control.x;++stage) {
        const bool primary=stage==0,curved=primary?control.z!=0:(control.y&(1U<<(stage-1)))!=0;
        ReflectionSpecularPlane plane;
        if(primary){plane.a=receiver.a;plane.b=receiver.b;plane.c=receiver.c;}else plane=planes[stage-1];
        OpticalJet3 n;if(curved)n=jp3(liquid.planeNormal.xyz);else n=jet_oriented(jet_plane_normal(plane),outgoing);
        const OpticalJet den=jdot(outgoing,n),distance=jd(jdot(jsub3(jp3(curved?liquid.planePoint.xyz:plane.a.xyz),origin),n),den);
        if(iabs(den.v).lo<=ic(!primary && curved?1.e-8:1.e-12).hi)return false;
        if(primary) {
            ray.depth=jm(distance,outgoing.z);
            if(distance.v.lo<=0 || ray.depth.v.lo<receiver.extentClip.z || ray.depth.v.hi>receiver.extentClip.w)return false;
        }else if(distance.v.lo<=bias.v.hi || distance.v.hi>=65536)return false;
        OpticalJet3 hit=jadd3(origin,jscale(outgoing,distance));
        if(curved) {
            OpticalJet radius;if(primary)radius=distance;else radius=jlength(hit);
            const OpticalJet footprint=jd(jd(radius,jp(max(liquid.projection.x,1))),jmax(jabs(den),jr(4,100)));
            n=jet_liquid_normal(liquid,outgoing,distance,footprint,hit);
        }else if(!jet_inside_face(plane,hit))return false;
        bias=jmax(jr(primary && !curved && !reflection_specular_analytic(plane)?1:5,100),jm(distance,jr(1,100000)));
        origin=jadd3(hit,jscale(n,bias));outgoing=jreflect(outgoing,n);
        if(primary && !curved && receiver.settings.x!=0) {
            const Interval ay=iabs(outgoing.y.v);OpticalJet3 t;
            if(ay.hi<ic(.95).lo)t=junit(jcross(outgoing,jp3(float3(0,1,0))));
            else if(ay.lo>ic(.95).hi)t=junit(jcross(outgoing,jp3(float3(1,0,0))));
            else return false;
            const int2 taps[8]={int2(500,0),int2(-500,0),int2(0,500),int2(0,-500),int2(612,612),int2(-612,612),int2(612,-612),int2(-612,-612)};
            const int2 tap=taps[uint(receiver.settings.y)];
            const OpticalJet3 rough=junit(jadd3(outgoing,jscale(jadd3(jscale(t,jr(tap.x,1000)),jscale(jcross(outgoing,t),jr(tap.y,1000))),jq(jp(receiver.settings.x)))));
            const Interval dot=jdot(rough,n).v;
            if(dot.lo>0)outgoing=rough;else if(dot.hi>0)return false;
        }
        if(intervalFailed || !jetBranchKnown)return false;
    }
    ray.origin=origin;ray.outgoing=outgoing;ray.bias=bias;
    return !intervalFailed && jetBranchKnown;
}
// Cross-product coordinates with a fixed omitted axis. Nonzero outgoing on
// that axis plus positive forward travel proves the two-zero residual is
// equivalent to the complete forward alignment, not an antiparallel branch.
bool optical_jet_residual(OpticalJetRay ray,Interval3 target,bool finiteTerminal,
    float focal,uint omitted,out OpticalJet e0,out OpticalJet e1) {
    e0=e1=jp(0);if(omitted>2)return false;
    OpticalJet3 travel=jconstant3(target);if(finiteTerminal)travel=jsub3(travel,ray.origin);
    const OpticalJet len=jlength(travel);
    if(len.v.lo<=(finiteTerminal?ray.bias.v.hi:0) || jdot(travel,ray.outgoing).v.lo<=0)return false;
    Interval axis=ray.outgoing.z.v;if(omitted==0)axis=ray.outgoing.x.v;else if(omitted==1)axis=ray.outgoing.y.v;
    if(izero(axis))return false;
    const OpticalJet3 e=jscale(jcross(junit(travel),ray.outgoing),jp(focal));
    if(omitted==0){e0=e.y;e1=e.z;}else if(omitted==1){e0=e.z;e1=e.x;}else{e0=e.x;e1=e.y;}
    return !intervalFailed && jetBranchKnown;
}
struct OpticalLocalRoot {uint status;float4 enclosure;float contraction;};
bool optical_interval_valid(Interval a) {return isfinite(a.lo) && isfinite(a.hi) && a.lo<=a.hi;}
// C is an arbitrary finite binary32 preconditioner. We do NOT trust a divide
// approximation as an interval inverse. All certificate arithmetic is outward.
// A strict enclosure plus row contraction <1 is a Banach fixed-point proof for
// the canonical continuous map (including derivative hulls at continuous clamps).
// It certifies this box ONLY. Global cover/identity/angular/RGB gates remain.
OpticalLocalRoot optical_local_root(float4 box,float2 centre,Interval f0,Interval f1,
    Interval j00,Interval j01,Interval j10,Interval j11) {
    OpticalLocalRoot r;r.status=0;r.enclosure=0;r.contraction=1.e30;
    if(intervalFailed || !jetBranchKnown || !all(isfinite(box)) || !all(isfinite(centre))
        || any(box.xy>=box.zw) || any(centre<=box.xy) || any(centre>=box.zw)
        || !optical_interval_valid(f0) || !optical_interval_valid(f1)
        || !optical_interval_valid(j00) || !optical_interval_valid(j01)
        || !optical_interval_valid(j10) || !optical_interval_valid(j11))return r;
    const float4 m=float4((j00.lo+j00.hi)*.5,(j01.lo+j01.hi)*.5,(j10.lo+j10.hi)*.5,(j11.lo+j11.hi)*.5);
    precise float det=m.x*m.w-m.y*m.z;
    if(!all(isfinite(m)) || !isfinite(det) || det==0)return r;
    const float4 c=float4(m.w,-m.y,-m.z,m.x)/det;
    if(!all(isfinite(c)) || izero(isub(imul(ip(c.x),ip(c.w)),imul(ip(c.y),ip(c.z)))))return r;
    const Interval a=isub(ip(1),iadd(imul(ip(c.x),j00),imul(ip(c.y),j10)));
    const Interval b=ineg(iadd(imul(ip(c.x),j01),imul(ip(c.y),j11)));
    const Interval d=ineg(iadd(imul(ip(c.z),j00),imul(ip(c.w),j10)));
    const Interval e=isub(ip(1),iadd(imul(ip(c.z),j01),imul(ip(c.w),j11)));
    const Interval x=isub(iv(box.x,box.z),ip(centre.x)),y=isub(iv(box.y,box.w),ip(centre.y));
    const Interval bx=isub(ip(centre.x),iadd(imul(ip(c.x),f0),imul(ip(c.y),f1)));
    const Interval by=isub(ip(centre.y),iadd(imul(ip(c.z),f0),imul(ip(c.w),f1)));
    const Interval kx=iadd(bx,iadd(imul(a,x),imul(b,y))),ky=iadd(by,iadd(imul(d,x),imul(e,y)));
    const Interval norm=imax(iadd(iabs(a),iabs(b)),iadd(iabs(d),iabs(e)));
    if(intervalFailed)return r;
    r.enclosure=float4(kx.lo,ky.lo,kx.hi,ky.hi);r.contraction=norm.hi;
    if(kx.hi<box.x || kx.lo>box.z || ky.hi<box.y || ky.lo>box.w)r.status=2; // Local no-root, NOT angular exclusion.
    else if(norm.hi<1 && kx.lo>box.x && kx.hi<box.z && ky.lo>box.y && ky.hi<box.w)r.status=1;
    return r;
}
