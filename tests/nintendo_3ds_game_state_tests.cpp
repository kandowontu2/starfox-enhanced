#include "starfox/platform/nintendo_3ds/game_state_storage.hpp"
#include "starfox/state/archive.hpp"
#include "starfox/state/container.hpp"
#include "starfox/audio/spc700_audio.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

using namespace starfox;
using namespace platform::nintendo_3ds;
namespace {
unsigned checks{};
constexpr std::uint32_t cartridge=0x99112233,manifest=0x88112233;
void require(bool value,const char* text) {++checks;if(!value) throw std::runtime_error(text);}
template<class F> void rejects(F f) {bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Unsafe native state accepted");}
GameStateData data(unsigned variation=0) {
    GameStateData result;const std::array<std::uint8_t,4> payload{2,3,4,std::uint8_t(variation)};
    result.game=state::pack(0x47414d01U,cartridge,payload);result.audio=state::pack(0x53504301U,0,payload);
    result.audio_phase=std::uint8_t(variation%3);result.pending_audio={{0,1,100},{3,2,3200}};
    result.scene_revision=42;result.grid={true,true,42,{120,-4},{-8,80}};result.grid_start={-8,80};return result;
}
// Independent field writer: no production native encoder in this oracle.
std::vector<std::uint8_t> packet(const GameStateData& d,std::uint32_t crc=cartridge) {
    state::Writer w;w(d.game,d.audio,d.audio_phase,std::uint32_t(d.pending_audio.size()));
    for(auto p:d.pending_audio) w(p.port,p.value,p.clock_offset);
    w(d.grid.initialized,d.grid.committed,d.grid.number,d.grid.previous,d.grid.start,d.scene_revision,d.grid_start);
    return state::pack(0x33445310U,crc,w.bytes());
}
std::vector<std::uint8_t> journal(std::uint64_t generation,std::span<const std::uint8_t> bytes,std::uint32_t m=manifest) {
    state::Writer w;w(generation,cartridge,std::vector<std::uint8_t>(bytes.begin(),bytes.end()));return state::pack(0x33445311U,m,w.bytes());
}
struct Temporary {
    std::filesystem::path path;
    Temporary() {
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<64;++i) {
            const auto candidate=std::filesystem::temp_directory_path()/("sfe-3ds-states-"+std::to_string(stamp)+"-"+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) {path=candidate;return;}
        }
        throw std::runtime_error("No isolated state test directory");
    }
    ~Temporary() {
        std::error_code ignored;GameStateStorage storage(path.generic_string(),manifest,cartridge);
        for(unsigned i=0;i<GameStateStorage::slots;++i) for(unsigned j=0;j<2;++j) std::filesystem::remove(storage.slot_path(i,j),ignored);
        std::filesystem::remove(path/"3ds-save-0.dat",ignored);std::filesystem::remove(path,ignored);
    }
};
std::vector<std::uint8_t> read(const std::string& path) {std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};}
void replace(const std::string& path,std::span<const std::uint8_t> bytes) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);file.exceptions(std::ios::failbit|std::ios::badbit);
    file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));file.close();
}
void codec() {
    for(unsigned phase=0;phase<3;++phase) {
        const auto original=data(phase);const auto saved=encode_game_state(original,cartridge);
        require(saved==packet(original),"Native state differs from independent field oracle");
        const auto decoded=decode_game_state(saved,cartridge);
        require(decoded.game==original.game && decoded.audio==original.audio && decoded.pending_audio==original.pending_audio
            && decoded.audio_phase==phase && decoded.grid==original.grid && decoded.grid_start==original.grid_start
            && decoded.scene_revision==42,"Native state lost partial audio/source grid carry");
        for(std::size_t i=0;i<saved.size();++i) {auto bad=saved;bad[i]^=1;rejects([&]{static_cast<void>(decode_game_state(bad,cartridge));});}
        for(std::size_t i=0;i<saved.size();++i) rejects([&]{static_cast<void>(decode_game_state(std::span(saved).first(i),cartridge));});
        rejects([&]{static_cast<void>(decode_game_state(saved,cartridge^1));});
        auto extended=saved;extended.push_back(0);rejects([&]{static_cast<void>(decode_game_state(extended,cartridge));});
    }
    for(unsigned field=0;field<7;++field) {
        auto bad=data();switch(field) {
        case 0:bad.audio_phase=3;break;case 1:bad.pending_audio[0].port=4;break;
        case 2:bad.pending_audio.resize(maximum_pending_audio_writes+1);break;
        case 3:bad.grid.initialized=false;break;case 4:bad.grid.number=43;break;
        case 5:bad.grid.committed=false;break;case 6:bad.scene_revision=UINT64_MAX;break;
        }
        rejects([&]{static_cast<void>(encode_game_state(bad,cartridge));});
        rejects([&]{static_cast<void>(decode_game_state(packet(bad),cartridge));});
    }
    std::vector<std::uint8_t> oversized(maximum_game_state_bytes+1);rejects([&]{static_cast<void>(decode_game_state(oversized,cartridge));});
}
void synthetic_spc_snapshot() {
    // Original public SMP program, not cartridge code: copy CPU input port 0
    // into RAM[0], write a DIFFERENT constant to output port 0, loop forever.
    // Upstream copy_state used to replace that input with the output on Save.
    constexpr std::array<std::uint8_t,9> program{0xe4,0xf4,0xc4,0x00,0x8f,0x5a,0xf4,0x2f,0xf7};
    std::vector<simulation::ApuPortWrite> upload{{2,0,0},{3,4,0},{1,1,0},{0,0xcc,0}};
    for(auto byte:program) upload.push_back({1,byte,0});
    upload.insert(upload.end(),{{2,0,0},{3,4,0},{1,0,0},{0,0xcc,0}});
    audio::Spc700Audio live,reference;
    static_cast<void>(live.prime_upload_sequence(upload));static_cast<void>(reference.prime_upload_sequence(upload));
    require(live.driver_loaded() && reference.driver_loaded(),"Public synthetic SMP program did not load");
    const std::array commands{simulation::ApuPortWrite{0,0x21,200}};
    require(live.render_logic_tick(commands)==reference.render_logic_tick(commands),"Synthetic audio priming differs");
    require(live.output_ports()[0]==0x5a,"Public SMP input/output distinction not exercised");
    const auto saved=live.save_state();audio::Spc700Audio restored;restored.load_state(saved);
    require(restored.save_state()==saved,"Synthetic SMP exact input-port restore failed");
    for(unsigned block=0;block<24;++block) {
        const auto before=live.save_state();
        for(unsigned i=0;i<17;++i) require(live.save_state()==before,"Repeated snapshot changed live SMP registers");
        const auto expected=reference.render_logic_tick({});
        require(live.render_logic_tick({})==expected && restored.render_logic_tick({})==expected,"Snapshot changed future DSP samples");
        const auto state=reference.save_state();
        require(live.save_state()==state && restored.save_state()==state,"Snapshot corrupted CPU input ports/next SMP instructions");
    }
    // Optional input-port extension reads old format too, without pretending
    // old archives contained the incoming registers they never stored.
    const auto payload=state::unpack(saved,0x53504301U,0);
    audio::Spc700Audio legacy;legacy.load_state(state::pack(0x53504301U,0,payload.first(payload.size()-8)));
    require(legacy.driver_loaded(),"Prior SPC format no longer loads");
}
void disk() {
    Temporary temp;GameStateStorage storage(temp.path.generic_string(),manifest,cartridge);
    const auto first=packet(data(1)),second=packet(data(2));
    rejects([&]{storage.save(0,first);});rejects([&]{static_cast<void>(storage.load(10));});
    rejects([&]{static_cast<void>(storage.slot_path(0,2));});
    // Distinct fixed names, including every logical slot, cannot touch settings.
    const std::array<std::uint8_t,3> settings{8,9,10};replace((temp.path/"3ds-save-0.dat").string(),settings);
    for(unsigned slot=0;slot<10;++slot) {
        require(!storage.load(slot).info.found && storage.current(slot).writable,"Fresh state slot read-only");
        require(storage.save(slot,first) && storage.current(slot).generation==1,"State first generation failed");
        require(read(storage.slot_path(slot,0))==journal(1,first),"State journal fields differ from independent oracle");
    }
    require(read((temp.path/"3ds-save-0.dat").string())==std::vector<std::uint8_t>(settings.begin(),settings.end()),"Game states changed settings/battery files");
    require(!storage.save(0,first),"Unchanged state rewritten");require(storage.save(0,second),"Alternate state generation failed");
    require(read(storage.slot_path(0,0))==journal(1,first) && read(storage.slot_path(0,1))==journal(2,second),"New state destroyed its backup");
    GameStateStorage reopened(temp.path.generic_string(),manifest,cartridge);require(reopened.load(0).bytes==second,"State lost at process reopen");
    replace(storage.slot_path(0,1),std::span(second).first(7));
    const auto recovered=reopened.load(0);require(recovered.bytes==first && recovered.info.found && recovered.info.writable && !recovered.info.warning.empty(),"Partial state update failed to recover backup");
    require(reopened.save(0,second),"Damaged alternate state not repairable");
    require(reopened.save(0,first),"New state generation not committed");
    rejects([&]{storage.save(0,first);});require(read(storage.slot_path(0,0))==journal(3,first),"Stale owner clobbered state");
    GameStateStorage wrong(temp.path.generic_string(),manifest^1,cartridge);require(!wrong.load(0).info.writable,"Foreign companion state accepted");
    rejects([&]{wrong.save(0,first);});
    GameStateStorage other(temp.path.generic_string(),manifest,cartridge^1);
    require(other.slot_path(0,0)!=storage.slot_path(0,0) && !other.load(0).info.found,"Different cartridges share state paths");
    const std::array<std::uint8_t,2> garbage{1,2};replace(storage.slot_path(1,0),garbage);replace(storage.slot_path(1,1),garbage);
    const auto broken=reopened.load(1);require(!broken.info.found && !broken.info.writable,"All-corrupt state allowed overwrite");
    rejects([&]{reopened.save(1,first);});require(read(storage.slot_path(1,0))==std::vector<std::uint8_t>(garbage.begin(),garbage.end()),"Corrupt states were deleted");
    replace(storage.slot_path(2,0),journal(9,first));replace(storage.slot_path(2,1),journal(9,second));
    require(!reopened.load(2).info.writable,"Conflicting same-generation states accepted");
    replace(storage.slot_path(3,0),journal(UINT64_MAX,first));require(reopened.load(3).info.found,"Largest generation rejected");
    rejects([&]{reopened.save(3,second);});require(read(storage.slot_path(3,0))==journal(UINT64_MAX,first),"State generation wrapped");
    std::filesystem::create_directory(storage.slot_path(4,1));static_cast<void>(reopened.load(4));
    rejects([&]{reopened.save(4,second);});require(read(storage.slot_path(4,0))==journal(1,first),"Write failure destroyed last good state");
    std::filesystem::remove(storage.slot_path(4,1));
    std::vector<std::uint8_t> large(GameStateStorage::maximum_file_bytes+1);replace(storage.slot_path(5,1),large);
    require(reopened.load(5).bytes==first && !reopened.current(5).warning.empty(),"Oversized SD file did not recover bounded backup");
    rejects([&]{GameStateStorage bad("",manifest,cartridge);});
}
}
int main() try {
    codec();synthetic_spc_snapshot();disk();std::cout<<"3DS state envelope/journal/SPC: "<<checks<<" checks passed; public synthetic packets/SMP program, not actual cartridge/SD hardware acceptance\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
