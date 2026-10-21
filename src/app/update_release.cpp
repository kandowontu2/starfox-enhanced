#include "starfox/app/update_release.hpp"
#include <charconv>
#include <map>
#include <stdexcept>
#include <vector>

namespace starfox::app {
std::optional<UpdateVersion> parse_update_version(std::string_view text) {
    if (text.starts_with('v')) text.remove_prefix(1);
    UpdateVersion result{};
    unsigned count = 0;
    while (!text.empty() && count < result.size()) {
        const auto end = text.find('.');
        const auto part = text.substr(0, end);
        if (part.empty() || (part.size() > 1 && part.front() == '0')) return {};
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), result[count++]);
        if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size()) return {};
        if (end == std::string_view::npos) return count >= 3 ? std::optional{result} : std::nullopt;
        text.remove_prefix(end + 1);
    }
    return {};
}
namespace {
// Small, bounded JSON reader, not a text/regex search: release bodies and
// nested author objects must never masquerade as authoritative asset fields.
struct Json {
    enum Kind { null, boolean, number, string, object, array } kind = null;
    std::string value;
    std::map<std::string, Json> members;
    std::vector<Json> items;
    const Json& at(const char* key) const {
        if (kind != object || !members.contains(key)) throw std::runtime_error("Incomplete release metadata");
        return members.at(key);
    }
    std::string str() const {
        if (kind != string) throw std::runtime_error("Invalid release text field");
        return value;
    }
    bool flag() const {
        if (kind != boolean) throw std::runtime_error("Invalid release flag");
        return value == "true";
    }
};
class Reader {
    std::string_view input;
    std::size_t pos{};
    std::size_t nodes{};
    [[noreturn]] void fail() const { throw std::runtime_error("Invalid release JSON"); }
    void ws() { while (pos < input.size() && (input[pos]==' ' || input[pos]=='\n' || input[pos]=='\r' || input[pos]=='\t')) ++pos; }
    char take() { if (pos == input.size()) fail(); return input[pos++]; }
    unsigned hex4() {
        unsigned n = 0;
        for (int i=0;i<4;++i) {
            const auto c=take();
            const auto d=c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:c>='A' && c<='F'?c-'A'+10:16;
            if (d == 16) fail();
            n = n*16+unsigned(d);
        }
        return n;
    }
    std::string quoted() {
        if (take() != '"') fail();
        std::string out;
        while (true) {
            unsigned char c=static_cast<unsigned char>(take());
            if (c=='"') return out;
            if (c<32) fail();
            if (c!='\\') { out += char(c); continue; }
            switch (take()) {
            case '"': out+='"'; break;
            case '\\': out+='\\'; break;
            case '/': out+='/'; break;
            case 'b': out+='\b'; break;
            case 'f': out+='\f'; break;
            case 'n': out+='\n'; break;
            case 'r': out+='\r'; break;
            case 't': out+='\t'; break;
            case 'u': {
                auto n=hex4();
                if (n>=0xd800 && n<=0xdbff) {
                    if (take()!='\\' || take()!='u') fail();
                    const auto low=hex4();
                    if (low<0xdc00 || low>0xdfff) fail();
                    n=0x10000+((n-0xd800)<<10)+(low-0xdc00);
                } else if (n>=0xdc00 && n<=0xdfff) fail();
                if (n<0x80) out+=char(n);
                else {
                    if (n>=0x10000) out+=char(0xf0|(n>>18));
                    if (n>=0x800) out+=char((n>=0x10000?0x80:0xe0)|((n>>12)&63));
                    out+=char((n>=0x800?0x80:0xc0)|((n>>6)&63));out+=char(0x80|(n&63));
                }
                break;
            }
            default: fail();
            }
        }
    }
    Json read(unsigned depth) {
        if (depth>32 || ++nodes>50000) fail();
        ws(); Json j;
        const char c=take();
        if (c=='{' || c=='[') {
            j.kind=c=='{'?Json::object:Json::array;
            ws(); const auto close=c=='{'?'}':']';
            if (pos<input.size() && input[pos]==close) {++pos;return j;}
            do {
                ws();
                if (c=='{') {
                    const auto key=quoted();ws();if(take()!=':') fail();
                    if (!j.members.emplace(key,read(depth+1)).second) fail();
                } else j.items.push_back(read(depth+1));
                ws();const auto next=take();if(next==close) return j;if(next!=',') fail();
            } while(true);
        }
        if(c=='"') {--pos;j.kind=Json::string;j.value=quoted();return j;}
        const auto start=pos-1;
        while(pos<input.size() && input[pos]!=',' && input[pos]!=']' && input[pos]!='}'
            && input[pos]!=' ' && input[pos]!='\r' && input[pos]!='\n' && input[pos]!='\t') ++pos;
        j.value=std::string(input.substr(start,pos-start));
        if(j.value=="null") return j;
        if(j.value=="true" || j.value=="false") {j.kind=Json::boolean;return j;}
        // Parse JSON's number grammar, including unknown numeric metadata.
        std::size_t i=0;
        if(j.value[i]=='-') ++i;
        const auto digits=[&] {const auto begin=i;while(i<j.value.size() && j.value[i]>='0' && j.value[i]<='9') ++i;return i>begin;};
        if(i==j.value.size()) fail();
        if(j.value[i]=='0') ++i;else if(!digits()) fail();
        if(i<j.value.size() && j.value[i]=='.') {++i;if(!digits()) fail();}
        if(i<j.value.size() && (j.value[i]=='e' || j.value[i]=='E')) {++i;if(i<j.value.size() && (j.value[i]=='+' || j.value[i]=='-')) ++i;if(!digits()) fail();}
        if(i!=j.value.size()) fail();
        j.kind=Json::number;return j;
    }
public:
    explicit Reader(std::string_view text):input(text) {if(text.empty() || text.size()>1024*1024) fail();}
    Json parse() {auto j=read(0);ws();if(pos!=input.size()) fail();return j;}
};
}
UpdateRelease parse_update_release(std::string_view json, std::string_view installed, std::string_view suffix) {
    const auto root=Reader(json).parse();
    if(root.at("draft").flag() || root.at("prerelease").flag()) throw std::runtime_error("Release is not stable");
    const auto tag=root.at("tag_name").str();
    const auto latest=parse_update_version(tag),current=parse_update_version(installed);
    if(!latest || !current) throw std::runtime_error("Unsupported release version");
    UpdateRelease out;
    out.version=tag.starts_with('v')?tag.substr(1):tag;
    out.page=std::string(update_repository)+"/releases/tag/"+tag;
    if(root.at("html_url").str()!=out.page) throw std::runtime_error("Unexpected release repository");
    out.newer=*latest>*current;
    const auto& assets=root.at("assets");
    if(assets.kind!=Json::array || assets.items.size()>128) throw std::runtime_error("Invalid release assets");
    if(suffix.empty()) return out;
    const auto name="StarFoxEnhanced-"+out.version+"-"+std::string(suffix);
    for(const auto& asset:assets.items) {
        if(asset.at("name").str()!=name) continue;
        if(!out.asset_url.empty()) throw std::runtime_error("Duplicate release package");
        const auto url=std::string(update_repository)+"/releases/download/"+tag+"/"+name;
        if(asset.at("browser_download_url").str()!=url || asset.at("state").str()!="uploaded")
            throw std::runtime_error("Unexpected release download");
        const auto& size=asset.at("size");
        const auto parsed=std::from_chars(size.value.data(),size.value.data()+size.value.size(),out.bytes);
        if(size.kind!=Json::number || parsed.ec!=std::errc{} || parsed.ptr!=size.value.data()+size.value.size()
            || out.bytes==0 || out.bytes>4ULL*1024*1024*1024) throw std::runtime_error("Invalid update size");
        // A digest-less asset is readable manually, but must never auto-install.
        const auto found=asset.members.find("digest");
        if(found==asset.members.end() || found->second.kind==Json::null) continue;
        const auto digest=found->second.str();
        if(digest.size()!=71 || !digest.starts_with("sha256:") || digest.find_first_not_of("0123456789abcdef",7)!=std::string::npos)
            throw std::runtime_error("Invalid update checksum");
        out.sha256=digest.substr(7);out.asset_url=url;
    }
    return out;
}
} // namespace starfox::app
