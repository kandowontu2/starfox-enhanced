namespace starfox::platform::nintendo_3ds {
inline void baseline_pack_pica_texture(PicaImage source,std::span<std::uint8_t> destination) {
    const auto layout=pica_texture_layout(source);
    if(destination.size()!=layout.bytes || !destination.data())
        throw std::invalid_argument("Incomplete 3DS GPU texture allocation");
    const auto src=reinterpret_cast<std::uintptr_t>(source.pixels.data()),dst=reinterpret_cast<std::uintptr_t>(destination.data());
    if((dst>=src && dst-src<source.pixels.size()) || (src>=dst && src-dst<destination.size()))
        throw std::invalid_argument("3DS texture upload requires independent storage");
    detail::pack_pica_plane(layout,source.width,source.height,source.pitch,source.channels,source.repeat,
        source.pixels.data(),[&](unsigned texel,const std::uint8_t* pixel) {
        // The shader converts top-left V to 1-V, and PICA's texture sampler
        // then addresses rows from bottom to top. Store source rows directly;
        // reversing them here as well flips uploaded artwork on both LCDs.
        const auto to=texel*4;
        destination[to]=source.channels==4?pixel[3]:255; // ABGR bytes.
        destination[to+1]=pixel[2];destination[to+2]=pixel[1];destination[to+3]=pixel[0];
    });
}
class BaselinePicaTexturePacker {
public:
    void release() noexcept {std::vector<std::uint8_t>().swap(dirty_);}
    [[nodiscard]] std::size_t capacity() const noexcept {return dirty_.capacity();}
    PicaTextureUploadRange pack(PicaImage image,std::span<const std::uint8_t> previous,
        std::span<std::uint8_t> destination) {
        const auto layout=pica_texture_layout(image);
        if(destination.size()!=layout.bytes || !destination.data())
            throw std::invalid_argument("Incomplete 3DS changed-tile texture allocation");
        const auto independent=[&](std::span<const std::uint8_t> source) {
            if(source.empty())return;
            const auto src=reinterpret_cast<std::uintptr_t>(source.data());
            const auto dst=reinterpret_cast<std::uintptr_t>(destination.data());
            if((dst>=src && dst-src<source.size()) || (src>=dst && src-dst<destination.size()))
                throw std::invalid_argument("3DS changed-tile upload requires independent storage");
        };
        independent(image.pixels);independent(previous);
        const auto full=[&] {
            baseline_pack_pica_texture(image,destination);
            return PicaTextureUploadRange{0,layout.bytes,layout.width*layout.height/64};
        };
        if(previous.empty())return full();
        const unsigned row_bytes=image.width*image.channels;
        if(previous.size()!=std::size_t(row_bytes)*image.height || !previous.data())
            throw std::invalid_argument("Incomplete 3DS changed-tile residency base");
        // Spatially broad changes (scrolling/full-screen palette flashes) need
        // no dirty-map traversal. This is a conservative choice of full packing,
        // never permission to skip a changed texel based on sampled equality.
        const auto row_changed=[&](unsigned y) {
            return std::memcmp(image.pixels.data()+std::size_t(y)*image.pitch,
                previous.data()+std::size_t(y)*row_bytes,row_bytes)!=0;
        };
        if(image.height>=3 && row_changed(0) && row_changed(image.height/2)
            && row_changed(image.height-1))return full();
        const unsigned columns=(image.width+7)/8,rows=(image.height+7)/8;
        dirty_.assign(columns*rows,0); // Reuses bounded warm scratch, before GPU writes.
        unsigned changed=0;
        for(unsigned y=0;y<image.height;++y) {
            const auto* source=image.pixels.data()+std::size_t(y)*image.pitch;
            const auto* old=previous.data()+std::size_t(y)*row_bytes;
            if(std::memcmp(source,old,row_bytes)==0)continue;
            for(unsigned x=0;x<image.width;x+=8) {
                auto& dirty=dirty_[(y/8)*columns+x/8];
                if(dirty)continue;
                const unsigned bytes=std::min(8U,image.width-x)*image.channels;
                if(std::memcmp(source+x*image.channels,old+x*image.channels,bytes)==0)continue;
                dirty=1;
                // Dense changes use the existing complete packer; do not pay
                // a second full comparison pass for scrolling/palette flashes.
                if(++changed>dirty_.size()/2)return full();
            }
        }
        if(!changed)return {};
        PicaTextureUploadRange result{layout.bytes,0,0};
        unsigned end=0;
        for(unsigned ty=0;ty<layout.height;ty+=8) for(unsigned tx=0;tx<layout.width;tx+=8) {
            const auto logical_x=(image.repeat?tx%image.width:std::min(tx,image.width-1))/8;
            const auto logical_y=(image.repeat?ty%image.height:std::min(ty,image.height-1))/8;
            if(!dirty_[logical_y*columns+logical_x])continue;
            const unsigned offset=((ty/8)*(layout.width/8)+tx/8)*256;
            result.offset=std::min(result.offset,offset);end=offset+256;++result.tiles;
            std::array<const std::uint8_t*,8> source_rows{};
            std::array<unsigned,8> source_columns{};
            for(unsigned i=0;i<8;++i) {
                const auto y=image.repeat?(ty+i)%image.height:std::min(ty+i,image.height-1);
                const auto x=image.repeat?(tx+i)%image.width:std::min(tx+i,image.width-1);
                source_rows[i]=image.pixels.data()+std::size_t(y)*image.pitch;
                source_columns[i]=x*image.channels;
            }
            unsigned texel=0;
            for(const auto& xy:detail::pica_morton_coordinates) {
                const auto* pixel=source_rows[xy[1]]+source_columns[xy[0]];
                const auto to=offset+4*texel++;
                destination[to]=image.channels==4?pixel[3]:255;
                destination[to+1]=pixel[2];destination[to+2]=pixel[1];destination[to+3]=pixel[0];
            }
        }
        result.bytes=end-result.offset;
        return result;
    }
private:
    std::vector<std::uint8_t> dirty_;
};
}
