#include "../src/render/model_host_clock.hpp"
#include <iostream>
#include <thread>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value) throw std::runtime_error(message);}
}
int main() try {
    using namespace starfox::render;
    require(!model_thread_cpu_now(false).valid,"Disabled thread clock must remain unavailable");
    const auto disabled=model_host_clock_now(false,true);
    require(!disabled.cpu.valid && disabled.wall==std::chrono::steady_clock::time_point{},"Disabled host clock sampled");
    require(!model_thread_cpu_delta({4,true},{3,true}).valid,"Backward CPU clock accepted");
    require(!model_thread_cpu_delta({4,false},{9,true}).valid,"Missing start accepted");
    require(!model_thread_cpu_delta({4,true},{9,false}).valid,"Missing stop accepted");
    const auto delta=model_thread_cpu_delta({4,true},{9,true});
    require(delta.valid && delta.ticks==5,"100 ns CPU units changed");
    const auto start=model_host_clock_now(true,true);
    if(!start.cpu.valid) {
        require(std::string_view(model_thread_cpu_provider())=="unsupported","Supported CPU clock query failed");
        std::cout<<"Host clock: disabled/invalid guards passed; CPU clock explicitly unsupported\n";
        return 0;
    }
    const auto stop=start.wall+std::chrono::milliseconds(100);
    volatile std::uint64_t result=1;
    while(std::chrono::steady_clock::now()<stop) {
        auto value=result;
        for(unsigned i=0;i<1000;++i) value=(value*1664525+1013904223)^(value>>13);
        result=value;
    }
    const auto worked=model_host_clock_now(true,true);
    const auto work=model_thread_cpu_delta(start.cpu,worked.cpu);
    require(work.valid && work.ticks>0,"Busy thread had no CPU time");
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    const auto slept=model_host_clock_now(true,true);
    const auto sleep=model_thread_cpu_delta(worked.cpu,slept.cpu);
    const auto wall=std::chrono::duration_cast<std::chrono::nanoseconds>(slept.wall-worked.wall).count()/100;
    require(sleep.valid && wall>0,"CPU clock lost validity after wait");
    require(sleep.ticks<std::uint64_t(wall)/2,"CPU counter counted sleeping as executing work");
    std::cout<<"Host clock: "<<model_thread_cpu_provider()<<", disabled/invalid guards, positive executing CPU and excluded sleep passed; work="
        <<work.ticks<<" sleep="<<sleep.ticks<<" sleep-wall="<<wall<<" (100 ns)\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
