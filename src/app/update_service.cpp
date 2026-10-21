#include "starfox/app/update_service.hpp"
#include "update_build.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <random>
#include <stdexcept>
#include <sstream>
#include <vector>
#if defined(STARFOX_LINUX_UPDATES)
#include <sys/stat.h>
#include <unistd.h>
#endif
#if defined(STARFOX_WINDOWS_UPDATES)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace starfox::app {
namespace {
using Sink=std::function<void(const char*,std::size_t)>;
#if defined(STARFOX_WINDOWS_UPDATES)
struct HttpHandle {
    HINTERNET value{};
    ~HttpHandle() {if(value) WinHttpCloseHandle(value);}
    operator HINTERNET() const {return value;}
};
void fetch(std::string url,std::uint64_t limit,const Sink& sink,const std::atomic_bool& cancel) {
    HttpHandle session{WinHttpOpen(L"StarFoxEnhanced-Updater/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session) throw std::runtime_error("Cannot start HTTPS request");
    WinHttpSetTimeouts(session,10000,10000,10000,10000);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(limit>1024*1024?15:1);
    // Follow only HTTPS redirects to GitHub's actual release CDN. Never send
    // credentials, suppress certificate checks or downgrade to HTTP.
    for(int redirects=0;redirects<=5;++redirects) {
        if(cancel) throw std::runtime_error("Cancelled");
        const std::wstring wide(url.begin(),url.end());
        URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);
        parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=DWORD(-1);
        if(!WinHttpCrackUrl(wide.c_str(),DWORD(wide.size()),0,&parts) || parts.nScheme!=INTERNET_SCHEME_HTTPS
            || parts.nPort!=443) throw std::runtime_error("Unsafe update URL");
        const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
        if(host!=L"api.github.com" && host!=L"github.com" && host!=L"release-assets.githubusercontent.com")
            throw std::runtime_error("Unexpected update server");
        const auto path=std::wstring(parts.lpszUrlPath,parts.dwUrlPathLength)
            + std::wstring(parts.lpszExtraInfo?parts.lpszExtraInfo:L"",parts.dwExtraInfoLength);
        HttpHandle connection{WinHttpConnect(session,host.c_str(),443,0)};
        HttpHandle request{connection?WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,
            WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr};
        DWORD no_redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if(!request || !WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&no_redirect,sizeof(no_redirect))
            || !WinHttpSendRequest(request,L"Accept: application/vnd.github+json\r\n",DWORD(-1),nullptr,0,0,0)
            || !WinHttpReceiveResponse(request,nullptr)) throw std::runtime_error("HTTPS request failed - check connection");
        DWORD code{},length=sizeof(code);
        if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
            nullptr,&code,&length,nullptr)) throw std::runtime_error("Missing HTTP status");
        if(code==301 || code==302 || code==303 || code==307 || code==308) {
            std::array<wchar_t,8192> location{};length=DWORD(location.size()*sizeof(wchar_t));
            if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_LOCATION,nullptr,location.data(),&length,nullptr))
                throw std::runtime_error("Invalid update redirect");
            const std::wstring next(location.data());
            if(std::any_of(next.begin(),next.end(),[](wchar_t c){return c<33 || c>126;})) throw std::runtime_error("Invalid redirect URL");
            url.assign(next.begin(),next.end());continue;
        }
        if(code!=200) throw std::runtime_error(code==403 || code==429?"GitHub rate limit - try later":
            code==404?"No published release found":"Update server returned an error");
        std::uint64_t total=0;std::array<char,65536> buffer{};
        while(true) {
            if(cancel) throw std::runtime_error("Cancelled");
            if(std::chrono::steady_clock::now()>deadline) throw std::runtime_error("Update request timed out");
            DWORD read{};
            if(!WinHttpReadData(request,buffer.data(),DWORD(buffer.size()),&read)) throw std::runtime_error("Download interrupted");
            if(!read) return;
            total+=read;if(total>limit) throw std::runtime_error("Download exceeded expected size");
            sink(buffer.data(),read);
        }
    }
    throw std::runtime_error("Too many update redirects");
}
std::string powershell_path() {
    std::array<wchar_t,32768> system{};
    const auto count=GetSystemDirectoryW(system.data(),UINT(system.size()));
    if(!count || count>=system.size()) throw std::runtime_error("Cannot locate Windows PowerShell");
    const auto path=std::filesystem::path(std::wstring(system.data(),count))/L"WindowsPowerShell/v1.0/powershell.exe";
    const auto utf8=path.u8string();return {utf8.begin(),utf8.end()};
}
std::string path_utf8(const std::filesystem::path& path) {
    const auto value=path.u8string();return {value.begin(),value.end()};
}
bool helper(const std::filesystem::path& work,const std::filesystem::path& target,
    const UpdateRelease& release,bool apply,const std::atomic_bool& cancel) {
    std::vector<std::string> args{powershell_path(),"-NoLogo","-NoProfile","-NonInteractive",
        "-WindowStyle","Hidden","-ExecutionPolicy","RemoteSigned","-File",path_utf8(work/"apply-update.ps1"),
        "-Mode",apply?"Apply":"Prepare","-Work",path_utf8(work),"-Target",path_utf8(target),
        "-Digest",release.sha256,"-Bytes",std::to_string(release.bytes)};
    if(apply) {
        FILETIME creation{},exit{},kernel{},user{};
        if(!GetProcessTimes(GetCurrentProcess(),&creation,&exit,&kernel,&user)) return false;
        const auto ticks=((std::uint64_t(creation.dwHighDateTime)<<32)|creation.dwLowDateTime)+504911232000000000ULL;
        args.insert(args.end(),{"-ParentId",std::to_string(GetCurrentProcessId()),"-ParentStartTicks",std::to_string(ticks)});
    }
    std::vector<const char*> pointers;for(const auto& arg:args) pointers.push_back(arg.c_str());pointers.push_back(nullptr);
    const auto props=SDL_CreateProperties();
    SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,pointers.data());
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,SDL_PROCESS_STDIO_NULL);
    auto* process=SDL_CreateProcessWithProperties(props);SDL_DestroyProperties(props);
    if(!process) return false;
    int code{};bool success=false;
    if(apply) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(std::chrono::steady_clock::now()<deadline) {
            if(std::filesystem::exists(work/"handoff-ready")) {success=true;break;}
            if(SDL_WaitProcess(process,false,&code)) break;
            SDL_Delay(25);
        }
        if(!success) {SDL_KillProcess(process,true);SDL_WaitProcess(process,true,&code);}
    } else {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);
        while(!SDL_WaitProcess(process,false,&code)) {
            if(cancel || std::chrono::steady_clock::now()>deadline) {
                SDL_KillProcess(process,true);SDL_WaitProcess(process,true,&code);code=1;break;
            }
            SDL_Delay(25);
        }
        success=code==0;
    }
    SDL_DestroyProcess(process);return success;
}
#endif

