#include "starfox/platform/nintendo_3ds/pica_program_binding.hpp"
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
std::set<const void*> alive;
unsigned destroyed{};
struct Program {
    explicit Program(unsigned id):id(id) {alive.insert(this);}
    ~Program() {alive.erase(this);++destroyed;}
    unsigned id;
};
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
}
int main() try {
    using starfox::platform::nintendo_3ds::PicaProgramBinding;
    Program* sdk_borrowed{};
    unsigned last_id{};
    const auto sdk_bind=[&](Program& next) {
        if(sdk_borrowed) {
            require(alive.contains(sdk_borrowed),"Citro3D read a freed previous program");
            last_id=sdk_borrowed->id;
        }
        sdk_borrowed=&next;
    };
    {
        PicaProgramBinding<Program> slot;
        auto first=std::make_unique<Program>(1);
        slot.bind(*first,sdk_bind);
        slot.retire(first);
        require(!first && destroyed==0,"Bound shader died at GPU shutdown");
        // Failed/unbound construction cannot retire the shader SDK remembers.
        auto failed=std::make_unique<Program>(99);
        slot.retire(failed);
        require(destroyed==1 && alive.contains(sdk_borrowed),"Unbound owner replaced borrowed shader");
        auto second=std::make_unique<Program>(2);
        try {slot.bind(*second,[](Program&) {throw std::runtime_error("bind failure");});}
        catch(const std::runtime_error&) {}
        require(alive.contains(sdk_borrowed),"Failed bind retired previous shader");
        slot.bind(*second,sdk_bind);
        require(last_id==1 && destroyed==2,"Previous program was not inspected then retired");
        slot.bind(*second,sdk_bind); // Ordinary frames do not retire active storage.
        slot.retire(second);
        for(unsigned i=3;i<100;++i) {
            auto next=std::make_unique<Program>(i);
            require(alive.size()==2,"Retired shader storage grew across menu/game transitions");
            slot.bind(*next,sdk_bind);
            require(last_id==i-1 && alive.size()==1,"Rebinding lifetime contract lost");
            slot.retire(next);
        }
    }
    require(alive.empty() && destroyed==100,"Program storage leaked at process shutdown");
    std::cout << "PICA shader lifetime: teardown/rebind, repeated frames, failed allocation/bind and bounded retirement passed\n";
    return 0;
} catch(const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
