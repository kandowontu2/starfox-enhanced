#include "starfox/platform/nintendo_3ds/game_storage.hpp"
#include "starfox/state/archive.hpp"
#include "starfox/state/container.hpp"
#include <cerrno>
#include <cstdio>
#include <limits>

namespace starfox::platform::nintendo_3ds {
namespace {
constexpr std::uint32_t schema=0x33445304U,fps_schema=0x33445303U,bindings_schema=0x33445302U,legacy_schema=0x33445301U;
struct Decoded {std::uint64_t generation;GameSaveData data;};
struct Scan {
    std::array<std::optional<Decoded>,2> slots;
    std::optional<unsigned> newest;
    std::string warning;
};
struct FileClose {void operator()(std::FILE* file) const noexcept {if(file) std::fclose(file);}};
using File=std::unique_ptr<std::FILE,FileClose>;
void validate(const GameSaveData& data) {
    const auto& p=data.preferences;
    if((data.experience!=simulation::Experience::original && data.experience!=simulation::Experience::starfox_ex)
        || (p.timing!=simulation::TimingMode::original_speed && p.timing!=simulation::TimingMode::unlocked_20_fps)
        || p.music>100 || p.sfx>100 || p.language>5 || p.laser>2
        || (p.level && (p.level<11 || p.level>99 || p.level%10==0))
        || p.separation<1 || p.separation>64 || p.convergence<16
        || (p.render_fps!=30 && p.render_fps!=60) || !p.hud_layout.valid()
        || (!data.ex_sram.empty() && data.ex_sram.size()!=GameStorage::ex_sram_bytes)
        || (data.ex_sram.empty() && data.ex_rom_crc!=0) || !data.bindings.valid())
        throw std::runtime_error("Invalid 3DS settings or EX save bank");
}
template<class Archive,class Data> void fields(Archive& a,Data& data) {
    auto& p=data.preferences;
    a(data.experience,data.preview,p.timing,p.music,p.sfx,p.language,p.laser,p.level,
        p.swap,p.god,p.bombs,p.boost,p.lives,p.planet_cheat,p.separation,p.convergence,
        data.ex_rom_crc,data.ex_sram);
}
std::vector<std::uint8_t> encode(std::uint64_t generation,const GameSaveData& data,std::uint32_t manifest) {
    validate(data);
    if(!generation) throw std::runtime_error("Invalid 3DS save generation");
    state::Writer writer;writer(generation);fields(writer,data);
    writer(data.bindings.sources,data.bindings.deadzone);
    writer(data.preferences.render_fps,data.preferences.show_fps);
    for(const auto& placement:data.preferences.hud_layout.widgets)
        writer(placement.x,placement.y,placement.quarters,placement.visible);
    auto bytes=state::pack(schema,manifest,writer.bytes());
    if(bytes.size()>GameStorage::maximum_file_bytes) throw std::runtime_error("3DS save exceeds SD size limit");
    return bytes;
}
Decoded decode(std::span<const std::uint8_t> bytes,std::uint32_t manifest) {
    if(bytes.size()<12) throw std::runtime_error("Truncated 3DS SD envelope");
    const auto stored=std::uint32_t(bytes[8])|(std::uint32_t(bytes[9])<<8)
        |(std::uint32_t(bytes[10])<<16)|(std::uint32_t(bytes[11])<<24);
    if(stored!=schema && stored!=fps_schema && stored!=bindings_schema && stored!=legacy_schema) throw std::runtime_error("Unsupported 3DS SD schema");
    state::Reader reader(state::unpack(bytes,stored,manifest));Decoded result;
    reader(result.generation);fields(reader,result.data);
    // Old checksummed settings/SRAM remain valid and receive the original
    // Nintendo layout. A later changed save upgrades only the alternate slot.
    if(stored!=legacy_schema) reader(result.data.bindings.sources,result.data.bindings.deadzone);
    if(stored==schema || stored==fps_schema) reader(result.data.preferences.render_fps,result.data.preferences.show_fps);
    if(stored==schema) for(auto& placement:result.data.preferences.hud_layout.widgets)
        reader(placement.x,placement.y,placement.quarters,placement.visible);
    reader.finish();validate(result.data);
    if(!result.generation) throw std::runtime_error("Invalid 3DS save generation");
    return result;
}
std::optional<std::vector<std::uint8_t>> read(const std::string& path) {
    errno=0;File file(std::fopen(path.c_str(),"rb"));
    if(!file) {
        if(errno==ENOENT) return {};
        throw std::runtime_error("Cannot open SD save slot");
    }
    if(std::fseek(file.get(),0,SEEK_END)!=0) throw std::runtime_error("Cannot seek SD save slot");
    const auto size=std::ftell(file.get());
    if(size<=0 || static_cast<unsigned long>(size)>GameStorage::maximum_file_bytes)
        throw std::runtime_error("Invalid or oversized SD save slot");
    if(std::fseek(file.get(),0,SEEK_SET)!=0) throw std::runtime_error("Cannot rewind SD save slot");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if(std::fread(bytes.data(),1,bytes.size(),file.get())!=bytes.size()
        || std::fgetc(file.get())!=EOF || std::ferror(file.get()))
        throw std::runtime_error("SD save slot changed or could not be read completely");
    return bytes;
}
Scan scan(const GameStorage& storage,std::uint32_t manifest) {
    Scan result;
    for(unsigned i=0;i<2;++i) {
        try {if(const auto bytes=read(storage.slot_path(i))) result.slots[i]=decode(*bytes,manifest);}
        catch(const std::exception& error) {
            if(!result.warning.empty()) result.warning+='\n';
            result.warning+="SLOT "+std::to_string(i)+": "+error.what();
        }
    }
    if(result.slots[0] && result.slots[1]
        && result.slots[0]->generation==result.slots[1]->generation
        && result.slots[0]->data!=result.slots[1]->data) {
        result.slots={};result.warning="Conflicting SD save slots at the same generation";
    }
    for(unsigned i=0;i<2;++i) if(result.slots[i]
        && (!result.newest || result.slots[i]->generation>result.slots[*result.newest]->generation))
        result.newest=i;
    return result;
}
void write(const std::string& path,std::span<const std::uint8_t> bytes) {
    File file(std::fopen(path.c_str(),"wb"));
    if(!file) throw std::runtime_error("Cannot create SD save slot; check free space/write access");
    if(std::fwrite(bytes.data(),1,bytes.size(),file.get())!=bytes.size() || std::fflush(file.get())!=0)
        throw std::runtime_error("Cannot write SD save slot; the previous valid slot was preserved");
    if(std::fclose(file.release())!=0) throw std::runtime_error("Cannot close SD save slot; previous valid slot preserved");
}
} // namespace
GameStorage::GameStorage(std::string directory,std::uint32_t manifest):directory_(std::move(directory)),manifest_(manifest) {
    if(directory_.empty() || directory_.find('\0')!=std::string::npos) throw std::invalid_argument("Invalid native save directory");
    if(directory_.back()!='/') directory_+='/';
}
std::string GameStorage::slot_path(unsigned slot) const {
    if(slot>1) throw std::invalid_argument("Invalid 3DS save slot");
    return directory_+"3ds-save-"+std::to_string(slot)+".dat";
}
const GameSaveLoad& GameStorage::load() {
    const auto disk=scan(*this,manifest_);
    GameSaveLoad next;next.warning=disk.warning;
    next.found=disk.newest.has_value();next.writable=next.found || next.warning.empty();
    if(disk.newest) next.data=disk.slots[*disk.newest]->data;
    const auto generation=disk.newest?disk.slots[*disk.newest]->generation:0;
    current_=std::move(next);newest_=disk.newest;generation_=generation;initialized_=true;
    return current_;
}
bool GameStorage::save(const GameSaveData& data) {
    if(!initialized_) throw std::logic_error("Load native SD storage before saving");
    if(!current_.writable) throw std::runtime_error("No compatible valid SD slot; existing files preserved. Back them up before recovery.");
    validate(data);
    if(current_.found && data==current_.data && current_.warning.empty()) return false;
    // Detect a different/replaced owner before touching either slot. Native
    // uses a single writer; this is not an inter-process locking protocol.
    const auto disk=scan(*this,manifest_);
    if(disk.newest.has_value()!=newest_.has_value()
        || (disk.newest && (disk.slots[*disk.newest]->generation!=generation_
            || disk.slots[*disk.newest]->data!=current_.data))
        || (!disk.newest && !disk.warning.empty()))
        throw std::runtime_error("SD saves changed or became unreadable; reopen storage before writing");
    if(generation_==std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("SD save generation limit reached");
    const auto next_generation=generation_+1;
    const auto target=disk.newest?1U-*disk.newest:0U;
    const auto bytes=encode(next_generation,data,manifest_);
    write(slot_path(target),bytes);
    const auto verified=read(slot_path(target));
    if(!verified || *verified!=bytes) throw std::runtime_error("SD save verification failed; previous valid slot preserved");
    // Construct before replacing current_, so even allocation failure cannot
    // publish a partial cached data/generation pair.
    GameSaveLoad next{data,true,true,{}};
    current_=std::move(next);newest_=target;generation_=next_generation;
    return true;
}
} // namespace starfox::platform::nintendo_3ds
