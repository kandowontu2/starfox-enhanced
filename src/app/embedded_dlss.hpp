#pragma once

#if defined(STARFOX_EMBEDDED_DLSS) && defined(_WIN32) && !defined(STARFOX_UWP)
#include "embedded_dlss_manifest.hpp"
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <cwctype>

namespace starfox::app {
// Windows cannot load a DLL directly from RCDATA. Restore the exact embedded
// bytes into a versioned per-user cache, never the exe folder/system directories.
// Every file is compared with its resource before any adapter code is loaded.
inline std::filesystem::path embedded_dlss_directory() {
    using namespace embedded_dlss;
    const auto fail=[] {throw std::runtime_error("Cannot prepare embedded DLSS runtime cache");};
    PWSTR local{};
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local))) fail();
    const std::filesystem::path base{local};CoTaskMemFree(local);
    const auto parent=base/L"StarFoxEnhanced"/L"dlss";
    const auto directory=parent/package_id;
    // Inspect each parent before creating its child: never follow a substituted
    // product/cache directory while preparing the private runtime.
    for(const auto& path:{base/L"StarFoxEnhanced",parent,directory}) {
        if(!CreateDirectoryW(path.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS) fail();
        const auto attributes=GetFileAttributesW(path.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY) ||
            (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) fail();
    }
    struct Lock {
        HANDLE handle{};bool owned{};
        ~Lock(){if(owned) ReleaseMutex(handle);if(handle) CloseHandle(handle);}
    } lock;
    const auto name=L"Local\\StarFoxEnhanced.DlssCache."+std::wstring(package_id,package_id+64);
    lock.handle=CreateMutexW(nullptr,FALSE,name.c_str());if(!lock.handle) fail();
    const auto waited=WaitForSingleObject(lock.handle,5000);
    if(waited!=WAIT_OBJECT_0 && waited!=WAIT_ABANDONED) fail();lock.owned=true;
    for(const auto& file:files) {
        const auto module=GetModuleHandleW(nullptr);
        const auto resource=FindResourceW(module,MAKEINTRESOURCEW(file.id),MAKEINTRESOURCEW(10));
        const auto loaded=resource?LoadResource(module,resource):nullptr;
        const auto size=resource?SizeofResource(module,resource):0;
        const auto* bytes=loaded?static_cast<const unsigned char*>(LockResource(loaded)):nullptr;
        if(!bytes || !size) fail();
        const auto path=directory/file.name;
        const auto identical=[&] {
            const auto attributes=GetFileAttributesW(path.c_str());
            if(attributes==INVALID_FILE_ATTRIBUTES) return false;
            if(attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)) fail();
            std::ifstream input{path,std::ios::binary|std::ios::ate};
            if(!input || input.tellg()!=std::streamoff(size)) return false;
            input.seekg(0);std::array<char,65536> block{};
            for(std::size_t offset=0;offset<size;) {
                const auto count=std::min<std::size_t>(block.size(),size-offset);
                if(!input.read(block.data(),std::streamsize(count)) ||
                    !std::equal(block.begin(),block.begin()+count,bytes+offset,
                        [](char a,unsigned char b){return static_cast<unsigned char>(a)==b;})) return false;
                offset+=count;
            }
            return true;
        };
        if(identical()) continue;
        GUID temporary_id{};if(FAILED(CoCreateGuid(&temporary_id))) fail();
        wchar_t temporary_suffix[40]{};
        if(!StringFromGUID2(temporary_id,temporary_suffix,40)) fail();
        const auto temporary=directory/(std::filesystem::path(file.name).wstring()+L".pending-"+temporary_suffix);
        struct Pending {
            std::filesystem::path path;HANDLE handle{INVALID_HANDLE_VALUE};bool created{};
            ~Pending(){if(handle!=INVALID_HANDLE_VALUE) CloseHandle(handle);if(created) DeleteFileW(path.c_str());}
        } pending{temporary};
        pending.handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY,nullptr);
        if(pending.handle==INVALID_HANDLE_VALUE) fail();pending.created=true;
        DWORD written{};
        if(!WriteFile(pending.handle,bytes,size,&written,nullptr) || written!=size ||
            !FlushFileBuffers(pending.handle)) fail();
        CloseHandle(pending.handle);pending.handle=INVALID_HANDLE_VALUE;
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) fail();
        pending.created=false;
        if(!identical()) fail();
    }
    // Plugin discovery must not see unrecognized DLLs in the cache directory.
    for(const auto& entry:std::filesystem::directory_iterator(directory)) {
        auto extension=entry.path().extension().wstring();
        std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t c){return std::towlower(c);});
        if(extension==L".dll" && std::none_of(files.begin(),files.end(),
            [&](const auto& file){return entry.path().filename()==file.name;})) fail();
    }
    return directory;
}
}
#endif
