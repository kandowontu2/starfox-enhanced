#pragma once
// Test-only binary64 FORWARD optics over submitted native geometry. This does
// not call the production inverse, shared GPU wave, history consumer or guide.
// Readbacks are optical inputs only; none are uploaded back into the player.
#include "../tools/reflected_liquid_oracle.hpp"
#include <bit>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace reflected_curved_owner_oracle {
using V=reflected_liquid_oracle::V;
using reflected_liquid_oracle::add;
using reflected_liquid_oracle::scale;
using reflected_liquid_oracle::dot;
using reflected_liquid_oracle::unit;
inline V sub(V a,V b) {return add(a,scale(b,-1));}
inline V cross(V a,V b) {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
struct Plane {V a{},b{},c{};};
struct Inputs {
    std::vector<Plane> current,previous;
    std::vector<unsigned> mapping;
    reflected_liquid_oracle::Frame liquid;
    std::array<double,4> current_projection{};
    // Raw column-major located eye views, not the production cube transform
    // or a downloaded inverse guide. Translation never moves an infinite sky.
    std::array<float,16> current_eye_view{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::array<float,16> previous_eye_view=current_eye_view;
    // Actual encoded CURRENT receiver transform, not an optical inverse.
    // The old frame above is still the independently retraced wave surface.
    std::optional<std::array<float,12>> current_liquid_transform;
};
inline void eye_views(Inputs& inputs,const std::array<float,16>& current,const std::array<float,16>& previous) {
    for(const auto& view:{current,previous}) {
        for(float value:view)if(!std::isfinite(value))throw std::runtime_error("Nonfinite independent eye view");
        if(view[3]!=0 || view[7]!=0 || view[11]!=0 || view[15]!=1)
            throw std::runtime_error("Nonaffine independent eye view");
        for(unsigned r=0;r<3;++r)for(unsigned s=0;s<3;++s) {
            double value=0;for(unsigned c=0;c<3;++c)value+=double(view[c*4+r])*view[c*4+s];
            if(std::abs(value-double(r==s))>.0001)throw std::runtime_error("Nonrigid independent eye view");
        }
        const V a{view[0],view[4],view[8]},b{view[1],view[5],view[9]},c{view[2],view[6],view[10]};
        if(dot(a,cross(b,c))<=0)throw std::runtime_error("Reflected independent eye basis");
    }
    inputs.current_eye_view=current;inputs.previous_eye_view=previous;
}
inline V old_environment_direction(const Inputs& inputs,V direction) {
    // DXR (+Y down,+Z forward) -> current RH view -> world -> old RH view
    // -> DXR. Independently multiply the two actual views in binary64.
    constexpr double sign[3]{1,-1,-1};V world{},old{};
    for(unsigned c=0;c<3;++c)for(unsigned r=0;r<3;++r)
        world[c]+=double(inputs.current_eye_view[c*4+r])*sign[r]*direction[r];
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)
        old[r]+=sign[r]*double(inputs.previous_eye_view[c*4+r])*world[c];
    return unit(old);
}
inline bool valid_feature(V feature,unsigned kind) {
    for(double value:feature)if(!std::isfinite(value))return false;
    if(kind==1)return feature[2]==0 && feature[0]>=0 && feature[1]>=0 && feature[0]+feature[1]<=1;
    return (kind==2 || kind==3) && std::abs(dot(feature,feature)-1)<.0001;
}
inline Inputs decode(std::span<const unsigned> words,unsigned previous_offset,unsigned vertices,
    const std::array<float,32>& frame,std::array<double,4> current_projection) {
    Inputs result;result.current_projection=current_projection;
    if(vertices%3 || previous_offset%4
        || words.size()<std::size_t(previous_offset/4)+std::size_t(vertices)*4+vertices/3)
        throw std::runtime_error("Truncated native owner optical geometry");
    const auto vertex=[&](unsigned at) {
        if(words[at+3]!=std::bit_cast<unsigned>(1.F))
            throw std::runtime_error("Owner oracle finite vertex is not W=1");
        return V{std::bit_cast<float>(words[at]),std::bit_cast<float>(words[at+1]),std::bit_cast<float>(words[at+2])};
    };
    for(unsigned i=0;i<vertices/3;++i) {
        const unsigned now=i*12,old=previous_offset/4+now;
        result.current.push_back({vertex(now),vertex(now+4),vertex(now+8)});
        result.previous.push_back({vertex(old),vertex(old+4),vertex(old+8)});
        result.mapping.push_back(words[previous_offset/4+vertices*4+i]);
    }
    auto& f=result.liquid;
    for(unsigned i=0;i<3;++i) {
        f.point[i]=frame[i];f.normal[i]=frame[4+i];f.offset[i]=frame[11+i*4];
        for(unsigned j=0;j<3;++j)f.rotation[i*3+j]=frame[8+i*4+j];
    }
    for(unsigned i=0;i<4;++i)f.projection[i]=frame[20+i];
    f.width=unsigned(frame[24]);f.height=unsigned(frame[25]);
    f.near=frame[26];f.far=frame[27];f.time=frame[28];f.material=unsigned(frame[29]);
    return result;
}
inline std::array<double,2> bary(Plane p,V hit) {
    const auto u=sub(p.b,p.a),v=sub(p.c,p.a),r=sub(hit,p.a);
    const double aa=dot(u,u),ab=dot(u,v),bb=dot(v,v),ra=dot(r,u),rb=dot(r,v),det=aa*bb-ab*ab;
    return {(bb*ra-ab*rb)/det,(aa*rb-ab*ra)/det};
}
inline bool inside(Plane p,V hit) {
    const auto b=bary(p,hit);
    return std::isfinite(b[0]) && std::isfinite(b[1]) && b[0]>=-.00001 && b[1]>=-.00001 && b[0]+b[1]<=1.00001;
}
inline V point(Plane p,std::array<double,2> b) {return add(p.a,add(scale(sub(p.b,p.a),b[0]),scale(sub(p.c,p.a),b[1])));}
struct Path {
    Plane receiver{};
    Plane terminal_plane{};
    std::array<Plane,4> mirrors{};
    V terminal{};
    unsigned hops{},liquid_mask{},lobe{};
    double roughness{};
    bool liquid_primary{},finite_terminal{};
};
struct Ray {V hit{},origin{},outgoing{};double depth{},bias{};bool valid{};};
inline Ray forward(const Inputs& inputs,const Path& p,double x,double y,bool finite_faces=true) {
    const auto& f=inputs.liquid;Ray r{};
    if(p.liquid_primary) {
        const auto ray=reflected_liquid_oracle::optical(f,x,y);if(!ray.valid)return r;
        r.hit=ray.hit;r.depth=ray.depth;r.bias=ray.bias;
        r.origin=add(ray.hit,scale(ray.normal,ray.bias));r.outgoing=reflected_liquid_oracle::reflect(ray.direction,ray.normal);
    } else {
        if(x<0 || y<0 || x>=f.width || y>=f.height)return r;
        const V d=unit({(x-f.projection[2])/f.projection[0],(y-f.projection[3])/f.projection[1],1});
        V n=unit(cross(sub(p.receiver.b,p.receiver.a),sub(p.receiver.c,p.receiver.a)));
        if(dot(n,d)>0)n=scale(n,-1);
        const double den=dot(n,d),distance=dot(n,p.receiver.a)/den;r.depth=distance*d[2];
        if(std::abs(den)<=1.e-12 || distance<=0 || r.depth<f.near || r.depth>f.far)return r;
        r.hit=scale(d,distance);if(finite_faces && !inside(p.receiver,r.hit))return r;
        r.bias=std::max(.01,distance*1.e-5);r.origin=add(r.hit,scale(n,r.bias));
        const auto reflected=reflected_liquid_oracle::reflect(d,n);
        const auto tangent=unit(cross(reflected,std::abs(reflected[1])<.95?V{0,1,0}:V{1,0,0})),bitangent=cross(reflected,tangent);
        constexpr double taps[8][2]{{.5,0},{-.5,0},{0,.5},{0,-.5},{.612,.612},{-.612,.612},{.612,-.612},{-.612,-.612}};
        r.outgoing=unit(add(reflected,scale(add(scale(tangent,taps[p.lobe][0]),scale(bitangent,taps[p.lobe][1])),p.roughness*p.roughness)));
        if(dot(r.outgoing,n)<=0)r.outgoing=reflected;
    }
    for(unsigned h=0;h<p.hops;++h) {
        const bool liquid=(p.liquid_mask&(1U<<h))!=0;const auto plane=p.mirrors[h];
        V n=liquid?f.normal:unit(cross(sub(plane.b,plane.a),sub(plane.c,plane.a)));
        if(dot(n,r.outgoing)>0)n=scale(n,-1);
        const double den=dot(n,r.outgoing),distance=dot(sub(liquid?f.point:plane.a,r.origin),n)/den;
        if(std::abs(den)<(liquid?1.e-8:1.e-12) || distance<=r.bias || distance>=65536)return {};
        r.hit=add(r.origin,scale(r.outgoing,distance));
        if(liquid) {
            auto world=add(reflected_liquid_oracle::row(r.hit,f),f.offset);
            const double footprint=std::sqrt(dot(r.hit,r.hit))/std::max(f.projection[0],1.)/std::max(std::abs(den),.04);
            double dx=.055*std::cos(world[0]*.018+world[2]*.011-f.time*.8)*reflected_liquid_oracle::band(footprint,.022)
                +.025*std::cos(world[0]*.047-world[2]*.025+f.time*1.2)*reflected_liquid_oracle::band(footprint,.054);
            double dz=.045*std::cos(world[2]*.022-world[0]*.009-f.time*.65)*reflected_liquid_oracle::band(footprint,.024)
                -.020*std::cos(world[0]*.047-world[2]*.025+f.time*1.2)*reflected_liquid_oracle::band(footprint,.054);
            if(f.material==3) {
                auto wave=reflected_liquid_oracle::lava(world[0],world[2],f.time,footprint);
                const auto travel=reflected_liquid_oracle::row(r.outgoing,f);
                const double shift=std::clamp(-wave[0]/std::max(travel[1],.12),-distance*.2,distance*.2);
                world=add(world,scale(travel,shift));r.hit=add(r.hit,scale(r.outgoing,shift));
                wave=reflected_liquid_oracle::lava(world[0],world[2],f.time,footprint);dx=wave[1];dz=wave[2];
            }
            n=unit(reflected_liquid_oracle::col({dx,-1,dz},f));if(dot(n,r.outgoing)>0)n=scale(n,-1);
        } else if(finite_faces && !inside(plane,r.hit))return {};
        r.bias=std::max(.05,distance*1.e-5);r.origin=add(r.hit,scale(n,r.bias));
        r.outgoing=reflected_liquid_oracle::reflect(r.outgoing,n);
    }
    r.valid=true;return r;
}
inline std::optional<std::array<double,2>> residual(const Inputs& i,const Path& p,double x,double y) {
    const auto r=forward(i,p,x,y,false);if(!r.valid)return {};
    auto travel=p.finite_terminal?sub(p.terminal,r.origin):p.terminal;
    if(dot(travel,travel)<=(p.finite_terminal?r.bias*r.bias:1.e-20))return {};
    travel=unit(travel);if(dot(travel,r.outgoing)<=0)return {};
    const auto t=unit(cross(r.outgoing,std::abs(r.outgoing[1])<.95?V{0,1,0}:V{1,0,0}));
    const double focal=std::max(i.liquid.projection[0],i.liquid.projection[1]);
    return std::array<double,2>{dot(travel,t)*focal,dot(travel,cross(r.outgoing,t))*focal};
}
struct Solution {double x{},y{},depth{},error{};bool valid{},bounded_seed{},ambiguous{};};
struct ForwardRoot {double x{},y{},depth{},error{};bool valid{};};
inline std::optional<std::array<double,2>> liquid_receiver_initializer(
    const Inputs& i,unsigned x,unsigned y,double current_depth) {
    if(!i.current_liquid_transform || !std::isfinite(current_depth) || current_depth<=0)return {};
    const auto& current=*i.current_liquid_transform;const auto& projection=i.current_projection;
    for(double value:projection)if(!std::isfinite(value))return {};
    for(float value:current)if(!std::isfinite(value))return {};
    if(projection[0]<=0 || projection[1]<=0 || i.liquid.projection[0]<=0 || i.liquid.projection[1]<=0)return {};
    const V hit{(x+.5-projection[2])*current_depth/projection[0],
        (y+.5-projection[3])*current_depth/projection[1],current_depth};
    // Independently transport a literal current primary point to world space,
    // then solve the old affine basis with pivoted binary64 elimination. This
    // is an INITIALIZER only: forward convergence/old visibility still decide
    // eligibility, and no GPU inverse or output colour selects a branch.
    double matrix[3][4]{};
    for(unsigned r=0;r<3;++r) {
        matrix[r][3]=current[r*4+3]-i.liquid.offset[r];
        for(unsigned c=0;c<3;++c) {
            matrix[r][3]+=hit[c]*current[c*4+r];
            matrix[r][c]=i.liquid.rotation[c*3+r];
        }
        for(double value:matrix[r])if(!std::isfinite(value))return {};
    }
    for(unsigned column=0;column<3;++column) {
        unsigned pivot=column;
        for(unsigned row=column+1;row<3;++row)
            if(std::abs(matrix[row][column])>std::abs(matrix[pivot][column]))pivot=row;
        if(std::abs(matrix[pivot][column])<=1.e-20)return {};
        for(unsigned c=0;c<4;++c)std::swap(matrix[column][c],matrix[pivot][c]);
        const double divisor=matrix[column][column];
        for(unsigned c=column;c<4;++c)matrix[column][c]/=divisor;
        for(unsigned row=0;row<3;++row)if(row!=column) {
            const double factor=matrix[row][column];
            for(unsigned c=column;c<4;++c)matrix[row][c]-=factor*matrix[column][c];
        }
    }
    const V old{matrix[0][3],matrix[1][3],matrix[2][3]};
    if(!std::isfinite(old[2]) || old[2]<=0)return {};
    std::array<double,2> seed{i.liquid.projection[0]*old[0]/old[2]+i.liquid.projection[2],
        i.liquid.projection[1]*old[1]/old[2]+i.liquid.projection[3]};
    for(double value:seed)if(!std::isfinite(value))return {};
    return seed;
}
inline ForwardRoot solve_forward(const Inputs& i,const Path& p,double sx,double sy) {
    // Binary64 central differences and monotone backtracking over forward
    // optics. No production inverse, output colour or guide is an input.
    constexpr double step=1.e-4;
    for(unsigned iteration=0;iteration<48;++iteration) {
        const auto e=residual(i,p,sx,sy);if(!e)return {};
        const double best=std::hypot((*e)[0],(*e)[1]);if(best<1.e-9)break;
        const auto ax=residual(i,p,sx-step,sy),bx=residual(i,p,sx+step,sy),ay=residual(i,p,sx,sy-step),by=residual(i,p,sx,sy+step);
        if(!ax || !bx || !ay || !by)return {};
        const double a=((*bx)[0]-(*ax)[0])/(2*step),b=((*by)[0]-(*ay)[0])/(2*step),
            c=((*bx)[1]-(*ax)[1])/(2*step),d=((*by)[1]-(*ay)[1])/(2*step),det=a*d-b*c;
        if(!std::isfinite(det) || std::abs(det)<1.e-20)return {};
        double dx=(d*(*e)[0]-b*(*e)[1])/det,dy=(a*(*e)[1]-c*(*e)[0])/det;
        const double radius=std::hypot(dx,dy);if(radius>8) {dx*=8/radius;dy*=8/radius;}
        bool advanced=false;
        for(unsigned trial=0;trial<14;++trial) {
            const double fraction=std::ldexp(1.,-int(trial));const auto next=residual(i,p,sx-dx*fraction,sy-dy*fraction);
            if(next && std::hypot((*next)[0],(*next)[1])<best) {sx-=dx*fraction;sy-=dy*fraction;advanced=true;break;}
        }
        if(!advanced)return {};
    }
    const auto ray=forward(i,p,sx,sy);const auto error=residual(i,p,sx,sy);
    if(!ray.valid || !error || std::hypot((*error)[0],(*error)[1])>=1.e-8)return {};
    return {sx,sy,ray.depth,std::hypot((*error)[0],(*error)[1]),true};
}
inline std::optional<std::array<double,2>> nominal_receiver_initializer(const Inputs& i,const Path& p) {
    // Independent plane-unfolding INITIALIZER. The old waves/finite faces are
    // still fully retraced by solve_forward and source_guard; nominal planes
    // never replace those optics or certify a colour footprint themselves.
    V virtual_feature=p.terminal;
    const auto unfold=[&](V position,V normal) {
        normal=unit(normal);
        virtual_feature=sub(virtual_feature,scale(normal,2*dot(p.finite_terminal?sub(virtual_feature,position):virtual_feature,normal)));
    };
    for(unsigned reverse=p.hops;reverse>0;--reverse) {
        const unsigned hop=reverse-1;
        if(p.liquid_mask&(1U<<hop))unfold(i.liquid.point,i.liquid.normal);
        else {const auto plane=p.mirrors[hop];unfold(plane.a,cross(sub(plane.b,plane.a),sub(plane.c,plane.a)));}
    }
    if(p.liquid_primary)unfold(i.liquid.point,i.liquid.normal);
    else unfold(p.receiver.a,cross(sub(p.receiver.b,p.receiver.a),sub(p.receiver.c,p.receiver.a)));
    for(double value:virtual_feature)if(!std::isfinite(value))return {};
    if(virtual_feature[2]<=0 || i.liquid.width==0 || i.liquid.height==0)return {};
    std::array<double,2> seed{i.liquid.projection[0]*virtual_feature[0]/virtual_feature[2]+i.liquid.projection[2],
        i.liquid.projection[1]*virtual_feature[1]/virtual_feature[2]+i.liquid.projection[3]};
    for(unsigned c=0;c<2;++c) {
        if(!std::isfinite(seed[c]))return {};
        seed[c]=std::clamp(seed[c],0.,double(c?i.liquid.height:i.liquid.width)-.001);
    }
    return seed;
}
template<class Visibility>
inline Solution previous_source(const Inputs& i,unsigned primary,std::span<const unsigned,16> record,
    double roughness,unsigned lobe,unsigned x,unsigned y,Visibility visible,double current_depth=0,
    std::span<const std::array<double,2>> old_visibility_starts={}) {
    if(old_visibility_starts.size()>65536)throw std::runtime_error("Unbounded independent visibility initializers");
    Path p;p.hops=record[4]&7U;p.liquid_mask=(record[4]>>4)&15U;p.lobe=lobe;p.roughness=roughness;
    const unsigned kind=record[4]>>8;
    if(p.hops>4 || kind<1 || kind>3 || lobe>=8)return {};
    p.liquid_primary=primary==0xfffffffdU;p.finite_terminal=kind==1;
    const auto old_plane=[&](unsigned id)->std::optional<Plane> {
        if(id>=i.mapping.size() || i.mapping[id]>=i.previous.size())return {};
        return i.previous[i.mapping[id]];
    };
    if(!p.liquid_primary) {const auto plane=old_plane(primary);if(!plane)return {};p.receiver=*plane;}
    for(unsigned h=0;h<p.hops;++h)if(!(p.liquid_mask&(1U<<h))) {
        const auto plane=old_plane(record[h]);if(!plane)return {};p.mirrors[h]=*plane;
    }
    V feature{std::bit_cast<float>(record[6]),std::bit_cast<float>(record[7]),std::bit_cast<float>(record[8])};
    if(!valid_feature(feature,kind))return {};
    if(p.finite_terminal) {
        const auto plane=old_plane(record[5]);if(!plane)return {};p.terminal_plane=*plane;p.terminal=point(*plane,{feature[0],feature[1]});
        // The native solver's terminal parameter is a float4 ABI. Decode the
        // barycentric endpoint in binary64, then encode that parameter once.
        for(auto& component:p.terminal)component=float(component);
    }
    else p.terminal=old_environment_direction(i,feature);
    double sx=x+.5,sy=y+.5;
    if(p.liquid_primary && i.current_liquid_transform) {
        const auto seed=liquid_receiver_initializer(i,x,y,current_depth);if(!seed)return {};
        // The initializer crosses a float2 ABI, independently encoded here.
        sx=float((*seed)[0]);sy=float((*seed)[1]);
    }
    if(!p.liquid_primary) {
        if(primary>=i.current.size())return {};
        const auto& projection=i.current_projection;const auto plane=i.current[primary];
        const V d=unit({(sx-projection[2])/projection[0],(sy-projection[3])/projection[1],1});
        const V n=unit(cross(sub(plane.b,plane.a),sub(plane.c,plane.a)));
        const auto prior=point(p.receiver,bary(plane,scale(d,dot(plane.a,n)/dot(d,n))));
        if(prior[2]<=0)return {};
        sx=i.liquid.projection[0]*prior[0]/prior[2]+i.liquid.projection[2];
        sy=i.liquid.projection[1]*prior[1]/prior[2]+i.liquid.projection[3];
    }
    const double seed_x=sx,seed_y=sy;
    const auto qualify=[&](const ForwardRoot& root)->Solution {
        if(!root.valid)return {};
        // The inverse returns a float4, and bilinear colour lookup consumes that
        // encoded coordinate. Preserve this real boundary (including edge roots
        // rounding to .5); do not invent an epsilon, integer snap or GPU reference.
        Solution result{double(float(root.x))-.5,double(float(root.y))-.5,double(float(root.depth)),root.error,true,false};
        if(visible(result))return result;
        // The actual colour solver also accepts a <=1/4096-pixel Newton step
        // with <=.002 angular residual. An exact binary64 root can straddle a
        // different tap at a float boundary. Independently qualify the authored
        // receiver initializer under BOTH existing bounds, with finite forward
        // geometry and every nonzero old tap validated by the caller. Never choose
        // a source from the GPU inverse or by whether its colour matches output.
        const double encoded_x=float(seed_x),encoded_y=float(seed_y);
        const auto bounded=[&](double cx,double cy)->Solution {
            if(std::abs(cx-root.x)>1./4096 || std::abs(cy-root.y)>1./4096)return {};
            const auto ray=forward(i,p,cx,cy);const auto e=residual(i,p,cx,cy);
            if(!ray.valid || !e || std::max(std::abs((*e)[0]),std::abs((*e)[1]))>.002)return {};
            constexpr double h=1.e-4;
            const auto ax=residual(i,p,cx-h,cy),bx=residual(i,p,cx+h,cy),
                ay=residual(i,p,cx,cy-h),by=residual(i,p,cx,cy+h);
            if(!ax || !bx || !ay || !by)return {};
            const double a=((*bx)[0]-(*ax)[0])/(2*h),b=((*by)[0]-(*ay)[0])/(2*h),
                c=((*bx)[1]-(*ax)[1])/(2*h),d=((*by)[1]-(*ay)[1])/(2*h),det=a*d-b*c;
            if(!std::isfinite(det) || std::abs(det)<1.e-20)return {};
            const double dx=(d*(*e)[0]-b*(*e)[1])/det,dy=(a*(*e)[1]-c*(*e)[0])/det;
            if(!std::isfinite(dx) || !std::isfinite(dy) || std::abs(dx)>1./4096 || std::abs(dy)>1./4096)return {};
            Solution candidate{cx-.5,cy-.5,double(float(ray.depth)),std::hypot((*e)[0],(*e)[1]),true,true};
            return visible(candidate)?candidate:Solution{};
        };
        result=bounded(encoded_x,encoded_y);if(result.valid)return result;
        // The native exit returns float4 coordinates, not an exact real root.
        // Probe symmetric representable ABI neighbours of the authored seed
        // AND independently converged root: the native small-step exit can
        // occur after Newton iterations, not only at its initializer.
        // Every candidate must satisfy the existing residual AND independently
        // computed small-Newton-step bounds plus complete old optical guards.
        // No hidden tap is dropped, and RGB cannot choose a candidate.
        constexpr int directions[8][2]{{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,-1},{-1,1},{1,1}};
        const double centres[2][2]{{encoded_x,encoded_y},{double(float(root.x)),double(float(root.y))}};
        // Include every representable offset in this bounded range: an exit
        // can be 17 ULPs from the encoded root, not just a power of two.
        for(const auto& centre:centres)for(unsigned ulps=1;ulps<=64;++ulps)for(const auto& direction:directions) {
            float cx=float(centre[0]),cy=float(centre[1]);
            for(unsigned n=0;n<ulps;++n) {
                if(direction[0])cx=std::nextafter(cx,direction[0]>0?INFINITY:-INFINITY);
                if(direction[1])cy=std::nextafter(cy,direction[1]>0?INFINITY:-INFINITY);
            }
            result=bounded(cx,cy);if(result.valid)return result;
        }
        return {};
    };
    const auto local=solve_forward(i,p,sx,sy);
    if(local.valid) {
        const auto qualified=qualify(local);if(qualified.valid)return qualified;
    }
    // A monotone local solve can stall at a nonzero minimum of a wavy optical
    // field OR converge to an old footprint which is not visible. Probe bounded
    // symmetric starts around the authored receiver, not
    // around a downloaded GPU inverse. Require strict forward convergence,
    // finite geometry and full old visibility for each surviving branch.
    // Ambiguous qualified roots are NOT resolved using output RGB.
    Solution recovered{};
    constexpr int starts[8][2]{{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}};
    const auto nominal=nominal_receiver_initializer(i,p);
    const std::array<std::array<double,2>,2> centres{{{seed_x,seed_y},nominal.value_or(std::array<double,2>{seed_x,seed_y})}};
    for(unsigned centre=0;centre<(nominal?2U:1U);++centre)
        for(unsigned radius=centre?0:1;radius<=3;++radius)for(unsigned start_index=0;start_index<(radius?8U:1U);++start_index) {
            const auto& start=starts[start_index];
            const auto candidate=qualify(solve_forward(i,p,centres[centre][0]+double(radius)*start[0],centres[centre][1]+double(radius)*start[1]));
            if(!candidate.valid)continue;
            if(recovered.valid && (std::abs(recovered.x-candidate.x)>1./4096
                || std::abs(recovered.y-candidate.y)>1./4096))return {0,0,0,0,false,false,true};
            recovered=candidate;
    }
    if(recovered.valid || old_visibility_starts.empty())return recovered;
    // Exhaust a bounded set of independently authored old visible-pixel
    // initializers only when the local/recovery solves found NO qualified
    // branch. These are raw old coverage coordinates, never the GPU inverse.
    // Multiple qualified roots remain ambiguous, regardless of their RGB.
    // The finite initializer set is not a proof of global uniqueness.
    for(const auto& start:old_visibility_starts) {
        const auto candidate=qualify(solve_forward(i,p,start[0],start[1]));
        if(!candidate.valid)continue;
        if(recovered.valid && (std::abs(recovered.x-candidate.x)>1./4096
            || std::abs(recovered.y-candidate.y)>1./4096))return {0,0,0,0,false,false,true};
        recovered=candidate;
    }
    return recovered;
}
}
