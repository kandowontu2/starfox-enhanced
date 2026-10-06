#pragma once
#include "starfox/platform/nintendo_3ds/game_presentation.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/scaled_text_renderer.hpp"
#include "starfox/platform/nintendo_3ds/pica_shapes.hpp"
#include "starfox/vr/source_pose_interpolation.hpp"
#include <map>

namespace starfox::platform::nintendo_3ds {
struct GameModelCoverage {
    unsigned models{},shadows{},text_glyphs{},particles{},primitives{};
    std::size_t cached_shape_bytes{};
};
// Actual cartridge object-list adapter. This emits native depth geometry,
// source shadows, scaled text and particles, not the background/PPU compositor.
// The ROM must outlive this owner. Preparation never advances GameSession;
// paired eyes share one completed/interpolated source and native draw stream.
class GameModels {
public:
    GameModels(const assets::RomImage&,const assets::SymbolMap&);
    // Transactional: conversion failure retains the last complete model frame,
    // but is reported to the host (which must not present it as the new scene).
    // Returned spans are borrowed until the next successful prepare/frame call.
    PicaFrame prepare(const GamePresentation&);
    [[nodiscard]] GameModelCoverage coverage() const noexcept {return coverage_;}
private:
    struct CachedShape {std::shared_ptr<const assets::Shape> shape;std::size_t bytes{};std::uint64_t used{};};
    std::shared_ptr<const assets::Shape> shape(std::uint32_t,std::uint16_t,
        const assets::ShapeHeader* parent=nullptr);
    void text(PicaShapes&,const simulation::GameObject&,const render::RenderPose&,
        const render::Palette256&,GameModelCoverage&,PicaShapeOrder,std::optional<PicaClip>);
    void particles(PicaShapes&,const vr::GameSceneSnapshot&,simulation::ObjectHandle,
        const render::RenderPose&,double,const render::Palette256&,GameModelCoverage&,PicaShapeOrder,std::optional<PicaClip>);
    assets::ShapeDecoder decoder_;
    render::SoftwareRenderer renderer_;
    render::ScaledTextRenderer text_;
    vr::SceneInterpolationRules rules_;
    std::array<std::uint16_t,3> colours_{};
    std::uint16_t intro_laser_{};
    std::map<std::array<std::uint32_t,3>,CachedShape> shapes_;
    std::size_t cached_bytes_{};
    std::uint64_t epoch_{};
    PicaShapes geometry_;
    GameModelCoverage coverage_;
};
} // namespace starfox::platform::nintendo_3ds
