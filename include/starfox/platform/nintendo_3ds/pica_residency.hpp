#pragma once
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"
#include <type_traits>

namespace starfox::platform::nintendo_3ds {
// The presenter waits for its preceding GPU work before calling prepare.
// Upload the borrowed scene directly; retain one reusable CPU comparison
// cache, not a newly allocated complete-scene staging vector every frame.
class PicaVertexResidency {
public:
    static_assert(std::is_nothrow_copy_constructible_v<PicaVertex> && std::is_nothrow_copy_assignable_v<PicaVertex>);
    template<class Upload>
    bool prepare(std::span<const PicaVertex> source,Upload upload) {
        if(source.size()>pica_vertex_limit)
            throw std::invalid_argument("Invalid 3DS resident vertex count");
        if(valid_ && source.size()==vertices_.size()
            && std::equal(source.begin(),source.end(),vertices_.begin())) return false;
        // Allocate BEFORE touching GPU storage. After reserve, PicaVertex's
        // nonthrowing copy can commit without an allocation failure leaving
        // the VBO newer than a still-valid CPU comparison cache.
        vertices_.reserve(source.size());
        valid_=false;
        if(!source.empty()) upload(source);
        vertices_.assign(source.begin(),source.end());valid_=true;
        return true;
    }
    [[nodiscard]] bool valid() const noexcept {return valid_;}
    [[nodiscard]] std::span<const PicaVertex> vertices() const noexcept {return vertices_;}
    [[nodiscard]] std::size_t capacity() const noexcept {return vertices_.capacity();}
private:
    std::vector<PicaVertex> vertices_;
    bool valid_{};
};

struct PicaTextureRetention {
    bool colour{},layers{};
};
// Allocated (padded) dimensions, not logical image dimensions or row stride.
// Retain same-size storage for palette/scroll changes; discard obsolete A8
// ownership even when the colour allocation can be reused.
inline PicaTextureRetention pica_texture_retention(PicaImage image,
    unsigned colour_width,unsigned colour_height,unsigned layer_width,unsigned layer_height) {
    const auto layout=pica_texture_layout(image);
    return {colour_width==layout.width && colour_height==layout.height,
        !image.source_layers.empty() && layer_width==layout.width && layer_height==layout.height};
}
inline void release_pica_texture_cache(std::vector<std::uint8_t>& cache) noexcept {
    // clear() leaves each inactive slot's old peak allocation resident.
    std::vector<std::uint8_t>().swap(cache);
}
// One reusable comparison cache per native colour/ownership allocation. Pack
// directly into the GPU allocation, not a fresh complete-image staging copy.
// Reserve both caches before changing GPU bytes, then commit nonthrowing byte
// copies. An upload failure invalidates that plane so retrying an older image
// repairs native bytes instead of accepting stale CPU equality as residency.
class PicaTextureResidency {
public:
    void release_colour() noexcept {
        release_pica_texture_cache(pixels_);colour_valid_=false;
        width_=height_=channels_=0;repeat_=false;
    }
    void release_layers() noexcept {
        release_pica_texture_cache(layers_);layers_valid_=true;
        layer_width_=layer_height_=classes_=0;layer_repeat_=false;
    }
    bool colour_matches(PicaImage image) const noexcept {
        return colour_valid_ && image.width==width_ && image.height==height_
            && image.channels==channels_ && image.repeat==repeat_
            && rows_equal(image.pixels,image.pitch,image.width*image.channels,image.height,pixels_);
    }
    bool layers_match(PicaImage image) const noexcept {
        if(!layers_valid_)return false;
        if(image.source_layers.empty())return layers_.empty();
        return !layers_.empty() && image.width==layer_width_ && image.height==layer_height_
            && image.repeat==layer_repeat_
            && rows_equal(image.source_layers,image.layer_pitch,image.width,image.height,layers_);
    }
    template<class ColourUpload,class LayerUpload>
    bool prepare(PicaImage image,ColourUpload colour_upload,LayerUpload layer_upload) {
        const auto layout=pica_texture_layout(image);
        const bool colour_same=colour_matches(image),layers_same=layers_match(image);
        if(colour_same && layers_same)return false;
        // A layout change/reserve must not invalidate its own borrowed input.
        for(const auto input:{image.pixels,image.source_layers})
            for(const auto retained:{std::span<const std::uint8_t>(pixels_),std::span<const std::uint8_t>(layers_)}) {
                if(input.empty() || retained.empty())continue;
                const auto from=reinterpret_cast<std::uintptr_t>(input.data()),saved=reinterpret_cast<std::uintptr_t>(retained.data());
                if((from>=saved && from-saved<retained.size()) || (saved>=from && saved-from<input.size()))
                    throw std::invalid_argument("3DS texture residency requires independent source storage");
            }
        auto classes=classes_;
        if(!image.source_layers.empty() && (!colour_same || !layers_same))
            classes=validate_pica_layers(image);
        if(!colour_same)pixels_.reserve(std::size_t(image.width)*image.height*image.channels);
        if(!layers_same && !image.source_layers.empty())
            layers_.reserve(std::size_t(image.width)*image.height);
        if(!colour_same) {
            colour_valid_=false;colour_upload(image,layout);
            copy_rows(image.pixels,image.pitch,image.width*image.channels,image.height,pixels_);
            width_=image.width;height_=image.height;channels_=image.channels;repeat_=image.repeat;
            colour_valid_=true;
        }
        if(!layers_same) {
            layers_valid_=false;layer_upload(image,layout,image.source_layers.empty()?0:classes);
            if(image.source_layers.empty())release_layers();
            else {
                copy_rows(image.source_layers,image.layer_pitch,image.width,image.height,layers_);
                layer_width_=image.width;layer_height_=image.height;layer_repeat_=image.repeat;
                classes_=classes;layers_valid_=true;
            }
        }
        return true;
    }
    std::span<const std::uint8_t> pixels() const noexcept {return pixels_;}
    std::span<const std::uint8_t> layers() const noexcept {return layers_;}
    std::size_t colour_capacity() const noexcept {return pixels_.capacity();}
    std::size_t layer_capacity() const noexcept {return layers_.capacity();}
    unsigned classes() const noexcept {return classes_;}
private:
    static bool rows_equal(std::span<const std::uint8_t> source,unsigned pitch,unsigned row_bytes,
        unsigned height,const std::vector<std::uint8_t>& cache) noexcept {
        if(cache.size()!=std::size_t(row_bytes)*height || !source.data()
            || pitch<row_bytes || !height
            || source.size()<std::size_t(pitch)*(height-1)+row_bytes)return false;
        for(unsigned y=0;y<height;++y)
            if(!std::equal(source.begin()+std::size_t(y)*pitch,
                source.begin()+std::size_t(y)*pitch+row_bytes,cache.begin()+std::size_t(y)*row_bytes))return false;
        return true;
    }
    static void copy_rows(std::span<const std::uint8_t> source,unsigned pitch,unsigned row_bytes,
        unsigned height,std::vector<std::uint8_t>& cache) {
        cache.resize(std::size_t(row_bytes)*height); // prepare reserved before upload.
        for(unsigned y=0;y<height;++y)
            std::copy_n(source.begin()+std::size_t(y)*pitch,row_bytes,cache.begin()+std::size_t(y)*row_bytes);
    }
    std::vector<std::uint8_t> pixels_,layers_;
    unsigned width_{},height_{},channels_{},layer_width_{},layer_height_{},classes_{};
    bool colour_valid_{},layers_valid_{true},repeat_{},layer_repeat_{};
};
// The native presenter calls this ONLY after its previous GPU work completes.
// Preflight everything, release ALL obsolete allocations, THEN upload. Updating
// slots one at a time can exceed 4 MiB during a valid 4 MiB -> 4 MiB transition.
// The shared sequence is also exercised with an allocation-ledger test owner;
// that test is not a Citro3D/physical-device allocation or performance result.
template<class Resident,std::size_t Extent>
void update_pica_texture_residency(std::span<const PicaImage> images,PicaImage dashboard,
    std::span<Resident,Extent> textures,Resident& lower) {
    if(images.size()>textures.size() || textures.size()>pica_texture_limit)
        throw std::invalid_argument("Invalid 3DS resident texture slots");
    auto bytes=pica_resident_texture_bytes(dashboard);
    if(bytes>pica_texture_budget) throw std::invalid_argument("3DS resident dashboard budget exceeded");
    for(const auto image:images) {
        const auto size=pica_resident_texture_bytes(image);
        if(size>pica_texture_budget-bytes) throw std::invalid_argument("3DS resident texture budget exceeded");
        bytes+=size;
    }
    for(std::size_t i=images.size();i<textures.size();++i) textures[i].release();
    for(std::size_t i=0;i<images.size();++i) textures[i].prepare_layout(images[i]);
    lower.prepare_layout(dashboard);
    for(std::size_t i=0;i<images.size();++i) textures[i].update(images[i]);
    lower.update(dashboard);
}
} // namespace starfox::platform::nintendo_3ds
