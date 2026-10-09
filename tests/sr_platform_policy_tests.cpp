#include "starfox/render/renderer_backend.hpp"
#include <iostream>
#include <stdexcept>
#include <string_view>
using starfox::render::RendererBackend;
void require(bool value,const char* text) {if(!value) throw std::runtime_error(text);}
int main() try {
    unsigned checks{};
    for(unsigned output=0;output<10;++output) for(bool requested:{false,true}) for(bool connected:{false,true}) {
        const auto selection=starfox::render::displayxr_menu_selection(output,requested,connected);
        require(selection.stereo_output==(requested && connected && output==9U?0U:output),
            "Unavailable DisplayXR stranded the previous stereo output");++checks;
        require(selection.requested==(requested && connected),"Failed initial DisplayXR selection kept retrying");++checks;
        require(selection.unavailable==(requested && !connected),"DisplayXR failure label lost");++checks;
    }
    for(bool sr:{false,true}) for(bool intel:{false,true}) for(bool neural:{false,true}) {
        for(const auto backend:{RendererBackend::automatic,RendererBackend::vulkan,RendererBackend::direct3d12}) {
            const std::string_view actual=starfox::render::windows_gpu_driver_preference(backend,sr,intel,neural);
            const auto expected=backend==RendererBackend::vulkan?"vulkan":backend==RendererBackend::direct3d12
                || sr || intel || neural?"direct3d12":"vulkan";
            require(actual==expected,"SR AUTO must select D3D12 without overriding an explicit GPU backend");++checks;
        }
    }
    for(bool may_refresh:{false,true}) {
        starfox::render::SrPlatformBackendRequest policy;
        require(!policy.selected(),"SR default must remain off");++checks;
        for(unsigned frame=0;frame<100;++frame) {
            require(!policy.update(true,false,may_refresh) && policy.selected(),"Preview OFF performed SR backend work");++checks;
        }
        require(policy.update(true,true,may_refresh)==may_refresh,"Scene entry lost deferred backend selection");++checks;
        for(unsigned frame=0;frame<100;++frame) {
            require(!policy.update(true,true,may_refresh),"Unchanged SR selection rebuilt the renderer every frame");++checks;
        }
        require(!policy.update(false,false,may_refresh) && !policy.selected(),"Leaving SR rebuilt the plain menu");++checks;
        require(policy.update(false,true,may_refresh)==may_refresh,"Leaving SR did not restore AUTO preference for the next scene");++checks;
        require(!policy.update(false,true,may_refresh),"Leaving SR repeated backend refresh");++checks;
        require(!policy.update(true,false,may_refresh),"SR plain-menu selection must defer");++checks;
        policy.renderer_created();
        require(!policy.update(true,true,may_refresh),"Software-to-GPU creation triggered a second backend rebuild");++checks;
        require(policy.selected(),"Renderer creation lost selected SR output");++checks;
    }
    std::cout<<checks<<" SR backend/Preview-OFF/one-shot policy assertions passed; no physical panel claim\n";
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
