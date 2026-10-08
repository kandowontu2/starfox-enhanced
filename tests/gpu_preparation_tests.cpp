#include "starfox/render/gpu_preparation.hpp"
#include "starfox/render/submission_retirement.hpp"
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>

using namespace starfox::render;
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
int main() {
    {
        std::vector<unsigned> pending{1,2},waited,released;
        std::array<bool,6> complete{};
        bool permit_wait=true;
        const auto query=[&](unsigned fence){return complete.at(fence);};
        const auto wait=[&](unsigned fence){
            waited.push_back(fence);
            if(!permit_wait) return false;
            complete.at(fence)=true;return true;
        };
        const auto release=[&](unsigned fence){
            require(complete.at(fence),"Released an in-flight fence to the reusable pool");
            released.push_back(fence);
        };
        require(retire_submission_queue(pending,2,query,wait,release)
            && waited.empty() && released.empty() && pending==std::vector<unsigned>{1,2},
            "Encoding ahead waited/released incomplete work below the queue limit");
        complete[1]=true;
        require(retire_submission_queue(pending,2,query,wait,release)
            && waited==std::vector<unsigned>{1} && released==waited && pending==std::vector<unsigned>{2},
            "Completed work was not joined and retired in order");
        pending.insert(pending.end(),{3,4});
        require(retire_submission_queue(pending,2,query,wait,release)
            && pending==std::vector<unsigned>{3,4} && released==std::vector<unsigned>{1,2},
            "Queue pressure did not wait only for the oldest submission");
        pending.push_back(5);permit_wait=false;
        require(!retire_submission_queue(pending,2,query,wait,release)
            && pending==std::vector<unsigned>{3,4,5} && released==std::vector<unsigned>{1,2},
            "A failed wait discarded a pending fence");
        permit_wait=true;
        require(retire_submission_queue(pending,0,query,wait,release) && pending.empty()
            && released==std::vector<unsigned>{1,2,3,4,5},"Readback/mode change did not drain every fence");
    }
    require(SDL_Init(SDL_INIT_EVENTS),SDL_GetError());
    PreparationEvents events{[](void*) noexcept {SDL_PumpEvents();},nullptr};
    {
        ScopedPreparationEvents scope(&events);
        const auto sentinel_type=SDL_RegisterEvents(1);
        require(sentinel_type!=Uint32(-1),SDL_GetError());
        SDL_Event sentinel{};sentinel.type=sentinel_type;sentinel.user.code=73;
        require(SDL_PushEvent(&sentinel),SDL_GetError());
        SDL_SetError("stale owner error");
        auto* failed=prepare_gpu_resource([]() -> SDL_GPUShader* {
            SDL_SetError("intentional compiler failure");
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            return nullptr;
        });
        require(!failed && std::string_view(SDL_GetError())=="intentional compiler failure","Compiler's SDL error lost across threads");
        SDL_Event delivered{};
        require(SDL_PeepEvents(&delivered,1,SDL_GETEVENT,sentinel_type,sentinel_type)==1
            && delivered.user.code==73,"Preparation pump consumed queued user input");
        int owned_resource=42;
        auto* created=prepare_gpu_resource([&] {return &owned_resource;});
        require(created==&owned_resource && *created==42,"Prepared resource not returned to its owner");
        require(events.jobs==2 && events.pumps>=2,"GPU preparation scope was not exercised");
    }
    SDL_Quit();
    std::cout<<"PASS: bounded submission retirement, retained in-flight/failed fences; compiler SDL error returned; events retained; borrowed owner resource joined\n";
}
