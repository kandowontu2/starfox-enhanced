#include "starfox/platform/nintendo_3ds/game_state.hpp"
#include "starfox/state/archive.hpp"
#include "starfox/state/container.hpp"
#include <limits>

namespace starfox::platform::nintendo_3ds {
namespace {
void validate(const GameStateData& data,std::uint32_t rom_crc) {
    if(data.audio_phase>2 || data.pending_audio.size()>maximum_pending_audio_writes
        || data.scene_revision==std::numeric_limits<std::uint64_t>::max()
        || (data.grid.initialized && data.grid.number>data.scene_revision)
        || data.game.size()>maximum_game_state_bytes || data.audio.size()>maximum_game_state_bytes)
        throw std::runtime_error("Invalid or oversized native game timeline");
    render::GridLineHistory grid;grid.restore(data.grid);
    for(const auto& write:data.pending_audio) if(write.port>3)
        throw std::runtime_error("Invalid pending native APU port");
    static_cast<void>(state::unpack(data.game,0x47414d01U,rom_crc));
    static_cast<void>(state::unpack(data.audio,0x53504301U,0));
}
}
std::vector<std::uint8_t> encode_game_state(const GameStateData& data,std::uint32_t rom_crc) {
    validate(data,rom_crc);
    state::Writer writer;
    writer(data.game,data.audio,data.audio_phase,std::uint32_t(data.pending_audio.size()));
    for(const auto& write:data.pending_audio) writer(write.port,write.value,write.clock_offset);
    writer(data.grid.initialized,data.grid.committed,data.grid.number,data.grid.previous,data.grid.start,
        data.scene_revision,data.grid_start);
    auto result=state::pack(game_state_schema,rom_crc,writer.bytes());
    if(result.size()>maximum_game_state_bytes) throw std::runtime_error("Native save state exceeds 4 MiB limit");
    return result;
}
GameStateData decode_game_state(std::span<const std::uint8_t> bytes,std::uint32_t rom_crc) {
    if(bytes.size()>maximum_game_state_bytes) throw std::runtime_error("Native save state exceeds 4 MiB limit");
    state::Reader reader(state::unpack(bytes,game_state_schema,rom_crc));GameStateData result;
    std::uint32_t count{};reader(result.game,result.audio,result.audio_phase,count);
    if(count>maximum_pending_audio_writes) throw std::runtime_error("Oversized pending native audio queue");
    result.pending_audio.resize(count);
    for(auto& write:result.pending_audio) reader(write.port,write.value,write.clock_offset);
    reader(result.grid.initialized,result.grid.committed,result.grid.number,result.grid.previous,result.grid.start,
        result.scene_revision,result.grid_start);
    reader.finish();validate(result,rom_crc);return result;
}
} // namespace starfox::platform::nintendo_3ds
