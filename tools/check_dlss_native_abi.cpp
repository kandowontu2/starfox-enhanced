// Build with the game's MinGW compiler; use the separately built MSVC adapter.
#include "starfox/render/dlss_native.h"
#include <windows.h>
#include <filesystem>
#include <cstring>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    const auto path=std::filesystem::canonical(argv[1]);
    auto module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module) return 3;
    auto evaluate=reinterpret_cast<decltype(&starfox_dlss_evaluate_v1)>(GetProcAddress(module,"starfox_dlss_evaluate_v1"));
    auto configure=reinterpret_cast<decltype(&starfox_dlss_configure_v1)>(GetProcAddress(module,"starfox_dlss_configure_v1"));
    bool ok=evaluate && configure;
    if(ok) {
        StarfoxDlssFrameV1 frame{};frame.size=sizeof(frame);char error[128]{};
        ok=evaluate(nullptr,&frame,error,sizeof(error))!=0 && std::strcmp(error,"Invalid DLSS frame")==0;
        frame.size=0;
        ok=ok && evaluate(nullptr,&frame,error,sizeof(error))!=0 && std::strcmp(error,"DLSS frame ABI mismatch")==0;
        uint32_t w=17,h=19;
        ok=ok && configure(nullptr,0,0,1280,720,&w,&h,error,sizeof(error))!=0 && w==17 && h==19;
        char bounded[2]{'X','Y'};
        ok=ok && evaluate(nullptr,nullptr,bounded,1)!=0 && bounded[0]==0 && bounded[1]=='Y';
    }
    FreeLibrary(module);
    std::cout<<(ok?"PASS: MinGW caller / MSVC DLSS adapter ABI, rejection and bounded errors\n":"FAIL: DLSS ABI\n");
    return ok?0:1;
}
