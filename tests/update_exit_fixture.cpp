// Tiny, asset-free stand-in for a restarted game. No SDL or embedded assets.
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int main(int argc,char**) {
    if(argc>1) {Sleep(30000);return 0;}
    wchar_t path[32768]{};
    const auto size=GetModuleFileNameW(nullptr,path,32768);
    if(!size || size>=32768) return 1;
    wchar_t* last=path;
    for(auto* p=path;*p;++p) if(*p==L'\\') last=p+1;
    const wchar_t name[]=L"update-started.txt";
    if(last-path+sizeof(name)/sizeof(wchar_t)>32768) return 2;
    for(unsigned i=0;i<sizeof(name)/sizeof(wchar_t);++i) last[i]=name[i];
    const auto file=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return 3;
    DWORD written{};const char text[]="Update restart fixture ran\n";
    const auto ok=WriteFile(file,text,sizeof(text)-1,&written,nullptr);CloseHandle(file);
    return ok?0:4;
}
#else
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
int main(int argc,char**) {
    if(argc>1) {sleep(30);return 0;}
    char path[4096]{};
    const auto size=readlink("/proc/self/exe",path,sizeof(path)-1);
    if(size<=0 || size>=sizeof(path)-1) return 1;
    auto* last=std::strrchr(path,'/');if(!last) return 2;
    if(last-path+sizeof("/update-started.txt")>sizeof(path)) return 3;
    std::strcpy(last,"/update-started.txt");
    const auto file=open(path,O_WRONLY|O_CREAT|O_TRUNC,0600);if(file<0) return 4;
    const char text[]="Update restart fixture ran\n";
    const auto written=write(file,text,sizeof(text)-1);close(file);
    return written==sizeof(text)-1?0:5;
}
#endif
