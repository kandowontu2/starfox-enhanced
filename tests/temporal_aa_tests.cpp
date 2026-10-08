#include "starfox/render/temporal_aa.hpp"
#include <iostream>
#include <limits>
using namespace starfox::render;
void require(bool yes,const char* message) {if(!yes) throw std::runtime_error(message);}
int main() {
    TemporalAa taa;
    std::vector<std::uint8_t> first(5*3*4,255),second;
    std::vector<TemporalAaGuide> guides(15,{0,0,10,10,true,true});
    for(unsigned i=0;i<15;++i) for(unsigned c=0;c<3;++c) first[i*4+c]=(i%5)*50;
    require(taa.resolve(5,3,first,guides,1)==first,"first frame changed");
    second=first;second[7*4]=125;
    auto out=taa.resolve(5,3,second,guides,1);
    require(out[7*4]>100 && out[7*4]<125,"valid history not accumulated");
    require(out[7*4+3]==255,"alpha changed");
    guides[7].eligible=false;second[7*4]=40;
    require(taa.resolve(5,3,second,guides,1)[7*4]==40,"HUD accumulated history");
    guides[7].eligible=true;guides[7].valid=false;
    require(taa.resolve(5,3,first,guides,1)==first,"unknown motion accumulated history");
    guides[7].valid=true;guides[7].previous_depth=100;
    require(taa.resolve(5,3,second,guides,1)[7*4]==40,"disoccluded history reused");
    guides[7].previous_depth=10;guides[7].motion_x=std::numeric_limits<float>::quiet_NaN();
    require(taa.resolve(5,3,first,guides,1)[7*4]==100,"invalid motion accepted");
    guides[7].motion_x=0;
    require(taa.resolve(5,3,second,guides,2)==second,"scene cut reused history");
    taa.reset();require(taa.resolve(5,3,first,guides,2)==first,"reset reused history");
    // Reproject one pixel to the right in the previous frame.
    guides[7].motion_x=1;second=first;second[7*4]=130;
    out=taa.resolve(5,3,second,guides,2);
    require(out[7*4]>130,"motion direction incorrect");
    // Manual golden: only same-phase pixels establish the clamp. A bright
    // adjacent stripe must neither seed history nor widen the legal range.
    taa.reset();first.assign(60,255);guides.assign(15,{0,0,10,10,true,true,1});
    for(unsigned i=0;i<15;++i) for(unsigned c=0;c<3;++c) first[i*4+c]=100;
    (void)taa.resolve(5,3,first,guides,3,.5F);second=first;second[7*4]=120;second[6*4]=80;
    guides[8].pattern_phase=2;second[8*4]=250;
    out=taa.resolve(5,3,second,guides,3,.5F);
    require(out[7*4]==110,"same-phase history not accumulated");
    guides[7].motion_x=1;out=taa.resolve(5,3,first,guides,3,.5F);
    require(out[7*4]==100,"cross-phase history accepted");
    guides[7].motion_x=.01F;second[7*4]=120;
    require(taa.resolve(5,3,second,guides,3,.5F)[7*4]==120,"fractional mixed-phase footprint was renormalized");
    std::cout<<"TAA reprojection, clipping, disocclusion, HUD, reset and exact pattern-phase tests passed\n";
}
