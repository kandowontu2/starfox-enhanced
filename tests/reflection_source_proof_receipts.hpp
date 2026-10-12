#pragma once
#include "starfox/render/reflection_source_diagnostics.hpp"
#include <SDL3/SDL.h>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>

// Diagnostic output only. Preserve every query and the complete proof prefix
// before streamed scratch is reused. Never sample these downloads as renderer
// input or use an inverse/proof result as the independent optics oracle.
struct ReflectionSourceProofLayout {
    std::uint32_t queries_bytes{},bytes{};
    static std::optional<ReflectionSourceProofLayout> make(std::uint64_t total,
        std::uint32_t query_stride,std::uint32_t proof_stride,std::uint32_t proof_bytes) noexcept {
        if(!total || query_stride!=48 || proof_stride!=24576 || proof_bytes!=576)return {};
        if(total>16U*1024*1024/(std::uint64_t(query_stride)+proof_bytes))return {};
        const auto bytes=total*(std::uint64_t(query_stride)+proof_bytes);
        return ReflectionSourceProofLayout{std::uint32_t(total*query_stride),std::uint32_t(bytes)};
    }
};

class ReflectionSourceProofReceipts {
public:
    struct Api {
        decltype(&SDL_CreateGPUTransferBuffer) create{SDL_CreateGPUTransferBuffer};
        decltype(&SDL_BeginGPUCopyPass) begin{SDL_BeginGPUCopyPass};
        decltype(&SDL_DownloadFromGPUBuffer) download{SDL_DownloadFromGPUBuffer};
        decltype(&SDL_EndGPUCopyPass) end{SDL_EndGPUCopyPass};
        decltype(&SDL_MapGPUTransferBuffer) map{SDL_MapGPUTransferBuffer};
        decltype(&SDL_UnmapGPUTransferBuffer) unmap{SDL_UnmapGPUTransferBuffer};
        decltype(&SDL_ReleaseGPUTransferBuffer) release{SDL_ReleaseGPUTransferBuffer};
    };
    ~ReflectionSourceProofReceipts() {release();}
    ReflectionSourceProofReceipts()=default;
    explicit ReflectionSourceProofReceipts(Api api):api_(api){}
    ReflectionSourceProofReceipts(const ReflectionSourceProofReceipts&)=delete;
    ReflectionSourceProofReceipts& operator=(const ReflectionSourceProofReceipts&)=delete;
    void observe(const starfox::render::ReflectionSourceStageEvent& event) {
        using starfox::render::ReflectionSourceStage;
        if(!selected || event.stage!=ReflectionSourceStage::composition || event.eye || event.sample)return;
        const auto layout=ReflectionSourceProofLayout::make(event.total,event.query_stride,event.proof_stride,event.proof_bytes);
        if(!layout || !event.device || !event.command || !event.queries || !event.proofs
            || !event.count || event.count>64 || event.first!=next_ || event.first>event.total
            || event.count>event.total-event.first)
            throw std::runtime_error("Malformed/out-of-order source proof receipt stream");
        auto* device=static_cast<SDL_GPUDevice*>(event.device);
        if(!transfer_) {
            const SDL_GPUTransferBufferCreateInfo info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,layout->bytes,0};
            transfer_=api_.create(device,&info);
            if(!transfer_)throw std::runtime_error(SDL_GetError());
            device_=device;total_=event.total;layout_=*layout;
        } else if(device_!=device || total_!=event.total || layout_.bytes!=layout->bytes)
            throw std::runtime_error("Source proof receipt changed device/extent while pending");
        auto* copy=api_.begin(static_cast<SDL_GPUCommandBuffer*>(event.command));
        if(!copy)throw std::runtime_error(SDL_GetError());
        const SDL_GPUBufferRegion query{static_cast<SDL_GPUBuffer*>(event.queries),0,event.count*event.query_stride};
        const SDL_GPUTransferBufferLocation query_to{transfer_,std::uint32_t(event.first*event.query_stride)};
        api_.download(copy,&query,&query_to);
        for(unsigned i=0;i<event.count;++i) {
            const SDL_GPUBufferRegion proof{static_cast<SDL_GPUBuffer*>(event.proofs),i*event.proof_stride,event.proof_bytes};
            const SDL_GPUTransferBufferLocation proof_to{transfer_,layout_.queries_bytes+std::uint32_t((event.first+i)*event.proof_bytes)};
            api_.download(copy,&proof,&proof_to);
        }
        api_.end(copy);next_+=event.count;
    }
    // Caller supplies exact completed-submission evidence, not elapsed time.
    // On incomplete/device-lost submissions, release without reading receipts.
    std::vector<unsigned> retired(bool completed) {
        if(!completed || !transfer_ || next_!=total_)
            throw std::runtime_error("Source proof receipt read before complete exact submission retirement");
        const auto* mapped=api_.map(device_,transfer_,false);
        if(!mapped)throw std::runtime_error(SDL_GetError());
        std::vector<unsigned> words(layout_.bytes/4);
        std::memcpy(words.data(),mapped,layout_.bytes);api_.unmap(device_,transfer_);
        release();return words;
    }
    void release() noexcept {
        if(transfer_)api_.release(device_,transfer_);
        transfer_=nullptr;device_=nullptr;total_=next_=0;layout_={};
    }
    bool selected{};
private:
    Api api_{};
    SDL_GPUDevice* device_{};SDL_GPUTransferBuffer* transfer_{};
    ReflectionSourceProofLayout layout_{};std::uint64_t total_{},next_{};
};
