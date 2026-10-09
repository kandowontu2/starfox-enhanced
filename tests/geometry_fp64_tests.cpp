#include "../src/render/shaders/geometry_fp64.hlsli"
#include "../src/render/shaders/geometry_fp64_div_radix16.hlsli"
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
Sf64 split(double value) {
    const auto bits=std::bit_cast<std::uint64_t>(value);
    return sf_make(std::uint32_t(bits),std::uint32_t(bits>>32));
}
std::uint64_t bits(Sf64 value) {return std::uint64_t(value.hi)<<32|value.lo;}
int main() try {
    // MBLACKFACE's edge-on polygon after near clipping must remain negative.
    const double nearCamera[5][3]{
        {-17.677669529663689,-17.677669529663689,562},
        {3535.5339059327371,3535.533905932738,5512},
        {3535.5339059327375,3535.533905932738,0},
        {313.95541084682736,313.95541084682736,0},
        {-17.677669529663682,-17.677669529663689,462}};
    Sf64 projected[5][2];double hostProjected[5][2];
    for(unsigned i=0;i<5;++i)for(unsigned c=0;c<2;++c) {
        const double depth=nearCamera[i][2]==0?1.:nearCamera[i][2];
        volatile double product=nearCamera[i][c]*256.;
        volatile double quotient=product/depth;
        hostProjected[i][c]=(c==0?112.:96.)+quotient;
        projected[i][c]=sf_add(split(c==0?112.:96.),sf_div(sf_mul(split(nearCamera[i][c]),split(256.)),split(depth)));
        if(bits(projected[i][c])!=std::bit_cast<std::uint64_t>(hostProjected[i][c]))
            throw std::runtime_error("MBLACKFACE near projection mismatch");
    }
    Sf64 area=split(0.);double hostArea=0.;
    for(unsigned i=0;i<5;++i) {
        const unsigned j=(i+1)%5;
        volatile double a=hostProjected[i][0]*hostProjected[j][1];
        volatile double b=hostProjected[j][0]*hostProjected[i][1];
        hostArea+=a-b;
        area=sf_add(area,sf_sub(sf_mul(projected[i][0],projected[j][1]),sf_mul(projected[j][0],projected[i][1])));
    }
    if(!(hostArea<0.) || bits(area)!=std::bit_cast<std::uint64_t>(hostArea))
        throw std::runtime_error("MBLACKFACE near polygon area sign/bits mismatch");
    // SHIP_0_C's clipped edge lands exactly on a 2x rounding boundary.
    const auto shipAmount=sf_div(sf_sub(split(224.),split(146.5)),sf_sub(split(318.80769230769232),split(146.5)));
    const auto shipY=sf_add(split(46.75),sf_mul(sf_sub(split(81.211538461538467),split(46.75)),shipAmount));
    if(bits(shipY)!=std::bit_cast<std::uint64_t>(62.25))
        throw std::runtime_error("SHIP_0_C viewport quarter-pixel boundary mismatch");
    std::uint32_t state=0x971283U;
    const auto random=[&]() {state=state*1664525U+1013904223U;return state;};
    std::uint64_t checked=0;
    std::uint64_t divisions=0;
    std::uint64_t products=0;
    std::uint64_t lossy_pairs=0,transport_samples=0;
    const auto check_u32_product=[](std::uint32_t a,std::uint32_t b) {
        if(bits(sf_u32_product(a,b))!=std::uint64_t(a)*b)
            throw std::runtime_error("Portable full-width uint32 product/carry mismatch");
    };
    for(std::uint32_t a:{0U,1U,0xffffU,0x10000U,0x7fffffffU,0x80000000U,0xffff0000U,0xffffffffU})
        for(std::uint32_t b:{0U,1U,0xffffU,0x10000U,0x7fffffffU,0x80000000U,0xffff0000U,0xffffffffU})
            check_u32_product(a,b);
    for(unsigned i=0;i<1000000;++i) {
        const auto a=random(),b=random();check_u32_product(a,b);
    }
    for(unsigned angle=0;angle<65536;++angle) {
        const double radians=angle*2*std::numbers::pi/65536.;
        for(double value:{std::sin(radians),std::cos(radians)}) {
            const float hi=float(value),lo=float(value-double(hi));
            const float tail=float((value-double(hi))-double(lo));
            const auto rebuilt=sf_add(sf_add(split(double(hi)),split(double(lo))),split(double(tail)));
            if(bits(rebuilt)!=std::bit_cast<std::uint64_t>(value))
                throw std::runtime_error("Three-float trigonometric packing mismatch");
        }
    }
    for(unsigned i=0;i<1000000;++i) {
        const auto raw=random();const float value=std::bit_cast<float>(raw);
        if(std::isfinite(value) && bits(sf_from_float_bits(raw))!=std::bit_cast<std::uint64_t>(double(value)))
            throw std::runtime_error("Float widening mismatch");
    }
    const auto check_mul=[&](double a,double b) {
        volatile double reference=a*b;
        const auto expected=std::isnormal(reference) || a==0. || b==0.
            ? std::bit_cast<std::uint64_t>(double(reference)) : bits(sf_invalid());
        if(bits(sf_mul(split(a),split(b)))!=expected)
            throw std::runtime_error("Portable binary64 multiplication mismatch");
        ++products;
    };
    const auto check_div=[&](double a,double b) {
        volatile double reference=a/b;
        const auto expected=std::isnormal(reference) || (a==0. && b!=0.)
            ? std::bit_cast<std::uint64_t>(double(reference)) : bits(sf_invalid());
        if(bits(sf_div(split(a),split(b)))!=expected)
            throw std::runtime_error("Portable binary64 division mismatch");
        if(bits(sf_div_radix16(split(a),split(b)))!=expected)
            throw std::runtime_error("Radix16 binary64 division mismatch");
        ++divisions;
    };
    // Limb carries, sticky-only low bits and rounding across both possible
    // product leading bits. Native double is the oracle, not the old shader.
    for(std::uint64_t x:{0ULL,1ULL,0xffffULL,0x10000ULL,0xffffffffULL,
        0x100000000ULL,0x7ffffffffffffULL,0x8000000000000ULL,
        0xffffffffffffeULL,0xfffffffffffffULL})
        for(std::uint64_t y:{0ULL,1ULL,0xffffULL,0x10000ULL,0xffffffffULL,
            0x100000000ULL,0x7ffffffffffffULL,0x8000000000000ULL,
            0xffffffffffffeULL,0xfffffffffffffULL}) {
            const auto a=std::bit_cast<double>((1023ULL<<52)|x);
            const auto b=std::bit_cast<double>((1023ULL<<52)|y);
            check_mul(a,b);check_mul(-a,b);check_mul(a,-b);
            check_div(a,b);check_div(-a,b);check_div(a,-b);
        }
    const auto check=[&](double a,double b) {
        volatile double sum=a+b,difference=a-b;
        const auto expected=[](double value) {
            return std::isfinite(value) && (value==0. || std::isnormal(value))
                ? std::bit_cast<std::uint64_t>(value) : bits(sf_invalid());
        };
        if(bits(sf_add(split(a),split(b)))!=expected(sum)
            || bits(sf_sub(split(a),split(b)))!=expected(difference)) {
            std::cerr<<std::hex<<std::bit_cast<std::uint64_t>(a)<<' '<<std::bit_cast<std::uint64_t>(b)<<'\n';
            throw std::runtime_error("Portable binary64 addition/subtraction mismatch");
        }
        ++checked;
    };
    for(double a:{0.,-0.,1.,-1.,112.,-112.,.5,-.5,1e-40,1e40})
        for(double b:{0.,-0.,1.,-1.,112.,-112.,.5,-.5,1e-40,1e40})check(a,b);
    for(unsigned i=0;i<1000000;++i) {
        const auto make=[&]() {
            const auto hi=(random()&0x800fffffU)|((700U+random()%600U)<<20);
            return std::bit_cast<double>((std::uint64_t(hi)<<32)|random());
        };
        const auto a=make(),b=make();check(a,b);check(a,-a);check_div(a,b);check_mul(a,b);
        if(sf_to_float_bits(split(a))!=std::bit_cast<std::uint32_t>(float(a)))
            throw std::runtime_error("Float narrowing mismatch");
        if(std::abs(a)>=1e-12 && std::abs(a)<=1e12) {
            const float hi=float(a),lo=float(a-double(hi));
            const auto pair=sf_add(split(double(hi)),split(double(lo)));
            if(bits(pair)!=bits(split(a)))++lossy_pairs;
            const float tail=float((a-double(hi))-double(lo));
            if(bits(sf_add(pair,split(double(tail))))!=bits(split(a)))
                throw std::runtime_error("Three-float camera transport mismatch");
            ++transport_samples;
        }
        check(a,std::nextafter(a,0.));
    }
    // Exercise every supported exponent, ties, carries, cancellation and the
    // deliberately unsupported subnormal-result/overflow boundaries.
    for(unsigned exponent=1;exponent<2047;++exponent) {
        const double a=std::bit_cast<double>(std::uint64_t(exponent)<<52);
        const double adjacent=std::nextafter(a,std::numeric_limits<double>::infinity());
        check(a,a);check(a,-a);check(a,adjacent);check(a,-adjacent);
        check_div(a,adjacent);check_div(a,3.);check_div(3.,a);
        check_mul(a,adjacent);check_mul(a,3.);check_mul(a,0.5);
        if(exponent>53) {
            const double half_ulp=std::ldexp(a,-53);
            check(a,half_ulp);check(adjacent,half_ulp);
            check(-a,-half_ulp);check(-adjacent,-half_ulp);
        }
    }
    check(std::numeric_limits<double>::max(),std::numeric_limits<double>::max());
    for(float value:{0.f,-0.f,std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(),1.f,std::numeric_limits<float>::max()}) {
        const double a=double(value),b=double(std::nextafter(value,std::numeric_limits<float>::infinity()));
        if(!std::isfinite(b))continue;
        const double midpoint=a+(b-a)*0.5;
        for(double sample:{a,b,midpoint,std::nextafter(midpoint,a),std::nextafter(midpoint,b)})
            if(sf_to_float_bits(split(sample))!=std::bit_cast<std::uint32_t>(float(sample)))
                throw std::runtime_error("Float narrowing boundary mismatch");
    }
    for(double a:{0.,-0.,1.,-1.})for(double b:{0.,-0.,1.,-1.,3.}) {check_div(a,b);check_mul(a,b);}
    if(sf_valid(sf_add(sf_make(1,0),split(1.))))throw std::runtime_error("Subnormal input accepted");
    if(sf_valid(sf_add(sf_make(0,0x7ff00000U),split(1.))))throw std::runtime_error("Infinite input accepted");
    // Invalid input semantics must match, not merely finite random quotients.
    for(const auto a:{sf_make(1,0),sf_make(0,0x7ff00000U),sf_make(1,0x7ff80000U),split(1.),split(-0.)})
        for(const auto b:{sf_make(1,0),sf_make(0,0x7ff00000U),sf_make(1,0x7ff80000U),split(1.),split(-0.)})
            if(bits(sf_div_radix16(a,b))!=bits(sf_div(a,b)))
                throw std::runtime_error("Radix16 invalid/signed-zero division contract mismatch");
    std::cout<<checked<<" portable binary64 add/sub pairs match host bits\n";
    std::cout<<divisions<<" reference and radix16 binary64 divisions match independent host bits\n";
    std::cout<<products<<" portable binary64 products match host bits\n";
    std::cout<<"1000064 full-width uint32 products match host bits\n";
    std::cout<<lossy_pairs<<" of "<<transport_samples<<" geometry-range doubles lose bits in two-float transport; three-float reconstruction exact\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
