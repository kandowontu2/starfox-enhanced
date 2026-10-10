#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include "native_rgba_shadow_fixture.hpp"

namespace coverage_under_test {
using uint=std::uint32_t;
using std::abs;using std::floor;using std::isfinite;
template<class T> T as_type(uint value) {return std::bit_cast<T>(value);}
#define device
#include "starfox/render/metal_native_material_coverage.inc"
#undef device
}
namespace {
using namespace native_reflection_fixture;
using coverage_under_test::starfox_native_material_coverage;
void check(bool ok,const char* reason) {if(!ok)throw std::runtime_error(reason);}
unsigned oracle(const NativeOpacity& m,double u,double v,unsigned x,unsigned y) {
    if(!m.valid)return 0;
    if(!m.textured) {
        unsigned divisor=std::max(1u,m.scale);
        return m.solid_alpha[m.scale && (((x/divisor)^(y/divisor))&1u)]==255?255u:0u;
    }
    double tx=double(m.uv[0])*(1-u-v)+double(m.uv[2])*u+double(m.uv[4])*v;
    double ty=double(m.uv[1])*(1-u-v)+double(m.uv[3])*u+double(m.uv[5])*v;
    if(m.clamp && (tx<0 || ty<0 || tx>=2 || ty>=2))return 0;
    unsigned i=(unsigned(int(std::floor(ty)))&1u)*2u+(unsigned(int(std::floor(tx)))&1u);
    return m.texel_alpha[i]==255?255u:0u;
}
}
int main() {
    try {
        std::uint64_t comparisons=0,opaque=0,holes=0;
        for(unsigned variant=0;variant<native_shadow_variant_count;++variant) {
            const auto fixture=native_shadow_source(variant);
            const auto count=unsigned(fixture.opacity.size()),binding=unsigned(fixture.words.size());
            const auto bytes=binding*4u;
            // Interior binary-exact barycentrics avoid ambiguous UV boundaries.
            // This exercises every original24 variant, not fewer ray fixtures.
            for(unsigned a=0;a<16;++a)for(unsigned b=0;b<16-a;++b) {
                const float u=float(2*a+1)/32.0f,v=float(2*b+1)/32.0f;
                for(unsigned primitive=0;primitive<count;++primitive)
                    for(unsigned y=0;y<17;++y)for(unsigned x=0;x<19;++x) {
                        unsigned actual=starfox_native_material_coverage(primitive,u,v,x,y,
                            fixture.words.data(),count,bytes,binding);
                        unsigned expected=oracle(fixture.opacity[primitive],u,v,x,y);
                        check((actual>>24u)==expected,"Independent24-variant opacity mismatch");
                        ++comparisons;if(expected)++opaque;else ++holes;
                    }
            }
            check(!starfox_native_material_coverage(count,.25f,.25f,0,0,fixture.words.data(),count,bytes,binding),"Out-of-range primitive accepted");
            check(!starfox_native_material_coverage(0,.25f,.25f,0,0,fixture.words.data(),count,bytes,binding-1u),"Short bound buffer accepted");
            check(!starfox_native_material_coverage(0,.25f,.25f,0,0,fixture.words.data(),count,bytes-1u,binding),"Misaligned payload accepted");
            check(!starfox_native_material_coverage(0,.25f,.25f,0,0,fixture.words.data(),0x04000000u,bytes,binding),"Overflowing material count accepted");
            check(!starfox_native_material_coverage(0,std::numeric_limits<float>::quiet_NaN(),.25f,0,0,
                fixture.words.data(),count,bytes,binding),"Nonfinite barycentric accepted");
        }
        auto solid=native_shadow_source(0);
        check(starfox_native_material_coverage(0,.25f,.25f,0,0,solid.words.data(),4,
            unsigned(solid.words.size()*4u),unsigned(solid.words.size()))==0xff000000u,"Opaque native black became a hole");
        auto indexed=native_shadow_source(4);
        bool native_zero=false;
        for(unsigned a=0;a<16;++a)for(unsigned b=0;b<16-a;++b) {
            unsigned value=starfox_native_material_coverage(0,float(2*a+1)/32.0f,float(2*b+1)/32.0f,0,0,
                indexed.words.data(),4,unsigned(indexed.words.size()*4u),unsigned(indexed.words.size()));
            native_zero|=value==0xff000000u;
        }
        check(native_zero,"Native palette index zero never remained opaque");
        check(comparisons==24u*136u*4u*17u*19u && opaque && holes,"Complete opacity comparison coverage missing");
        std::cout<<"PASS Metal-compatible exact decoder CPU opacity comparisons="<<comparisons
            <<" original-variants="<<native_shadow_variant_count<<" opaque="<<opaque<<" holes="<<holes
            <<"; NO Metal GPU/producer/reflection-colour/performance acceptance\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
