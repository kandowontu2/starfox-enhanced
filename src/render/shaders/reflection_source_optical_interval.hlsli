// Diagnostic continuum enclosures, not sampled/finite-start uniqueness claims.
// Arithmetic failure is UNKNOWN, never permission to discard a source box.
// IEEE binary32 endpoints are expanded outwards. Quotient/square-root bounds
// are additionally checked with the existing compensated exact-product core.
#include "reflection_specular_path_motion.hlsli"
#include "reflection_liquid_hit_motion.hlsli"
#define STARFOX_CURVED_SCALAR_ONLY 1
#include "reflection_curved_precision.hlsli"
struct Interval {float lo,hi;};
struct Interval3 {Interval x,y,z;};
static bool intervalFailed=false;
#if defined(STARFOX_OPTICAL_INTERVAL_NOINLINE_ALL)
#define OPTICAL_BOUNDARY [noinline]
#else
#define OPTICAL_BOUNDARY
#endif
#if defined(STARFOX_OPTICAL_SCALAR_INLINE) && !defined(STARFOX_OPTICAL_SHARED_SCALAR)
#define OPTICAL_SCALAR_BOUNDARY
#else
#define OPTICAL_SCALAR_BOUNDARY [noinline]
#endif
float interval_down(float x) {
    if(!isfinite(x)) {intervalFailed=true;return x;}
    if(x==0)return -asfloat(0x00800000U);
    return asfloat(asuint(x)+(x<0?1U:0xffffffffU));
}
float interval_up(float x) {
    if(!isfinite(x)) {intervalFailed=true;return x;}
    if(x==0)return asfloat(0x00800000U);
    return asfloat(asuint(x)+(x>0?1U:0xffffffffU));
}
Interval iv(float lo,float hi) {Interval r;r.lo=lo;r.hi=hi;return r;}
Interval ip(float x) {return iv(x,x);}
Interval ic(float x) {return iv(interval_down(x),interval_up(x));}
bool interval_finite_ordered(Interval a) {return isfinite(a.lo) && isfinite(a.hi) && a.lo<=a.hi;}
bool interval_exact_point(Interval a,float x) {
    // Test the representation, not a possibly denormal-flushing comparison.
    // Both signed zero representations denote the same exact real constant.
    if(x==0)return ((asuint(a.lo)|asuint(a.hi))&0x7fffffffU)==0;
    return asuint(a.lo)==asuint(x) && asuint(a.hi)==asuint(x);
}
#include "reflection_source_directed_rounding.hlsli"
// DXC defines __spirv__ for its Vulkan target. That target already preserves
// the interval value-return functions; keep that form without extra private
// endpoint storage. DXIL uses the shared scalar-only non-entry ABI below.
#if defined(__spirv__)
Interval iadd(Interval a,Interval b) {
    if(interval_finite_ordered(a) && interval_finite_ordered(b)) {
        if(interval_exact_point(a,0))return b;
        if(interval_exact_point(b,0))return a;
    }
    return iv(optical_add_bound(a.lo,b.lo,false),optical_add_bound(a.hi,b.hi,true));
}
#else
// Share the full scalar endpoint operations rather than cloning their guarded
// identity/rounding bodies into every jet operation. Auxiliary endpoints are
// invocation-private and consumed immediately; no recursion or precision change.
static float interval_add_upper=0;
OPTICAL_SCALAR_BOUNDARY float interval_add_pair(float alo,float ahi,float blo,float bhi) {
    const Interval a=iv(alo,ahi),b=iv(blo,bhi);
    // These are exact real identities, not approximate roundoff estimates.
    // Keep an existing enclosure unchanged rather than adding fictitious ULPs
    // at every zero entry of an exact axis/calibration matrix. Invalid inputs
    // never enter the fast path; all nonidentity arithmetic stays outward.
    if(interval_finite_ordered(a) && interval_finite_ordered(b)) {
        if(interval_exact_point(a,0)){interval_add_upper=b.hi;return b.lo;}
        if(interval_exact_point(b,0)){interval_add_upper=a.hi;return a.lo;}
    }
    const float resultLo=optical_add_bound(a.lo,b.lo,false);
    interval_add_upper=optical_add_bound(a.hi,b.hi,true);return resultLo;
}
Interval iadd(Interval a,Interval b) {
    const float lo=interval_add_pair(a.lo,a.hi,b.lo,b.hi);return iv(lo,interval_add_upper);
}
#endif
Interval ineg(Interval a) {
    // Negation only changes the IEEE sign bit. In a native Vulkan FTZ
    // environment, floating FNegate flushed a min-subnormal endpoint to zero
    // in the direct interval-identity regression. Keep this exact operation
    // integer/bitwise, including signed zeros; no arithmetic precision mode,
    // epsilon or enclosure is changed and nonfinite inputs remain nonfinite.
    return iv(asfloat(asuint(a.hi)^0x80000000U),asfloat(asuint(a.lo)^0x80000000U));
}
Interval isub(Interval a,Interval b) {return iadd(a,ineg(b));}
#if defined(__spirv__)
Interval imul(Interval a,Interval b) {
    if(interval_finite_ordered(a) && interval_finite_ordered(b)) {
        if(interval_exact_point(a,0) || interval_exact_point(b,0))return ip(0);
        if(interval_exact_point(a,1))return b;if(interval_exact_point(b,1))return a;
        if(interval_exact_point(a,-1))return ineg(b);if(interval_exact_point(b,-1))return ineg(a);
    }
    // Reuse only bit-identical corner inputs of finite ordered intervals.
    // The four mathematical corners and their exact directed values remain;
    // invalid intervals still execute the original four calls in original order.
    const bool finiteCorners=interval_finite_ordered(a) && interval_finite_ordered(b);
    const bool sameA=finiteCorners && asuint(a.lo)==asuint(a.hi);
    const bool sameB=finiteCorners && asuint(b.lo)==asuint(b.hi);
    const float p=optical_product_bounds(a.lo,b.lo),u=optical_product_upper;
    float q=p,v=u;
    if(!sameB){q=optical_product_bounds(a.lo,b.hi);v=optical_product_upper;}
    float r=p,w=u;
    if(!sameA){r=optical_product_bounds(a.hi,b.lo);w=optical_product_upper;}
    float s=q,x=v;
    if(!sameA){
        if(sameB){s=r;x=w;}
        else{s=optical_product_bounds(a.hi,b.hi);x=optical_product_upper;}
    }
    return iv(min(min(p,q),min(r,s)),max(max(u,v),max(w,x)));
}
#else
static float interval_multiply_upper=0;
OPTICAL_SCALAR_BOUNDARY float interval_multiply_pair(float alo,float ahi,float blo,float bhi) {
    const Interval a=iv(alo,ahi),b=iv(blo,bhi);
    if(interval_finite_ordered(a) && interval_finite_ordered(b)) {
        if(interval_exact_point(a,0) || interval_exact_point(b,0)){interval_multiply_upper=0;return 0;}
        if(interval_exact_point(a,1)){interval_multiply_upper=b.hi;return b.lo;}
        if(interval_exact_point(b,1)){interval_multiply_upper=a.hi;return a.lo;}
        if(interval_exact_point(a,-1)){
            const Interval result=ineg(b);interval_multiply_upper=result.hi;return result.lo;
        }
        if(interval_exact_point(b,-1)){
            const Interval result=ineg(a);interval_multiply_upper=result.hi;return result.lo;
        }
    }
    // Reuse only bit-identical corner inputs of finite ordered intervals.
    // The four mathematical corners and their exact directed values remain;
    // invalid intervals still execute the original four calls in original order.
    const bool finiteCorners=interval_finite_ordered(a) && interval_finite_ordered(b);
    const bool sameA=finiteCorners && asuint(a.lo)==asuint(a.hi);
    const bool sameB=finiteCorners && asuint(b.lo)==asuint(b.hi);
    const float p=optical_product_bounds(a.lo,b.lo),u=optical_product_upper;
    float q=p,v=u;
    if(!sameB){q=optical_product_bounds(a.lo,b.hi);v=optical_product_upper;}
    float r=p,w=u;
    if(!sameA){r=optical_product_bounds(a.hi,b.lo);w=optical_product_upper;}
    float s=q,x=v;
    if(!sameA){
        if(sameB){s=r;x=w;}
        else{s=optical_product_bounds(a.hi,b.hi);x=optical_product_upper;}
    }
    const float resultLo=min(min(p,q),min(r,s));
    interval_multiply_upper=max(max(u,v),max(w,x));return resultLo;
}
Interval imul(Interval a,Interval b) {
    const float lo=interval_multiply_pair(a.lo,a.hi,b.lo,b.hi);return iv(lo,interval_multiply_upper);
}
#endif
Interval isquare(Interval a) {
    const float minimum=a.lo<=0 && a.hi>=0?0:min(abs(a.lo),abs(a.hi)),maximum=max(abs(a.lo),abs(a.hi));
    return iv(max(0,interval_down(minimum*minimum)),interval_up(maximum*maximum));
}
Interval iabs(Interval a) {return iv(a.lo<=0 && a.hi>=0?0:min(abs(a.lo),abs(a.hi)),max(abs(a.lo),abs(a.hi)));}
Interval imax(Interval a,Interval b) {return iv(max(a.lo,b.lo),max(a.hi,b.hi));}
Interval imin(Interval a,Interval b) {return iv(min(a.lo,b.lo),min(a.hi,b.hi));}
Interval ihull(Interval a,Interval b) {return iv(min(a.lo,b.lo),max(a.hi,b.hi));}
Interval iclamp(Interval a,Interval lo,Interval hi) {return imin(imax(a,lo),hi);}
bool izero(Interval a) {return a.lo<=0 && a.hi>=0;}
// Checking the exact float product avoids assuming a driver's divide/sqrt
// estimate has a particular accuracy. Unbracketed results fail the whole box.
OPTICAL_SCALAR_BOUNDARY float quotient_bound(float a,float b,bool upper) {
    float q=a/b;
    [loop] for(uint n=0;n<8;++n) {
        if(!isfinite(q) || abs(q)>1.e30) {intervalFailed=true;return q;}
        // Identical compensated product, kept as scalar SSA so the DXIL
        // non-entry ABI can share this full checked core without vector ops.
        const CurvedScalar product=curved_dd_mul_core(q,0,b,0);
        if(!isfinite(product.high) || !isfinite(product.low)) {intervalFailed=true;return q;}
        const bool below=product.high<a || (product.high==a && product.low<0),
            above=a<product.high || (a==product.high && 0<product.low);
        if(b<0) {if(upper?!above:!below)return q;}
        else if(upper?!below:!above)return q;
        q=upper?interval_up(q):interval_down(q);
    }
    intervalFailed=true;return q;
}
// Keep the complete eight checked quotient evaluations at one scalar-only
// non-entry boundary. DXIL can share this body rather than cloning it into
// every automatic-differentiation operation. The auxiliary upper endpoint is
// private to a shader invocation, like intervalFailed; the caller consumes it
// immediately. No recursion, new approximation, or bracket/rounding change.
static float interval_divide_upper=0;
OPTICAL_SCALAR_BOUNDARY float interval_divide_pair(float alo,float ahi,float blo,float bhi) {
    // A repeated endpoint has exactly the same checked quotient and failure
    // state. Reuse only bit-identical corners of finite ordered intervals;
    // distinct/invalid corners keep every original eight-step scalar check.
    const bool finiteCorners=interval_finite_ordered(iv(alo,ahi)) && interval_finite_ordered(iv(blo,bhi));
    const bool sameA=finiteCorners && asuint(alo)==asuint(ahi);
    const bool sameB=finiteCorners && asuint(blo)==asuint(bhi);
    const float p=quotient_bound(alo,blo,false),u=quotient_bound(alo,blo,true);
    float q=p,v=u;
    if(!sameB){q=quotient_bound(alo,bhi,false);v=quotient_bound(alo,bhi,true);}
    float r=p,w=u;
    if(!sameA){r=quotient_bound(ahi,blo,false);w=quotient_bound(ahi,blo,true);}
    float s=q,x=v;
    if(!sameA){
        if(sameB){s=r;x=w;}
        else{s=quotient_bound(ahi,bhi,false);x=quotient_bound(ahi,bhi,true);}
    }
    // Retain the original initial sentinels and left-to-right corner folds.
    const float lo=min(min(min(min(1.e30,p),q),r),s);
    interval_divide_upper=max(max(max(max(-1.e30,u),v),w),x);return lo;
}
OPTICAL_BOUNDARY Interval idiv(Interval a,Interval b) {
    if(izero(b) || max(abs(b.lo),abs(b.hi))>1.e30 || max(abs(a.lo),abs(a.hi))>1.e30) {
        intervalFailed=true;return iv(-1.e30,1.e30);
    }
    // Only AFTER the original zero-denominator/range guard. A zero numerator
    // over a finite nonzero interval is exactly zero; division by +/-1 merely
    // preserves/reverses its existing endpoints. No quotient estimate, extra
    // bracket step, tolerance change or unvalidated input is admitted.
    if(interval_finite_ordered(a) && interval_finite_ordered(b)) {
        if(interval_exact_point(a,0))return ip(0);
        if(interval_exact_point(b,1))return a;if(interval_exact_point(b,-1))return ineg(a);
    }
    const float lo=interval_divide_pair(a.lo,a.hi,b.lo,b.hi);
    return iv(interval_down(lo),interval_up(interval_divide_upper));
}
OPTICAL_SCALAR_BOUNDARY float sqrt_bound(float a,bool upper) {
    float q=sqrt(a);
    [loop] for(uint n=0;n<8;++n) {
        const CurvedScalar square=curved_dd_mul_core(q,0,q,0);
        if(!isfinite(square.high) || !isfinite(square.low)) {intervalFailed=true;return q;}
        const bool below=square.high<a || (square.high==a && square.low<0),
            above=a<square.high || (a==square.high && 0<square.low);
        if(upper?!below:!above)return q;
        q=upper?interval_up(q):max(0,interval_down(q));
    }
    intervalFailed=true;return q;
}
OPTICAL_BOUNDARY Interval isqrt(Interval a) {
    if(a.hi<0 || a.hi>1.e30) {intervalFailed=true;return iv(0,1.e30);}
    return iv(max(0,interval_down(sqrt_bound(max(0,a.lo),false))),interval_up(sqrt_bound(max(0,a.hi),true)));
}
Interval iratio(float n,float d) {
    // Only these known constants use prechecked reciprocal enclosures. No
    // estimate from a native divide is trusted. All other denominators retain
    // the checked quotient path. Multiplication remains outward rounded.
#define REFLECTED_RECIPROCAL(D,L,H) if(d==D)return imul(ip(n),iv(asfloat(L),asfloat(H)));
#include "reflection_source_optical_reciprocals.inc"
#undef REFLECTED_RECIPROCAL
    return idiv(ip(n),ip(d));
}
Interval ipi() {return ic(3.14159265358979323846);}
OPTICAL_BOUNDARY Interval isin_body(Interval a) {
    if(!isfinite(a.lo) || !isfinite(a.hi) || max(abs(a.lo),abs(a.hi))>1048576) {intervalFailed=true;return iv(-1,1);}
    // Any integer multiple of 2*pi is legal. An imprecise centre quotient
    // merely produces a wider reduced interval; it does not change periodicity.
    const float n=floor(((a.lo+a.hi)*.5)/6.2831853071795864769+.5);
    Interval x=isub(a,imul(ip(n*2),ipi())),half=imul(ipi(),ip(.5));
    if(x.lo>half.hi)x=isub(ipi(),x);
    else if(x.hi<-half.hi)x=isub(ineg(ipi()),x);
    if(x.lo<-half.hi || x.hi>half.hi)return iv(-1,1);
    const float coefficient[9]={1,-.16666666666666666667,.008333333333333333333,
        -.00019841269841269841270,2.7557319223985890653e-6,-2.5052108385441718775e-8,
        1.6059043836821614599e-10,-7.6471637318198164759e-13,2.8114572543455207632e-15};
    const Interval square=isquare(x);Interval sum=ic(coefficient[8]);
    // Keep the same eight ordered Horner operations, coefficients and
    // outward rounding. Vulkan needs one bounded loop body rather than
    // eight expanded copies of the compensated interval arithmetic.
    // This controls compilation structure, not precision or proof scope.
#if defined(__spirv__)
    [loop]
#else
    [unroll]
#endif
    for(int k=7;k>=0;--k)sum=iadd(imul(sum,square),ic(coefficient[k]));
    // Taylor degree 17: |remainder| <= (pi/2)^19/19! < 3e-13.
    // Use 4e-12 to also enclose the scalar production polynomial approximation.
    return iclamp(iadd(imul(x,sum),iv(-4.e-12,4.e-12)),ip(-1),ip(1));
}
// Publish both endpoints of the SAME complete interval polynomial once.
// Invocation-private, consumed immediately; isin_body has no sine recursion.
// All coefficients, Horner operations, input/refusal guards and bounds remain.
static float interval_sine_upper=0;
OPTICAL_SCALAR_BOUNDARY float sine_bounds(float lo,float hi) {
    const Interval value=isin_body(iv(lo,hi));interval_sine_upper=value.hi;return value.lo;
}
OPTICAL_SCALAR_BOUNDARY float sine_bound(float lo,float hi,bool upper) {
    const float lower=sine_bounds(lo,hi);return upper?interval_sine_upper:lower;
}
Interval isin(Interval a) {const float lo=sine_bounds(a.lo,a.hi);return iv(lo,interval_sine_upper);}
Interval icos(Interval a) {return isin(iadd(a,imul(ipi(),ip(.5))));}
Interval3 i3(Interval x,Interval y,Interval z) {Interval3 v;v.x=x;v.y=y;v.z=z;return v;}
Interval3 p3(float3 v) {return i3(ip(v.x),ip(v.y),ip(v.z));}
Interval3 vadd(Interval3 a,Interval3 b) {return i3(iadd(a.x,b.x),iadd(a.y,b.y),iadd(a.z,b.z));}
Interval3 vsub(Interval3 a,Interval3 b) {return vadd(a,i3(ineg(b.x),ineg(b.y),ineg(b.z)));}
Interval3 vmul(Interval3 a,Interval b) {return i3(imul(a.x,b),imul(a.y,b),imul(a.z,b));}
Interval vdot(Interval3 a,Interval3 b) {return iadd(iadd(imul(a.x,b.x),imul(a.y,b.y)),imul(a.z,b.z));}
Interval3 vcross(Interval3 a,Interval3 b) {return i3(isub(imul(a.y,b.z),imul(a.z,b.y)),isub(imul(a.z,b.x),imul(a.x,b.z)),isub(imul(a.x,b.y),imul(a.y,b.x)));}
Interval vlength(Interval3 a) {return isqrt(iadd(iadd(isquare(a.x),isquare(a.y)),isquare(a.z)));}
OPTICAL_BOUNDARY Interval3 vunit(Interval3 a) {return vmul(a,idiv(ip(1),vlength(a)));}
Interval3 vreflect(Interval3 a,Interval3 n) {return vsub(a,vmul(n,imul(ip(2),vdot(a,n))));}
Interval3 vhull(Interval3 a,Interval3 b) {return i3(ihull(a.x,b.x),ihull(a.y,b.y),ihull(a.z,b.z));}
Interval3 vrow(Interval3 a,ReflectionLiquidFrame f) {
    return vadd(vadd(vmul(p3(f.rotation0.xyz),a.x),vmul(p3(f.rotation1.xyz),a.y)),vmul(p3(f.rotation2.xyz),a.z));
}
Interval3 vcol(Interval3 a,ReflectionLiquidFrame f) {return i3(vdot(p3(f.rotation0.xyz),a),vdot(p3(f.rotation1.xyz),a),vdot(p3(f.rotation2.xyz),a));}
Interval iband(Interval f,Interval frequency) {const Interval s=isquare(imul(f,frequency));return idiv(ip(1),iadd(ip(1),isquare(s)));}
Interval ihash(int x,int z) {uint n=uint(x)*1597334677U ^ uint(z)*3812015801U;n^=n>>16;n*=2246822519U;n^=n>>13;return iratio(n&65535U,65535);}
OPTICAL_BOUNDARY Interval3 ilava(Interval x,Interval z,Interval t,Interval footprint) {
    const Interval bend=iadd(isub(imul(x,iratio(6,1000)),imul(z,iratio(8,1000))),imul(t,iratio(11,100))),filter=iband(footprint,iratio(10,1000));
    const Interval a=iadd(isub(iadd(imul(x,iratio(18,1000)),imul(z,iratio(11,1000))),imul(t,iratio(45,100))),imul(imul(iratio(65,100),isin(bend)),filter));
    const Interval b=iadd(isub(imul(x,iratio(47,1000)),imul(z,iratio(25,1000))),imul(t,iratio(60,100)));
    const Interval c=isub(isub(imul(z,iratio(22,1000)),imul(x,iratio(9,1000))),imul(t,iratio(32,100)));
    const Interval fa=iband(footprint,iratio(22,1000)),fb=iband(footprint,iratio(54,1000)),fc=iband(footprint,iratio(24,1000));
    Interval h=iadd(iadd(imul(imul(ip(9),isin(a)),fa),imul(imul(ip(2.5),isin(b)),fb)),imul(imul(ip(5),isin(c)),fc));
    const Interval cb=imul(icos(bend),filter);
    Interval dx=isub(iadd(imul(imul(imul(ip(9),iadd(iratio(18,1000),imul(iratio(39,10000),cb))),icos(a)),fa),imul(imul(iratio(1175,10000),icos(b)),fb)),imul(imul(iratio(45,1000),icos(c)),fc));
    Interval dz=iadd(isub(imul(imul(imul(ip(9),isub(iratio(11,1000),imul(iratio(52,10000),cb))),icos(a)),fa),imul(imul(iratio(625,10000),icos(b)),fb)),imul(imul(iratio(110,1000),icos(c)),fc));
    const Interval ripple=isub(iadd(imul(x,iratio(173,1000)),imul(z,iratio(129,1000))),imul(t,iratio(73,100))),fr=iband(footprint,iratio(216,1000));
    h=iadd(h,imul(imul(iratio(38,100),isin(ripple)),fr));dx=iadd(dx,imul(imul(iratio(6574,100000),icos(ripple)),fr));dz=iadd(dz,imul(imul(iratio(4902,100000),icos(ripple)),fr));
    if(footprint.lo<24) {
        const Interval xx=idiv(x,ip(128)),zz=idiv(z,ip(128));
        if(floor(xx.lo)!=floor(xx.hi) || floor(zz.lo)!=floor(zz.hi)) {
            // Across a hash-cell boundary include both dome/no-dome branches.
            // Bound 6*A*bx/(128*r^2) by 256 (A<=10,r>=.055,|bx|<=1).
            h=iadd(h,iv(0,10));dx=iadd(dx,iv(-256,256));dz=iadd(dz,iv(-256,256));
        } else {
            const int cx=int(floor(xx.lo)),cz=int(floor(zz.lo));const Interval seed=ihash(cx,cz);
            if(seed.hi>.64) {
                const Interval bx=isub(isub(xx,ip(cx)),iadd(iratio(28,100),imul(iratio(44,100),ihash(cx+19,cz))));
                const Interval bz=isub(isub(zz,ip(cz)),iadd(iratio(28,100),imul(iratio(44,100),ihash(cx,cz+29))));
                Interval phase=iadd(imul(t,iratio(14,100)),imul(seed,ip(7)));
                if(floor(phase.lo)==floor(phase.hi))phase=isub(phase,ip(floor(phase.lo)));else phase=iv(0,1);
                const Interval life=isquare(isin(imul(phase,ic(3.14159265)))),radius=iadd(iratio(55,1000),imul(iratio(14,100),phase)),rr=isquare(radius);
                const Interval dome=iclamp(isub(ip(1),idiv(iadd(isquare(bx),isquare(bz)),rr)),ip(0),ip(1));
                const Interval amplitude=imul(imul(ip(10),life),iband(footprint,iratio(18,100))),dd=isquare(dome);
                Interval dh=imul(imul(amplitude,dd),dome),derivative=idiv(imul(imul(ip(-6),amplitude),dd),imul(ip(128),rr));
                Interval ddx=imul(derivative,bx),ddz=imul(derivative,bz);
                if(seed.lo<=.64 || footprint.hi>=24) {dh=ihull(dh,ip(0));ddx=ihull(ddx,ip(0));ddz=ihull(ddz,ip(0));}
                h=iadd(h,dh);dx=iadd(dx,ddx);dz=iadd(dz,ddz);
            }
        }
    }
    return i3(h,dx,dz);
}
Interval3 oriented(Interval3 n,Interval3 direction) {
    const Interval dot=vdot(n,direction);
    if(dot.lo>0)return vmul(n,ip(-1));
    if(dot.hi<=0)return n;
    return vhull(n,vmul(n,ip(-1)));
}
Interval3 plane_normal(ReflectionSpecularPlane p) {
    if(reflection_specular_analytic(p))return vunit(p3(p.b.xyz));
    return vunit(vcross(vsub(p3(p.b.xyz),p3(p.a.xyz)),vsub(p3(p.c.xyz),p3(p.a.xyz))));
}
bool outside_face(ReflectionSpecularPlane p,Interval3 hit) {
    if(reflection_specular_analytic(p))return false;
    const Interval3 u=vsub(p3(p.b.xyz),p3(p.a.xyz)),v=vsub(p3(p.c.xyz),p3(p.a.xyz)),r=vsub(hit,p3(p.a.xyz));
    const Interval aa=vdot(u,u),ab=vdot(u,v),bb=vdot(v,v),ra=vdot(r,u),rb=vdot(r,v),det=isub(imul(aa,bb),isquare(ab));
    if(det.lo<=0)return false;
    const Interval x=idiv(isub(imul(bb,ra),imul(ab,rb)),det),y=idiv(isub(imul(aa,rb),imul(ab,ra)),det);
    return x.hi<ic(-.00001).lo || y.hi<ic(-.00001).lo || iadd(x,y).lo>ic(1.00001).hi;
}
OPTICAL_BOUNDARY Interval3 liquid_normal(ReflectionLiquidFrame f,Interval3 direction,Interval distance,Interval footprint,inout Interval3 hit) {
    Interval3 p=vadd(vrow(hit,f),p3(float3(f.rotation0.w,f.rotation1.w,f.rotation2.w)));const Interval t=ip(f.settings.x);Interval dx,dz;
    if(f.settings.y==3) {
        // One static lava call site: evaluate displacement, then the displaced
        // normal. Keep the loop to avoid cloning this large interval program
        // into both sites inside a driver's pipeline compiler.
        dx=dz=ip(0);const Interval3 travel=vrow(direction,f);
        [loop] for(uint evaluation=0;evaluation<2;++evaluation) {
            const Interval3 wave=ilava(p.x,p.z,t,footprint);
            if(!evaluation) {
                const Interval limit=imul(distance,iratio(2,10)),shift=iclamp(idiv(ineg(wave.x),imax(travel.y,iratio(12,100))),ineg(limit),limit);
                p=vadd(p,vmul(travel,shift));hit=vadd(hit,vmul(direction,shift));
            } else {dx=wave.y;dz=wave.z;}
        }
    } else {
        const Interval a=isub(iadd(imul(p.x,iratio(18,1000)),imul(p.z,iratio(11,1000))),imul(t,iratio(8,10)));
        const Interval b=iadd(isub(imul(p.x,iratio(47,1000)),imul(p.z,iratio(25,1000))),imul(t,iratio(12,10)));
        const Interval c=isub(isub(imul(p.z,iratio(22,1000)),imul(p.x,iratio(9,1000))),imul(t,iratio(65,100)));
        dx=iadd(imul(imul(iratio(55,1000),icos(a)),iband(footprint,iratio(22,1000))),imul(imul(iratio(25,1000),icos(b)),iband(footprint,iratio(54,1000))));
        dz=isub(imul(imul(iratio(45,1000),icos(c)),iband(footprint,iratio(24,1000))),imul(imul(iratio(20,1000),icos(b)),iband(footprint,iratio(54,1000))));
    }
    return oriented(vunit(vcol(i3(dx,ip(-1),dz),f)),direction);
}
// True means all physical paths in this entire box are impossible or exceed
// the UNCHANGED .002-pixel angular gate. False includes every unresolved case.
OPTICAL_BOUNDARY bool optical_excluded_target(float4 box,ReflectionRoughFrame receiver,ReflectionLiquidFrame liquid,
    ReflectionSpecularPlane planes[4],uint4 control,Interval3 target,bool finiteTerminal) {
    Interval3 outgoing=vunit(i3(idiv(isub(iv(box.x,box.z),ip(receiver.projection.z)),ip(receiver.projection.x)),
        idiv(isub(iv(box.y,box.w),ip(receiver.projection.w)),ip(receiver.projection.y)),ip(1)));
    Interval3 origin=p3(float3(0,0,0));Interval bias=ip(0);
    // Primary plus every ordered hop share one interval-forward body, and in
    // particular one static liquid-normal call. Primary depth/footprint/bias
    // and rough quadrature still use their native rules; no hop is omitted.
    [loop] for(uint stage=0;stage<=control.x;++stage) {
        const bool primary=stage==0,curved=primary?control.z!=0:(control.y&(1U<<(stage-1)))!=0;
        ReflectionSpecularPlane plane;
        if(primary) {plane.a=receiver.a;plane.b=receiver.b;plane.c=receiver.c;}
        else plane=planes[stage-1];
        Interval3 n;if(curved)n=p3(liquid.planeNormal.xyz);else n=oriented(plane_normal(plane),outgoing);
        const Interval denominator=vdot(outgoing,n),distance=idiv(vdot(vsub(p3(curved?liquid.planePoint.xyz:plane.a.xyz),origin),n),denominator);
        if(primary) {
            const Interval depth=imul(distance,outgoing.z);
            if(distance.hi<=0 || depth.hi<receiver.extentClip.z || depth.lo>receiver.extentClip.w)return !intervalFailed;
        } else if(distance.hi<=bias.lo || distance.lo>=65536)return !intervalFailed;
        Interval3 hit=vadd(origin,vmul(outgoing,distance));
        if(curved) {
            Interval radius;if(primary)radius=distance;else radius=vlength(hit);
            const Interval footprint=idiv(idiv(radius,ip(max(liquid.projection.x,1))),imax(iabs(denominator),iratio(4,100)));
            n=liquid_normal(liquid,outgoing,distance,footprint,hit);
        } else if(outside_face(plane,hit))return !intervalFailed;
        bias=imax(iratio(primary && !curved && !reflection_specular_analytic(plane)?1:5,100),imul(distance,iratio(1,100000)));
        origin=vadd(hit,vmul(n,bias));outgoing=vreflect(outgoing,n);
        if(primary && !curved && receiver.settings.x!=0) {
            const Interval ay=iabs(outgoing.y);Interval3 t;
            if(ay.hi<.95)t=vunit(vcross(outgoing,p3(float3(0,1,0))));
            else if(ay.lo>=.95)t=vunit(vcross(outgoing,p3(float3(1,0,0))));
            else {intervalFailed=true;return false;}
            const int2 taps[8]={int2(500,0),int2(-500,0),int2(0,500),int2(0,-500),int2(612,612),int2(-612,612),int2(612,-612),int2(-612,-612)};
            const int2 tap=taps[uint(receiver.settings.y)];
            const Interval3 rough=vunit(vadd(outgoing,vmul(vadd(vmul(t,iratio(tap.x,1000)),vmul(vcross(outgoing,t),iratio(tap.y,1000))),isquare(ip(receiver.settings.x)))));
            const Interval nd=vdot(rough,n);if(nd.lo>0)outgoing=rough;else if(nd.hi>0)outgoing=vhull(outgoing,rough);
        }
        if(intervalFailed)return false;
    }
    Interval3 travel=target;if(finiteTerminal)travel=vsub(travel,origin);
    const Interval len=vlength(travel);
    if(vdot(travel,outgoing).hi<=0 || len.hi<=0)return !intervalFailed;
    const Interval3 error=vcross(travel,outgoing);
    // Both orthogonal tangent residuals <= .002 imply each component of this
    // cross product <= sqrt(2)*.002/focal*|travel|. Use 2 as an outward bound.
    const float cap=interval_up(quotient_bound(interval_up(len.hi*.004),max(receiver.projection.x,receiver.projection.y),true));
    return !intervalFailed && (error.x.lo>cap || error.x.hi<-cap || error.y.lo>cap || error.y.hi<-cap || error.z.lo>cap || error.z.hi<-cap);
}
// Legacy diagnostic endpoint ABI, with unchanged point arithmetic.
OPTICAL_BOUNDARY bool optical_excluded(float4 box,ReflectionRoughFrame receiver,ReflectionLiquidFrame liquid,
    ReflectionSpecularPlane planes[4],uint4 control,float4 terminal) {
    return optical_excluded_target(box,receiver,liquid,planes,control,p3(terminal.xyz),terminal.w!=0);
}
