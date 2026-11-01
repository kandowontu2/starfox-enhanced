#include "starfox/render/backdrop_image.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

int main() try {
    using starfox::render::BackdropImage;
    const std::array<std::uint8_t,81> bytes{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,3,0,0,0,2,8,2,0,0,0,18,22,241,77,0,0,0,24,73,68,65,84,120,156,99,248,207,192,192,0,193,255,255,255,231,81,178,96,96,96,0,0,70,161,6,97,3,96,28,182,0,0,0,0,73,69,78,68,174,66,96,130};
    const auto image=BackdropImage::decode(bytes);
    if(image.width!=3 || image.height!=2 || image.pixels!=std::vector<std::uint32_t>{
        0xff0000ff,0xff00ff00,0xffff0000,0xffffffff,0xff38220c,0xff000000})
        throw std::runtime_error("PNG channel order or row layout changed");
    for(std::size_t n=0;n<bytes.size();++n) {
        bool rejected=false;
        try {BackdropImage::decode(std::span(bytes).first(n));} catch(const std::runtime_error&) {rejected=true;}
        if(!rejected) throw std::runtime_error("Truncated PNG accepted");
    }
    auto damaged=bytes;damaged[50]^=1;
    bool rejected=false;
    try {BackdropImage::decode(damaged);} catch(const std::runtime_error&) {rejected=true;}
    if(!rejected) throw std::runtime_error("PNG checksum corruption accepted");
    damaged=bytes;damaged[16]=0x7f;
    rejected=false;
    try {BackdropImage::decode(damaged);} catch(const std::runtime_error&) {rejected=true;}
    if(!rejected) throw std::runtime_error("Oversized PNG accepted");
    std::cout<<"PNG pixels, every truncated prefix, corruption and dimension guards passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
