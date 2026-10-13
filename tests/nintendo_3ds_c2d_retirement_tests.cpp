#include <iostream>
#include <set>
#include <stdexcept>

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
int main() try {
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
