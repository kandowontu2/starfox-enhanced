#include "starfox/render/responsive_preparation.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>

using namespace starfox::render;
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct Events {
    std::thread::id owner=std::this_thread::get_id();
    bool wrong_thread{},inherited_context{},reentrant_worker{};
    unsigned calls{};
    static void pump(void* user) noexcept {
        auto& self=*static_cast<Events*>(user);++self.calls;
        self.wrong_thread|=std::this_thread::get_id()!=self.owner;
        self.inherited_context|=preparation_events!=nullptr;
        self.reentrant_worker|=responsive_prepare([&] {return std::this_thread::get_id()!=self.owner;});
    }
};
int main() {
    const auto owner=std::this_thread::get_id();
    require(responsive_prepare([] {return std::this_thread::get_id();})==owner,"No-scope work moved threads");
    Events events;PreparationEvents context{Events::pump,&events};
    {
        ScopedPreparationEvents scope(&context);
        require(responsive_prepare([&] {
            require(preparation_events==nullptr,"Owner context leaked to compiler");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            return std::this_thread::get_id();
        })!=owner,"Scoped compiler did not leave owner thread");
        require(events.calls>=10,"Owner events not serviced frequently during long preparation");
        require(!events.wrong_thread && !events.inherited_context && !events.reentrant_worker,"Pump violated thread/reentrancy contract");
        bool caught=false;
        try {responsive_prepare([]() -> int {throw std::runtime_error("compiler failure");});}
        catch(const std::runtime_error& e) {caught=std::string_view(e.what())=="compiler failure";}
        require(caught,"Compiler exception lost");
        require(preparation_events==&context,"Exception lost preparation scope");
        {
            ScopedPreparationEvents suspend(nullptr);
            require(responsive_prepare([] {return std::this_thread::get_id();})==owner,"Disabled scope launched worker");
        }
        require(preparation_events==&context,"Nested scope lost owner");
        auto result=responsive_prepare([] {return std::make_unique<int>(42);});
        require(*result==42,"Move-only preparation result lost");
        bool prepared=false;
        responsive_prepare([&] {prepared=true;});
        require(prepared,"Void preparation was not joined");
        for(unsigned repeat=0;repeat<100;++repeat)
            require(responsive_prepare([repeat] {return repeat;})==repeat,"Sequential preparation result corrupted");
    }
    require(preparation_events==nullptr,"Scope remained after teardown");
    require(context.jobs==104 && context.pumps==events.calls,"Preparation metrics corrupted");
    std::cout<<"PASS: joined preparation, main-thread pumps, errors, nested scopes, repeated jobs\n";
}
