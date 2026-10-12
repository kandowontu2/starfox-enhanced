// Extra precision for ordered old-path geometry, using ONLY binary32 shader
// operations. Keep low words until the terminal angular residual is formed:
// rounding a direction at each curved hop amplifies errors at the next dome.
// This is not a new lava appearance, a flat liquid proxy, or cached radiance.
#ifndef STARFOX_REFLECTION_CURVED_PRECISION
#define STARFOX_REFLECTION_CURVED_PRECISION

// Non-overlapping high/low words. `precise` prevents contraction/reassociation
// of the error-free transforms; no shaderFloat64, shaderInt64 or native FMA.
// SPIR-V owners must additionally retain the whole-program IEEE environment
// (including SignedZeroInfNanPreserve 32) and query that capability. Helper
// NoContraction decorations alone failed the actual nested division gate.
struct CurvedScalar {float high,low;};
CurvedScalar curved_scalar(float high,float low) {CurvedScalar r;r.high=high;r.low=low;return r;}
CurvedScalar curved_dd_add_core(float ax,float ay,float bx,float by) {
    precise float s=ax+bx,v=s-ax;
    precise float e=(ax-(s-v))+(bx-v);
    precise float t=ay+by,w=t-ay,tail=(ay-(t-w))+(by-w);
    // Also retain the low-word addition error. When large phase products
    // cancel, that error is part of the small remainder, not negligible noise.
    e+=t;
    precise float h=s+e,l=e-(h-s);l+=tail;
    precise float high=h+l,low=l-(high-h);return curved_scalar(high,low);
}
float2 curved_dd_add(float2 a,float2 b) {
    CurvedScalar r=curved_dd_add_core(a.x,a.y,b.x,b.y);return float2(r.high,r.low);
}
float2 curved_dd_sub(float2 a,float2 b) {return curved_dd_add(a,-b);}
CurvedScalar curved_dd_mul_core(float ax,float ay,float bx,float by) {
    precise float p=ax*bx;
    precise float sa=ax*4097,ah=sa-(sa-ax),al=ax-ah;
    precise float sb=bx*4097,bh=sb-(sb-bx),bl=bx-bh;
    precise float e=((ah*bh-p)+ah*bl+al*bh)+al*bl+ax*by+ay*bx+ay*by;
    precise float h=p+e,l=e-(h-p);return curved_scalar(h,l);
}
float2 curved_dd_mul(float2 a,float2 b) {
    CurvedScalar r=curved_dd_mul_core(a.x,a.y,b.x,b.y);return float2(r.high,r.low);
}
CurvedScalar curved_dd_div_core(float ax,float ay,float bx,float by) {
    CurvedScalar q=curved_scalar(ax/bx,0),p=curved_dd_mul_core(q.high,q.low,bx,by);
    CurvedScalar r=curved_dd_add_core(ax,ay,-p.high,-p.low);
    q=curved_dd_add_core(q.high,q.low,r.high/bx,0);
    p=curved_dd_mul_core(q.high,q.low,bx,by);r=curved_dd_add_core(ax,ay,-p.high,-p.low);
    return curved_dd_add_core(q.high,q.low,(r.high+r.low)/bx,0);
}
float2 curved_dd_div(float2 a,float2 b) {
    CurvedScalar r=curved_dd_div_core(a.x,a.y,b.x,b.y);return float2(r.high,r.low);
}
float2 curved_dd_ratio(float numerator,float denominator) {
    return curved_dd_div(float2(numerator,0),float2(denominator,0));
}
float curved_dd_float(float2 a) {return a.x+a.y;}
bool curved_dd_less(float2 a,float2 b) {return a.x<b.x || (a.x==b.x && a.y<b.y);}
float2 curved_dd_max(float2 a,float2 b) {return curved_dd_less(a,b)?b:a;}
float2 curved_dd_min(float2 a,float2 b) {return curved_dd_less(a,b)?a:b;}
float2 curved_dd_abs(float2 a) {return curved_dd_less(a,0)?-a:a;}
float2 curved_dd_sqrt(float2 a) {
    float s=sqrt(a.x);
    return curved_dd_add(float2(s,0),curved_dd_div(curved_dd_sub(a,curved_dd_mul(float2(s,0),float2(s,0))),float2(2*s,0)));
}
float2 curved_dd_floor(float2 a) {
    float h=floor(a.x);
    return curved_dd_add(float2(h,0),float2(floor(curved_dd_float(curved_dd_sub(a,float2(h,0)))),0));
}
#ifndef STARFOX_CURVED_TRIG_TABLE
#define STARFOX_CURVED_TRIG_TABLE 0
#endif
#ifndef STARFOX_CURVED_NOINLINE
#define STARFOX_CURVED_NOINLINE [noinline]
#endif
#ifndef STARFOX_CURVED_UNROLL
#define STARFOX_CURVED_UNROLL [unroll]
#endif
#ifndef STARFOX_CURVED_TRIG_BINDING
#define STARFOX_CURVED_TRIG_BINDING :register(t1,space0)
#endif
#if STARFOX_CURVED_TRIG_TABLE
// Shared immutable sin/cos high/low words at 1024 equally spaced phases.
// The owner uploads this 16 KiB once; neither old geometry nor radiance is
// stored here. The scalar-only polynomial remains a table-free fallback.
ByteAddressBuffer curvedTrig STARFOX_CURVED_TRIG_BINDING;
#endif
STARFOX_CURVED_NOINLINE float curved_dd_sin_core(float ax,float ay,uint word) {
#if STARFOX_CURVED_TRIG_TABLE
    const float stepHigh=3.1415927410125732421875/512,stepLow=-8.742278000372475e-8/512;
    CurvedScalar q=curved_dd_div_core(ax,ay,stepHigh,stepLow),n=curved_dd_add_core(q.high,q.low,.5,0);
    float nh=floor(n.high),nl=floor((n.high-nh)+n.low);
    // Keep both integer words: the bounded domain can exceed 2^24 table cells.
    // A single binary32 nearest-cell index would silently lose those low bits.
    CurvedScalar nearest=curved_dd_add_core(nh,0,nl,0);
    uint cell=(uint(int(nh))+uint(int(nl)))&1023U;
    uint4 base=curvedTrig.Load4(cell*16);
    // Cancel each product before combining it: normalizing the large product
    // first would round away small phase bits even with a high/low pair.
    CurvedScalar p=curved_dd_mul_core(nearest.high,0,stepHigh,0);
    CurvedScalar delta=curved_dd_add_core(ax,ay,-p.high,-p.low);
    p=curved_dd_mul_core(nearest.low,0,stepHigh,0);
    delta=curved_dd_add_core(delta.high,delta.low,-p.high,-p.low);
    p=curved_dd_mul_core(nearest.high,0,stepLow,0);
    delta=curved_dd_add_core(delta.high,delta.low,-p.high,-p.low);
    p=curved_dd_mul_core(nearest.low,0,stepLow,0);
    delta=curved_dd_add_core(delta.high,delta.low,-p.high,-p.low);
    // The high/low step omits a ~6.7e-18 tail. At the supported phase limit
    // that accumulates to more than the scalar error gate; keep its product
    // separate instead of rounding it back into stepLow.
    CurvedScalar tail=curved_dd_mul_core(nearest.high,nearest.low,-6.699705075948761e-18,0);
    delta=curved_dd_add_core(delta.high,delta.low,-tail.high,-tail.low);
    // |delta| <= pi/1024. These small corrections retain the high/low phase;
    // their binary32 rounding is far below the unchanged scalar error gate.
    precise float square=delta.high*delta.high;
    precise float sc=delta.high*square*(-.16666666666666666667+square*.008333333333333333333);
    precise float cc=square*(-.5+square*.041666666666666666667)-delta.high*delta.low;
    CurvedScalar sine=curved_dd_add_core(delta.high,delta.low,sc,0),cosine=curved_dd_add_core(1,0,cc,0);
    // Cosine rotates the bounded table pair, not the large input phase.
    // Adding pi/2 before reduction would lose low phase bits at the limit.
    const bool cosineWord=(word&2U)!=0;
    p=curved_dd_mul_core(asfloat(cosineWord?base.z:base.x),asfloat(cosineWord?base.w:base.y),cosine.high,cosine.low);
    q=curved_dd_mul_core(cosineWord?-asfloat(base.x):asfloat(base.z),cosineWord?-asfloat(base.y):asfloat(base.w),sine.high,sine.low);
    CurvedScalar sum=curved_dd_add_core(p.high,p.low,q.high,q.low);
    return (word&1U)==0?sum.high:sum.low;
#else
    // DXIL non-entry functions require scalar SSA, including loop phis.
    // Keep this shared polynomial scalar instead of hiding vector phis in a
    // noinline float2 return (which the DXIL validator correctly rejects).
    const float piHigh=3.1415927410125732421875,piLow=-8.742278000372475e-8;
    CurvedScalar q=curved_dd_div_core(ax,ay,piHigh*2,piLow*2),n=curved_dd_add_core(q.high,q.low,.5,0);
    float nearest=floor(n.high);nearest+=floor((n.high-nearest)+n.low);
    CurvedScalar p=curved_dd_mul_core(nearest,0,piHigh*2,0);
    CurvedScalar x=curved_dd_add_core(ax,ay,-p.high,-p.low);
    p=curved_dd_mul_core(nearest,0,piLow*2,0);
    x=curved_dd_add_core(x.high,x.low,-p.high,-p.low);
    CurvedScalar tail=curved_dd_mul_core(nearest,0,-6.860497997771531e-15,0);
    x=curved_dd_add_core(x.high,x.low,-tail.high,-tail.low);
    if((word&2U)!=0) x=curved_dd_add_core(x.high,x.low,piHigh*.5,piLow*.5);
    if(x.high>piHigh*.5 || (x.high==piHigh*.5 && x.low>piLow*.5))
        x=curved_dd_add_core(piHigh,piLow,-x.high,-x.low);
    if(x.high<-piHigh*.5 || (x.high==-piHigh*.5 && x.low<-piLow*.5))
        x=curved_dd_add_core(-piHigh,-piLow,-x.high,-x.low);
    // The same degree-17 Taylor polynomial, in Horner form. Each coefficient
    // is the high/low binary32 split of (-1)^k/(2k+1)!: do not round it to a
    // single float. This removes eight repeated extended-precision divisions
    // per sine word without changing the wave, range reduction or error gate.
    const float high[9]={1,-.1666666716337204,.0083333337679505348,
        -.00019841270113829523,2.7557318844628753e-6,-2.5052107943679403e-8,
        1.6059044372074283e-10,-7.6471636098127127e-13,2.8114573589663704e-15};
    const float low[9]={0,4.9670538793122887e-9,-4.3461720333759502e-10,
        2.7255968749334558e-12,3.7935712242972291e-14,-4.4176230446483665e-16,
        -5.3525265115627256e-18,-1.2200710471178288e-20,-1.0462084739763658e-22};
    CurvedScalar square=curved_dd_mul_core(x.high,x.low,x.high,x.low),sum=curved_scalar(high[8],low[8]);
    STARFOX_CURVED_UNROLL for(int k=7;k>=0;--k) {
        p=curved_dd_mul_core(sum.high,sum.low,square.high,square.low);
        sum=curved_dd_add_core(p.high,p.low,high[k],low[k]);
    }
    sum=curved_dd_mul_core(x.high,x.low,sum.high,sum.low);
    return (word&1U)==0?sum.high:sum.low;
#endif
}
float2 curved_dd_sin(float2 a) {
    // Scalar return avoids an illegal DXIL vector/aggregate-return ABI.
    // Both words evaluate the identical phase; neither rounds the phase away.
    return float2(curved_dd_sin_core(a.x,a.y,0),curved_dd_sin_core(a.x,a.y,1));
}
float2 curved_dd_cos(float2 a) {
    return float2(curved_dd_sin_core(a.x,a.y,2),curved_dd_sin_core(a.x,a.y,3));
}
#if !defined(STARFOX_CURVED_SCALAR_ONLY)
struct CurvedVector {float2 x,y,z;};
CurvedVector curved_vec(float2 x,float2 y,float2 z) {CurvedVector r;r.x=x;r.y=y;r.z=z;return r;}
CurvedVector curved_vec_float(float3 a) {return curved_vec(float2(a.x,0),float2(a.y,0),float2(a.z,0));}
float3 curved_vec_result(CurvedVector a) {return float3(curved_dd_float(a.x),curved_dd_float(a.y),curved_dd_float(a.z));}
CurvedVector curved_vec_add(CurvedVector a,CurvedVector b) {return curved_vec(curved_dd_add(a.x,b.x),curved_dd_add(a.y,b.y),curved_dd_add(a.z,b.z));}
CurvedVector curved_vec_sub(CurvedVector a,CurvedVector b) {return curved_vec_add(a,curved_vec(-b.x,-b.y,-b.z));}
CurvedVector curved_vec_scale(CurvedVector a,float2 b) {return curved_vec(curved_dd_mul(a.x,b),curved_dd_mul(a.y,b),curved_dd_mul(a.z,b));}
float2 curved_vec_dot(CurvedVector a,CurvedVector b) {
    return curved_dd_add(curved_dd_add(curved_dd_mul(a.x,b.x),curved_dd_mul(a.y,b.y)),curved_dd_mul(a.z,b.z));
}
CurvedVector curved_vec_cross(CurvedVector a,CurvedVector b) {
    return curved_vec(curved_dd_sub(curved_dd_mul(a.y,b.z),curved_dd_mul(a.z,b.y)),
        curved_dd_sub(curved_dd_mul(a.z,b.x),curved_dd_mul(a.x,b.z)),curved_dd_sub(curved_dd_mul(a.x,b.y),curved_dd_mul(a.y,b.x)));
}
float2 curved_vec_length(CurvedVector a) {return curved_dd_sqrt(curved_vec_dot(a,a));}
CurvedVector curved_vec_unit(CurvedVector a) {return curved_vec_scale(a,curved_dd_div(float2(1,0),curved_vec_length(a)));}
CurvedVector curved_vec_reflect(CurvedVector a,CurvedVector n) {
    return curved_vec_sub(a,curved_vec_scale(n,curved_dd_mul(float2(2,0),curved_vec_dot(a,n))));
}
CurvedVector curved_vec_row(CurvedVector a,ReflectionLiquidFrame f) {
    return curved_vec_add(curved_vec_add(curved_vec_scale(curved_vec_float(f.rotation0.xyz),a.x),
        curved_vec_scale(curved_vec_float(f.rotation1.xyz),a.y)),curved_vec_scale(curved_vec_float(f.rotation2.xyz),a.z));
}
CurvedVector curved_vec_col(CurvedVector a,ReflectionLiquidFrame f) {
    return curved_vec(curved_vec_dot(curved_vec_float(f.rotation0.xyz),a),
        curved_vec_dot(curved_vec_float(f.rotation1.xyz),a),curved_vec_dot(curved_vec_float(f.rotation2.xyz),a));
}
float2 curved_band(float2 footprint,float2 frequency) {
    float2 x=curved_dd_mul(footprint,frequency),square=curved_dd_mul(x,x);
    return curved_dd_div(float2(1,0),curved_dd_add(float2(1,0),curved_dd_mul(square,square)));
}
float2 curved_hash(int x,int z) {
    uint n=uint(x)*1597334677u ^ uint(z)*3812015801u;
    n^=n>>16;n*=2246822519u;n^=n>>13;return curved_dd_ratio(n&65535u,65535);
}
// The exact optical part of lava_surface.inc: broad crossing waves, bent swell,
// ripple and continuously growing/collapsing domes. Appearance/heat/crust are
// deliberately absent, since this helper cannot reuse them from an old frame.
CurvedVector curved_lava(float2 x,float2 z,float2 time,float2 footprint) {
    float2 bend=curved_dd_add(curved_dd_sub(curved_dd_mul(x,curved_dd_ratio(6,1000)),curved_dd_mul(z,curved_dd_ratio(8,1000))),curved_dd_mul(time,curved_dd_ratio(11,100)));
    float2 filter=curved_band(footprint,curved_dd_ratio(10,1000));
    float2 a=curved_dd_add(curved_dd_sub(curved_dd_add(curved_dd_mul(x,curved_dd_ratio(18,1000)),curved_dd_mul(z,curved_dd_ratio(11,1000))),
        curved_dd_mul(time,curved_dd_ratio(45,100))),curved_dd_mul(curved_dd_mul(curved_dd_ratio(65,100),curved_dd_sin(bend)),filter));
    float2 b=curved_dd_add(curved_dd_sub(curved_dd_mul(x,curved_dd_ratio(47,1000)),curved_dd_mul(z,curved_dd_ratio(25,1000))),curved_dd_mul(time,curved_dd_ratio(60,100)));
    float2 c=curved_dd_sub(curved_dd_sub(curved_dd_mul(z,curved_dd_ratio(22,1000)),curved_dd_mul(x,curved_dd_ratio(9,1000))),curved_dd_mul(time,curved_dd_ratio(32,100)));
    float2 fa=curved_band(footprint,curved_dd_ratio(22,1000)),fb=curved_band(footprint,curved_dd_ratio(54,1000)),fc=curved_band(footprint,curved_dd_ratio(24,1000));
    float2 h=curved_dd_add(curved_dd_add(curved_dd_mul(curved_dd_mul(float2(9,0),curved_dd_sin(a)),fa),curved_dd_mul(curved_dd_mul(float2(2.5,0),curved_dd_sin(b)),fb)),curved_dd_mul(curved_dd_mul(float2(5,0),curved_dd_sin(c)),fc));
    float2 cb=curved_dd_mul(curved_dd_cos(bend),filter);
    float2 dx=curved_dd_sub(curved_dd_add(curved_dd_mul(curved_dd_mul(curved_dd_mul(float2(9,0),curved_dd_add(curved_dd_ratio(18,1000),curved_dd_mul(curved_dd_ratio(39,10000),cb))),curved_dd_cos(a)),fa),
        curved_dd_mul(curved_dd_mul(curved_dd_ratio(1175,10000),curved_dd_cos(b)),fb)),curved_dd_mul(curved_dd_mul(curved_dd_ratio(45,1000),curved_dd_cos(c)),fc));
    float2 dz=curved_dd_add(curved_dd_sub(curved_dd_mul(curved_dd_mul(curved_dd_mul(float2(9,0),curved_dd_sub(curved_dd_ratio(11,1000),curved_dd_mul(curved_dd_ratio(52,10000),cb))),curved_dd_cos(a)),fa),
        curved_dd_mul(curved_dd_mul(curved_dd_ratio(625,10000),curved_dd_cos(b)),fb)),curved_dd_mul(curved_dd_mul(curved_dd_ratio(110,1000),curved_dd_cos(c)),fc));
    float2 ripple=curved_dd_sub(curved_dd_add(curved_dd_mul(x,curved_dd_ratio(173,1000)),curved_dd_mul(z,curved_dd_ratio(129,1000))),curved_dd_mul(time,curved_dd_ratio(73,100)));
    float2 fr=curved_band(footprint,curved_dd_ratio(216,1000));
    h=curved_dd_add(h,curved_dd_mul(curved_dd_mul(curved_dd_ratio(38,100),curved_dd_sin(ripple)),fr));
    dx=curved_dd_add(dx,curved_dd_mul(curved_dd_mul(curved_dd_ratio(6574,100000),curved_dd_cos(ripple)),fr));
    dz=curved_dd_add(dz,curved_dd_mul(curved_dd_mul(curved_dd_ratio(4902,100000),curved_dd_cos(ripple)),fr));
    float2 xx=curved_dd_div(x,float2(128,0)),zz=curved_dd_div(z,float2(128,0));
    float2 cx=curved_dd_floor(xx),cz=curved_dd_floor(zz),seed=curved_hash(int(cx.x),int(cz.x));
    if(curved_dd_less(curved_dd_ratio(64,100),seed) && curved_dd_less(footprint,float2(24,0))) {
        float2 bx=curved_dd_sub(curved_dd_sub(xx,cx),curved_dd_add(curved_dd_ratio(28,100),curved_dd_mul(curved_dd_ratio(44,100),curved_hash(int(cx.x)+19,int(cz.x)))));
        float2 bz=curved_dd_sub(curved_dd_sub(zz,cz),curved_dd_add(curved_dd_ratio(28,100),curved_dd_mul(curved_dd_ratio(44,100),curved_hash(int(cx.x),int(cz.x)+29))));
        float2 phase=curved_dd_add(curved_dd_mul(time,curved_dd_ratio(14,100)),curved_dd_mul(seed,float2(7,0)));
        phase=curved_dd_sub(phase,curved_dd_floor(phase));
        float2 life=curved_dd_sin(curved_dd_mul(phase,float2(3.1415927410125732421875,-9.10125732421875e-8)));life=curved_dd_mul(life,life);
        float2 radius=curved_dd_add(curved_dd_ratio(55,1000),curved_dd_mul(curved_dd_ratio(14,100),phase)),rr=curved_dd_mul(radius,radius);
        float2 dome=curved_dd_max(0,curved_dd_min(float2(1,0),curved_dd_sub(float2(1,0),curved_dd_div(curved_dd_add(curved_dd_mul(bx,bx),curved_dd_mul(bz,bz)),rr))));
        float2 amplitude=curved_dd_mul(curved_dd_mul(float2(10,0),life),curved_band(footprint,curved_dd_ratio(18,100))),dd=curved_dd_mul(dome,dome);
        h=curved_dd_add(h,curved_dd_mul(curved_dd_mul(amplitude,dd),dome));
        float2 derivative=curved_dd_div(curved_dd_mul(curved_dd_mul(float2(-6,0),amplitude),dd),curved_dd_mul(float2(128,0),rr));
        dx=curved_dd_add(dx,curved_dd_mul(derivative,bx));dz=curved_dd_add(dz,curved_dd_mul(derivative,bz));
    }
    return curved_vec(h,dx,dz);
}
bool curved_liquid_sample(ReflectionLiquidFrame f,CurvedVector direction,float2 distance,float2 footprint,
    inout CurvedVector hit,out CurvedVector normal) {
    normal=curved_vec_float(0);
    CurvedVector p=curved_vec_add(curved_vec_row(hit,f),curved_vec_float(float3(f.rotation0.w,f.rotation1.w,f.rotation2.w)));
    // Bound argument reduction and cell conversion, rather than manufacturing
    // a converged history guide for an unrepresentable phase/cell. Geometry
    // outside this domain must be recomputed currently by the history owner.
    if(any(abs(curved_vec_result(p))>1048576) || f.settings.x>1048576) return false;
    float2 time=float2(f.settings.x,0),dx,dz;
    if(f.settings.y==3) {
        CurvedVector lava=curved_lava(p.x,p.z,time,footprint),travel=curved_vec_row(direction,f);
        float2 limit=curved_dd_mul(distance,curved_dd_ratio(2,10));
        float2 shift=curved_dd_max(-limit,curved_dd_min(limit,curved_dd_div(-lava.x,curved_dd_max(travel.y,curved_dd_ratio(12,100)))));
        p=curved_vec_add(p,curved_vec_scale(travel,shift));hit=curved_vec_add(hit,curved_vec_scale(direction,shift));
        if(any(abs(curved_vec_result(p))>1048576)) return false;
        lava=curved_lava(p.x,p.z,time,footprint);dx=lava.y;dz=lava.z;
    } else {
        float2 a=curved_dd_sub(curved_dd_add(curved_dd_mul(p.x,curved_dd_ratio(18,1000)),curved_dd_mul(p.z,curved_dd_ratio(11,1000))),curved_dd_mul(time,curved_dd_ratio(8,10)));
        float2 b=curved_dd_add(curved_dd_sub(curved_dd_mul(p.x,curved_dd_ratio(47,1000)),curved_dd_mul(p.z,curved_dd_ratio(25,1000))),curved_dd_mul(time,curved_dd_ratio(12,10)));
        float2 c=curved_dd_sub(curved_dd_sub(curved_dd_mul(p.z,curved_dd_ratio(22,1000)),curved_dd_mul(p.x,curved_dd_ratio(9,1000))),curved_dd_mul(time,curved_dd_ratio(65,100)));
        dx=curved_dd_add(curved_dd_mul(curved_dd_mul(curved_dd_ratio(55,1000),curved_dd_cos(a)),curved_band(footprint,curved_dd_ratio(22,1000))),
            curved_dd_mul(curved_dd_mul(curved_dd_ratio(25,1000),curved_dd_cos(b)),curved_band(footprint,curved_dd_ratio(54,1000))));
        dz=curved_dd_sub(curved_dd_mul(curved_dd_mul(curved_dd_ratio(45,1000),curved_dd_cos(c)),curved_band(footprint,curved_dd_ratio(24,1000))),
            curved_dd_mul(curved_dd_mul(curved_dd_ratio(20,1000),curved_dd_cos(b)),curved_band(footprint,curved_dd_ratio(54,1000))));
    }
    normal=curved_vec_unit(curved_vec_col(curved_vec(dx,float2(-1,0),dz),f));
    if(curved_dd_less(0,curved_vec_dot(normal,direction))) normal=curved_vec_scale(normal,float2(-1,0));
    return all(isfinite(curved_vec_result(hit))) && all(isfinite(curved_vec_result(normal)));
}
#endif // !STARFOX_CURVED_SCALAR_ONLY
#endif // STARFOX_REFLECTION_CURVED_PRECISION
