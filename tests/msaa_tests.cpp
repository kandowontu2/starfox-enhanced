#include "starfox/render/msaa.hpp"
#include <iostream>
#include <limits>
using namespace starfox::render;
void require(bool ok,const char* what) {if(!ok) throw std::runtime_error(what);}
int main() {
    for(unsigned count:{2U,4U,8U}) {
        const unsigned full=(1U<<count)-1;
        // Several tracer steps can land on one output row, with holes between
        // them. A bounding span would incorrectly paint x=2 and x=3 here.
        const std::array<MsaaRowSpan,4> repeated{{{.25f,1.25f},{4.25f,5.25f},{.75f,1.5f},{4.25f,5.25f}}};
        require(msaa_row_coverage(repeated,2,count)==0 && msaa_row_coverage(repeated,3,count)==0,
            "Repeated-row MSAA filled a gap between tracer spans");
        for(unsigned x=0;x<7;++x) {
            unsigned expected=0;
            for(unsigned s=0;s<count;++s) {
                const float at=x+.5f+msaa_offsets[count-2+s].x;
                if((at>=.25f && at<1.5f) || (at>=4.25f && at<5.25f)) expected|=1U<<s;
            }
            require(msaa_row_coverage(repeated,x,count)==expected,"Repeated-row sample union mismatch");
            auto reversed=repeated;std::reverse(reversed.begin(),reversed.end());
            require(msaa_row_coverage(reversed,x,count)==expected,"Repeated-row traversal order changed coverage");
        }
        const std::array<MsaaRowSpan,3> invalid{{{2,1},{0,std::numeric_limits<float>::infinity()},{1,1}}};
        require(msaa_row_coverage(invalid,1,count)==0,"Invalid repeated span covered samples");
        const std::array<MsaaRowSpan,2> adjoining{{{-2,.5f},{.5f,2}}};
        require(msaa_row_coverage(adjoining,0,count)==full,"Repeated-row adjacent spans cracked");
        const std::array<MsaaPoint,3> large_a{{{1000.125f,1000.25f},{7990.5f,1000.25f},{1000.125f,7990.75f}}};
        const std::array<MsaaPoint,3> large_b{{large_a[1],{7990.5f,7990.75f},large_a[2]}};
        for(unsigned i=0;i<256;++i) {
            const unsigned x=1100+i*25,y=8990-x;
            const auto a=msaa_coverage(large_a,x,y,count),b=msaa_coverage(large_b,x,y,count);
            require((a|b)==full && (a&b)==0,"High-resolution MSAA shared edge cracked or overlapped");
        }
        for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
            const std::array<MsaaPoint,3> a{{{0,0},{16,0},{0,16}}},b{{{16,0},{16,16},{0,16}}};
            const auto ca=msaa_coverage(a,x,y,count),cb=msaa_coverage(b,x,y,count);
            require((ca|cb)==full,"MSAA crack along shared diagonal");
            require((ca&cb)==0,"MSAA double-owned shared sample");
            const std::array<MsaaPoint,3> reverse{a[0],a[2],a[1]};
            require(msaa_coverage(reverse,x,y,count)==ca,"MSAA winding changed coverage");
        }
        const std::array<MsaaPoint,3> half{{{-10,-10},{.5f,-10},{.5f,20}}};
        const auto mask=msaa_coverage(half,0,0,count);
        require(mask!=0 && mask!=full,"MSAA vertical boundary has no partial coverage");
        std::array<std::array<float,4>,8> storage{};
        std::span<std::array<float,4>> samples(storage.data(),count);
        unsigned shades=0;
        msaa_paint(samples,mask,[&]{++shades;return std::array<float,4>{1,.5f,0,1};});
        require(shades==1,"MSAA shaded once per sample instead of once per primitive/pixel");
        msaa_paint(samples,0,[&]{++shades;return std::array<float,4>{};});
        require(shades==1,"MSAA shaded an uncovered primitive");
        const auto color=msaa_resolve(samples);
        require(color[0]>.0f && color[0]<1 && color[1]==color[0]*.5f,"MSAA coverage resolve failed");
        msaa_paint(samples,full,[&]{++shades;return std::array<float,4>{.25f,.25f,.25f,1};});
        require(msaa_resolve(samples)==std::array<float,4>{.25f,.25f,.25f,1},"MSAA painter overwrite failed");
        require(msaa_coverage({MsaaPoint{0,0},MsaaPoint{0,0},MsaaPoint{0,0}},0,0,count)==0,"Degenerate triangle covered samples");
        require(msaa_coverage({MsaaPoint{0,0},MsaaPoint{1,0},MsaaPoint{0,std::numeric_limits<float>::quiet_NaN()}},0,0,count)==0,"Nonfinite triangle covered samples");
    }
    std::cout<<"MSAA 2/4/8 coverage, shared edges, winding, pixel-frequency shading and resolve passed\n";
}
