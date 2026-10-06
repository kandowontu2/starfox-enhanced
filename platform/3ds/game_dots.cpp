#include "starfox/platform/nintendo_3ds/game_dots.hpp"
#include "starfox/render/dust_renderer.hpp"
#include "starfox/render/palette.hpp"

namespace starfox::platform::nintendo_3ds {
namespace {
double word_difference(double value,double origin) {
    auto d=std::fmod(value-origin,65536.);
    if(d>32767.) d-=65536.;else if(d< -32768.) d+=65536.;
    return d;
}
bool excluded(simulation::GameFlowState flow) {
    return flow==simulation::GameFlowState::planet_select || flow==simulation::GameFlowState::planet_travel
        || flow==simulation::GameFlowState::continue_choice;
}
using Pixel=std::array<double,2>;
std::vector<Pixel> clip(std::vector<Pixel> polygon,const std::array<double,3>& line,double limit,bool above) {
    std::vector<Pixel> result;if(polygon.empty()) return result;
    const auto distance=[&](Pixel p){return line[0]*p[0]+line[1]*p[1]+line[2]-limit;};
    auto a=polygon.back();auto da=distance(a);
    for(auto b:polygon) {
        const auto db=distance(b);const bool ia=above?da>=0:da<=0,ib=above?db>=0:db<=0;
        if(ia!=ib) {const auto t=da/(da-db);result.push_back({std::lerp(a[0],b[0],t),std::lerp(a[1],b[1],t)});}
        if(ib) result.push_back(b);
        a=b;da=db;
    }
    return result;
}
}
GameDots::GameDots(const assets::RomImage& rom,const assets::SymbolMap& symbols) {
    std::optional<std::uint32_t> address;
    for(auto candidate:symbols.find("STAR_COLS"))
        if((candidate&0xffff)>=0x8000 && ((candidate>>16)&255)<0x7e) {address=candidate;break;}
    if(!address) throw std::runtime_error("Missing native cartridge STAR_COLS");
    for(unsigned i=0;i<star_colours_.size();++i) star_colours_[i]=rom.read8(*address+i);
}
PicaFrame GameDots::prepare(const GamePresentation& source) {
    if(!source.current || !source.previous || !source.raster || !source.raster->ppu
        || !std::isfinite(source.interpolation_alpha) || source.current->dust_point_count>simulation::kMaximumDustPoints)
        throw std::invalid_argument("Incomplete native dust/grid snapshot");
    static_cast<void>(PicaProjection(source.plan,0));
    if(source.plan.stereo) static_cast<void>(PicaProjection(source.plan,1));
    const auto& scene=*source.current;const auto& old=*source.previous;
    for(const auto& transform:{scene.camera,old.camera})
        for(double value:{transform.x,transform.y,transform.z})
            if(!std::isfinite(value) || std::abs(value)>std::numeric_limits<std::int32_t>::max())
                throw std::invalid_argument("Invalid native dust/grid camera");
    auto alpha=std::clamp(source.interpolation_alpha,0.,1.);
    if(scene.scene_epoch!=old.scene_epoch || scene.flow!=old.flow || scene.dots_mode!=old.dots_mode
        || scene.grid_lines!=old.grid_lines || scene.paused
        || timing::camera_transform_is_discontinuous(old.camera,scene.camera)) alpha=1;
    const auto camera=timing::interpolate(old.camera,scene.camera,alpha);
    const auto matrix=simulation::interpolate_rotation_matrix_q15(old.view_matrix,scene.view_matrix,alpha);
    auto words=source.raster->ppu->cgram;
    std::copy(scene.model_palette.begin(),scene.model_palette.end(),words.begin()+112);
    const auto palette=render::apply_snes_brightness(render::decode_bgr555_palette(words),source.raster->brightness);
    std::vector<PicaVertex> next;
    if(!excluded(scene.flow) && scene.dots_mode)
        next.reserve(scene.dots_mode<0?scene.dust_point_count*12:scene.grid_lines?18:225*12);
    GameDotCoverage count;count.ink_updates=coverage_.ink_updates;
    const auto quad=[&](double x,double y,double z,double ox,double oy,render::Rgba8 colour,double offset_x=0,double offset_y=0) {
        // Retain angular one-source-pixel ink, without snapping the interpolated
        // camera back to its old tick. Y is source-down before the PICA conversion.
        const auto size=z/256.;
        const std::array<std::array<double,2>,4> corners{{{ox,oy},{ox+1,oy},{ox+1,oy+1},{ox,oy+1}}};
        for(unsigned corner:{0U,1U,2U,0U,2U,3U}) next.push_back({
            {float(x+(corners[corner][0]+offset_x)*size),float(-y-(corners[corner][1]+offset_y)*size),float(z)},
            {colour.r/255.F,colour.g/255.F,colour.b/255.F,1},{}});
    };
    if(!excluded(scene.flow) && scene.dots_mode<0) {
        const bool controls=scene.flow==simulation::GameFlowState::controls_type || scene.flow==simulation::GameFlowState::controls_choice;
        for(unsigned i=0;i<scene.dust_point_count;++i) {
            const auto& p=scene.dust_points[i];
            const auto x=word_difference(p.x,camera.x),y=word_difference(p.y,camera.y),z=word_difference(p.z,camera.z);
            const auto cx=(x*matrix[0]+y*matrix[3]+z*matrix[6])/32768.;
            const auto cy=(x*matrix[1]+y*matrix[4]+z*matrix[7])/32768.;
            const auto cz=(x*matrix[2]+y*matrix[5]+z*matrix[8])/32768.;
            if(cz<256) continue;
            const auto depth=std::min(cz,4095.);
            const auto colour=palette[std::uint8_t(112+star_colours_[((scene.dust_point_count-i)&3)*16+(unsigned(depth)>>8)])];
            const auto dx=controls?scene.source_vanishing_point[0]-112:0,dy=controls?scene.source_vanishing_point[1]-96:0;
            quad(cx,cy,depth,0,0,colour,dx,dy);
            if(cz<1024) quad(cx,cy,depth,-1,1,colour,dx,dy);
            ++count.dust;
        }
    } else if(!excluded(scene.flow) && scene.dots_mode>0 && !scene.grid_lines) {
        const auto lattice=render::source_grid_lattice(camera,matrix);auto row=lattice.origin;
        for(unsigned z=0;z<15;++z) {
            auto point=row;
            for(unsigned x=0;x<15;++x) {
                if(point[2]>256) {
                    const auto depth=std::min<int>(point[2],12287);
                    quad(point[0],point[1],depth,0,0,palette[126]);
                    if(point[2]<512) quad(point[0],point[1],depth,-1,1,palette[126]);
                    ++count.grid;
                }
                for(unsigned axis=0;axis<3;++axis) point[axis]=simulation::add16(point[axis],lattice.x_step[axis]);
            }
            for(unsigned axis=0;axis<3;++axis) row[axis]=simulation::add16(row[axis],lattice.z_step[axis]);
        }
    }
    unsigned draw_count=next.empty()?0:1,texture_count=0;
    std::array<PicaDraw,pica_raster_max_strips*2> draws{};
    if(draw_count) draws[0].count=unsigned(next.size());
    std::vector<std::uint8_t> next_ink,next_rgba;
    std::optional<std::array<std::int16_t,14>> next_key;
    std::optional<std::array<std::uint8_t,4>> next_colour;
    unsigned ink_width=0;
    if(!excluded(scene.flow) && scene.dots_mode>0 && scene.grid_lines) {
        const auto lattice=render::source_grid_lattice(camera,matrix);
        const auto& a=lattice.x_step;const auto& b=lattice.z_step;
        const std::array<double,3> n{double(a[1])*b[2]-double(a[2])*b[1],double(a[2])*b[0]-double(a[0])*b[2],double(a[0])*b[1]-double(a[1])*b[0]};
        const auto d=n[0]*lattice.origin[0]+n[1]*lattice.origin[1]+n[2]*lattice.origin[2];
        const auto sign=d<0?-1.:1.;const auto distance=std::abs(d);
        const std::array<double,3> denominator{sign*n[0]/source.plan.focal_x,sign*n[1]/source.plan.focal_y,
            sign*(n[2]-n[0]*200/source.plan.focal_x-n[1]*120/source.plan.focal_y)};
        const unsigned guard=distance?pica_receiver_guard(source.plan,
            {denominator[0]/distance,denominator[1]/distance,denominator[2]/distance}):pica_scenery_guard(source.plan);
        ink_width=top_width+guard*2;
        std::array<std::int16_t,14> key{};
        key[0]=simulation::wrap16(std::int64_t(std::trunc(camera.x)));
        key[1]=simulation::wrap16(std::int64_t(std::trunc(camera.y)));
        key[2]=simulation::wrap16(std::int64_t(std::trunc(camera.z)));
        std::copy(matrix.begin(),matrix.end(),key.begin()+3);
        std::copy(scene.grid_line_start.begin(),scene.grid_line_start.end(),key.begin()+12);
        // Slider-only changes reuse a previously sufficient decoded field.
        // A new source camera/key retires extra coverage instead of retaining
        // the largest allocation for an entire stage.
        if(ink_key_ && *ink_key_==key) ink_width=std::max(ink_width,ink_width_);
        const unsigned raster_guard=(ink_width-top_width)/2;
        const unsigned canonical_left=(ink_width-224)/2;
        const auto canonical=render::project_source_grid(camera,matrix,224,192);
        count.connections=unsigned(canonical.count);
        const bool changed=!ink_key_ || *ink_key_!=key || ink_width_!=ink_width;
        if(changed) {
            render::Framebuffer centre(224,192),expanded(ink_width,screen_height);
            render::DustRenderer::draw_grid_lines_frame({canonical,scene.grid_line_start},centre);
            const auto wide=render::project_source_grid(camera,matrix,ink_width,screen_height);
            render::DustRenderer::draw_grid_lines_frame({wide,{simulation::add16(scene.grid_line_start[0],std::int16_t(canonical_left)),
                std::int16_t(scene.grid_line_start[1]+24)}},expanded);
            // Preserve the exact authored central 224x192 sequence. Extra LCD
            // guard ink never changes the source's canonical carried endpoint.
            next_ink=expanded.pixels();
            for(unsigned y=0;y<192;++y) std::copy_n(centre.pixels().begin()+y*224,224,next_ink.begin()+(y+24)*ink_width+canonical_left);
            next_key=key;
        }
        const auto colour=palette[126];
        const std::array<std::uint8_t,4> rgba_colour{colour.r,colour.g,colour.b,colour.a};
        if(changed || !ink_colour_ || *ink_colour_!=rgba_colour) {
            const auto& pixels=changed?next_ink:ink_;next_rgba.resize(ink_width*screen_height*4);
            for(unsigned i=0;i<pixels.size();++i) if(pixels[i]) {
                next_rgba[i*4]=colour.r;next_rgba[i*4+1]=colour.g;next_rgba[i*4+2]=colour.b;next_rgba[i*4+3]=255;
            }
            next_colour=rgba_colour;++count.ink_updates;
        }
        const auto triangle=[&](const std::vector<Pixel>& polygon,bool finite,double left,unsigned width) {
            for(unsigned i=1;i+1<polygon.size();++i) for(unsigned corner:{0U,i,i+1}) {
                const auto p=polygon[corner];
                const auto q=denominator[0]*p[0]+denominator[1]*p[1]+denominator[2];
                const auto z=finite?distance/q:1.;
                next.push_back({finite?Point3{float((p[0]-200)*z/source.plan.focal_x),float((120-p[1])*z/source.plan.focal_y),float(z)}
                    :Point3{float(p[0]),float(p[1]),0},{1,1,1,1},{float((p[0]-left)/width),float(p[1]/screen_height)}});
            }
        };
        // The authored carry can include ink outside the finite ground frustum.
        // Keep that original far-field ink at infinity (not HUD depth), while
        // every physically visible receiver pixel gets real plane depth.
        for(unsigned finite=0;finite<2;++finite) for(unsigned start=0;start<ink_width;start+=pica_raster_strip_width) {
            if(finite && !distance) continue;
            const auto width=std::min(pica_raster_strip_width,ink_width-start);
            const double left=double(start)-raster_guard,right=left+width;
            const std::vector<Pixel> rectangle{{left,0},{right,0},{right,240},{left,240}};
            const auto polygon=finite?clip(clip(rectangle,denominator,distance/source.plan.far_plane,true),denominator,distance/source.plan.near_plane,false)
                :distance?clip(rectangle,denominator,distance/source.plan.far_plane,false):rectangle;
            const unsigned first=unsigned(next.size());triangle(polygon,bool(finite),left,width);
            if(next.size()>first) {
                auto& draw=draws[draw_count++];draw.first=first;draw.count=unsigned(next.size())-first;
                draw.texture=start/pica_raster_strip_width;draw.alpha_blend=true;
                if(finite) draw.projected_uv=true;
                else {draw.space=PicaSpace::scenery;draw.depth_test=draw.depth_write=false;}
            }
        }
        texture_count=(ink_width+pica_raster_strip_width-1)/pica_raster_strip_width;
    }
    if(next.size()>pica_vertex_limit) throw std::length_error("Native dust/grid exceeds geometry budget");
    const auto scene_clip=game_controls_clip(scene.flow);
    for(unsigned i=0;i<draw_count;++i) draws[i].clip=scene_clip;
    std::array<PicaImage,pica_raster_max_strips> images{};
    if(texture_count) {
        const auto pixels=std::span<const std::uint8_t>(next_colour?next_rgba:rgba_);
        for(unsigned i=0;i<texture_count;++i) {
            const unsigned start=i*pica_raster_strip_width,width=std::min(pica_raster_strip_width,ink_width-start);
            images[i]={pixels.subspan(start*4),width,screen_height,ink_width*4,4};
        }
    }
    validate_pica_group({source.plan,next,std::span(draws).first(draw_count),std::span(images).first(texture_count)},512*256*4);
    vertices_=std::move(next);draws_=draws;coverage_=count;
    if(next_key) {ink_=std::move(next_ink);ink_key_=next_key;}
    if(next_colour) {rgba_=std::move(next_rgba);ink_colour_=next_colour;}
    if(texture_count) {image_=images;ink_width_=ink_width;}
    else {
        // Do not retain the connected-grid image throughout an entire space
        // stage. Empty frames also retire its borrowed upload and cache key.
        image_={};ink_width_=0;ink_key_.reset();ink_colour_.reset();
        std::vector<std::uint8_t>().swap(ink_);std::vector<std::uint8_t>().swap(rgba_);
    }
    return {source.plan,vertices_,std::span(draws_).first(draw_count),std::span(image_).first(texture_count),{0,0,0}};
}
} // namespace starfox::platform::nintendo_3ds
