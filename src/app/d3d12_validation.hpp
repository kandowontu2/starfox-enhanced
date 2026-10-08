#pragma once
// Explicit player diagnostics only: retain lossless critical messages through
// real SDK shutdown and SDL renderer destruction. No normal-frame readback,
// submission, wait, queue clearing or enlarged message storage.
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

class PlayerD3d12Validation {
    std::vector<Microsoft::WRL::ComPtr<ID3D12InfoQueue>> queues_;
public:
    void capture(void* device) {
        if(!device || !std::getenv("STARFOX_TEST_FRAMES")
            || !std::getenv("STARFOX_TEST_GPU_VALIDATION")
            || !std::getenv("STARFOX_TEST_DLSS_VALIDATION")) return;
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> queue;
        if(FAILED(static_cast<ID3D12Device*>(device)->QueryInterface(__uuidof(ID3D12InfoQueue),
            reinterpret_cast<void**>(queue.GetAddressOf()))))
            throw std::runtime_error("Player D3D12 validation queue unavailable");
        if(queue->GetNumMessagesDiscardedByMessageCountLimit())
            throw std::runtime_error("Player D3D12 validation overflowed before capture");
        D3D12_MESSAGE_SEVERITY critical[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
        D3D12_MESSAGE_SEVERITY chatter[]{D3D12_MESSAGE_SEVERITY_WARNING,D3D12_MESSAGE_SEVERITY_INFO,D3D12_MESSAGE_SEVERITY_MESSAGE};
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=critical;
        filter.DenyList.NumSeverities=3;filter.DenyList.pSeverityList=chatter;
        if(FAILED(queue->AddStorageFilterEntries(&filter)))
            throw std::runtime_error("Player D3D12 critical-message capture failed");
        // A later SDL owner may reuse the same native device and replace its
        // storage filter. Reapply capture without discarding preceding errors.
        if(std::none_of(queues_.begin(),queues_.end(),[&](const auto& held){return held.Get()==queue.Get();}))
            queues_.push_back(std::move(queue));
    }
    void report(const char* stage) const noexcept {
        for(const auto& queue:queues_) {
            const auto discarded=queue->GetNumMessagesDiscardedByMessageCountLimit();
            D3D12_MESSAGE_SEVERITY critical[]{D3D12_MESSAGE_SEVERITY_ERROR,D3D12_MESSAGE_SEVERITY_CORRUPTION};
            D3D12_INFO_QUEUE_FILTER filter{};filter.AllowList.NumSeverities=2;filter.AllowList.pSeverityList=critical;
            if(FAILED(queue->PushRetrievalFilter(&filter))) {
                std::cerr<<"dlss-validation: FAILED retrieval at "<<stage<<'\n';continue;
            }
            const auto count=queue->GetNumStoredMessagesAllowedByRetrievalFilter();
            std::cerr<<"dlss-validation: stage="<<stage<<" critical="<<count<<" discarded="<<discarded<<'\n';
            for(UINT64 i=0;i<std::min<UINT64>(count,16);++i) {
                SIZE_T size{};
                if(FAILED(queue->GetMessage(i,nullptr,&size))) {
                    std::cerr<<"dlss-validation: FAILED message size\n";continue;
                }
                try {
                    std::vector<unsigned char> storage(size);
                    auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                    if(FAILED(queue->GetMessage(i,message,&size))) std::cerr<<"dlss-validation: FAILED message read\n";
                    else std::cerr<<"dlss-validation: ID "<<message->ID<<": "<<message->pDescription<<'\n';
                } catch(...) {std::cerr<<"dlss-validation: FAILED message allocation\n";}
            }
            queue->PopRetrievalFilter();
            if(count || discarded) std::cerr<<"dlss-validation: FAILED critical/overflow guard\n";
        }
    }
};
