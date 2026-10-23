// Adapted from Esteban PDN / starwing-3ds, 01bf4925951515244cdb03d58ad722417919d6ba.
// Imported with the author's permission recorded in ../UPSTREAM-STARWING.md.
#include "frame_3ds.hpp"
#include "hud_layout.hpp"
#include "shape_memory.hpp"
#include "native_canvas.hpp"
#include "stereo_3ds.hpp"

#include "starfox/render/palette.hpp"
#include "starfox/compat/bit_cast.hpp"
#include "starfox/platform/nintendo_3ds/frame_profile.hpp"
#include "starfox/platform/nintendo_3ds/bg1_overlay_dependencies.hpp"

#include "mono_clock.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>
#ifdef STARWING_DEEP_PROFILE
#include <cstdio>
#endif

namespace starfox::platform_3ds {
namespace {
// Presentation must not alter the VM's open-bus latch or source timeline.
std::uint16_t reference_read_word(const simulation::MapVm& map,std::uint32_t address) {
    return map.peek_ram_word(address).value();
}
std::uint8_t reference_read_byte(const simulation::MapVm& map,std::uint32_t address) {
    return map.peek_ram_byte(address).value();
}

render::RenderSettings model_settings() {
    render::RenderSettings settings;
    settings.colour_index_base = 7U * 16U;
    return settings;
}

bool same_bg2_tiles(const simulation::SnesPpuState& a,
    const simulation::SnesPpuState& b) noexcept {
    return a.background_mode == b.background_mode
        && a.main_screen == b.main_screen
        && a.mosaic == b.mosaic
        && a.bg2_tile_size_16 == b.bg2_tile_size_16
        && a.bg2_character_base == b.bg2_character_base
        && a.bg2_screen_base == b.bg2_screen_base
        && a.bg2_screen_size == b.bg2_screen_size
        && a.bg2_scroll_x == b.bg2_scroll_x
        && a.bg2_scroll_y == b.bg2_scroll_y
        && a.bg2_vertical_offsets_enabled == b.bg2_vertical_offsets_enabled
        && a.bg2_horizontal_offsets_enabled == b.bg2_horizontal_offsets_enabled
        && (!a.bg2_horizontal_offsets_enabled
            || a.bg2_horizontal_offsets == b.bg2_horizontal_offsets)
        && a.bg2_scanline_scroll_enabled == b.bg2_scanline_scroll_enabled
        && (!a.bg2_scanline_scroll_enabled
            || a.bg2_scanline_scroll_y == b.bg2_scanline_scroll_y)
        && a.tunnel_scene == b.tunnel_scene;
}

#ifdef STARWING_DEEP_PROFILE
bool same_mode1_tiles(const simulation::SnesPpuState& a,
    const simulation::SnesPpuState& b) noexcept {
    return same_bg2_tiles(a, b)
        && a.bg1_tile_size_16 == b.bg1_tile_size_16
        && a.bg1_character_base == b.bg1_character_base
        && a.bg1_screen_base == b.bg1_screen_base
        && a.bg1_screen_size == b.bg1_screen_size
        && a.bg1_scroll_x == b.bg1_scroll_x
        && a.bg1_scroll_y == b.bg1_scroll_y
        && a.bg3_tile_size_16 == b.bg3_tile_size_16
        && a.bg3_character_base == b.bg3_character_base
        && a.bg3_screen_base == b.bg3_screen_base
        && a.bg3_screen_size == b.bg3_screen_size
        && a.bg3_scroll_x == b.bg3_scroll_x
        && a.bg3_scroll_y == b.bg3_scroll_y;
}
#endif

bool same_mode1_bg1(const simulation::SnesPpuState& a,
    const simulation::SnesPpuState& b) noexcept {
    return a.background_mode == b.background_mode
        && (a.main_screen & 0x01U) == (b.main_screen & 0x01U)
        && a.mosaic == b.mosaic
        && a.bg1_tile_size_16 == b.bg1_tile_size_16
        && a.bg1_character_base == b.bg1_character_base
        && a.bg1_screen_base == b.bg1_screen_base
        && a.bg1_screen_size == b.bg1_screen_size
        && a.bg1_scroll_x == b.bg1_scroll_x
        && a.bg1_scroll_y == b.bg1_scroll_y;
}

bool same_mode1_bg3(const simulation::SnesPpuState& a,
    const simulation::SnesPpuState& b) noexcept {
    return a.background_mode == b.background_mode
        && (a.main_screen & 0x04U) == (b.main_screen & 0x04U)
        && a.mosaic == b.mosaic
        && a.bg3_tile_size_16 == b.bg3_tile_size_16
        && a.bg3_character_base == b.bg3_character_base
        && a.bg3_screen_base == b.bg3_screen_base
        && a.bg3_screen_size == b.bg3_screen_size
        && a.bg3_scroll_x == b.bg3_scroll_x
        && a.bg3_scroll_y == b.bg3_scroll_y;
}

// BG2 scroll/HDMA can change on every 60 Hz presentation even when the
// Super FX world and OBJ foreground are unchanged. PICA owns BG2 in this
// path, so those registers must not force a CPU foreground rebuild.
bool same_mode2_foreground(const simulation::SnesPpuState& a,
    const simulation::SnesPpuState& b) noexcept {
    return a.background_mode == b.background_mode
        && a.bg3_high_priority == b.bg3_high_priority
        && a.bg1_tile_size_16 == b.bg1_tile_size_16
        && a.bg3_tile_size_16 == b.bg3_tile_size_16
        && a.bg4_tile_size_16 == b.bg4_tile_size_16
        && a.mosaic == b.mosaic && a.object_select == b.object_select
        && a.bg1_character_base == b.bg1_character_base
        && a.bg1_screen_base == b.bg1_screen_base
        && a.bg1_screen_size == b.bg1_screen_size
        && a.bg1_scroll_x == b.bg1_scroll_x
        && a.bg1_scroll_y == b.bg1_scroll_y
        && a.bg3_character_base == b.bg3_character_base
        && a.bg3_screen_base == b.bg3_screen_base
        && a.bg3_screen_size == b.bg3_screen_size
        && a.bg3_scroll_x == b.bg3_scroll_x
        && a.bg3_scroll_y == b.bg3_scroll_y
        && a.main_screen == b.main_screen
        && a.tunnel_scene == b.tunnel_scene;
}

bool same_ppu_metadata(const simulation::SnesPpuState& a,
    const simulation::SnesPpuState& b) noexcept {
    return a.background_mode == b.background_mode
        && a.bg3_high_priority == b.bg3_high_priority
        && a.bg1_tile_size_16 == b.bg1_tile_size_16
        && a.bg2_tile_size_16 == b.bg2_tile_size_16
        && a.bg3_tile_size_16 == b.bg3_tile_size_16
        && a.bg4_tile_size_16 == b.bg4_tile_size_16
        && a.mosaic == b.mosaic && a.object_select == b.object_select
        && a.bg1_character_base == b.bg1_character_base
        && a.bg1_screen_base == b.bg1_screen_base
        && a.bg1_screen_size == b.bg1_screen_size
        && a.bg1_scroll_x == b.bg1_scroll_x
        && a.bg1_scroll_y == b.bg1_scroll_y
        && a.bg2_character_base == b.bg2_character_base
        && a.bg2_screen_base == b.bg2_screen_base
        && a.bg2_screen_size == b.bg2_screen_size
        && a.bg2_scroll_x == b.bg2_scroll_x
        && a.bg2_scroll_y == b.bg2_scroll_y
        && a.bg3_screen_base == b.bg3_screen_base
        && a.bg3_screen_size == b.bg3_screen_size
        && a.bg3_character_base == b.bg3_character_base
        && a.bg3_scroll_x == b.bg3_scroll_x
        && a.bg3_scroll_y == b.bg3_scroll_y
        && a.main_screen == b.main_screen
        && a.bg2_vertical_offsets_enabled == b.bg2_vertical_offsets_enabled
        && a.bg2_horizontal_offsets == b.bg2_horizontal_offsets
        && a.bg2_horizontal_offsets_enabled == b.bg2_horizontal_offsets_enabled
        && a.bg2_scanline_scroll_y == b.bg2_scanline_scroll_y
        && a.bg2_scanline_scroll_enabled == b.bg2_scanline_scroll_enabled
        && a.tunnel_scene == b.tunnel_scene;
}

void capture_ppu_metadata(simulation::SnesPpuState& destination,
    const simulation::SnesPpuState& source) noexcept {
    destination.background_mode = source.background_mode;
    destination.bg3_high_priority = source.bg3_high_priority;
    destination.bg1_tile_size_16 = source.bg1_tile_size_16;
    destination.bg2_tile_size_16 = source.bg2_tile_size_16;
    destination.bg3_tile_size_16 = source.bg3_tile_size_16;
    destination.bg4_tile_size_16 = source.bg4_tile_size_16;
    destination.mosaic = source.mosaic;
    destination.object_select = source.object_select;
    destination.bg1_character_base = source.bg1_character_base;
    destination.bg1_screen_base = source.bg1_screen_base;
    destination.bg1_screen_size = source.bg1_screen_size;
    destination.bg1_scroll_x = source.bg1_scroll_x;
    destination.bg1_scroll_y = source.bg1_scroll_y;
    destination.bg2_character_base = source.bg2_character_base;
    destination.bg2_screen_base = source.bg2_screen_base;
    destination.bg2_screen_size = source.bg2_screen_size;
    destination.bg2_scroll_x = source.bg2_scroll_x;
    destination.bg2_scroll_y = source.bg2_scroll_y;
    destination.bg3_screen_base = source.bg3_screen_base;
    destination.bg3_screen_size = source.bg3_screen_size;
    destination.bg3_character_base = source.bg3_character_base;
    destination.bg3_scroll_x = source.bg3_scroll_x;
    destination.bg3_scroll_y = source.bg3_scroll_y;
    destination.main_screen = source.main_screen;
    destination.bg2_vertical_offsets_enabled = source.bg2_vertical_offsets_enabled;
    destination.bg2_horizontal_offsets = source.bg2_horizontal_offsets;
    destination.bg2_horizontal_offsets_enabled = source.bg2_horizontal_offsets_enabled;
    destination.bg2_scanline_scroll_y = source.bg2_scanline_scroll_y;
    destination.bg2_scanline_scroll_enabled = source.bg2_scanline_scroll_enabled;
    destination.tunnel_scene = source.tunnel_scene;
}

} // namespace

Frame3ds::Frame3ds(const assets::RomImage& rom,
    const assets::SymbolMap& symbols)
    : rom_(rom), symbols_(symbols), decoder_(rom, symbols),
      trigonometry_(simulation::TrigTables::load(rom, symbols)),
      models_(model_settings()), text_(rom, symbols), dust_(rom, symbols) {
    top_hud_occupied_indices_.reserve(4096U);
    items_.reserve(simulation::kMaximumObjects);
    world_cache_objects_.reserve(simulation::kMaximumObjects);
    world_cache_render_items_.reserve(simulation::kMaximumObjects);
}

void Frame3ds::capture_after_tick(const simulation::GameSimulation& game) {
    const auto& map = game.map();
    const auto camera = timing::TransformSnapshot{
        static_cast<std::int16_t>(reference_read_word(map,ram_symbol("VIEWPOSX"))),
        static_cast<std::int16_t>(reference_read_word(map,ram_symbol("VIEWPOSY"))),
        static_cast<std::int16_t>(reference_read_word(map,ram_symbol("VIEWPOSZ"))),
        reference_read_word(map,ram_symbol("VIEWROTXW")),
        reference_read_word(map,ram_symbol("VIEWROTYW")),
        reference_read_word(map,ram_symbol("VIEWROTZW"))};
    auto objects = render::capture_object_snapshots(
        game.objects(), trigonometry_);
    if (!snapshots_ready_) {
        current_camera_ = previous_camera_ = camera;
        current_objects_ = objects;
        previous_objects_ = std::move(objects);
        snapshots_ready_ = true;
    } else {
        previous_camera_ = current_camera_;
        current_camera_ = camera;
        previous_objects_ = std::move(current_objects_);
        current_objects_ = std::move(objects);
        if (game.scene_revision() != snapshot_scene_revision_
            || game.flow_state() != snapshot_flow_
            || timing::camera_transform_is_discontinuous(
                previous_camera_, current_camera_)) {
            previous_camera_ = current_camera_;
            previous_objects_ = current_objects_;
        }
    }
    snapshot_scene_revision_ = game.scene_revision();
    snapshot_flow_ = game.flow_state();
}

const assets::Shape* Frame3ds::load_base_shape(
    std::uint16_t shape, std::uint16_t colour) {
    const auto key = (static_cast<std::uint32_t>(shape) << 16U) | colour;
    if (invalid_shapes_.contains(key)) return nullptr;
    auto found = shapes_.find(key);
    if (found == shapes_.end() && colour != 0U) {
        const auto original = shapes_.find(std::uint32_t{shape} << 16U);
        if (original != shapes_.end() && original->second.header.colour_pointer == colour)
            return &original->second;
    }
    if (found == shapes_.end()) {
        try {
            found = shapes_.emplace(key, decoder_.decode(shape, {}, colour)).first;
        } catch (const std::exception&) {
            invalid_shapes_.insert(key);
            return nullptr;
        }
    }
    // References survive unordered_map rehashes when a LOD is inserted.
    return &found->second;
}

const assets::Shape* Frame3ds::load_lod_shape(const assets::Shape& base,
    std::uint16_t pointer, std::uint16_t colour) {
    if (pointer == static_cast<std::uint16_t>(base.header.address)) return &base;
    const auto material = colour != 0U ? colour : base.header.colour_pointer;
    const auto key = (std::uint64_t{base.header.address} << 32U)
        | (std::uint64_t{pointer} << 16U) | material;
    if (invalid_lods_.contains(key)) return nullptr;
    auto found = lod_shapes_.find(key);
    if (found == lod_shapes_.end()) {
        try {
            found = lod_shapes_.emplace(key,
                decoder_.decode_lod(base.header, pointer, colour)).first;
        } catch (const std::exception&) {
            invalid_lods_.insert(key);
            return nullptr;
        }
    }
    return &found->second;
}

Frame3ds::PreloadStats Frame3ds::preload_catalog(std::size_t budget,
    PreloadProgress progress, void* context) {
    PreloadStats stats;
    std::vector<std::uint16_t> candidates;
    for (const auto& [name, addresses] : symbols_.entries()) {
        static_cast<void>(name);
        for (auto address : addresses)
            if (address >= 0x8000U && address <= 0xffffU)
                candidates.push_back(static_cast<std::uint16_t>(address));
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    stats.candidates = static_cast<unsigned>(candidates.size());
    for (const auto& [key, shape] : shapes_) stats.bytes += shape_storage_bytes(shape);
    for (const auto& [key, shape] : lod_shapes_) stats.bytes += shape_storage_bytes(shape);
    const auto retain = [&](auto& cache, auto key, assets::Shape&& shape) {
        const auto bytes = shape_storage_bytes(shape);
        if (stats.bytes > budget || bytes > budget - stats.bytes) {
            stats.limited = true;
            return false;
        }
        cache.emplace(key, std::move(shape));
        stats.bytes += bytes;
        ++stats.decoded;
        return true;
    };
    // First cover every full model with its authored material. Compact LODs
    // must be decoded from their parent; a standalone decode loses scale.
    unsigned visited = 0U;
    std::vector<std::uint16_t> bases;
    for (auto address : candidates) {
        if (progress && !progress(context, visited++, stats.candidates)) break;
        const auto key = std::uint32_t{address} << 16U;
        if (shapes_.contains(key)) { bases.push_back(address); continue; }
        if (!decoder_.looks_like_shape_header(address)) continue;
        try {
            auto shape = decoder_.decode(address);
            if (shape.header.compact) continue;
            if (retain(shapes_, key, std::move(shape))) bases.push_back(address);
        } catch (const std::exception&) { ++stats.rejected; }
    }
    // Preload distance and shadow variants, followed by the three source
    // flash/damage colour tables. Runtime demand loading remains mandatory
    // for unsymbolized assets or when a console reaches its memory budget.
    const std::array<std::uint16_t, 4> colours{
        0U, colour_symbol("ID_1_C"), colour_symbol("RED_C"), colour_symbol("WHITE_C")};
    visited = 0U;
    for (auto colour : colours) for (auto address : bases) {
        if (progress && !progress(context, visited++,
                static_cast<unsigned>(bases.size() * colours.size()))) return stats;
        const auto key = (std::uint32_t{address} << 16U) | colour;
        const auto& original = shapes_.at(std::uint32_t{address} << 16U);
        const auto* base = &original;
        if (colour != 0U && colour != original.header.colour_pointer) {
            auto found = shapes_.find(key);
            if (found == shapes_.end()) {
                try {
                    if (!retain(shapes_, key, decoder_.decode(address, {}, colour))) continue;
                    found = shapes_.find(key);
                } catch (const std::exception&) { ++stats.rejected; continue; }
            }
            base = &found->second;
        }
        for (auto pointer : {base->header.lod1_pointer, base->header.lod2_pointer,
                 base->header.lod3_pointer, base->header.shadow_pointer}) {
            if (pointer == address) continue;
            const auto material = colour ? colour : base->header.colour_pointer;
            const auto lod_key = (std::uint64_t{address} << 32U)
                | (std::uint64_t{pointer} << 16U) | material;
            if (lod_shapes_.contains(lod_key)) continue;
            try { static_cast<void>(retain(lod_shapes_, lod_key,
                decoder_.decode_lod(base->header, pointer, colour))); }
            catch (const std::exception&) { ++stats.rejected; }
        }
    }
    std::size_t vertices = 0, faces = 0, polygon = 0;
    const auto measure = [&](const assets::Shape& shape) {
        vertices = std::max(vertices, shape.vertices.size());
        for (const auto& frame : shape.frames) vertices = std::max(vertices, frame.vertices.size());
        auto ordered = shape.faces.size();
        for (const auto& batch : shape.face_batches) ordered += batch.faces.size();
        faces = std::max(faces, ordered);
        for (const auto& face : shape.faces) polygon = std::max(polygon, face.vertex_indices.size());
        for (const auto& batch : shape.face_batches)
            for (const auto& face : batch.faces) polygon = std::max(polygon, face.vertex_indices.size());
    };
    for (const auto& [key, shape] : shapes_) measure(shape);
    for (const auto& [key, shape] : lod_shapes_) measure(shape);
    model_scratch_.transformed_vertices.reserve(vertices);
    model_scratch_.projected.reserve(vertices);
    model_scratch_.ordered_faces.reserve(faces);
    model_scratch_.camera_polygon.reserve(polygon + 8U);
    model_scratch_.camera_clipping_scratch.reserve(polygon + 8U);
    model_scratch_.polygon.reserve(polygon + 8U);
    model_scratch_.raster_polygon.reserve(polygon + 8U);
    model_scratch_.clipping_scratch.reserve(polygon + 8U);
    return stats;
}

unsigned Frame3ds::preload_active_shapes(const simulation::GameSimulation& game) {
    const auto before = shapes_.size() + lod_shapes_.size();
    const auto special = colour_symbol("ID_1_C");
    const auto red = colour_symbol("RED_C");
    const auto white = colour_symbol("WHITE_C");
    for (const auto handle : game.draw_order()) {
        if (!game.objects().is_active(handle)) continue;
        const auto& object = game.objects().at(handle);
        if (object.shape == 0U || (object.strategy_flags[3] & 0x08U) != 0U) continue;
        const auto flags = object.strategy_flags[0];
        const auto colour = static_cast<std::uint16_t>(
            (flags & 0x40U) != 0U ? 0U
            : (flags & 0x02U) != 0U && (flags & 0x20U) == 0U
                ? ((flags & 0x01U) != 0U ? red : white)
                : ((flags & 0x01U) != 0U ? special : object.colour_table));
        const auto* base = load_base_shape(object.shape, colour);
        if (!base) continue;
        for (auto pointer : {base->header.lod1_pointer, base->header.lod2_pointer,
                 base->header.lod3_pointer, base->header.shadow_pointer})
            static_cast<void>(load_lod_shape(*base, pointer, colour));
    }
    return static_cast<unsigned>(shapes_.size() + lod_shapes_.size() - before);
}

bool Frame3ds::can_reuse_presentation(
    const simulation::GameSimulation& game, bool gpu_bg2,
    double interpolation_alpha, bool defer_gpu_foreground) const {
    const bool deferred = defer_gpu_foreground && gpu_bg2
        && game.map().ppu_state().background_mode == 2U && stereo_strength_ == 0U
        && (game.flow_state() == simulation::GameFlowState::gameplay
            || game.flow_state() == simulation::GameFlowState::training);
    if (!last_draw_valid_ || last_frame_gpu_bg2_ != gpu_bg2
        || last_frame_deferred_foreground_ != deferred
        || last_interpolation_alpha_ != interpolation_alpha
        || last_flow_ != game.flow_state()
        || last_scene_revision_ != game.scene_revision()) return false;
    const auto& map = game.map();
    const auto& ppu = map.ppu_state();
    if (gpu_bg2) {
        if (map.native_model_draw().active
            || last_ppu_memory_revision_ != map.ppu_memory_revision()
            || !same_mode2_foreground(last_ppu_, ppu)) return false;
        return true;
    }
    if (last_ppu_memory_revision_ != map.ppu_memory_revision()
        || !same_ppu_metadata(last_ppu_, ppu)) return false;
    const auto x = ppu.background_mode == 3U ? ppu.bg2_scroll_x
        : static_cast<std::int16_t>(reference_read_word(map,ram_symbol("BG2XSCROLL")));
    const auto y = ppu.background_mode == 3U ? ppu.bg2_scroll_y
        : static_cast<std::int16_t>(reference_read_word(map,ram_symbol("BG2SCROLL")));
    return x == background_scroll_x_ && y == background_scroll_y_;
}

std::uint32_t Frame3ds::ram_symbol(const char *name) const {
    for (const auto address : symbols_.find(name)) {
        if ((address >> 16U) == 0U || (address >> 16U) == 0x7eU)
            return address;
    }
    return 0U;
}

std::uint32_t Frame3ds::fx_symbol(const char *name) const {
    for (const auto address : symbols_.find(name)) {
        if ((address >> 16U) == 0x70U) return address;
    }
    return 0U;
}

std::uint16_t Frame3ds::colour_symbol(const char *name) const {
    for (const auto address : symbols_.find(name)) {
        if ((address & 0xffffU) >= 0x8000U
            && ((address >> 16U) & 0xffU) < 0x70U)
            return static_cast<std::uint16_t>(address);
    }
    return 0U;
}

std::pair<std::int16_t, std::int16_t>
Frame3ds::current_background_scroll(
    const simulation::GameSimulation& game) const {
    const auto& map = game.map();
    const auto& ppu = map.ppu_state();
    if (ppu.background_mode == 3U)
        return {ppu.bg2_scroll_x, ppu.bg2_scroll_y};
    return {
        static_cast<std::int16_t>(reference_read_word(map,
            ram_symbol("BG2XSCROLL"))),
        static_cast<std::int16_t>(reference_read_word(map,
            ram_symbol("BG2SCROLL")))};
}

void Frame3ds::set_stereo_strength(std::uint8_t strength) noexcept {
    strength = std::min<std::uint8_t>(strength, 8U);
    if (strength != stereo_strength_) {
        stereo_strength_ = strength;
        last_draw_valid_ = false;
    }
}

const render::Framebuffer& Frame3ds::draw(
    const simulation::GameSimulation& game, bool gpu_bg2,
    double interpolation_alpha, bool defer_gpu_foreground) {
    const auto stage_begin = reference_system_tick();
    world_cache_snapshot_us_ = 0U;
    if (!snapshots_ready_) capture_after_tick(game);
    interpolation_alpha = std::clamp(interpolation_alpha, 0.0, 1.0);
#ifdef STARWING_DEEP_PROFILE
    const auto profile_begin = osGetTime();
    static unsigned mode1_hits = 0U, mode1_builds = 0U;
    static unsigned mode2_hits = 0U, mode2_builds = 0U;
    static unsigned mode1_vram_changes = 0U, mode1_scroll_changes = 0U;
    static unsigned mode1_other_changes = 0U;
    static unsigned mode1_main_changes = 0U, mode1_mosaic_changes = 0U;
    static unsigned mode1_bg2_extra_changes = 0U, mode1_base_changes = 0U;
    static unsigned mode1_tunnel_changes = 0U;
    static unsigned mode1_bg1_builds = 0U, mode1_bg2_builds = 0U;
    static unsigned mode1_bg3_builds = 0U;
#endif
    const auto& map = game.map();
    const auto& ppu = map.ppu_state();
    const auto read_ram = [&](const char *name) {
        return reference_read_word(map,ram_symbol(name));
    };
    const auto read_fx = [&](const char *name) {
        return reference_read_word(map,fx_symbol(name));
    };
    auto background_x = static_cast<std::int16_t>(read_ram("BG2XSCROLL"));
    auto background_y = static_cast<std::int16_t>(read_ram("BG2SCROLL"));
    if (ppu.background_mode == 3U) {
        background_x = ppu.bg2_scroll_x;
        background_y = ppu.bg2_scroll_y;
    }
    background_scroll_x_ = background_x;
    background_scroll_y_ = background_y;

    const bool mode2_gameplay = ppu.background_mode == 2U
        && (game.flow_state() == simulation::GameFlowState::gameplay
            || game.flow_state() == simulation::GameFlowState::training);
    const bool level_flow = game.flow_state() == simulation::GameFlowState::gameplay
        || game.flow_state() == simulation::GameFlowState::training;
    const bool stereo = mode2_gameplay && stereo_strength_ != 0U;
    const bool deferred_foreground = defer_gpu_foreground && mode2_gameplay
        && gpu_bg2 && !stereo;
    // Keep the cartridge's status sprites available for both screens, but
    // compose them only after the source raster has reached native width.
    const auto draw_source_sprites = [&](std::uint8_t priority) {
        sprites_.draw_objects(ppu, frame_, priority, 0, true, false,
            nullptr, false, nullptr, sprite_selection_);
    };
    frame_.clear(0U);
    // The PICA BG2 path needs exact foreground coverage: palette index zero
    // can be an opaque sprite or model pixel, not just an empty pixel.
    if (mode2_gameplay) frame_.begin_write_coverage();
    Mode1BackgroundCache* mode1_cache = nullptr;
    if (ppu.background_mode == 1U) {
        // The title repeatedly switches between two character bases while
        // leaving VRAM untouched. Keep both decoded layer variants so that
        // each switch does not rasterize the entire SNES background again.
        for (auto& candidate : mode1_caches_) {
            if (candidate.valid
                && candidate.ppu_vram_revision == map.ppu_vram_revision()
                && same_mode1_bg1(candidate.ppu, ppu)
                && same_bg2_tiles(candidate.ppu, ppu)
                && candidate.scroll_x == background_x
                && candidate.scroll_y == background_y
                && same_mode1_bg3(candidate.ppu, ppu)) {
                mode1_cache = &candidate;
                break;
            }
        }
        if (mode1_cache == nullptr) {
            for (auto& candidate : mode1_caches_) {
                if (!candidate.valid) {
                    mode1_cache = &candidate;
                    break;
                }
            }
        }
        if (mode1_cache == nullptr) {
            mode1_cache = &mode1_caches_[mode1_next_victim_];
            mode1_next_victim_ = static_cast<std::uint8_t>(
                (mode1_next_victim_ + 1U) % mode1_caches_.size());
        }
        auto& cache = *mode1_cache;
        const bool same_bg1 = cache.valid
            && cache.ppu_vram_revision == map.ppu_vram_revision()
            && same_mode1_bg1(cache.ppu, ppu);
        const bool same_bg2 = cache.valid
            && cache.ppu_vram_revision == map.ppu_vram_revision()
            && same_bg2_tiles(cache.ppu, ppu)
            && cache.scroll_x == background_x
            && cache.scroll_y == background_y
            && (!ppu.tunnel_scene || cache.cgram == ppu.cgram);
        const bool same_bg3 = cache.valid
            && cache.ppu_vram_revision == map.ppu_vram_revision()
            && same_mode1_bg3(cache.ppu, ppu);
        const bool same_background = same_bg1 && same_bg2 && same_bg3;
        if (same_background) ++background_cache_hits_;
        else ++background_cache_misses_;
#ifdef STARWING_DEEP_PROFILE
        if (same_background) ++mode1_hits;
        else {
            ++mode1_builds;
            if (!same_bg1) ++mode1_bg1_builds;
            if (!same_bg2) ++mode1_bg2_builds;
            if (!same_bg3) ++mode1_bg3_builds;
            if (cache.valid) {
                if (cache.ppu.vram != ppu.vram) ++mode1_vram_changes;
                if (cache.scroll_x != background_x
                    || cache.scroll_y != background_y
                    || cache.ppu.bg1_scroll_x != ppu.bg1_scroll_x
                    || cache.ppu.bg1_scroll_y != ppu.bg1_scroll_y
                    || cache.ppu.bg2_scroll_x != ppu.bg2_scroll_x
                    || cache.ppu.bg2_scroll_y != ppu.bg2_scroll_y
                    || cache.ppu.bg3_scroll_x != ppu.bg3_scroll_x
                    || cache.ppu.bg3_scroll_y != ppu.bg3_scroll_y)
                    ++mode1_scroll_changes;
                if (cache.ppu.vram == ppu.vram
                    && cache.scroll_x == background_x
                    && cache.scroll_y == background_y
                    && !same_mode1_tiles(cache.ppu, ppu))
                    ++mode1_other_changes;
                if (cache.ppu.main_screen != ppu.main_screen)
                    ++mode1_main_changes;
                if (cache.ppu.mosaic != ppu.mosaic)
                    ++mode1_mosaic_changes;
                if (cache.ppu.bg2_horizontal_offsets_enabled
                        != ppu.bg2_horizontal_offsets_enabled
                    || cache.ppu.bg2_vertical_offsets_enabled
                        != ppu.bg2_vertical_offsets_enabled
                    || cache.ppu.bg2_scanline_scroll_enabled
                        != ppu.bg2_scanline_scroll_enabled)
                    ++mode1_bg2_extra_changes;
                if (cache.ppu.bg1_character_base != ppu.bg1_character_base
                    || cache.ppu.bg2_character_base != ppu.bg2_character_base
                    || cache.ppu.bg3_character_base != ppu.bg3_character_base
                    || cache.ppu.bg1_screen_base != ppu.bg1_screen_base
                    || cache.ppu.bg2_screen_base != ppu.bg2_screen_base
                    || cache.ppu.bg3_screen_base != ppu.bg3_screen_base)
                    ++mode1_base_changes;
                if (cache.ppu.tunnel_scene != ppu.tunnel_scene
                    || (ppu.tunnel_scene
                        && cache.ppu.cgram != ppu.cgram))
                    ++mode1_tunnel_changes;
            }
        }
#endif
        if (!same_bg3) {
            cache.bg3_low.clear();
            cache.bg3_high.clear();
            backgrounds_.draw_bg3_split(ppu,
                cache.bg3_low, cache.bg3_high);
        }
        if (!same_bg2) {
            cache.bg2_low.clear();
            cache.bg2_high.clear();
            backgrounds_.draw_bg2_mode1_split(ppu, background_x,
                background_y, cache.bg2_low, cache.bg2_high);
        }
        if (!same_bg1) {
            cache.bg1.clear();
            backgrounds_.draw_bg1(ppu, cache.bg1, render::TilePriorityPass::all);
        }
        if (!same_background) {
            capture_ppu_metadata(cache.ppu, ppu);
            cache.cgram = ppu.cgram;
            cache.ppu_vram_revision = map.ppu_vram_revision();
            cache.scroll_x = background_x;
            cache.scroll_y = background_y;
            cache.valid = true;
        }
        const auto restore = [this](const render::Framebuffer& layer) {
            render::composite_transparent_layer(layer, frame_, {});
        };
        restore(cache.bg3_low);
        draw_source_sprites(0U);
        if (!ppu.bg3_high_priority)
            restore(cache.bg3_high);
        draw_source_sprites(1U);
        restore(cache.bg2_low);
    } else if (mode2_gameplay) {
        // Keep only models and OBJ coverage in the source-sized foreground.
        // BG2 is owned by PICA or the native CPU fallback. Drawing it here
        // marks the whole 256x224 source as foreground and hides the world
        // geometry in the centre when that foreground is placed over BG2.
    } else if (ppu.background_mode == 2U) {
        backgrounds_.draw_bg2(ppu, background_x, background_y,
            frame_, render::TilePriorityPass::low);
        draw_source_sprites(0U);
        draw_source_sprites(1U);
        backgrounds_.draw_bg2(ppu, background_x, background_y,
            frame_, render::TilePriorityPass::high);
    } else if (ppu.background_mode == 3U) {
        backgrounds_.draw_bg2(ppu, background_x, background_y,
            frame_, render::TilePriorityPass::low);
        draw_source_sprites(0U);
        backgrounds_.draw_bg1(ppu, frame_, render::TilePriorityPass::low);
        draw_source_sprites(1U);
        backgrounds_.draw_bg2(ppu, background_x, background_y,
            frame_, render::TilePriorityPass::high);
        draw_source_sprites(2U);
        backgrounds_.draw_bg1(ppu, frame_, render::TilePriorityPass::high);
    } else {
        backgrounds_.draw_bg2(ppu, background_x, background_y, frame_);
        backgrounds_.draw_bg3(ppu, frame_);
    }
    std::array<std::uint8_t, 224U> background_left_edge{};
    std::array<std::uint8_t, 224U> background_right_edge{};
    const auto native_world_scene = game.flow_state()
            == simulation::GameFlowState::gameplay
        || game.flow_state() == simulation::GameFlowState::training
        || game.flow_state() == simulation::GameFlowState::intro;
    if (native_world_scene && !mode2_gameplay) {
        const auto& background = frame_.pixels();
        for (std::uint32_t y = 0U; y < 224U; ++y) {
            background_left_edge[y] = background[y * 256U];
            background_right_edge[y] = background[y * 256U + 255U];
        }
    }
    const auto stage_background = reference_system_tick();
#ifdef STARWING_DEEP_PROFILE
    const auto profile_background = osGetTime();
#endif

    const auto camera = timing::interpolate(
        previous_camera_, current_camera_, interpolation_alpha);
    const auto camera_x = static_cast<std::int16_t>(std::lround(camera.x));
    const auto camera_y = static_cast<std::int16_t>(std::lround(camera.y));
    const auto camera_z = static_cast<std::int16_t>(std::lround(camera.z));
    const auto camera_pitch = static_cast<std::int16_t>(
        std::lround(camera.pitch));
    const auto camera_yaw = static_cast<std::int16_t>(
        std::lround(camera.yaw));
    const auto camera_roll = static_cast<std::int16_t>(
        std::lround(camera.roll));
    const auto vanish_x = static_cast<std::int16_t>(read_fx("M_VANISHX"));
    const auto vanish_y = static_cast<std::int16_t>(read_fx("M_VANISHY"));
    const auto frame_number = static_cast<std::uint8_t>(
        reference_read_byte(map,ram_symbol("GAMEFRAME")) & 0x7fU);
    const auto shadow_height = static_cast<std::int16_t>(
        read_ram("SHADOWHEIGHT"));
    const auto player_fly_mode = reference_read_byte(map,
        ram_symbol("PLAYERFLYMODE"));
    const auto depth_colours = read_fx("M_DEPTHSTAB");
    const auto depth_thresholds = read_fx("M_DEPTHTABLE");
    const auto dots_mode = starfox::bit_cast<std::int8_t>(
        reference_read_byte(map,ram_symbol("DOTSFLAG")));
    const auto dust_count = game.dust_point_count();
    const std::array<std::int32_t, 17U> world_key{
        camera_x, camera_y, camera_z, camera_pitch, camera_yaw, camera_roll,
        vanish_x, vanish_y, frame_number, shadow_height, player_fly_mode,
        static_cast<std::int32_t>(depth_colours),
        static_cast<std::int32_t>(depth_thresholds), dots_mode,
        static_cast<std::int32_t>(dust_count),
        static_cast<std::int32_t>(game.flow_state()),
        stereo ? stereo_strength_ : 0};
    const auto& draw_order = game.draw_order();
    const auto world_cache_validation_begin = reference_system_tick();
    bool world_cache_hit = world_cache_valid_
        && world_cache_scene_revision_ == game.scene_revision()
        && world_cache_key_ == world_key
        && world_cache_draw_order_ == draw_order
        && world_cache_particles_ == game.particles().particles()
        && world_cache_dust_ == game.dust().points()
        && world_cache_objects_.size() == draw_order.size();
    if (world_cache_hit) {
        for (std::size_t index = 0; index < draw_order.size(); ++index) {
            const auto handle = draw_order[index];
            const auto active = game.objects().is_active(handle);
            if (active != world_cache_objects_[index].has_value()
                || (active && game.objects().at(handle)
                    != *world_cache_objects_[index])) {
                world_cache_hit = false;
                break;
            }
        }
    }
    const auto matrix_for = [this](const timing::TransformSnapshot& pose) {
        return simulation::rotation_matrix_q15(trigonometry_,
            static_cast<std::int16_t>(pose.pitch),
            static_cast<std::int16_t>(pose.yaw),
            static_cast<std::int16_t>(pose.roll));
    };
    const auto view = interpolation_alpha >= 1.0
        ? matrix_for(current_camera_)
        : simulation::interpolate_rotation_matrix_q15(
            matrix_for(previous_camera_), matrix_for(current_camera_),
            interpolation_alpha);
    const timing::RenderTransform raster_camera{
        static_cast<double>(camera_x), static_cast<double>(camera_y),
        static_cast<double>(camera_z), static_cast<double>(camera_pitch),
        static_cast<double>(camera_yaw), static_cast<double>(camera_roll)};
    items_.clear();
    items_.reserve(draw_order.size());
    for (const auto handle : draw_order) {
        if (!game.objects().is_active(handle)) continue;
        const auto& object = game.objects().at(handle);
        if ((object.strategy_flags[3] & 0x08U) != 0U || object.shape == 0U)
            continue;
        auto transform = timing::RenderTransform{
            static_cast<double>(object.world_x),
            static_cast<double>(object.world_y),
            static_cast<double>(object.world_z),
            static_cast<double>(static_cast<std::uint16_t>(object.rotation_x)
                << 8U),
            static_cast<double>(static_cast<std::uint16_t>(object.rotation_y)
                << 8U),
            static_cast<double>(static_cast<std::uint16_t>(object.rotation_z)
                << 8U)};
        auto object_matrix = simulation::transpose_q15(
            simulation::rotation_matrix_q15(trigonometry_,
                simulation::wrap16(-static_cast<std::int32_t>(
                    static_cast<std::uint16_t>(object.rotation_x) << 8U)),
                simulation::wrap16(-static_cast<std::int32_t>(
                    static_cast<std::uint16_t>(object.rotation_y) << 8U)),
                simulation::wrap16(-static_cast<std::int32_t>(
                    static_cast<std::uint16_t>(object.rotation_z) << 8U))));
        const auto now = current_objects_.find(handle);
        const auto before = previous_objects_.find(handle);
        if (now != current_objects_.end()) {
            const auto& current = now->second;
            const bool same_entity = before != previous_objects_.end()
                && before->second.generation == current.generation
                && before->second.shape == current.shape
                && before->second.type == current.type
                && before->second.strategy_address == current.strategy_address;
            transform = same_entity
                ? timing::interpolate(before->second.transform,
                    current.transform, interpolation_alpha)
                : timing::interpolate(current.transform,
                    current.transform, 1.0);
            object_matrix = same_entity
                ? render::interpolate_object_rotation(before->second,
                    current, interpolation_alpha, 0U)
                : current.rotation_matrix;
        }
        const auto world_x = static_cast<std::int16_t>(std::lround(transform.x));
        const auto world_y = static_cast<std::int16_t>(std::lround(transform.y));
        const auto world_z = static_cast<std::int16_t>(std::lround(transform.z));
        const auto position = simulation::transform_q15(view, {
            simulation::subtract16(
                world_x, camera_x),
            simulation::subtract16(
                world_y, camera_y),
            simulation::subtract16(
                world_z, camera_z)});
        items_.push_back({handle, position, object_matrix, world_x, world_z});
    }
    // Alpha can advance while integer model positions and Q15 rotations stay
    // unchanged. Reuse only when the complete inputs consumed by the world
    // rasterizer are identical, including interpolation-derived poses.
    world_cache_hit = world_cache_hit
        && world_cache_view_ == view
        && world_cache_render_items_ == items_;
    world_cache_validation_us_ = static_cast<std::uint64_t>(
        (reference_system_tick() - world_cache_validation_begin)
            * (1000000.0 / reference_clock_frequency));
    const auto stage_world_prepare_end = reference_system_tick();
#ifdef STARWING_DEEP_PROFILE
    auto profile_items = osGetTime();
    auto profile_models = profile_items;
#endif
    if (world_cache_hit) {
        ++world_cache_hits_;
    } else {
        ++world_cache_misses_;
        const auto depth_symbols = symbols_.find("DEPTHTABLES");
        const auto depth_address = depth_symbols.empty() ? 0U : depth_symbols.front();
        const auto special_colour = colour_symbol("ID_1_C");
        const auto red_colour = colour_symbol("RED_C");
        const auto white_colour = colour_symbol("WHITE_C");
        const auto effective_colour = [&](const auto& object) {
            const auto flags = object.strategy_flags[0];
            if ((flags & 0x40U) != 0U) return std::uint16_t{};
            if ((flags & 0x02U) != 0U && (flags & 0x20U) == 0U)
                return static_cast<std::uint16_t>(
                    (flags & 0x01U) != 0U ? red_colour : white_colour);
            return static_cast<std::uint16_t>(
                (flags & 0x01U) != 0U ? special_colour : object.colour_table);
        };

        for (unsigned side = 0U; side < (stereo ? 2U : 1U); ++side) {
        auto& eye_world = side == 0U ? world_ : right_world_;
        eye_world.clear(0U);
        const int eye = stereo ? (side == 0U ? -int(stereo_strength_)
            : int(stereo_strength_)) : 0;
        if (dots_mode < 0) {
            dust_.draw(game.dust(), dust_count, raster_camera, view, eye_world,
                0, 0, 0, 0);
            if (game.flow_state() == simulation::GameFlowState::intro) {
                // Repeat only the camera-projected dust into the two native side
                // columns. All three projections share the same live 3D points,
                // so the margins turn and scroll with the cartridge's centre.
                dust_.draw(game.dust(), dust_count, raster_camera, view, eye_world,
                    -192, 0, 72, 400);
                dust_.draw(game.dust(), dust_count, raster_camera, view, eye_world,
                    192, 0, 0, 328);
            }
        } else if (dots_mode > 0)
            dust_.draw_grid(raster_camera, view, eye_world);

        const auto display_frame = [frame_number](std::uint8_t object_frame) {
            return (object_frame & 0x80U) != 0U
                ? static_cast<std::uint32_t>(object_frame & 0x7fU)
                : static_cast<std::uint32_t>(frame_number);
        };
#ifdef STARWING_DEEP_PROFILE
        profile_items = osGetTime();
        profile_models = profile_items;
#endif

        const auto make_pose = [&](const RenderItem& item, bool shadow) {
            const auto& object = game.objects().at(item.handle);
            const bool true_colour_shadow =
                (object.strategy_flags[0] & 0x04U) != 0U;
            auto position = item.position;
            if (shadow && !true_colour_shadow) {
                position = simulation::transform_q15(view, {
                    simulation::subtract16(item.world_x, camera_x),
                    simulation::subtract16(shadow_height, camera_y),
                    simulation::subtract16(item.world_z, camera_z)});
            }
            render::RenderPose pose;
            pose.x = position[0];
            pose.y = position[1];
            pose.z = position[2];
            pose.pitch = static_cast<std::uint16_t>(object.rotation_x) << 8U;
            pose.yaw = static_cast<std::uint16_t>(object.rotation_y) << 8U;
            pose.roll = static_cast<std::uint16_t>(object.rotation_z) << 8U;
            // The native top-screen canvas centres the 256x224 cartridge raster
            // at (72,8), with the source Super FX origin another (16,16) in.
            pose.vanish_x = vanish_x + 88;
            pose.vanish_y = vanish_y + 24;
            auto object_matrix = item.rotation_matrix;
            if (shadow) {
                object_matrix[1] = object_matrix[4] = object_matrix[7] = 0;
                if (!true_colour_shadow) {
                    pose.force_colour = true;
                    pose.forced_colour = 0x09U;
                }
            }
            pose.rotation_matrix = simulation::multiply_matrix_q15(
                object_matrix, view);
            pose.use_rotation_matrix = true;
            pose.animation_frame = display_frame(object.animation_frame);
            pose.colour_frame = display_frame(object.colour_frame);
            pose.explosion_progress = (object.flags & 0x01U) != 0U
                ? object.count : 0U;
            pose.texture_scroll_x = object.texture_scroll_x;
            pose.texture_scroll_y = object.texture_scroll_y;
            if (depth_address != 0U)
                render::apply_source_depth_tables(rom_, depth_address,
                    depth_thresholds, depth_colours, object.extended[21], pose);
            apply_stereo_eye(pose, eye);
            return pose;
        };

        if ((player_fly_mode & 0x08U) != 0U) {
            for (const auto& item : items_) {
                const auto& object = game.objects().at(item.handle);
                if ((object.strategy_flags[0] & 0x0cU) == 0U) continue;
                const auto colour = effective_colour(object);
                const auto* base = load_base_shape(object.shape, colour);
                if (base == nullptr) continue;
                const auto pointer = base->header.shadow_pointer;
                const auto* shape = load_lod_shape(*base, pointer, colour);
                if (!shape) continue;
                models_.draw(*shape, make_pose(item, true), eye_world,
                    false, nullptr, nullptr, nullptr, &model_scratch_);
            }
        }

        for (const auto& item : items_) {
            const auto& object = game.objects().at(item.handle);
            if ((object.strategy_flags[0] & 0x04U) != 0U) continue;
            const auto colour = effective_colour(object);
            // Precaching is optional. Intro, GAME OVER and later spawns can
            // reach this path with a cold cache and must still draw.
            const auto* base = load_base_shape(object.shape, colour);
            if (base == nullptr) continue;
            auto pose = make_pose(item, false);
            if ((object.strategy_flags[0] & 0x10U) != 0U) {
                particles_.draw_owner(game.particles(), item.handle,
                    pose, 1.0, eye_world);
                continue;
            }
            if ((object.strategy_flags[0] & 0x40U) != 0U) {
                text_.draw(object.colour_table, object.extended[21],
                    std::bit_cast<std::int8_t>(object.texture_scroll_x),
                    pose, eye_world);
                continue;
            }
            const auto pointer = assets::ShapeDecoder::select_lod_pointer(
                base->header, static_cast<double>(item.position[2]));
            const auto* shape = load_lod_shape(*base, pointer, colour);
            if (!shape) continue;
            if ((object.strategy_flags[0] & 0x20U) != 0U) {
                auto adjustment = static_cast<std::int16_t>(
                    std::bit_cast<std::int8_t>(object.texture_scroll_x));
                for (std::uint8_t shift = 0U;
                     shift < base->header.shift; ++shift)
                    adjustment = simulation::add16(adjustment, adjustment);
                auto diameter = simulation::add16(
                    base->header.size, adjustment);
                diameter = simulation::add16(diameter, diameter);
                pose.simple_scaled_sprite = true;
                pose.simple_sprite_colour = object.extended[21];
                pose.simple_sprite_world_size = diameter == 0 ? 1 : diameter;
            }
            models_.draw(*shape, pose, eye_world,
                false, nullptr, nullptr, nullptr, &model_scratch_);
        }
        } // Both eyes share decoded shapes, poses and source simulation.
#ifdef STARWING_DEEP_PROFILE
        profile_models = osGetTime();
#endif
        const auto world_cache_snapshot_begin = reference_system_tick();
        world_cache_key_ = world_key;
        world_cache_scene_revision_ = game.scene_revision();
        world_cache_draw_order_ = draw_order;
        world_cache_view_ = view;
        world_cache_render_items_ = items_;
        world_cache_objects_.clear();
        world_cache_objects_.reserve(draw_order.size());
        for (const auto handle : draw_order) {
            if (game.objects().is_active(handle))
                world_cache_objects_.emplace_back(game.objects().at(handle));
            else world_cache_objects_.emplace_back(std::nullopt);
        }
        world_cache_particles_ = game.particles().particles();
        world_cache_dust_ = game.dust().points();
        world_cache_valid_ = true;
        world_cache_snapshot_us_ = static_cast<std::uint64_t>(
            (reference_system_tick() - world_cache_snapshot_begin)
                * (1000000.0 / reference_clock_frequency));
    }

    const auto stage_world = reference_system_tick();
    // The wider world raster already uses full PPU coordinates. Preserve
    // projected geometry that crosses the original Super FX viewport edge.
    render::LayerCompositeSettings world_to_source{};
    world_to_source.offset_x = -72;
    world_to_source.offset_y = -8;
    if (!mode2_gameplay)
        render::composite_transparent_layer(world_, frame_, world_to_source);
    const auto stage_world_composite_end = reference_system_tick();

    const auto stage_native_model_begin = reference_system_tick();
    const auto& native_model = map.native_model_draw();
    if (native_model.active && native_model.shape != 0U) {
        const auto key = static_cast<std::uint32_t>(native_model.shape) << 16U;
        if (!invalid_shapes_.contains(key)) {
            auto found = shapes_.find(key);
            if (found == shapes_.end()) {
                try {
                    found = shapes_.emplace(key,
                        decoder_.decode(native_model.shape)).first;
                } catch (const std::exception&) {
                    invalid_shapes_.insert(key);
                }
            }
            if (found != shapes_.end()) {
                render::RenderPose pose;
                pose.x = native_model.x;
                pose.y = native_model.y;
                pose.z = native_model.z;
                pose.pitch = static_cast<std::uint16_t>(
                    (native_model.rotation_x & 0xffU) << 8U);
                pose.yaw = static_cast<std::uint16_t>(
                    (native_model.rotation_y & 0xffU) << 8U);
                pose.roll = static_cast<std::uint16_t>(
                    (native_model.rotation_z & 0xffU) << 8U);
                pose.rotation_matrix = simulation::rotation_matrix_q15(
                    trigonometry_, static_cast<std::int16_t>(pose.pitch),
                    static_cast<std::int16_t>(pose.yaw),
                    static_cast<std::int16_t>(pose.roll));
                pose.use_rotation_matrix = true;
                pose.vanish_x = native_model.vanish_x + 16;
                pose.vanish_y = native_model.vanish_y + 16;
                pose.animation_frame = native_model.animation_frame;
                pose.colour_frame = native_model.colour_frame;
                models_.draw(found->second, pose, frame_,
                    false, nullptr, nullptr, nullptr, &model_scratch_);
            }
        }
    }
#ifdef STARWING_DEEP_PROFILE
    const auto profile_native = osGetTime();
#endif
    const auto stage_native_model_end = reference_system_tick();

    if (game.experience() == simulation::Experience::starfox_ex) {
        if (mode2_gameplay && gpu_bg2) {
            // BG2 lives on PICA, leaving this already allocated CPU scenery
            // cache available. Reuse BG1 only after exact map/character and
            // black-transparency checks; RGB still comes from the live palette.
            if (ex_overlay_valid_
                && platform::nintendo_3ds::same_bg1_overlay_source(
                    mode2_background_ppu_, ppu)) ++ex_overlay_cache_hits_;
            else {
                ++ex_overlay_cache_misses_;
                mode2_background_valid_ = false;
                mode2_background_cache_.clear(0U);
                backgrounds_.draw_bg1(ppu, mode2_background_cache_,
                    render::TilePriorityPass::all, 0, false, 16U, true);
                ex_overlay_rows_.prepare(mode2_background_cache_);
                mode2_background_ppu_ = ppu;
                ex_overlay_valid_ = true;
            }
            ex_overlay_rows_.apply(mode2_background_cache_, frame_);
        } else {
            backgrounds_.draw_bg1(ppu, frame_,
                render::TilePriorityPass::all, 0, false, 16U, true);
        }
    }

    const auto briefing = game.briefing_state();
    if (briefing.active && briefing.message_address != 0U) {
        constexpr auto base = render::briefing_text_palette_base;
        text_.draw_game_text(briefing.message_address, 28, 171,
            frame_, base, 13U, 216,
            briefing.visible_message_characters);
    }
    const auto stage_overlay_end = reference_system_tick();

    if (ppu.background_mode == 1U) {
        draw_source_sprites(2U);
        if (game.flow_state() == simulation::GameFlowState::title) {
            // PUSH START belongs to the cartridge's high-priority BG2 tiles.
            // Leave the world and Arwing beneath them untouched; the prompt is
            // rendered on the lower 3DS screen instead.
            for (std::uint32_t y = 94U; y < 104U; ++y) {
                for (std::uint32_t x = 90U; x < 166U; ++x)
                    mode1_cache->bg2_high.set(x, y, 0U);
            }
            render::composite_transparent_layer(mode1_cache->bg2_high, frame_, {});
            render::composite_transparent_layer(mode1_cache->bg1, frame_, {});
            render::composite_transparent_layer(mode1_cache->bg3_high, frame_, {});
        } else {
            render::composite_transparent_layer(mode1_cache->bg2_high, frame_, {});
        }
    } else if (ppu.background_mode == 2U) {
        if (mode2_gameplay) {
            draw_source_sprites(0U);
            draw_source_sprites(1U);
        }
        draw_source_sprites(2U);
    }
    if (!level_flow && show_top_hud_ && game.meter_state().enabled)
        sprites_.draw_meters(game.meter_state(), frame_);
    draw_source_sprites(3U);
    if (ppu.background_mode == 1U && ppu.bg3_high_priority)
        render::composite_transparent_layer(mode1_cache->bg3_high, frame_, {});

    const auto dialogue = game.dialogue_state();
    if (!level_flow && show_top_hud_ && dialogue.active) {
        // The cartridge Super FX communications canvas is centred at
        // (16,16) inside the 256x224 top-screen raster.
        text_.draw_face(dialogue.portrait_frame, 64, 168, frame_,
            7U * 16U, dialogue.alternate_portraits);
        if (dialogue.text_visible) {
            auto text_y = dialogue.three_lines ? 153 : 169;
            const auto translated_lines =
                text_.translated_game_text_lines(dialogue.text_address, 92)
                    .size();
            if (translated_lines != 0U)
                text_y = std::min(text_y,
                    183 - 10 * (static_cast<int>(translated_lines) - 1));
            text_.draw_game_text(dialogue.text_address, 99, text_y + 17,
                frame_, 7U * 16U, 9U, 191);
            text_.draw_game_text(dialogue.text_address, 98, text_y + 16,
                frame_, 7U * 16U, std::nullopt, 190);
        }
        if (dialogue.meter_visible) {
            for (int y = 0; y < 12; ++y) {
                for (int x = 0; x < 44; ++x) {
                    if (x == 0 || x == 43 || y == 0 || y == 11)
                        frame_.set(98 + x, 193 + y, 126U);
                    else if (x >= 2 && x < 2 + dialogue.meter_health
                        && y >= 2 && y < 10)
                        frame_.set(98 + x, 193 + y, 114U);
                }
            }
        }
    }
    if (game.paused()) {
        for (const auto address : symbols_.find("PAUSETXT")) {
            if ((address & 0xffffU) >= 0x8000U
                && ((address >> 16U) & 0xffU) < 0x70U) {
                text_.draw_game_text(address, 106, 106, frame_);
                break;
            }
        }
    }
#ifdef STARWING_DEEP_PROFILE
    static unsigned frame_count = 0U;
    static std::uint64_t background_ms = 0U, item_ms = 0U;
    static std::uint64_t model_ms = 0U, native_ms = 0U, finish_ms = 0U;
    ++frame_count;
    background_ms += profile_background - profile_begin;
    item_ms += profile_items - profile_background;
    model_ms += profile_models - profile_items;
    native_ms += profile_native - profile_models;
    finish_ms += osGetTime() - profile_native;
    if (frame_count >= 20U) {
        if (FILE *log = std::fopen("sdmc:/3ds/Starwing/perf_draw.log", "a")) {
            std::fprintf(log, "frames=%u background=%llu items=%llu models=%llu native=%llu finish=%llu mode1_hits=%u mode1_builds=%u mode1_vram=%u mode1_scroll=%u mode1_other=%u main=%u mosaic=%u bg2extra=%u base=%u tunnel=%u bg1build=%u bg2build=%u bg3build=%u mode2_hits=%u mode2_builds=%u\n",
                frame_count,
                static_cast<unsigned long long>(background_ms),
                static_cast<unsigned long long>(item_ms),
                static_cast<unsigned long long>(model_ms),
                static_cast<unsigned long long>(native_ms),
                static_cast<unsigned long long>(finish_ms),
                mode1_hits, mode1_builds, mode1_vram_changes,
                mode1_scroll_changes, mode1_other_changes,
                mode1_main_changes, mode1_mosaic_changes,
                mode1_bg2_extra_changes, mode1_base_changes,
                mode1_tunnel_changes,
                mode1_bg1_builds, mode1_bg2_builds, mode1_bg3_builds,
                mode2_hits, mode2_builds);
            std::fclose(log);
        }
        frame_count = 0U;
        background_ms = item_ms = model_ms = native_ms = finish_ms = 0U;
        mode1_hits = mode1_builds = mode2_hits = mode2_builds = 0U;
        mode1_vram_changes = mode1_scroll_changes = mode1_other_changes = 0U;
        mode1_main_changes = mode1_mosaic_changes = 0U;
        mode1_bg2_extra_changes = mode1_base_changes = mode1_tunnel_changes = 0U;
        mode1_bg1_builds = mode1_bg2_builds = mode1_bg3_builds = 0U;
    }
#endif
    const auto stage_canvas_begin = reference_system_tick();
    if (deferred_foreground) {
        // The acquired GPU upload consumes these same world/source rasters
        // directly. Avoid a temporary full canvas, its coverage clear and two
        // scans; palette expansion applies the original painter/alpha rules.
        // CPU fallback and stereo still construct their complete indexed frame.
        frame_.end_write_coverage();
    } else if (mode2_gameplay) {
        frame_.end_write_coverage();
        wide_frame_.clear(0U);
        if (gpu_bg2) wide_frame_.begin_write_coverage();
        if (stereo) {
            right_frame_.end_write_coverage();
            right_frame_.clear(0U);
            if (gpu_bg2) right_frame_.begin_write_coverage();
        }
        if (!gpu_bg2) {
            // The same storage/snapshot is now CPU BG2, not cached EX BG1.
            // Invalidate even on a CPU background hit, before any next GPU draw.
            ex_overlay_valid_ = false;
            const bool same_background = mode2_background_valid_
                && mode2_background_scene_revision_ == game.scene_revision()
                && mode2_background_ppu_vram_revision_
                    == map.ppu_vram_revision()
                && mode2_background_cgram_ == ppu.cgram
                && mode2_background_x_ == background_x
                && mode2_background_y_ == background_y
                && same_bg2_tiles(mode2_background_ppu_, ppu);
#ifdef STARWING_DEEP_PROFILE
            if (same_background) ++mode2_hits;
            else ++mode2_builds;
#endif
            if (same_background) ++background_cache_hits_;
            else ++background_cache_misses_;
            if (same_background) {
                wide_frame_.copy_pixels_from(mode2_background_cache_);
            } else {
                backgrounds_.draw_bg2(ppu, background_x, background_y,
                    wide_frame_, render::TilePriorityPass::all,
                    72, true, true, false, 0U, {}, 8);
                mode2_background_cache_.copy_pixels_from(wide_frame_);
                capture_ppu_metadata(mode2_background_ppu_, ppu);
                mode2_background_cgram_ = ppu.cgram;
                mode2_background_ppu_vram_revision_
                    = map.ppu_vram_revision();
                mode2_background_scene_revision_ = game.scene_revision();
                mode2_background_x_ = background_x;
                mode2_background_y_ = background_y;
                mode2_background_valid_ = true;
            }
        }
        if (stereo && !gpu_bg2) right_frame_.copy_pixels_from(wide_frame_);
        render::composite_transparent_layer(world_, wide_frame_, {});
        overlay_source_coverage_on_native(frame_, wide_frame_);
        if (stereo) {
            render::composite_transparent_layer(right_world_, right_frame_, {});
            overlay_source_coverage_on_native(frame_, right_frame_);
        }
    } else {
        const auto flow = game.flow_state();
        const auto style = flow == simulation::GameFlowState::gameplay
                || flow == simulation::GameFlowState::training
            ? NativeCanvasStyle::gameplay
            : flow == simulation::GameFlowState::intro
            ? NativeCanvasStyle::intro
            : flow == simulation::GameFlowState::controls_type
                || flow == simulation::GameFlowState::controls_choice
            ? NativeCanvasStyle::controls
            : flow == simulation::GameFlowState::planet_select
            ? NativeCanvasStyle::star_map
            : flow == simulation::GameFlowState::planet_travel
                || flow == simulation::GameFlowState::title
            ? NativeCanvasStyle::space
            : flow == simulation::GameFlowState::game_over
            ? NativeCanvasStyle::game_over
            : NativeCanvasStyle::plain;
        const int intro_vertical_shift=style==NativeCanvasStyle::intro
            ? static_cast<int>(std::lround(std::clamp(256.0*std::tan(
                static_cast<double>(camera_pitch)*
                (6.283185307179586/65536.0)), -4096.0, 4096.0))) : 0;
        compose_native_canvas(frame_, world_, wide_frame_, ppu, style,
            native_world_scene ? std::span<const std::uint8_t>{
                background_left_edge} : std::span<const std::uint8_t>{},
            native_world_scene ? std::span<const std::uint8_t>{
                background_right_edge} : std::span<const std::uint8_t>{},
            intro_vertical_shift);
    }
    // Status artwork remains owned by our existing lower-screen HUD.
    if (mode2_gameplay && gpu_bg2) wide_frame_.end_write_coverage();
    if (stereo && gpu_bg2) right_frame_.end_write_coverage();
    last_draw_stereo_ = stereo;
    capture_ppu_metadata(last_ppu_, ppu);
    last_ppu_memory_revision_ = game.map().ppu_memory_revision();
    last_flow_ = game.flow_state();
    last_scene_revision_ = game.scene_revision();
    last_frame_gpu_bg2_ = mode2_gameplay && gpu_bg2;
    last_frame_deferred_foreground_ = deferred_foreground;
    last_frame_wide_ = true;
    last_interpolation_alpha_ = interpolation_alpha;
    last_draw_valid_ = true;
    const auto stage_finish = reference_system_tick();
#if defined(STARFOX_3DS_PROFILE_FRAMES)
    if (auto* profile = platform::nintendo_3ds::active_frame_profile) {
        using platform::nintendo_3ds::FramePhase;
        profile->record(FramePhase::mono_background, stage_begin, stage_background);
        profile->record(FramePhase::mono_prepare, stage_background, stage_world_prepare_end);
        profile->record(FramePhase::mono_raster, stage_world_prepare_end, stage_world);
        profile->record(FramePhase::mono_finish, stage_world, stage_finish);
        // These are children of mono_finish, not extra frame work. Appending
        // just these two phases keeps the fixed 512-window buffer at 512 KiB.
        profile->record(FramePhase::mono_overlay, stage_native_model_end, stage_overlay_end);
        profile->record(FramePhase::mono_canvas, stage_canvas_begin, stage_finish);
    }
#endif
    constexpr auto tick_to_ms = 1000.0 / reference_clock_frequency;
    constexpr auto tick_to_us = 1'000'000.0 / reference_clock_frequency;
    last_draw_breakdown_ = {
        static_cast<float>((stage_background - stage_begin) * tick_to_ms),
        static_cast<float>((stage_world - stage_background) * tick_to_ms),
        static_cast<float>((stage_finish - stage_world) * tick_to_ms),
        static_cast<std::uint32_t>(
            (stage_world_prepare_end - stage_background) * tick_to_us),
        static_cast<std::uint32_t>(
            (stage_world - stage_world_prepare_end) * tick_to_us),
        static_cast<std::uint32_t>(
            (stage_world_composite_end - stage_world) * tick_to_us),
        static_cast<std::uint32_t>(
            (stage_native_model_end - stage_native_model_begin) * tick_to_us),
        static_cast<std::uint32_t>(
            (stage_finish - stage_native_model_end) * tick_to_us),
        static_cast<std::uint32_t>(
            (stage_overlay_end - stage_native_model_end) * tick_to_us),
        static_cast<std::uint32_t>(
            (stage_canvas_begin - stage_overlay_end) * tick_to_us),
        static_cast<std::uint32_t>(
            (stage_finish - stage_canvas_begin) * tick_to_us)};
    return last_frame();
}

} // namespace starfox::platform_3ds