#if defined(STARFOX_LINUX_UPDATES)
std::string process_start() {
    std::ifstream input("/proc/self/stat");std::string line;std::getline(input,line);
    const auto end=line.rfind(')');
    if(end==std::string::npos) throw std::runtime_error("Cannot identify updater parent");
    std::istringstream fields(line.substr(end+1));std::string value;
    for(int field=3;field<=22;++field) if(!(fields>>value)) throw std::runtime_error("Invalid process identity");
    return value;
}
bool linux_helper(const std::filesystem::path& work,const std::filesystem::path& target,
    const UpdateRelease& release,bool apply,const std::atomic_bool& cancel,
    const std::function<void(unsigned)>& progress={}) {
    std::vector<std::string> args{"/usr/bin/python3","-I",(work/"apply-update.py").string(),apply?"apply":"prepare",
        work.string(),target.string(),release.sha256,std::to_string(release.bytes)};
    if(apply) args.insert(args.end(),{"--parent-id",std::to_string(getpid()),"--parent-start",process_start()});
    else args.insert(args.end(),{"--url",release.asset_url});
    std::vector<const char*> pointers;for(const auto& arg:args) pointers.push_back(arg.c_str());pointers.push_back(nullptr);
    const auto props=SDL_CreateProperties();
    SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,pointers.data());
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,SDL_PROCESS_STDIO_NULL);
    auto* process=SDL_CreateProcessWithProperties(props);SDL_DestroyProperties(props);
    if(!process) return false;
    int code{};bool success=false;
    const auto deadline=std::chrono::steady_clock::now()+(apply?std::chrono::seconds(10):std::chrono::seconds(20*60));
    while(std::chrono::steady_clock::now()<deadline && !cancel) {
        if(apply && std::filesystem::exists(work/"handoff-ready")) {success=true;break;}
        if(SDL_WaitProcess(process,false,&code)) {success=!apply && code==0;break;}
        if(!apply && progress) {unsigned percent{};std::ifstream value(work/"progress");if(value>>percent) progress(std::min(100U,percent));}
        SDL_Delay(50);
    }
    if(!success) {SDL_KillProcess(process,true);SDL_WaitProcess(process,true,&code);}
    SDL_DestroyProcess(process);return success;
}
#endif

