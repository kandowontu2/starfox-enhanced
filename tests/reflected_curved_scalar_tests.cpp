// Compile the exact scalar HLSL prefix, with annotations/types adapted only.
// This is arithmetic regression coverage, not shader or game acceptance.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <random>
#include <stdexcept>

using uint=std::uint32_t;
using std::floor;
using std::sqrt;
struct float2 {
    float x,y;
    float2(float a=0):x(a),y(a) {}
    float2(float a,float b):x(a),y(b) {}
};
float2 operator-(float2 a) {return {-a.x,-a.y};}
struct uint4 {uint x,y,z,w;};
float asfloat(uint word) {return std::bit_cast<float>(word);}
struct ByteAddressBuffer {
    std::array<uint,4096> words{};
    ByteAddressBuffer() {
        for(int i=0;i<1024;++i) {
            const double phase=(i<=512?i:i-1024)*(2*std::numbers::pi/1024);
            const double sine=std::sin(phase),cosine=std::cos(phase);
            const float sh=float(sine),ch=float(cosine);
            words[i*4]=std::bit_cast<uint>(sh);
            words[i*4+1]=std::bit_cast<uint>(float(sine-sh));
            words[i*4+2]=std::bit_cast<uint>(ch);
            words[i*4+3]=std::bit_cast<uint>(float(cosine-ch));
        }
    }
    uint4 Load4(uint address) const {
        address/=4;
        return {words.at(address),words.at(address+1),words.at(address+2),words.at(address+3)};
    }
};

#define precise
#define STARFOX_CURVED_NOINLINE
#define STARFOX_CURVED_UNROLL
#define STARFOX_CURVED_TRIG_BINDING
#define STARFOX_CURVED_SCALAR_ONLY
#include "../src/render/shaders/reflection_curved_precision.hlsli"
#undef precise

long double exact(float2 value) {return static_cast<long double>(value.x)+value.y;}
void require(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
struct Errors {long double sine{},cosine{},product{},division{};unsigned phases{};};
void check_phase(float2 a,Errors& errors) {
    const auto phase=exact(a);
    const auto sine=std::abs(exact(curved_dd_sin(a))-std::sin(phase));
    const auto cosine=std::abs(exact(curved_dd_cos(a))-std::cos(phase));
    errors.sine=std::max(errors.sine,sine);errors.cosine=std::max(errors.cosine,cosine);
    ++errors.phases;
    if(sine>=1.e-9L || cosine>=1.e-9L) {
        std::cerr<<std::setprecision(18)<<"phase "<<phase<<", sine/cosine error "<<sine<<'/'<<cosine<<'\n';
    }
    require(sine<1.e-9L,"Full-domain scalar sine precision failure");
    require(cosine<1.e-9L,"Full-domain scalar cosine precision failure");
}
int main() try {
    Errors errors;
    // Include large-phase low-word cancellation, cell wrap/half-cell edges,
    // and both sides of the advertised phase domain. No wider tolerance.
    for(float high:{-1048576.F,-940693.75F,-707536.625F,0.F,707536.625F,940693.75F,1048576.F}) {
        for(float low:{-.03125F,-.0001F,0.F,.0001F,.03125F}) {
            const auto a=curved_dd_add(float2(high,0),float2(low,0));
            if(std::abs(exact(a))<=1048576) check_phase(a,errors);
        }
    }
    for(int cycle:{-166885,-153000,-1024,-1,0,1,1024,153000,166885}) {
        for(int cell=0;cell<1024;cell+=17) {
            for(double offset:{-.5,-1.e-8,0.,1.e-8,.5}) {
                const double phase=(cycle*1024.+cell+offset)*std::numbers::pi/512;
                if(std::abs(phase)>1048576) continue;
                const float high=float(phase);
                check_phase({high,float(phase-high)},errors);
            }
        }
    }
    std::mt19937 random(17351);
    std::uniform_real_distribution<float> world(-1048576,1048576),small(-.0001F,.0001F);
    for(unsigned i=0;i<250000;++i) {
        const auto a=curved_dd_add(float2(world(random),0),float2(small(random),0));
        const auto b=curved_dd_add(float2(world(random),0),float2(small(random),0));
        check_phase(a,errors);
        const auto av=exact(a),bv=exact(b);
        const auto product=std::abs(exact(curved_dd_mul(a,b))-av*bv)/std::max(1.L,std::abs(av*bv));
        errors.product=std::max(errors.product,product);
        require(product<1.e-12L,"Scalar product precision failure");
        if(std::abs(bv)>.01L) {
            const auto division=std::abs(exact(curved_dd_div(a,b))-av/bv)/std::max(1.L,std::abs(av/bv));
            errors.division=std::max(errors.division,division);
            require(division<1.e-12L,"Scalar division precision failure");
        }
    }
    std::cout<<std::setprecision(12)<<errors.phases<<" exact-header phase cases, table="
        <<STARFOX_CURVED_TRIG_TABLE<<"; maximum sine/cosine/product-relative/division-relative "
        <<errors.sine<<'/'<<errors.cosine<<'/'<<errors.product<<'/'<<errors.division<<'\n';
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
