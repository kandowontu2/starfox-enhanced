// Adapted from Esteban PDN / starwing-3ds, 01bf4925951515244cdb03d58ad722417919d6ba.
// Imported with the author's permission recorded in ../UPSTREAM-STARWING.md.
#pragma once
#include "starfox/assets/shape.hpp"

namespace starfox::platform_3ds {
// Conservative retained-storage estimate (capacities, not current lengths).
// Node/bookkeeping allowance keeps the preload budget above payload alone.
inline std::size_t shape_storage_bytes(const assets::Shape& s) {
    const auto storage = [](const auto& v) { return v.capacity() * sizeof(v[0]); };
    const auto blocks = [&](const auto& values) {
        std::size_t n = storage(values);
        for (const auto& value : values) n += storage(value.source_points);
        return n;
    };
    const auto faces = [&](const auto& values) {
        std::size_t n = storage(values);
        for (const auto& value : values) n += storage(value.vertex_indices);
        return n;
    };
    std::size_t n = sizeof(s) + 256U + s.name.capacity()
        + blocks(s.point_blocks) + storage(s.vertices)
        + (s.word_coordinates.capacity() + 7U) / 8U
        + storage(s.frames) + storage(s.visibilities) + faces(s.faces)
        + storage(s.face_batches) + storage(s.bsp_nodes) + storage(s.bsp_leaves)
        + storage(s.bsp_node_lookup) + storage(s.bsp_leaf_lookup)
        + storage(s.bsp_batch_lookup) + storage(s.colour_words)
        + storage(s.colour_materials) + storage(s.textures);
    for (const auto& f : s.frames)
        n += blocks(f.point_blocks) + storage(f.vertices)
            + (f.word_coordinates.capacity() + 7U) / 8U;
    for (const auto& b : s.face_batches) n += faces(b.faces);
    for (const auto& c : s.colour_materials) n += storage(c.animation_frames);
    for (const auto& t : s.textures) n += storage(t.texels);
    return n;
}
} // namespace starfox::platform_3ds
