#pragma once
#include "native_reflection_fixture.hpp"
// Binary64 ray/UV reference over immutable fixture inputs. No GPU hit, barycentric
// or colour readback feeds this reference; only the finished image is compared.
namespace native_reflection_fixture {
struct IndexedHit {
    double t{65536};unsigned primitive{},colour{},rejected{};bool found{},edge{};
};
inline IndexedHit trace_indexed(const Fixture& f,std::span<const unsigned char> texels,
    V origin,V direction,double minimum,unsigned x,unsigned y,double maximum=65536) {
    IndexedHit result;result.t=maximum;
    for(unsigned i=0;i<f.vertices.size();i+=3) {
        const auto at=[&](unsigned j){const auto& a=f.vertices[j];return V{a[0],a[1],a[2]};};
        const V a=at(i),e1=sub(at(i+1),a),e2=sub(at(i+2),a),q=cross(direction,e2);
        const double determinant=dot(e1,q);if(std::abs(determinant)<1e-10)continue;
        const V s=sub(origin,a),r=cross(s,e1);
        const double u=dot(s,q)/determinant,v=dot(direction,r)/determinant,t=dot(e2,r)/determinant;
        if(u<0 || v<0 || u+v>1 || t<=minimum || t>=maximum)continue;
        const auto& material=f.materials[i/3];
        unsigned index=material.dither && ((x+y)&1)?material.odd:material.even;
        bool boundary=std::min({u,v,1-u-v})<1e-4;
        if(material.textured) {
            const double tx=material.uv[0]*(1-u-v)+material.uv[2]*u+material.uv[4]*v;
            const double ty=material.uv[1]*(1-u-v)+material.uv[3]*u+material.uv[5]*v;
            boundary|=std::min({tx-std::floor(tx),std::ceil(tx)-tx,ty-std::floor(ty),std::ceil(ty)-ty})<1e-4;
            const unsigned xx=unsigned(int(std::floor(tx)))&material.umask;
            const unsigned yy=unsigned(int(std::floor(ty)))&material.vmask;
            const unsigned texel=texels[material.offset+yy*(material.umask+1)+xx];
            // Source index zero is a hole BEFORE colour-base addition. An
            // opaque texel can legitimately wrap to palette index zero.
            if(!texel){++result.rejected;result.edge|=boundary;continue;}
            index=(texel+material.base)&255;
        }
        if(t<result.t) {
            result.t=t;result.primitive=i/3;result.colour=f.palette[index]|0xff000000;
            result.found=true;result.edge|=boundary;
        }
    }
    return result;
}
// A transparent sheet in front of an opaque one, including exact coincidence
// and gaps smaller than the former restart epsilon. No shader-derived hits:
// coverage expectations use the independent binary64 ray/UV reference above.
template<class Dispatch>
void run_near_cutouts(Dispatch dispatch) {
    unsigned checked=0,opaque=0,clear=0,edges=0,rejected=0;
    const float spacing=std::nextafter(32.f,64.f)-32.f;
    for(float gap:{0.f,spacing,4*spacing})for(bool back_opaque:{false,true}) {
        Fixture f{};
        f.vertices={{{-30,-20,32,0},{30,-20,32,0},{30,20,32,0},{-30,-20,32,0},{30,20,32,0},{-30,20,32,0},
            {-30,-20,32+gap,0},{30,-20,32+gap,0},{30,20,32+gap,0},{-30,-20,32+gap,0},{30,20,32+gap,0},{-30,20,32+gap,0}}};
        for(unsigned i=0;i<4;++i) {
            f.materials[i].textured=1;f.materials[i].offset=i<2?0:1;f.materials[i].base=254;
            f.materials[i].uv={.37f,.29f,.37f,.29f,.37f,.29f};
        }
        std::array<unsigned char,16> ink{};ink[1]=back_opaque?2:0;f.palette[0]=0xff215fbd;
        Parameters p{};p.dimensions={width,height,1,0};p.camera={96,85,32.3f,24.1f};p.settings[2]=float(ink.size());
        p.environment[0]=0xff39c271;
        const auto actual=dispatch(p,f,ink);require(actual.size()==width*height,"Near cutout output extent");
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const V ray{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1};
            const auto expected=trace_indexed(f,ink,{},ray,1,x,y);
            if(expected.edge){++edges;continue;}
            const unsigned word=expected.found?p.environment[0]:0;
            if(actual[y*width+x]!=word)throw std::runtime_error("Near/coincident cutout discarded opaque candidate gap="
                +std::to_string(gap)+" back opaque="+std::to_string(back_opaque)+" pixel="+std::to_string(y*width+x));
            ++checked;opaque+=expected.found;clear+=!expected.found;rejected+=expected.rejected;
        }
    }
    require(checked>18000 && opaque>9000 && clear>9000 && rejected>18000,"Insufficient near/coincident cutout coverage");
    std::cout<<"Native near/coincident indexed cutouts: exact pixels="<<checked<<" opaque="<<opaque<<" clear="<<clear
        <<" rejected="<<rejected<<" boundary exclusions="<<edges<<"; input-only binary64 ray/UV reference\n";
}

