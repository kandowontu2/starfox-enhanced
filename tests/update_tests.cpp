#include "starfox/app/update_release.hpp"
#include "starfox/app/update_service.hpp"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) {
    if(!condition) {std::cerr<<"FAILED: "<<message<<'\n';std::exit(1);}
}
std::string fixture(std::string version="0.0.8.9",std::string suffix="windows-x64.zip") {
    const auto repo=std::string(starfox::app::update_repository);
    const auto tag="v"+version;
    const auto name="StarFoxEnhanced-"+version+"-"+suffix;
    return "{\"tag_name\":\""+tag+"\",\"draft\":false,\"prerelease\":false,\"html_url\":\""+repo+
        "/releases/tag/"+tag+"\",\"body\":\"Untrusted notes: tag_name v99.0.0 \\\"name\\\"\",\"assets\":[{\"name\":\""+name+
        "\",\"state\":\"uploaded\",\"size\":1234,\"digest\":\"sha256:"+std::string(64,'a')+
        "\",\"browser_download_url\":\""+repo+"/releases/download/"+tag+"/"+name+"\"}]}";
}
std::string replace(std::string text,const std::string& a,const std::string& b) {
    const auto pos=text.find(a);require(pos!=std::string::npos,"test replacement missing");
    text.replace(pos,a.size(),b);return text;
}
void rejects(const std::string& json) {
    try {starfox::app::parse_update_release(json,"0.0.8.8","windows-x64.zip");}
    catch(const std::runtime_error&) {return;}
    require(false,"unsafe/malformed metadata accepted");
}
}
int main(int argc,char** argv) {
    using namespace starfox::app;
    if(argc==5 && std::string_view(argv[1])=="--release-check") {
        std::ifstream file{argv[2]};
        require(bool(file),"release metadata fixture missing");
        const std::string json{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
        const auto release=parse_update_release(json,argv[3],argv[4]);
        require(release.newer && !release.asset_url.empty() && release.sha256.size()==64,
            "published release is not an installable update for this client");
        std::cout<<"Current: "<<argv[3]<<" Latest: "<<release.version<<" Package: "<<release.asset_url<<'\n';
        return 0;
    }
    if(argc==2 && std::string_view(argv[1])=="--live-check") {
        UpdateService updater{std::filesystem::current_path()};updater.check();
        UpdateStatus state;
        for(unsigned i=0;i<700;++i) {state=updater.status();if(state.phase!=UpdatePhase::checking) break;SDL_Delay(100);}
        std::cout<<"Current: "<<updater.installed_version()<<" Latest: "<<state.release.version<<" Status: "<<state.message<<'\n';
        return state.phase==UpdatePhase::current || state.phase==UpdatePhase::available?0:1;
    }
    require(parse_update_version("v0.0.8.10")>parse_update_version("0.0.8.9"),"lexicographic version comparison");
    require(parse_update_version("0.0.8")==parse_update_version("v0.0.8.0"),"three-part version normalization");
    require(parse_update_version("0.0.8.8")==parse_update_version("v0.0.8.8.0"),"hotfix zero normalization");
    require(parse_update_version("0.0.8.8.1")>parse_update_version("0.0.8.8"),"hotfix must be newer than its base");
    require(parse_update_version("0.0.8.9")>parse_update_version("0.0.8.8.1"),"next release must supersede hotfix");
    require(parse_update_version("0.0.8.8.10")>parse_update_version("0.0.8.8.9"),"numeric hotfix comparison");
    for(const auto* text:{"","v","1.2","1.2.3.","1.2.3.4.5.6","1.2.3.4.","1.2.3.4.01","1.2.3.4.4294967296","1.2.3-beta","01.2.3","1.-2.3","1.2.3/evil","4294967296.2.3"})
        require(!parse_update_version(text),"invalid version accepted");
    const auto newer=parse_update_release(fixture(),"0.0.8.8","windows-x64.zip");
    require(newer.newer && newer.version=="0.0.8.9" && newer.bytes==1234 && newer.sha256==std::string(64,'a'),"update asset selection");
    const auto addon_only=replace(fixture(),"0.0.8.9-windows-x64.zip","0.0.8.9-dlss-windows-x64.zip");
    require(parse_update_release(addon_only,"0.0.8.8","windows-x64.zip").asset_url.empty(),
        "DLSS add-on must never be selected as the game update");
    require(!parse_update_release(fixture("0.0.8.8"),"0.0.8.8","windows-x64.zip").newer,"same version offered");
    require(!parse_update_release(fixture("0.0.8.7"),"0.0.8.8","windows-x64.zip").newer,"downgrade offered");
    require(parse_update_release(fixture("0.0.8.10"),"0.0.8.9","windows-x64.zip").newer,"four-part versions misordered");
    const auto hotfix=parse_update_release(fixture("0.0.8.8.1"),"0.0.8.8","windows-x64.zip");
    require(hotfix.newer && hotfix.version=="0.0.8.8.1" && hotfix.asset_url.ends_with("StarFoxEnhanced-0.0.8.8.1-windows-x64.zip"),"hotfix version/asset truncated");
    require(!parse_update_release(fixture("0.0.8.8.1"),"0.0.8.8.1","windows-x64.zip").newer,"same hotfix offered");
    for(const auto* suffix:{"windows-x64.zip","windows-x86.zip","linux-x64.zip"}) {
        const auto update=parse_update_release(fixture("0.0.8.8.2",suffix),"0.0.8.8.1",suffix);
        require(update.newer && update.version=="0.0.8.8.2"
                && update.asset_url.ends_with(std::string{"StarFoxEnhanced-0.0.8.8.2-"}+suffix),
            "0.0.8.8.1 client cannot select the 0.0.8.8.2 platform update");
        require(!parse_update_release(fixture("0.0.8.8.2",suffix),"0.0.8.8.2",suffix).newer,
            "0.0.8.8.2 offers itself as an update");
        const auto next=parse_update_release(fixture("0.0.8.8.3",suffix),"0.0.8.8.2",suffix);
        require(next.newer && next.version=="0.0.8.8.3"
                && next.asset_url.ends_with(std::string{"StarFoxEnhanced-0.0.8.8.3-"}+suffix),
            "0.0.8.8.2 client cannot select the 0.0.8.8.3 platform update");
        require(!parse_update_release(fixture("0.0.8.8.3",suffix),"0.0.8.8.3",suffix).newer,
            "0.0.8.8.3 offers itself as an update");
    }
    require(!parse_update_release(fixture("0.0.8.8"),"0.0.8.8.1","windows-x64.zip").newer,"hotfix downgrade offered");
    require(parse_update_release(fixture("0.0.8.9"),"0.0.8.8.1","windows-x64.zip").newer,"hotfix blocks next release");
    for(const auto* suffix:{"windows-x86.zip","windows-pcvr.zip","android-arm64.apk",""})
        require(parse_update_release(fixture(),"0.0.8.8",suffix).asset_url.empty(),"wrong platform selected");
    require(parse_update_release(replace(fixture(),"\"digest\":\"sha256:"+std::string(64,'a')+"\"","\"digest\":null"),
        "0.0.8.8","windows-x64.zip").asset_url.empty(),"digest-less asset allowed automatic installation");
    rejects(replace(fixture(),"\"draft\":false","\"draft\":true"));
    rejects(replace(fixture(),"\"prerelease\":false","\"prerelease\":true"));
    rejects(replace(fixture(),"\"draft\":false","\"draft\":false,\"draft\":false"));
    rejects(replace(fixture(),"https://github.com/","https://evil.example/"));
    rejects(replace(fixture(),"/releases/download/","/releases/download/../"));
    rejects(replace(fixture(),"\"size\":1234","\"size\":0"));
    rejects(replace(fixture(),"\"size\":1234","\"size\":4294967297"));
    rejects(replace(fixture(),"\"size\":1234","\"size\":1.5"));
    rejects(replace(fixture(),"\"size\":1234","\"size\":\"1234\""));
    rejects(replace(fixture(),"sha256:","sha512:"));
    rejects(replace(fixture(),"\"state\":\"uploaded\"","\"state\":\"new\""));
    rejects(replace(fixture(),"\"body\":\"","\"body\":\"\\uD800"));
    rejects("[]");rejects("null");rejects("{");rejects(fixture()+"false");
    rejects(std::string(1024*1024+1,' '));
    auto escaped=replace(fixture(),"tag_name","tag\\u005fname");
    require(parse_update_release(escaped,"0.0.8.8","windows-x64.zip").newer,"JSON escape handling");
    {
        UpdateService idle{std::filesystem::current_path()};
        require(idle.status().phase==UpdatePhase::idle,"network work started without request");
        require(!idle.restart_to_install(),"restart allowed before verified update");
        require(!idle.install_supported(),"source checkout allows in-place update");
        require(parse_update_version(idle.installed_version()).has_value(),"installed version is not from build");
    }
    std::cout<<"Update version, release policy and idle service tests passed\n";
}
