#include "starfox/platform/nintendo_3ds/game_state_storage.hpp"
#include "starfox/assets/bps.hpp"
#include "starfox/state/archive.hpp"
#include "starfox/state/container.hpp"
#include <cerrno>
#include <cstdio>
#include <limits>
#include <memory>
#include <algorithm>

namespace starfox::platform::nintendo_3ds {
namespace {
constexpr std::uint32_t schema=0x33445311U;
struct Decoded {std::uint64_t generation{};std::vector<std::uint8_t> state;};
struct Scan {std::array<std::optional<Decoded>,2> files;std::optional<unsigned> newest;std::string warning;};
struct Close {void operator()(std::FILE* file) const noexcept {if(file) std::fclose(file);}};
using File=std::unique_ptr<std::FILE,Close>;
void slot_check(unsigned slot) {if(slot>=GameStateStorage::slots) throw std::invalid_argument("Invalid native state slot");}
std::optional<std::vector<std::uint8_t>> read(const std::string& path) {
    errno=0;File file(std::fopen(path.c_str(),"rb"));
    if(!file) {if(errno==ENOENT) return {};throw std::runtime_error("Cannot open SD state file");}
    if(std::fseek(file.get(),0,SEEK_END)!=0) throw std::runtime_error("Cannot seek SD state file");
    const auto size=std::ftell(file.get());
    if(size<=0 || static_cast<unsigned long>(size)>GameStateStorage::maximum_file_bytes)
        throw std::runtime_error("Invalid or oversized SD state file");
    if(std::fseek(file.get(),0,SEEK_SET)!=0) throw std::runtime_error("Cannot rewind SD state file");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if(std::fread(bytes.data(),1,bytes.size(),file.get())!=bytes.size()
        || std::fgetc(file.get())!=EOF || std::ferror(file.get()))
        throw std::runtime_error("SD state file changed or could not be read completely");
    return bytes;
}
Decoded decode(std::span<const std::uint8_t> bytes,std::uint32_t manifest,std::uint32_t crc) {
    state::Reader reader(state::unpack(bytes,schema,manifest));Decoded result;std::uint32_t cartridge{};
    reader(result.generation,cartridge,result.state);reader.finish();
    if(!result.generation || cartridge!=crc) throw std::runtime_error("Invalid generation or different state cartridge");
    static_cast<void>(decode_game_state(result.state,crc));return result;
}
Scan scan(const GameStateStorage& storage,unsigned slot,std::uint32_t manifest,std::uint32_t crc) {
    Scan result;
    for(unsigned i=0;i<2;++i) try {
        if(const auto bytes=read(storage.slot_path(slot,i))) result.files[i]=decode(*bytes,manifest,crc);
    } catch(const std::exception& error) {
        if(!result.warning.empty()) result.warning+='\n';
        result.warning+="GEN "+std::to_string(i)+": "+error.what();
    }
    if(result.files[0] && result.files[1] && result.files[0]->generation==result.files[1]->generation
        && result.files[0]->state!=result.files[1]->state) {
        result.files={};result.warning="Conflicting native states at the same generation";
    }
    for(unsigned i=0;i<2;++i) if(result.files[i]
        && (!result.newest || result.files[i]->generation>result.files[*result.newest]->generation)) result.newest=i;
    return result;
}
void write(const std::string& path,std::span<const std::uint8_t> bytes) {
    File file(std::fopen(path.c_str(),"wb"));
    if(!file) throw std::runtime_error("Cannot create SD state file; check free space/write access");
    if(std::fwrite(bytes.data(),1,bytes.size(),file.get())!=bytes.size() || std::fflush(file.get())!=0)
        throw std::runtime_error("Cannot write SD state; previous valid generation preserved");
    if(std::fclose(file.release())!=0) throw std::runtime_error("Cannot close SD state; previous valid generation preserved");
}
}
GameStateStorage::GameStateStorage(std::string directory,std::uint32_t manifest,std::uint32_t crc)
    :directory_(std::move(directory)),manifest_(manifest),rom_crc_(crc) {
    if(directory_.empty() || directory_.find('\0')!=std::string::npos) throw std::invalid_argument("Invalid native state directory");
    if(directory_.back()!='/') directory_+='/';
}
std::string GameStateStorage::slot_path(unsigned slot,unsigned gen) const {
    slot_check(slot);if(gen>1) throw std::invalid_argument("Invalid native state generation slot");
    char crc[9];std::snprintf(crc,sizeof(crc),"%08lx",static_cast<unsigned long>(rom_crc_));
    return directory_+"3ds-state-"+crc+"-"+std::to_string(slot)+"-"+std::to_string(gen)+".dat";
}
const GameStateInfo& GameStateStorage::current(unsigned slot) const {slot_check(slot);return cached_[slot].info;}
GameStateLoad GameStateStorage::load(unsigned slot) {
    slot_check(slot);auto disk=scan(*this,slot,manifest_,rom_crc_);Cached next;GameStateLoad result;
    next.info.found=disk.newest.has_value();next.info.writable=next.info.found || disk.warning.empty();
    next.info.warning=std::move(disk.warning);next.newest=disk.newest;next.initialized=true;
    if(disk.newest) {
        auto& data=*disk.files[*disk.newest];next.info.generation=data.generation;
        next.crc=assets::crc32(data.state);next.size=data.state.size();result.bytes=std::move(data.state);
    }
    result.info=next.info;cached_[slot]=std::move(next);return result;
}
bool GameStateStorage::save(unsigned slot,std::span<const std::uint8_t> bytes) {
    slot_check(slot);auto& cached=cached_[slot];
    if(!cached.initialized) throw std::logic_error("Read native state slot before saving");
    if(!cached.info.writable) throw std::runtime_error("No compatible state generation; back up existing files before recovery");
    static_cast<void>(decode_game_state(bytes,rom_crc_));
    auto disk=scan(*this,slot,manifest_,rom_crc_);
    if(disk.newest.has_value()!=cached.newest.has_value()
        || (disk.newest && (disk.files[*disk.newest]->generation!=cached.info.generation
            || disk.files[*disk.newest]->state.size()!=cached.size
            || assets::crc32(disk.files[*disk.newest]->state)!=cached.crc))
        || (!disk.newest && !disk.warning.empty()))
        throw std::runtime_error("Native state files changed; reopen the slot before saving");
    if(disk.newest && disk.warning.empty() && std::ranges::equal(disk.files[*disk.newest]->state,bytes)) return false;
    if(cached.info.generation==std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("Native state generation limit reached");
    Cached next;next.initialized=true;next.newest=disk.newest?1U-*disk.newest:0U;
    next.info={true,true,cached.info.generation+1,{}};next.size=bytes.size();next.crc=assets::crc32(bytes);
    state::Writer writer;writer(next.info.generation,rom_crc_,std::vector<std::uint8_t>(bytes.begin(),bytes.end()));
    auto encoded=state::pack(schema,manifest_,writer.bytes());
    if(encoded.size()>maximum_file_bytes) throw std::runtime_error("SD state journal exceeds file limit");
    // Release scanned large payloads before encoding reaches the SD writer.
    disk.files={};
    write(slot_path(slot,*next.newest),encoded);
    const auto verified=read(slot_path(slot,*next.newest));
    if(!verified || *verified!=encoded) throw std::runtime_error("SD state verification failed; previous generation preserved");
    cached=std::move(next);return true;
}
} // namespace starfox::platform::nintendo_3ds
