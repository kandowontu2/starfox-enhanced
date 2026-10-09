#pragma once
#include "starfox/vr/draw_packet.hpp"
#include <unordered_set>
namespace starfox::vr {
// Graphics-independent checks for the shared scene shader payload. Construct
// once per whole frame; cumulative budgets count shared immutable art once.
// Throws before GPU allocation/encoding. Never downgrades or drops a packet.
class ScenePacketValidator {
public:
    explicit ScenePacketValidator(bool compute_connected_grid=true,bool calibrated_ground=false)
        :compute_connected_grid_(compute_connected_grid),calibrated_ground_(calibrated_ground) {}
    void add(const DrawPacket&);
private:
    bool compute_connected_grid_;
    bool calibrated_ground_;
    std::size_t vertex_count_{},texel_count_{},artwork_words_{};
    std::unordered_set<const std::vector<uint32_t>*> counted_images_;
};
}
