#include "starfox/render/d3d12_descriptor_capacity.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
int main() try {
    std::uint64_t checks=0;
    for(const std::uint32_t capacity:{2048U,65536U})
        for(std::uint32_t cursor=0;cursor<=capacity+1;++cursor)
            for(std::uint32_t count=0;count<=128;++count) {
                const bool expected=std::uint64_t(cursor)+count>capacity;
                if(bool(Starfox_D3D12DescriptorReservationNeeded(cursor,capacity,count))!=expected)
                    throw std::runtime_error("Reservation differs from wide-integer oracle");
                ++checks;
            }
    for(const std::uint32_t cursor:{0U,UINT32_MAX-1,UINT32_MAX})
        for(const std::uint32_t count:{0U,1U,2U,UINT32_MAX}) {
            const bool expected=std::uint64_t(cursor)+count>UINT32_MAX;
            if(bool(Starfox_D3D12DescriptorReservationNeeded(cursor,UINT32_MAX,count))!=expected)
                throw std::runtime_error("Descriptor reservation addition overflow");
            ++checks;
        }
    if(!Starfox_D3D12DescriptorReservationNeeded(65535,65536,2)
        || Starfox_D3D12DescriptorReservationNeeded(65534,65536,2))
        throw std::runtime_error("Crash boundary/exact capacity regression");
    std::cout<<checks<<" descriptor capacity checks passed; native validation separate\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
