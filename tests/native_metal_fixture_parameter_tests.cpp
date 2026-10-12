#include "native_metal_fixture_parameters.hpp"
#include <bit>
#include <cstdio>
#include <cstring>

namespace {
using native_reflection_fixture::require;
float value(unsigned seed,unsigned field) {
    return std::bit_cast<float>(0x3f000000U+(seed<<9U)+field);
}
template<class T,std::size_t N>
void fill(std::array<T,N>& out,unsigned seed,unsigned first) {
    for(unsigned i=0;i<N;++i) {
        if constexpr(std::is_same_v<T,float>) out[i]=value(seed,first+i);
        else out[i]=seed*256U+first+i;
    }
}
template<std::size_t N>
void floats(std::array<unsigned,N>& out,unsigned at,const std::array<float,4>& input) {
    for(unsigned i=0;i<4;++i) out[at+i]=std::bit_cast<unsigned>(input[i]);
}
template<class T,std::size_t N>
bool exact(const T& actual,const std::array<unsigned,N>& expected) {
    static_assert(sizeof(T)==sizeof(expected));
    std::array<unsigned,N> words{};
    std::memcpy(words.data(),&actual,sizeof(actual));
    return words==expected;
}
}
int main() {
    static_assert(sizeof(float)==4 && sizeof(unsigned)==4);
    unsigned checks=0;
    for(unsigned seed=0;seed<1024;++seed) {
        native_reflection_fixture::Parameters p{};
        fill(p.dimensions,seed,0);fill(p.camera,seed,4);fill(p.settings,seed,8);
        p.settings[2]=float(seed%256U);
        fill(p.environment,seed,10);fill(p.point,seed,12);fill(p.normal,seed,16);
        fill(p.water,seed,20);fill(p.row0,seed,24);fill(p.row1,seed,28);fill(p.row2,seed,32);
        fill(p.enhanced_size,seed,40);fill(p.enhanced_motion,seed,44);
        fill(p.enhanced_plane,seed,48);fill(p.enhanced_projection,seed,52);
        fill(p.enhanced_palette,seed,56);fill(p.keep0,seed,60);fill(p.keep1,seed,64);
        fill(p.primary_range,seed,72);
        std::array<unsigned,104> expected{};
        std::copy(p.dimensions.begin(),p.dimensions.end(),expected.begin());
        floats(expected,4,p.camera);
        expected[8]=std::bit_cast<unsigned>(p.settings[0]);expected[9]=std::bit_cast<unsigned>(p.settings[1]);
        expected[10]=p.environment[0];expected[11]=unsigned(p.settings[2]);
        floats(expected,12,p.point);expected[15]=std::bit_cast<unsigned>(p.settings[3]);
        floats(expected,16,p.normal);floats(expected,20,p.water);
        floats(expected,24,p.row0);floats(expected,28,p.row1);floats(expected,32,p.row2);
        expected[36]=p.environment[1];expected[37]=p.environment[2];expected[38]=p.environment[3];
        std::copy(p.enhanced_size.begin(),p.enhanced_size.end(),expected.begin()+40);
        floats(expected,44,p.enhanced_motion);floats(expected,48,p.enhanced_plane);
        floats(expected,52,p.enhanced_projection);floats(expected,56,p.enhanced_palette);
        floats(expected,60,p.keep0);floats(expected,64,p.keep1);floats(expected,72,p.primary_range);
        const auto reflected=native_metal_fixture::convert(p);
        require(exact(reflected,expected),"Actual reflection conversion differs from all104 independently placed ABI words");
        auto lost_primary=reflected;lost_primary.primary_range={};
        require(!exact(lost_primary,expected),"Dropped primary-range negative control not detected");
        checks+=2;

        native_reflection_fixture::ShadowParameters s{};
        s.extent={float(seed+64U),float(seed+48U),value(seed,2),value(seed,3)};
        fill(s.center,seed,4);fill(s.point,seed,8);fill(s.normal,seed,12);
        for(unsigned i=0;i<16;++i)fill(s.lights[i],seed,16U+i*4U);
        fill(s.coverage,seed,80);fill(s.primary_range,seed,84);
        std::array<unsigned,88> shadow_expected{};
        shadow_expected[0]=unsigned(s.extent[0]);shadow_expected[1]=unsigned(s.extent[1]);
        shadow_expected[2]=std::bit_cast<unsigned>(s.extent[2]);shadow_expected[3]=std::bit_cast<unsigned>(s.extent[3]);
        floats(shadow_expected,4,s.center);floats(shadow_expected,8,s.point);floats(shadow_expected,12,s.normal);
        for(unsigned i=0;i<16;++i)floats(shadow_expected,16U+i*4U,s.lights[i]);
        std::copy(s.coverage.begin(),s.coverage.end(),shadow_expected.begin()+80);
        floats(shadow_expected,84,s.primary_range);
        const auto shadow=native_metal_fixture::convert(s);
        require(exact(shadow,shadow_expected),"Actual shadow conversion differs from all88 independently placed ABI words");
        auto lost_shadow_primary=shadow;lost_shadow_primary.primary_range={};
        require(!exact(lost_shadow_primary,shadow_expected),"Dropped shadow primary-range negative control not detected");
        checks+=2;
    }
    for(unsigned field=0;field<4;++field) {
        native_reflection_fixture::Parameters p{};
        if(field==0)p.material_info[0]=2;
        if(field==1)p.source_colour[0]=.25f;
        if(field==2)p.liquid_layers[0]=1024;
        if(field==3)p.cube_info[0]=64;
        bool rejected=false;
        try{(void)native_metal_fixture::convert(p);}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"Unowned fixture payload accepted or silently dropped");++checks;
    }
    require(exact(native_metal_fixture::convert(native_reflection_fixture::Parameters{}),std::array<unsigned,104>{}),
        "Legacy default reflection fields not zero");
    require(exact(native_metal_fixture::convert(native_reflection_fixture::ShadowParameters{}),std::array<unsigned,88>{}),
        "Legacy default shadow fields not zero");
    checks+=2;
    std::printf("PASS: %u real fixture conversion checks; all416/352 ABI bytes; no GPU/runtime acceptance\n",checks);
}
