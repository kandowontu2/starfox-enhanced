#pragma once
// Independent binary64 optical geometry for the native producer fixture.
// Do not include the production liquid implementation or GPU hit results.
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace reflected_liquid_oracle {
using V=std::array<double,3>;
inline V add(V a,V b) {return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
inline V scale(V a,double f) {return {a[0]*f,a[1]*f,a[2]*f};}
inline double dot(V a,V b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline V unit(V a) {return scale(a,1/std::sqrt(dot(a,a)));}
inline double band(double f,double frequency) {const double x=f*frequency;return 1/(1+x*x*x*x);}
inline double hash(int x,int z) {
    std::uint32_t n=std::uint32_t(x)*1597334677u ^ std::uint32_t(z)*3812015801u;
    n^=n>>16;n*=2246822519u;n^=n>>13;return double(n&65535)/65535;
}
inline std::array<double,3> lava(double x,double z,double t,double footprint) {
    const double bend=x*.006-z*.008+t*.11,filter=band(footprint,.010);
    const double a=x*.018+z*.011-t*.45+.65*std::sin(bend)*filter;
    const double b=x*.047-z*.025+t*.60,c=z*.022-x*.009-t*.32;
    const double fa=band(footprint,.022),fb=band(footprint,.054),fc=band(footprint,.024);
    double h=9*std::sin(a)*fa+2.5*std::sin(b)*fb+5*std::sin(c)*fc;
    double dx=9*(.018+.0039*std::cos(bend)*filter)*std::cos(a)*fa+.1175*std::cos(b)*fb-.045*std::cos(c)*fc;
    double dz=9*(.011-.0052*std::cos(bend)*filter)*std::cos(a)*fa-.0625*std::cos(b)*fb+.110*std::cos(c)*fc;
    const double ripple=x*.173+z*.129-t*.73,fr=band(footprint,.216);
    h+=.38*std::sin(ripple)*fr;dx+=.06574*std::cos(ripple)*fr;dz+=.04902*std::cos(ripple)*fr;
    const int cx=int(std::floor(x/128)),cz=int(std::floor(z/128));const double seed=hash(cx,cz);
    if(seed>.64 && footprint<24) {
        const double bx=x/128-cx-(.28+.44*hash(cx+19,cz)),bz=z/128-cz-(.28+.44*hash(cx,cz+29));
        const double phase=t*.14+seed*7-std::floor(t*.14+seed*7),life=std::pow(std::sin(phase*3.14159265),2);
        const double radius=.055+.14*phase,dome=std::clamp(1-(bx*bx+bz*bz)/(radius*radius),0.,1.);
        const double amplitude=10*life*band(footprint,.18),derivative=-6*amplitude*dome*dome/(128*radius*radius);
        h+=amplitude*dome*dome*dome;dx+=derivative*bx;dz+=derivative*bz;
    }
    return {h,dx,dz};
}
struct Frame {
    V point{},normal{},offset{};
    std::array<double,9> rotation{1,0,0,0,1,0,0,0,1};
    std::array<double,4> projection{};
    unsigned width{},height{},material{};
    double near{},far{},time{};
};
inline V row(V a,const Frame& f) {
    V r{};for(unsigned j=0;j<3;++j) for(unsigned i=0;i<3;++i) r[j]+=a[i]*f.rotation[i*3+j];return r;
}
inline V col(V a,const Frame& f) {
    V r{};for(unsigned i=0;i<3;++i) for(unsigned j=0;j<3;++j) r[i]+=f.rotation[i*3+j]*a[j];return r;
}
struct Ray {V direction{},hit{},normal{};double distance{},depth{},bias{};bool valid{};};
inline Ray optical(const Frame& f,double px,double py) {
    Ray r{};if(px<0 || py<0 || px>=f.width || py>=f.height) return r;
    r.direction=unit({(px-f.projection[2])/f.projection[0],(py-f.projection[3])/f.projection[1],1});
    const double den=dot(r.direction,f.normal);if(std::abs(den)<1.e-12) return r;
    r.distance=dot(f.point,f.normal)/den;r.depth=r.distance*r.direction[2];
    if(r.distance<=0 || r.depth<f.near || r.depth>f.far) return r;
    r.hit=scale(r.direction,r.distance);V p=add(row(r.hit,f),f.offset);
    const double footprint=r.distance/std::max(f.projection[0],1.)/std::max(std::abs(den),.04),t=f.time;
    double dx=.055*std::cos(p[0]*.018+p[2]*.011-t*.8)*band(footprint,.022)
        +.025*std::cos(p[0]*.047-p[2]*.025+t*1.2)*band(footprint,.054);
    double dz=.045*std::cos(p[2]*.022-p[0]*.009-t*.65)*band(footprint,.024)
        -.020*std::cos(p[0]*.047-p[2]*.025+t*1.2)*band(footprint,.054);
    if(f.material==3) {
        auto l=lava(p[0],p[2],t,footprint);const V travel=row(r.direction,f);
        const double shift=std::clamp(-l[0]/std::max(travel[1],.12),-r.distance*.2,r.distance*.2);
        p=add(p,scale(travel,shift));r.hit=add(r.hit,scale(r.direction,shift));
        l=lava(p[0],p[2],t,footprint);dx=l[1];dz=l[2];
    }
    r.normal=unit(col({dx,-1,dz},f));if(dot(r.normal,r.direction)>0) r.normal=scale(r.normal,-1);
    r.bias=std::max(.05,r.distance*1.e-5);r.valid=true;return r;
}
inline V reflect(V d,V n) {return add(d,scale(n,-2*dot(d,n)));}
inline double error(const Frame& f,V target,const Ray& ray,double px,double py) {
    if(!ray.valid) return INFINITY;
    const V to=add(target,scale(add(ray.hit,scale(ray.normal,ray.bias)),-1));
    if(dot(to,to)<=ray.bias*ray.bias) return INFINITY;
    const V incoming=reflect(unit(to),ray.normal);if(incoming[2]<=0) return INFINITY;
    return std::max(std::abs(f.projection[0]*incoming[0]/incoming[2]+f.projection[2]-px),
        std::abs(f.projection[1]*incoming[1]/incoming[2]+f.projection[3]-py));
}
}
