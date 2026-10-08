#pragma once
#include "starfox/assets/shape.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace starfox::render {
struct PackedBspNode {
    std::array<std::uint32_t,4> links{},batch{};
};
static_assert(sizeof(PackedBspNode)==32);
// Immutable model-local data. Face IDs address this owned face array, including
// line/sprite faces; geometry emitters must preserve that common index space.
struct PackedBsp {
    std::vector<PackedBspNode> nodes;
    std::vector<assets::Face> faces;
    std::vector<std::uint32_t> face_ids;
    std::uint32_t root{UINT32_MAX},output_capacity{},work_limit{},maximum_depth{};
};
// Flattened source shape.faces order is used without BSP or during explosion.
// Throws if the source graph exceeds the GPU's depth/output/work bounds.
// Missing links/batches, duplicate addresses and leaf precedence match the CPU.
[[nodiscard]] PackedBsp pack_bsp(const assets::Shape&,bool explosion=false);
// CPU source metadata only, shared by immutable readers in one recording.
// Never cache across source mutation or a new recording/frame. Join or cancel
// all readers before destroying the packet; resource waits do not advance it.
// Projection/visibility/material state is deliberately absent from this packet.
class PreparedBspSource {
public:
    PreparedBspSource(const assets::Shape&,bool explosion);
    [[nodiscard]] bool matches(const assets::Shape& shape,bool explosion) const noexcept {
        return source_==&shape && explosion_==explosion;
    }
    [[nodiscard]] const PackedBsp& graph() const noexcept {return graph_;}
    [[nodiscard]] const std::vector<std::array<std::int32_t,4>>& normals() const noexcept {return normals_;}
    [[nodiscard]] std::uint64_t storage_bytes() const noexcept;
private:
    const assets::Shape* source_{};
    bool explosion_{};
    PackedBsp graph_;
    std::vector<std::array<std::int32_t,4>> normals_;
};
}
