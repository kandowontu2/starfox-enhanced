#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/render/palette.hpp"

namespace starfox::platform::nintendo_3ds {
// Painter order changes occlusion, not camera projection. Controls uses this
// for its isolated player/shadow after the ordinary weapons demonstration.
enum class PicaShapeOrder {depth,painter};
// Owned conversion of the shared source primitives, not a crop of a rendered
// mono framebuffer. Append in cartridge draw order. Both native eyes consume
// the same camera geometry; only their projection uniforms differ.
class PicaShapes {
public:
    void clear();
    void append(const render::PreparedShapePrimitives&,std::span<const render::Rgba8>,
        std::array<double,2> source_origin={128,112},const FramePlan* span_plan=nullptr,
        PicaShapeOrder order=PicaShapeOrder::depth,std::optional<PicaClip> scene_clip=std::nullopt);
    // scene_clip intersects (never replaces) a primitive's authored effect
    // window. It only limits native LCD coverage, not its stereo geometry.
    // Borrowed spans remain valid only until the next append/clear/frame call.
    // The native presenter consumes/copies them synchronously.
    [[nodiscard]] PicaFrame frame(const FramePlan&,Rgb clear={8,15,28});
    // CPU RGBA storage, including inactive reusable slots and the conversion
    // workspace. Independently bounded; this is not padded GPU residency.
    [[nodiscard]] std::size_t texture_storage_bytes() const noexcept;
    static constexpr std::size_t texture_storage_limit=pica_texture_budget+256U*256U*4U;
private:
    struct Texture {
        std::vector<std::uint8_t> rgba;
        unsigned width{},height{};
        bool repeat{};
    };
    unsigned texture(Texture&);
    Texture& texture_workspace(std::size_t);
    void trim_texture_storage() noexcept;
    void submit(std::span<const PicaVertex>,unsigned texture,const PicaMatrix&,bool dither,
        std::array<std::uint8_t,4> odd,std::optional<PicaClip>,PicaShapeOrder,bool fan=false);
    std::vector<PicaVertex> vertices_;
    // Reused between faces/scenes; triangles are written directly to vertices_.
    std::vector<PicaVertex> boundary_;
    std::vector<PicaDraw> draws_;
    // Only the active prefix belongs to this scene. Inactive slots retain
    // bounded buffers, not cached palette/texel results. Every new source
    // image is decoded afresh before content-based active deduplication.
    std::vector<Texture> textures_;
    std::size_t texture_count_{};
    Texture texture_work_;
    std::optional<unsigned> parity_texture_;
    std::vector<PicaImage> views_;
};
} // namespace starfox::platform::nintendo_3ds
