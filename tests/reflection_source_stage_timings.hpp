#pragma once
#include "starfox/render/reflection_source_diagnostics.hpp"
#include "starfox/render/sdl_d3d12_bridge.h"
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_properties.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

// Fixture-only, opt-in profiling of ALL batches/lobes, not player enablement.
// Each query index is written once. No reads, waits, submits, input uploads or
// timestamp reuse while its command is pending. The fixture must retire the
// exact submission (or cancel the unsubmitted command) before retired().
class ReflectionSourceStageTimings {
public:
    using Lookup=const StarfoxSdlD3D12TimestampsV1*(*)(void*);
    static constexpr unsigned stages=10,batches_per_chunk=409,max_chunks=32;
    explicit ReflectionSourceStageTimings(bool enabled,Lookup lookup=lookup_bridge)
        :enabled_(enabled),lookup_(lookup) {}
    ReflectionSourceStageTimings(const ReflectionSourceStageTimings&)=delete;
    ReflectionSourceStageTimings& operator=(const ReflectionSourceStageTimings&)=delete;
    ~ReflectionSourceStageTimings() {
        // A failure to prove retirement must not turn into a use-after-free.
        // This bounded diagnostic-only retention is NOT resource acceptance.
        if(!chunks_.empty())std::cerr<<"Source stage timestamps retained without proved retirement: "<<chunks_.size()<<'\n';
    }
    bool enabled() const noexcept {return enabled_;}
    bool active() const noexcept {return !chunks_.empty();}
    static void observe(void* context,const starfox::render::ReflectionSourceStageEvent& event) {
        static_cast<ReflectionSourceStageTimings*>(context)->mark(event);
    }
    // AFTER the existing successful submission wait/renderer close, while the
    // device lives. Device removal is retirement, but never valid timing data.
    void retired(std::ostream& out,const char* reason,bool healthy=true) {
        for(unsigned index=0;index<groups_.size();++index) {
            auto& group=groups_[index];if(!group.total)continue;
            std::array<double,9> gpu_ms{};bool valid=healthy && group.completed==group.batches;
            for(const auto ci:group.chunks) {
                const auto& chunk=chunks_[ci];
                if(!valid)continue;
                std::vector<std::uint64_t> ticks(chunk.used*stages);
                valid=chunk.used && chunk.bridge->read(chunk.context,0,unsigned(ticks.size()),ticks.data());
                for(unsigned batch=0;valid && batch<chunk.used;++batch) {
                    const auto at=batch*stages;
                    valid=ticks[at]!=0;
                    for(unsigned stage=1;valid && stage<stages;++stage) {
                        valid=ticks[at+stage]>=ticks[at+stage-1];
                        if(valid)gpu_ms[stage-1]+=double(ticks[at+stage]-ticks[at+stage-1])*1000/double(chunk.hz);
                    }
                }
            }
            out<<"Source stage profile retirement="<<reason<<" eye="<<index/8<<" sample="<<index%8
                <<" records="<<group.total<<" batches="<<group.completed<<'/'<<group.batches;
            if(valid) {
                constexpr const char* names[]{"queries","optical","roots","witness","folds","guide","colour","composition","publication"};
                for(unsigned stage=0;stage<9;++stage)out<<' '<<names[stage]<<"-gpu-ms="<<gpu_ms[stage]
                    <<"-encode-ms="<<group.cpu_ms[stage];
            } else out<<" timing-unavailable-or-incomplete";
            out<<"; diagnostic instrumentation/contention, not player FPS qualification\n";
        }
        // No context is destroyed until the caller has proved retirement.
        for(auto& chunk:chunks_)chunk.bridge->destroy(chunk.context);
        chunks_.clear();groups_={};out.flush();
    }
private:
    struct Chunk {const StarfoxSdlD3D12TimestampsV1* bridge{};void* context{};std::uint64_t hz{};unsigned used{};};
    struct Group {
        void* command{};void* device{};std::uint64_t total{};
        unsigned batches{},completed{},next_stage{};std::vector<unsigned> chunks;
        std::array<double,9> cpu_ms{};std::chrono::steady_clock::time_point previous;
    };
    bool enabled_{};Lookup lookup_{};
    std::vector<Chunk> chunks_;std::array<Group,16> groups_{};
    static const StarfoxSdlD3D12TimestampsV1* lookup_bridge(void* device) {
        return static_cast<const StarfoxSdlD3D12TimestampsV1*>(SDL_GetPointerProperty(
            SDL_GetGPUDeviceProperties(static_cast<SDL_GPUDevice*>(device)),STARFOX_SDL_D3D12_TIMESTAMPS,nullptr));
    }
    static void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
    void mark(const starfox::render::ReflectionSourceStageEvent& event) {
        if(!enabled_)return;
        require(event.device && event.command && event.eye<2 && event.sample<8 && event.total
            && event.total<=std::uint64_t(batches_per_chunk)*64*max_chunks
            && event.first<event.total && event.first%64==0
            && event.count==std::min<std::uint64_t>(64,event.total-event.first),"Invalid full-source timestamp event");
        const auto now=std::chrono::steady_clock::now();const unsigned stage=unsigned(event.stage),batch=unsigned(event.first/64);
        require(stage<stages,"Invalid source timestamp stage");auto& group=groups_[event.eye*8+event.sample];
        if(!group.total) {
            require(!stage && !batch,"Source timestamp stream did not start at record zero");
            group.command=event.command;group.device=event.device;group.total=event.total;
            group.batches=unsigned((event.total+63)/64);
            const unsigned needed=(group.batches+batches_per_chunk-1)/batches_per_chunk;
            require(needed<=max_chunks-chunks_.size(),"Source timestamp context bound exceeded");
            require(lookup_,"Missing source timestamp bridge lookup");const auto* bridge=lookup_(event.device);
            require(bridge && bridge->version==1 && bridge->create && bridge->write
                && bridge->resolve && bridge->read && bridge->destroy,"Source D3D12 timestamp bridge unavailable");
            group.chunks.reserve(needed);chunks_.reserve(chunks_.size()+needed);
            for(unsigned chunk=0;chunk<needed;++chunk) {
                std::uint64_t hz{};auto* context=bridge->create(event.device,batches_per_chunk*stages,&hz);
                if(context && !hz)bridge->destroy(context); // Unwritten, never submitted.
                require(context && hz,"Source timestamp allocation failed");
                group.chunks.push_back(unsigned(chunks_.size()));chunks_.push_back({bridge,context,hz,0});
            }
        }
        require(group.command==event.command && group.device==event.device && group.total==event.total
            && group.completed==batch && group.next_stage==stage,"Source timestamp batch/stage/order changed while pending");
        auto& chunk=chunks_[group.chunks[batch/batches_per_chunk]];
        const unsigned local=batch%batches_per_chunk,query=local*stages+stage;
        require(chunk.bridge->write(event.command,chunk.context,query),"Source timestamp write failed");
        if(stage)group.cpu_ms[stage-1]+=std::chrono::duration<double,std::milli>(now-group.previous).count();
        group.previous=std::chrono::steady_clock::now();
        if(stage+1==stages) {
            require(chunk.bridge->resolve(event.command,chunk.context,local*stages,stages),"Source timestamp resolve failed");
            ++chunk.used;++group.completed;group.next_stage=0;
        } else ++group.next_stage;
    }
};