std::string metadata(const std::atomic_bool& cancel) {
    std::string result;
#if defined(STARFOX_WINDOWS_UPDATES)
    fetch(std::string(update_api),1024*1024,[&](const char* data,std::size_t n){result.append(data,n);},cancel);
#elif defined(STARFOX_CURL_UPDATE_CHECK)
    // curl is an OS tool on macOS and common Linux distributions. Fixed
    // absolute path + argument array: no shell, PATH search or interpolation.
    const char* args[]{"/usr/bin/curl","--silent","--show-error","--fail","--proto","=https",
        "--connect-timeout","10","--max-time","30","--max-filesize","1048576",
        "--user-agent","StarFoxEnhanced-Updater/1","--header","Accept: application/vnd.github+json",
        "https://api.github.com/repos/kandowontu2/starfox-enhanced/releases/latest",nullptr};
    auto* process=SDL_CreateProcess(args,true);
    if(!process) throw std::runtime_error("Update check needs /usr/bin/curl");
    auto* output=SDL_GetProcessOutput(process);
    std::array<char,4096> buffer{};bool oversized=false;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(35);
    while(output) {
        if(cancel || std::chrono::steady_clock::now()>deadline) {oversized=true;SDL_KillProcess(process,true);break;}
        const auto n=SDL_ReadIO(output,buffer.data(),buffer.size());
        if(!n) {
            if(SDL_GetIOStatus(output)==SDL_IO_STATUS_NOT_READY) {SDL_Delay(10);continue;}
            break;
        }
        if(cancel || result.size()+n>1024*1024) {oversized=true;SDL_KillProcess(process,true);break;}
        result.append(buffer.data(),n);
    }
    int code{};const bool success=SDL_WaitProcess(process,true,&code);SDL_DestroyProcess(process);
    if(oversized || !success || code!=0 || cancel) throw std::runtime_error("Update check failed - check connection or try later");
#else
    (void)cancel;
    throw std::runtime_error("Use RELEASE PAGE on this platform");
#endif
    return result;
}
std::string platform_package() {
#if defined(STARFOX_WINDOWS_UPDATES) && (defined(_M_X64) || defined(__x86_64__))
    return "windows-x64.zip";
#elif defined(STARFOX_WINDOWS_UPDATES) && (defined(_M_IX86) || defined(__i386__))
    return "windows-x86.zip";
#elif defined(STARFOX_LINUX_UPDATES) && defined(__x86_64__)
    return "linux-x64.zip";
#else
    return {}; // Never select an asset for an unsupported CPU/frontend.
#endif
}
bool busy(UpdatePhase phase) {return phase==UpdatePhase::checking || phase==UpdatePhase::downloading;}
}
UpdateService::UpdateService(std::filesystem::path directory):directory_(std::move(directory)) {}
UpdateService::~UpdateService() {cancel_=true;if(worker_.joinable()) worker_.join();cleanup_work();}
const char* UpdateService::installed_version() {return starfox_update_version;}
bool UpdateService::check_supported() {
#if defined(STARFOX_WINDOWS_UPDATES) || defined(STARFOX_CURL_UPDATE_CHECK)
    return true;
#else
    return false;
#endif
}
bool UpdateService::install_supported() const {
    return install_available_.load();
}
bool UpdateService::determine_install_support() const {
#if defined(STARFOX_WINDOWS_UPDATES) || defined(STARFOX_LINUX_UPDATES)
    if(platform_package().empty()) return false;
    std::error_code ec;
    if(!std::filesystem::is_regular_file(directory_/
#if defined(STARFOX_WINDOWS_UPDATES)
        "starfox_pc.exe"
#else
        "starfox_pc"
#endif
        ,ec)) return false;
    for(auto path=directory_;!path.empty();) {
        if(std::filesystem::exists(path/".git",ec) || ec) return false;
        const auto parent=path.parent_path();if(parent==path) break;path=parent;
    }
#if defined(STARFOX_LINUX_UPDATES)
    if(!std::filesystem::is_regular_file("/usr/bin/python3",ec) || access("/usr/bin/python3",X_OK)!=0
        || geteuid()==0 || access(directory_.c_str(),W_OK)!=0 || std::getenv("APPIMAGE")
        || std::getenv("FLATPAK_ID") || std::getenv("SNAP")) return false;
    for(const auto* root:{"/usr","/opt","/app","/snap","/nix","/var/lib/flatpak"}) {
        const auto path=directory_.lexically_normal().string();
        if(path==root || path.starts_with(std::string(root)+"/")) return false;
    }
    struct stat owner{};if(stat(directory_.c_str(),&owner)!=0 || owner.st_uid!=geteuid()) return false;
#endif
    return true;
#else
    return false;
#endif
}
UpdateStatus UpdateService::status() const {std::scoped_lock lock(mutex_);return status_;}
void UpdateService::set_status(UpdatePhase phase,std::string message) {
    std::scoped_lock lock(mutex_);status_.phase=phase;status_.message=std::move(message);
}
void UpdateService::cleanup_work() {
    // Only our freshly-created unique directory; never sweep other sessions,
    // cached backups, user files or a failed detached installation's evidence.
    if(!work_.empty()) {std::error_code ec;std::filesystem::remove_all(work_,ec);work_.clear();}
}
void UpdateService::cancel() {cancel_=true;}
void UpdateService::check() {
    if(busy(status().phase)) return;
    if(worker_.joinable()) worker_.join();
    cleanup_work();cancel_=false;set_status(UpdatePhase::checking,"CHECKING...");
    worker_=std::thread([this] {
        try {
            // SDL/proc paths may traverse a legitimate home-directory link.
            // Resolve the install root once before enforcing no-link writes.
            directory_=std::filesystem::canonical(directory_);
            install_available_=determine_install_support();
            const auto release=parse_update_release(metadata(cancel_),installed_version(),platform_package());
#if defined(STARFOX_LINUX_UPDATES)
            bool last_install_failed=false;
            if(char* pref=SDL_GetPrefPath("StarFoxEnhanced","Updates")) {
                const auto receipt=std::filesystem::path(pref)/"last-install-error.txt";SDL_free(pref);
                std::error_code ec;last_install_failed=std::filesystem::is_regular_file(receipt,ec);
                if(last_install_failed) std::filesystem::remove(receipt,ec);
            }
#endif
            std::scoped_lock lock(mutex_);status_.release=release;
            status_.phase=release.newer?UpdatePhase::available:UpdatePhase::current;
            status_.message=release.newer?"NEW VERSION AVAILABLE":
                *parse_update_version(installed_version())>*parse_update_version(release.version)?"LOCAL BUILD IS NEWER":"YOU ARE UP TO DATE";
#if defined(STARFOX_LINUX_UPDATES)
            if(last_install_failed) {status_.phase=UpdatePhase::failed;status_.message="LAST INSTALL FAILED - TRY AGAIN";}
#endif
        } catch(const std::exception& e) {set_status(UpdatePhase::failed,e.what());}
    });
}
void UpdateService::download() {
    const auto state=status();
    if(!install_supported() || !state.release.newer || state.release.asset_url.empty() || busy(state.phase) || state.phase==UpdatePhase::ready) return;
    if(worker_.joinable()) worker_.join();
    cleanup_work();cancel_=false;
    {std::scoped_lock lock(mutex_);status_.percent=0;}
    set_status(UpdatePhase::downloading,"DOWNLOADING...");
    worker_=std::thread([this,release=state.release] {
        try {
#if defined(STARFOX_WINDOWS_UPDATES) || defined(STARFOX_LINUX_UPDATES)
            char* pref=SDL_GetPrefPath("StarFoxEnhanced","Updates");
            if(!pref) throw std::runtime_error("Cannot create update cache");
            const std::filesystem::path preference(reinterpret_cast<const char8_t*>(pref));SDL_free(pref);
            const auto root=std::filesystem::canonical(preference);
            if(std::filesystem::space(root).available<release.bytes*4+256*1024*1024)
                throw std::runtime_error("Not enough disk space for a safe update");
            std::random_device random;
            for(int i=0;i<16;++i) {
                const auto candidate=root/("sfe-update-"+std::to_string(random())+std::to_string(random()));
                if(std::filesystem::create_directory(candidate)) {work_=candidate;break;}
            }
            if(work_.empty()) throw std::runtime_error("Cannot create unique update workspace");
#if defined(STARFOX_WINDOWS_UPDATES)
            // Check permissions before spending bandwidth. CREATE_NEW and
            // DELETE_ON_CLOSE guarantee this probe cannot overwrite a file.
            const auto probe=directory_/(work_.filename().wstring()+L".probe");
            const auto writable=CreateFileW(probe.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
                FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr);
            if(writable==INVALID_HANDLE_VALUE) throw std::runtime_error("Install folder is not writable - use release page");
            CloseHandle(writable);
            {
                std::ofstream script(work_/"apply-update.ps1",std::ios::binary);
                script<<starfox_update_script;if(!script) throw std::runtime_error("Cannot write update helper");
            }
            std::uint64_t total=0;
            {
                std::ofstream file(work_/"payload.zip",std::ios::binary);
                fetch(release.asset_url,release.bytes,[&](const char* data,std::size_t n) {
                    file.write(data,std::streamsize(n));if(!file) throw std::runtime_error("Cannot save update download");
                    total+=n;std::scoped_lock lock(mutex_);status_.percent=unsigned(total*100/release.bytes);
                },cancel_);
                file.close();if(!file || total!=release.bytes) throw std::runtime_error("Incomplete update download");
            }
            set_status(UpdatePhase::downloading,"VERIFYING PACKAGE...");
            if(cancel_) throw std::runtime_error("Cancelled");
            if(!helper(work_,directory_,release,false,cancel_)) throw std::runtime_error("Package verification failed - use release page");
            if(cancel_) throw std::runtime_error("Cancelled");
            set_status(UpdatePhase::ready,"READY - RESTART TO INSTALL");
#else
            std::filesystem::permissions(work_,std::filesystem::perms::owner_all);
            {
                std::ofstream script(work_/"apply-update.py",std::ios::binary);
                script<<starfox_linux_update_script;if(!script) throw std::runtime_error("Cannot write update helper");
            }
            if(!linux_helper(work_,directory_,release,false,cancel_,[this](unsigned percent) {
                std::scoped_lock lock(mutex_);status_.percent=percent;
                if(percent==100) status_.message="VERIFYING PACKAGE...";
            })) throw std::runtime_error("Update download or verification failed - use release page");
            if(cancel_) throw std::runtime_error("Cancelled");
            set_status(UpdatePhase::ready,"READY - RESTART TO INSTALL");
#endif
#else
            (void)release;throw std::runtime_error("Use your platform installer");
#endif
        } catch(const std::exception& e) {cleanup_work();set_status(UpdatePhase::failed,e.what());}
    });
}
bool UpdateService::restart_to_install() {
#if defined(STARFOX_WINDOWS_UPDATES) || defined(STARFOX_LINUX_UPDATES)
    const auto state=status();if(state.phase!=UpdatePhase::ready || work_.empty()) return false;
    if(worker_.joinable()) worker_.join();
    const bool started=
#if defined(STARFOX_WINDOWS_UPDATES)
        helper(work_,directory_,state.release,true,cancel_);
#else
        linux_helper(work_,directory_,state.release,true,cancel_);
#endif
    if(!started) {set_status(UpdatePhase::failed,"Cannot start update installer");return false;}
    work_.clear();return true; // Detached helper owns this directory now.
#else
    return false;
#endif
}
bool UpdateService::open_release_page() const {
    const auto release=status().release;
    const auto url=release.page.empty()?std::string(update_repository)+"/releases/latest":release.page;
    return SDL_OpenURL(url.c_str());
}
} // namespace starfox::app