template<class Dispatch>
void run_indexed_cutouts(Fixture fixture,Dispatch dispatch) {
    unsigned samples=0,clear=0,rejections=0,secondary=0,model_secondary=0,multiple=0,edges=0,max_error=0;
    std::array<unsigned,5> variants{};
    const auto normal_at=[&](unsigned primitive,V ray) {
        const unsigned i=primitive*3;
        const auto at=[&](unsigned j){const auto& a=fixture.vertices[j];return V{a[0],a[1],a[2]};};
        V n=unit(cross(sub(at(i+1),at(i)),sub(at(i+2),at(i))));return dot(n,ray)>0?scale(n,-1):n;
    };
    fixture.palette[0]=0xff5fbd21;fixture.palette[1]=0xffad3971;fixture.palette[2]=0xff317ce7;
    fixture.palette[254]=0xffcc6611;fixture.palette[255]=0xff349ca1;
    for(unsigned variant=0;variant<5;++variant)for(unsigned mirrored=0;mirrored<2;++mirrored)for(unsigned eye=0;eye<2;++eye)for(unsigned ground=0;ground<2;++ground) {
        std::array<unsigned char,16> texels{};
        for(unsigned i=0;i<texels.size();++i)texels[i]=variant==0?0:variant==3?2:((i+variant)&1)?2:0;
        for(unsigned i=0;i<4;++i) {
            auto& m=fixture.materials[i];m={};m.textured=variant!=4;m.dither=variant==4;
            m.even=0;m.odd=2;m.base=i&1?254:0;m.offset=i*4;m.umask=m.vmask=1;
            m.uv={-3.37f,-2.19f,12.13f,-2.19f,12.13f,9.41f};
            if(i&1)m.uv={-3.37f,-2.19f,12.13f,9.41f,-3.37f,9.41f};
        }
        Parameters p{};p.dimensions={width,height,1,0};p.camera={eye?40.f:96.f,eye?32.f:85.f,eye?33.4f:30.6f,22.7f};
        p.settings={0,float(ground),float(texels.size()),float(ground)};p.environment[0]=0xff29b50d;
        p.point={0,20,0,0};p.normal={0,-1,0,0};p.water={0,1,1,float((mirrored?16:0)|ground)};
        const auto actual=dispatch(p,fixture,texels);require(actual.size()==width*height,"Cutout output extent");
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
            const unsigned pixel=y*width+x;
            const V raw{(x+.5-p.camera[2])/p.camera[0],(y+.5-p.camera[3])/p.camera[1],1};
            const auto primary=ground?IndexedHit{}:trace_indexed(fixture,texels,{},raw,1,x,y);
            if(primary.edge){++edges;continue;}rejections+=primary.rejected;
            const double floor_t=raw[1]>0?20/raw[1]:65536;
            if((!ground && !primary.found) || (ground && (floor_t<=1 || floor_t>=65536))) {
                require(actual[pixel]==0,"Transparent primary texel still hides the scene");++clear;++variants[variant];continue;
            }
            const double primary_t=ground?floor_t:primary.t;
            V origin=scale(raw,primary_t),n=ground?V{0,-1,0}:normal_at(primary.primitive,raw);
            V direction=reflected_liquid_oracle::reflect(unit(raw),n);
            double lower=ground?std::max(.05,primary_t*std::sqrt(dot(raw,raw))*1e-5):std::max(.1,primary_t*1e-5);
            if(ground)origin=add(origin,scale(n,lower));
            unsigned expected=p.environment[0],hops=0;bool boundary=false;
            for(unsigned bounce=0;bounce<(mirrored || ground?4u:1u);++bounce) {
                const double to_floor=ground && direction[1]>1e-6?(20-origin[1])/direction[1]:65536;
                const double next_floor=to_floor>lower && to_floor<65536?to_floor:65536;
                const auto hit=trace_indexed(fixture,texels,origin,direction,lower,x,y,next_floor);
                if(hit.edge){boundary=true;break;}rejections+=hit.rejected;
                if(!hit.found && next_floor==65536)break;
                const double distance=hit.found?hit.t:next_floor;++hops;
                if(hit.found && !mirrored){expected=hit.colour;break;}
                n=hit.found?normal_at(hit.primitive,direction):V{0,-1,0};
                if(dot(n,direction)>0)n=scale(n,-1);
                lower=std::max(.05,distance*1e-5);
                origin=add(add(origin,scale(direction,distance)),scale(n,lower));direction=reflected_liquid_oracle::reflect(direction,n);
            }
            if(boundary){++edges;continue;}
            require((actual[pixel]>>24)==(ground?254u:255u),"Opaque source ink lost its model/ground coverage marker");
            for(unsigned c=0;c<3;++c) {
                const int a=(actual[pixel]>>(8*c))&255,e=(expected>>(8*c))&255;
                max_error=std::max(max_error,unsigned(std::abs(a-e)));
                if(std::abs(a-e)>2)throw std::runtime_error("Indexed cutout colour mismatch variant="+std::to_string(variant)
                    +" mirrored="+std::to_string(mirrored)+" eye="+std::to_string(eye)+" pixel="+std::to_string(pixel)
                    +" actual/expected="+std::to_string(a)+"/"+std::to_string(e));
            }
            ++samples;++variants[variant];secondary+=hops;model_secondary+=!ground?hops:0;multiple+=hops>1;
        }
    }
    if(!(samples>10000 && clear>10000 && rejections>10000 && secondary>100 && model_secondary>0 && multiple>0))
        throw std::runtime_error("Insufficient cutout primary/secondary/recursive coverage opaque="+std::to_string(samples)
            +" clear="+std::to_string(clear)+" rejections="+std::to_string(rejections)+" secondary="+std::to_string(secondary)
            +" model secondary="+std::to_string(model_secondary)+" multiple="+std::to_string(multiple));
    for(auto n:variants)require(n>1000,"Missing indexed cutout variant");
    std::cout<<"Native indexed cutouts: opaque="<<samples<<" clear="<<clear<<" rejected source texels="<<rejections
        <<" secondary hops="<<secondary<<" direct model secondary="<<model_secondary<<" recursive paths="<<multiple<<" source boundary exclusions="<<edges
        <<" max RGB byte error="<<max_error<<"; binary64 input-only ray/UV reference, NOT owner/presentation acceptance\n";
}
} // namespace native_reflection_fixture
