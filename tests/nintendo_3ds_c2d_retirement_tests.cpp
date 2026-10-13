#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>
#include "../platform/3ds/reference/display_retirement.hpp"

struct DVLB_s {unsigned id;};
struct Shader {DVLB_s* library;};
struct shaderProgram_s {Shader* vertexShader; Shader* geometryShader;};
static std::set<Shader*> live_instances;
static std::set<DVLB_s*> live_libraries;
static shaderProgram_s* sdk_borrowed;
static unsigned freed;
static void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
static void C3D_BindProgram(shaderProgram_s* next) {
    // Reproduce Citro3D's retained pointer across Fini/Init and its old-shader read.
    if(sdk_borrowed && sdk_borrowed!=next) {
        require(live_instances.contains(sdk_borrowed->vertexShader),"Rebind read a freed shader instance");
        require(live_libraries.contains(sdk_borrowed->vertexShader->library),"Rebind read a freed DVLB");
    }
    require(next && live_instances.contains(next->vertexShader),"Next program is not alive");
    sdk_borrowed=next;
}
static void shaderProgramFree(shaderProgram_s* program) {
    require(program!=sdk_borrowed,"Freed the program still borrowed by Citro3D");
    require(live_instances.erase(program->vertexShader)==1,"Double shader free");
    delete program->vertexShader;program->vertexShader=nullptr;++freed;
}
static void DVLB_Free(DVLB_s* library) {
    require(live_libraries.erase(library)==1,"Double DVLB free");delete library;
}
#include "../platform/3ds/reference/c2d_program_retirement.h"
struct Owner {
    shaderProgram_s program{};DVLB_s* library{};
    explicit Owner(unsigned id) {
        library=new DVLB_s{id};live_libraries.insert(library);
        program.vertexShader=new Shader{library};live_instances.insert(program.vertexShader);
    }
    void free() {shaderProgramFree(&program);DVLB_Free(library);library=nullptr;}
};
static void display_queue_retirement() {
    struct Target { bool linked; };
    // Cold close, C3D-only, C2D partial construction, one/two allocated
    // targets, linked left output, and paired output; idle and busy queues.
    for (unsigned cycle = 0; cycle < 100; ++cycle) for (unsigned stage = 0; stage < 7; ++stage) {
        bool c3d_ready = stage >= 1, c2d_ready = stage >= 2;
        Target first{stage >= 5}, second{stage >= 6};
        Target* left = stage >= 3 ? &first : nullptr;
        Target* right = stage >= 4 ? &second : nullptr;
        bool queue_pending = stage >= 5 && cycle % 2;
        unsigned detached = 0, freed_2d = 0, freed_3d = 0;
        std::vector<unsigned> order;
        const auto detach = [&](Target* target) {
            require(c3d_ready && target && target->linked, "Detached a dead or unlinked output");
            queue_pending = false; target->linked = false;
            ++detached; order.push_back(1);
        };
        const auto retire_2d = [&] {
            require(!queue_pending && c3d_ready && c2d_ready,
                "C2D freed borrowed buffers before GPU join or shader park lost its context");
            ++freed_2d; order.push_back(2);
        };
        const auto retire_3d = [&] {
            require(!queue_pending && !c2d_ready && c3d_ready,
                "C3D retired before C2D shader parking");
            ++freed_3d; order.push_back(3);
        };
        starfox::platform_3ds::retire_display_context(c3d_ready, c2d_ready,
            left, right, detach, retire_2d, retire_3d);
        require(!c3d_ready && !c2d_ready && !left && !right, "Retired display ownership survived close");
        require(detached == unsigned(stage >= 5) + unsigned(stage >= 6)
            && freed_2d == unsigned(stage >= 2) && freed_3d == unsigned(stage >= 1),
            "Partial construction freed an absent resource");
        require(std::is_sorted(order.begin(), order.end()), "Display shutdown order changed");
        const auto before = order;
        starfox::platform_3ds::retire_display_context(c3d_ready, c2d_ready,
            left, right, detach, retire_2d, retire_3d);
        require(order == before, "Repeated close freed resources twice");
    }
}
int main() try {
    display_queue_retirement();
    for(unsigned frame=0;frame<100;++frame) {
        Owner c2d(frame*2);
        C3D_BindProgram(&c2d.program);
        starfox_c2d_release_retired_program();
        starfox_c2d_retire_program(&c2d.program,&c2d.library);
        require(!c2d.library && !c2d.program.vertexShader,"C2D ownership was not transferred");
        require(live_instances.size()==1,"C2D retirement grew with each reopen");
        // Half the transitions reopen C2D directly. The other half bind the
        // source-effects renderer, which releases C2D only after binding.
        if(frame%2) {
            Owner native(frame*2+1);
            C3D_BindProgram(&native.program);
            starfox_c2d_release_retired_program();
            require(live_instances.size()==1,"Native rebind did not release old C2D shader");
            Owner replacement(frame*2+2);
            C3D_BindProgram(&replacement.program);
            native.free();
            starfox_c2d_retire_program(&replacement.program,&replacement.library);
        }
        // An allocation which never acquired a shader leaves the retired one alone.
        shaderProgram_s failed{};DVLB_s* missing{};
        starfox_c2d_retire_program(&failed,&missing);
        require(live_instances.size()==1 && live_libraries.size()==1,"Failed construction destroyed borrowed shader");
    }
    Owner final(999);
    C3D_BindProgram(&final.program);starfox_c2d_release_retired_program();
    sdk_borrowed=nullptr;final.free();
    require(live_instances.empty() && live_libraries.empty() && freed==201,"Retirement leaked shader resources");
    std::cout<<"C2D/PICA shader handoffs: reopen, alternating renderers, failed construction and bounded ownership passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
