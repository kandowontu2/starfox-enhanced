#include "starfox/platform/nintendo_3ds/game_layers.hpp"
#include <iostream>

namespace {
using namespace starfox;
using namespace platform::nintendo_3ds;
unsigned checks{};
std::string scenario;
void require(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
GamePresentation source(simulation::GameFlowState flow,unsigned mode) {
    auto scene=std::make_shared<vr::GameSceneSnapshot>();scene->flow=flow;
    auto ppu=std::make_shared<simulation::SnesPpuState>();ppu->background_mode=std::uint8_t(mode);
    ppu->main_screen=23;ppu->object_select=0;ppu->bg1_screen_size=ppu->bg2_screen_size=ppu->bg3_screen_size=0;
    // PPU base registers are VRAM WORD addresses; fixture writes below are bytes.
    ppu->bg1_character_base=0;ppu->bg2_character_base=0x1000;ppu->bg3_character_base=0x2000;
    ppu->bg1_screen_base=0x3000;ppu->bg2_screen_base=0x3200;ppu->bg3_screen_base=0x3400;
    for(unsigned number=0;number<128;++number) ppu->oam[number*4+1]=240;
    for(unsigned i=0;i<256;++i) ppu->cgram[i]=std::uint16_t((i%32)|(((i*3)%32)<<5)|(((i*7)%32)<<10));
    for(unsigned row=0;row<8;++row) {
        ppu->vram[(mode==3?64:32)+row*2]=255;
        ppu->vram[0x2000+32+row*2]=255;ppu->vram[0x4000+16+row*2]=255;
    }
    for(unsigned layer=0;layer<3;++layer) for(unsigned col:{7U,8U}) {
        const unsigned map=0x6000+layer*0x400,at=map+(5*32+col)*2;
        const unsigned tile=1|((layer+1)<<10)|(col==8?0x2000:0);
        ppu->vram[at]=std::uint8_t(tile);ppu->vram[at+1]=std::uint8_t(tile>>8);
    }
    auto raster=std::make_shared<GameRasterSnapshot>();raster->ppu=ppu;raster->brightness=15;
    GamePresentation frame;frame.current=frame.previous=scene;frame.raster=raster;
    frame.plan=plan_frame(0,true,ScreenUse::front_end);return frame;
}
std::pair<std::array<std::uint8_t,4>,unsigned> pixel(const PicaFrame& group,int x,int y) {
    std::pair<std::array<std::uint8_t,4>,unsigned> result{};
    for(const auto& draw:group.draws) {
        if(draw.texture==pica_no_texture) continue;
        const auto& image=group.textures[draw.texture];
        if(draw.space==PicaSpace::world) continue;
        const auto& origin=group.vertices[draw.first].position;
        const int ix=int(x)+72-int(origin[0]),iy=int(y)+8-int(origin[1]);
        if(ix<0 || iy<0 || ix>=int(image.width) || iy>=int(image.height)) continue;
        const auto at=std::size_t(iy)*image.pitch+unsigned(ix)*4;
        if(image.pixels[at+3]) result={
            {image.pixels[at],image.pixels[at+1],image.pixels[at+2],image.pixels[at+3]},
            image.source_layers.empty()?draw.source_layer:image.source_layers[std::size_t(iy)*image.layer_pitch+unsigned(ix)]};
    }
    return result;
}
std::pair<std::array<std::uint8_t,4>,unsigned> mono_receiver_pixel(const PicaFrame& group,int x,int y) {
    std::pair<std::array<std::uint8_t,4>,unsigned> result{};
    const std::array<double,2> sample{double(x)+72.5,double(y)+8.5};
    for(const auto& draw:group.draws) {
        if(draw.texture==pica_no_texture) continue;
        const auto& image=group.textures[draw.texture];
        for(unsigned i=draw.first;i<draw.first+draw.count;i+=3) {
            std::array<std::array<double,2>,3> triangle{};
            for(unsigned k=0;k<3;++k) {
                const auto& p=group.vertices[i+k].position;
                triangle[k]=draw.space==PicaSpace::world?std::array<double,2>{
                    200+group.plan.focal_x*p[0]/p[2],120-group.plan.focal_y*p[1]/p[2]}:std::array<double,2>{p[0],p[1]};
            }
            const auto cross=[](auto a,auto b,auto p){return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]);};
            const double area=cross(triangle[0],triangle[1],triangle[2]);if(std::abs(area)<1.e-12) continue;
            const std::array weights{cross(triangle[1],triangle[2],sample)/area,
                cross(triangle[2],triangle[0],sample)/area,cross(triangle[0],triangle[1],sample)/area};
            if(std::any_of(weights.begin(),weights.end(),[](double weight){return weight< -1.e-5;})) continue;
            double u=0,v=0,reciprocal=0;
            for(unsigned k=0;k<3;++k) {
                const auto& vertex=group.vertices[i+k];
                const double weight=draw.space==PicaSpace::world && !draw.projected_uv?weights[k]/vertex.position[2]:weights[k];
                reciprocal+=weight;u+=weight*vertex.uv[0];v+=weight*vertex.uv[1];
            }
            const auto tx=unsigned(std::clamp(u/reciprocal*image.width,0.,double(image.width-1)));
            const auto ty=unsigned(std::clamp(v/reciprocal*image.height,0.,double(image.height-1)));
            const auto at=std::size_t(ty)*image.pitch+tx*4;
            if(image.pixels[at+3]) result={{image.pixels[at],image.pixels[at+1],image.pixels[at+2],image.pixels[at+3]},
                image.source_layers.empty()?draw.source_layer:image.source_layers[std::size_t(ty)*image.layer_pitch+tx]};
        }
    }
    return result;
}
bool has(const PpuBatch& batch,PpuLayer layer,int priority) {
    return std::any_of(batch.passes.begin(),batch.passes.end(),[&](const auto& pass){return pass.layer==layer && pass.priority==priority;});
}
void map_single_occurrence() {
    using enum simulation::GameFlowState;
    for(auto flow:{planet_select,planet_travel}) for(unsigned priority:{0U,1U}) {
        auto frame=source(flow,3);
        auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);
        ppu->main_screen=1; // Isolate the source's eight-bit map artwork.
        // Deliberately mark both canonical edges. An extended 256-pixel tile
        // map repeats the right marker into the left LCD margin and vice versa.
        for(unsigned column:{0U,31U}) {
            const unsigned at=0x6000+(5*32+column)*2,word=1|(priority?0x2000:0);
            ppu->vram[at]=std::uint8_t(word);ppu->vram[at+1]=std::uint8_t(word>>8);
        }
        auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);
        raster->ppu=ppu;frame.raster=raster;
        GameLayers layers;const auto result=layers.prepare(frame);
        require(pixel(result.before_models,0,40).first[3]==255
            && pixel(result.before_models,248,40).first[3]==255,
            "Map policy removed its canonical edge artwork");
        require(pixel(result.before_models,-8,40).first[3]==0
            && pixel(result.before_models,256,40).first[3]==0,
            "Map travel repeats menu artwork in the outer LCD margins");
        const auto policy=game_layer_plan(frame);
        for(const auto& pass:policy.before_models.passes)
            require(!pass.extend_horizontal,"Map travel extends an authored menu pass");
        for(const auto& pass:policy.after_models.passes)
            require(!pass.extend_horizontal,"Map travel extends foreground menu artwork");
    }
    // Mode-1/2 travel can legitimately contain a world surround. Restrict
    // only the Mode-3 map, not its scenery or either eye's camera geometry.
    for(unsigned mode:{1U,2U}) {
        const auto policy=game_layer_plan(source(planet_travel,mode));
        require(std::any_of(policy.before_models.passes.begin(),policy.before_models.passes.end(),
            [](const auto& pass){return pass.layer==PpuLayer::bg2 && pass.extend_horizontal;}),
            "Map restriction suppressed travel's world surround");
    }
}
void priority_pixels() {
    using enum simulation::GameFlowState;
    for(unsigned mode:{1U,2U,3U}) {
        auto frame=source(planet_select,mode);GameLayers layers;const auto prepared=layers.prepare(frame);
        const auto low=pixel(prepared.before_models,56,40),high=pixel(prepared.before_models,64,40);
        require(low.first[3]==255 && high.first[3]==255,"Source priority cells lost opaque coverage");
        require(low.second==(mode==3?1U:2U) && high.second==(mode==3?1U:2U),"BG1 map buffer/BG2 painter order changed");
        if(mode==3) require(!has(game_layer_plan(frame).before_models,PpuLayer::bg3,0),"Mode 3 injected nonexistent BG3");
    }
    auto frame=source(title,1);GameLayers title_layers;auto result=title_layers.prepare(frame);
    require(pixel(result.before_models,64,40).second==2,"Title backdrop must precede models");
    require(pixel(result.after_models,56,40).second==1 && pixel(result.after_models,64,40).second==4,
        "Title must restore BG1 text then BG3 high over BG2 high/models");
    auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);ppu->cgram[17]=0;
    auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
    result=title_layers.prepare(frame);
    require(pixel(result.after_models,56,40).first==std::array<std::uint8_t,4>{0,0,0,255},"Title black text/outline became transparent");
    for(auto flow:{controls_type,controls_choice,continue_choice}) {
        frame=source(flow,1);GameLayers layers;result=layers.prepare(frame);
        require(pixel(result.after_models,56,40).first[3]==0 && pixel(result.after_models,64,40).second==2,
            "Controls/Continue restored low-priority backdrop over models or lost high frame");
    }
    for(unsigned mode:{1U,2U}) {
        frame=source(ex_pregame_menu,mode);GameLayers layers;result=layers.prepare(frame);
        require(pixel(result.before_models,56,40).second==1 && pixel(result.before_models,64,40).second==1,
            "EX menu lost native BG1 low/high text");
        const auto plan=game_layer_plan(frame);
        for(const auto& pass:plan.before_models.passes) if(pass.layer==PpuLayer::bg1)
            require(!pass.extend_horizontal && pass.guard_inset==16,"EX menu extended guard columns as artwork");
    }
}
void policy_contracts() {
    using enum simulation::GameFlowState;
    auto frame=source(title,1);auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
    scene->meters.extended=true;scene->ex_title_logo_screen=true;frame.current=scene;
    auto plan=game_layer_plan(frame);
    require(!has(plan.after_models,PpuLayer::bg1,-1),"EX animated logo covered native host model with BG1 bitmap");
    for(const auto& pass:plan.before_models.passes) if(pass.layer==PpuLayer::bg2)
        require(pass.extend_horizontal && !pass.wrap_horizontal,"EX title repeated logo atlas in margins");
    scene->ex_title_logo_screen=false;plan=game_layer_plan(frame);
    require(has(plan.after_models,PpuLayer::bg1,-1),"Regular EX title lost authored text");
    frame=source(gameplay,2);frame.sprites=render::SpriteSelection::world_only;
    plan=game_layer_plan(frame);
    require(plan.before_models.passes.size()==1 && plan.before_models.passes.front().layer==PpuLayer::bg2,
        "Mode 2 gameplay did redundant early OBJ priority passes");
    require(plan.after_models.passes.size()==4,"Gameplay world OBJ priorities not restored above models");
    for(const auto& pass:plan.after_models.passes)
        require(pass.sprites==render::SpriteSelection::world_only,"Gameplay reintroduced moved HUD by erasing a final image");
    scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);scene->native_ex_bitmap=true;scene->dialogue.active=true;
    frame.current=scene;plan=game_layer_plan(frame);
    require(!has(plan.after_models,PpuLayer::bg1,-1),"Moved EX dialogue duplicated in the upper bitmap");
    scene->flow=intro;frame.sprites=render::SpriteSelection::all;plan=game_layer_plan(frame);
    require(has(plan.after_models,PpuLayer::bg1,-1),"Intro lost dialogue before lower-HUD routing begins");
    scene->dialogue.active=false;scene->paused=true;plan=game_layer_plan(frame);
    for(const auto& pass:plan.after_models.passes) if(pass.layer==PpuLayer::bg1)
        require(pass.guard_inset==16 && pass.transparent_black && pass.mosaic_inset,"EX pause bitmap staging rules changed");
    frame=source(credits,1);auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->boss_roll=true;frame.raster=raster;
    plan=game_layer_plan(frame);
    require(has(plan.after_models,PpuLayer::objects,2) && has(plan.after_models,PpuLayer::bg2,1),"Boss roll lost Continue-style foreground frame");
}
void margins_and_cache() {
    auto frame=source(simulation::GameFlowState::controls_type,1);
    auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);
    // Native right field is BG2 palette 2, deliberately unlike CGRAM zero and
    // the left BG3 demonstration surround. Only the right field may extend.
    for(unsigned row=0;row<28;++row) {
        const auto right=0x6400+(row*32+31)*2,left=0x6800+(row*32)*2;
        ppu->vram[right]=1;ppu->vram[right+1]=8;ppu->vram[left]=1;ppu->vram[left+1]=12;
    }
    auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
    GameLayers layers;auto prepared=layers.prepare(frame);const auto field=pixel(prepared.before_models,255,40).first;
    require(prepared.clear==Rgb{field[0],field[1],field[2]},"Controls clear took miscolored native left edge/backdrop");
    const auto cached=layers.work();
    for(float slider:{0.F,.5F,1.F}) {
        frame.plan=plan_frame(slider,true,ScreenUse::world);prepared=layers.prepare(frame);
        const auto work=layers.work();
        require(work[0].decodes==cached[0].decodes && work[1].decodes==cached[1].decodes
            && work[0].colour_updates==cached[0].colour_updates && work[1].colour_updates==cached[1].colour_updates,
            "Slider rebuilt source priority artwork");
        Canvas lower;validate_pica_frame(prepared.before_models,lower.view());validate_pica_frame(prepared.after_models,lower.view());
    }
    raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->brightness=7;frame.raster=raster;
    prepared=layers.prepare(frame);const auto faded=layers.work();
    require(faded[0].decodes==cached[0].decodes && faded[0].colour_updates==cached[0].colour_updates+1,
        "Brightness fade reran tile priorities instead of recoloring coverage");
    ppu=std::make_shared<simulation::SnesPpuState>(*ppu);ppu->main_screen=0;
    raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
    prepared=layers.prepare(frame);
    require(prepared.before_models.draws.empty() && prepared.after_models.draws.empty(),"Disabled source layers rendered stale cache");
    bool rejected=false;ppu->background_mode=7;
    try {static_cast<void>(layers.prepare(frame));} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"Unsupported source mode was silently substituted");
}
void landscape_depth() {
    auto frame=source(simulation::GameFlowState::gameplay,2);
    auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
    scene->background_landscape=true;scene->landscape_grid_height=-145;scene->landscape_atlas_origin=232;
    frame.current=frame.previous=scene;
    auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);
    ppu->bg2_scroll_y=232;ppu->bg2_screen_size=3;ppu->bg2_vertical_offsets_enabled=true;
    for(unsigned i=0;i<4096;++i) {ppu->vram[0x6400+i*2]=1;ppu->vram[0x6401+i*2]=8;}
    // Quantized source HDMA steps, including a signed 8192-word crossing.
    for(int roll:{-16,-3,0,3,16}) {
        const int base=roll<0?8190:roll>0?8060:211;
        for(unsigned i=0;i<32;++i) {
            const unsigned word=0x4000|((base+roll*int(i+1))&8191),at=(0x2fa0+i)*2;
            ppu->vram[at]=std::uint8_t(word);ppu->vram[at+1]=std::uint8_t(word>>8);
        }
        auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=std::make_shared<simulation::SnesPpuState>(*ppu);
        frame.raster=raster;
        const auto plane=source_landscape_plane(frame);
        require(std::abs(plane.slope+roll/8.)<1.e-9 && plane.height==145,"Rolled ground inferred from palette, one edge step or arbitrary depth");
        GameLayers layers;frame.plan=plan_frame(1,true,ScreenUse::world);
        auto prepared=layers.prepare(frame);const auto work=layers.work();
        Canvas lower;validate_pica_frame(prepared.before_models,lower.view());
        const auto& group=prepared.before_models;
        require(group.draws.size()==2 && group.draws[0].space==PicaSpace::scenery
            && group.draws[1].space==PicaSpace::world && group.draws[1].depth_test && group.draws[1].depth_write
            && group.draws[1].projected_uv && group.draws[0].source_layer==2 && group.draws[1].source_layer==2,
            "Native landscape stayed a screen-depth HUD image or lost source ownership/depth");
        require(group.textures.size()==1 && group.textures[0].source_layers.empty(),"Landscape duplicated the entire texture or retained an unnecessary mixed-layer mask");
        const std::vector<PicaVertex> saved(group.vertices.begin(),group.vertices.end());
        for(unsigned i=6;i<saved.size();++i) {
            const auto& v=saved[i];const double z=v.position[2];
            const double x=200+256*v.position[0]/z,y=120-256*v.position[1]/z;
            require(std::abs(x+32-v.uv[0]*464)<.001 && std::abs(y-v.uv[1]*240)<.001,
                "Finite terrain lost the mono cartridge pixel registration");
            require(std::abs(z*(y-plane.centre-plane.slope*(x-200))-145*256)<.1,
                "Terrain vertices are not on the same source camera plane");
        }
        // Independent perspective interpolation oracle at interior points.
        // A plain UV sampler incorrectly stretches the native color bands.
        for(unsigned i=6;i+2<saved.size();i+=3) for(const auto weights:std::array<std::array<double,3>,3>{{{.2,.3,.5},{.7,.2,.1},{.1,.6,.3}}}) {
            double reciprocal=0,u=0,v=0,q=0;std::array<double,3> camera{};
            for(unsigned k=0;k<3;++k) {
                const auto& p=saved[i+k];const double inv=weights[k]/p.position[2];reciprocal+=inv;
                u+=inv*p.uv[0]*p.position[2];v+=inv*p.uv[1]*p.position[2];q+=inv*p.position[2];
                for(unsigned axis=0;axis<3;++axis) camera[axis]+=inv*p.position[axis];
            }
            for(auto& axis:camera) axis/=reciprocal;
            require(std::abs(u/q-(200+256*camera[0]/camera[2]+32)/464)<1.e-6
                && std::abs(v/q-(120-256*camera[1]/camera[2])/240)<1.e-6,
                "Homogeneous source UV/Q does not cancel ground perspective stretching");
            for(unsigned eye=0;eye<2;++eye) {
                const auto pixel=project(frame.plan,eye,float(camera[0]),float(camera[1]),float(camera[2]));
                require(pixel.has_value(),"Valid finite source ground disappeared from an eye projection");
            }
        }
        const auto& nearest=*std::min_element(saved.begin()+6,saved.end(),[](const auto& a,const auto& b){return a.position[2]<b.position[2];});
        const auto left=project(frame.plan,0,nearest.position[0],nearest.position[1],nearest.position[2]);
        const auto right=project(frame.plan,1,nearest.position[0],nearest.position[1],nearest.position[2]);
        require(left && right && std::abs((*left)[0]-(*right)[0]-2*background_offset(frame.plan,0))>.01,
            "Ground used only infinite scenery displacement instead of finite stereo disparity");
        for(float slider:{0.F,.5F,1.F}) {
            frame.plan=plan_frame(slider,true,ScreenUse::world);prepared=layers.prepare(frame);
            require(saved.size()==prepared.before_models.vertices.size() && std::equal(saved.begin(),saved.end(),prepared.before_models.vertices.begin()),
                "Slider changed shared terrain vertices instead of eye projection");
            require(layers.work()[0].decodes==work[0].decodes && layers.work()[0].colour_updates==work[0].colour_updates,
                "Slider reran source landscape tile/colour preparation");
            validate_pica_frame(prepared.before_models,lower.view());
        }
    }
    scene->flow=simulation::GameFlowState::ex_pregame_menu;
    require(!native_landscape_scene(frame),"EX menu terrain replaced its authored planar background");
    scene->flow=simulation::GameFlowState::gameplay;ppu->tunnel_scene=true;
    auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
    require(!native_landscape_scene(frame),"Tunnel artwork was misclassified as outdoor ground");
}
void atlas_landscape_depth() {
    for(int roll:{-3,0,3}) for(unsigned offset:{232U,1008U,8190U}) {
        auto frame=source(simulation::GameFlowState::gameplay,2);
        auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
        scene->background_landscape=true;scene->landscape_grid_height=-145;scene->landscape_atlas_origin=232;
        frame.current=frame.previous=scene;frame.plan=plan_frame(1,true,ScreenUse::world);
        auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);
        ppu->main_screen=2;ppu->bg2_scroll_y=232;ppu->bg2_vertical_offsets_enabled=true;
        for(unsigned y=0;y<8;++y) ppu->vram[0x2040+y*2]=std::uint8_t(y%2?0xaa:0x55);
        for(unsigned y=0;y<32;++y) for(unsigned x=0;x<32;++x) {
            const unsigned character=x%16==7 && y%8==3?2:x%16==3 && y>16?0:1;
            const unsigned tile=character|((y/4)<<10)|((x%2)<<14),at=0x6400+(y*32+x)*2;
            ppu->vram[at]=std::uint8_t(tile);ppu->vram[at+1]=std::uint8_t(tile>>8);
        }
        for(unsigned i=0;i<32;++i) {
            const unsigned word=0x4000|((int(offset)+roll*int(i+1))&8191),at=0x5f40+i*2;
            ppu->vram[at]=std::uint8_t(word);ppu->vram[at+1]=std::uint8_t(word>>8);
        }
        ppu->cgram[17]=0; // Nonzero source index with opaque black RGB.
        auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
        GameLayers native,baseline;const auto gpu=native.prepare(frame,pica_vertex_limit),cpu=baseline.prepare(frame);
        require(gpu.before_models.textures.size()==1 && gpu.before_models.textures[0].width==256,
            "Supported rolled landscape did not use its compact source atlas");
        const auto& group=gpu.before_models;
        require(group.draws.size()==2 && group.draws[0].space==PicaSpace::scenery
            && group.draws[1].space==PicaSpace::world && group.draws[1].projected_uv
            && group.draws[1].depth_test && group.draws[1].depth_write,
            "Atlas terrain lost source depth, infinity ordering or projected UV");
        require(group.textures[0].source_layers.empty() && group.vertices.size()<pica_vertex_limit,
            "Atlas terrain duplicated provenance or exceeded native geometry capacity");
        const auto plane=source_landscape_plane(frame);const auto& finite=group.draws[1];
        for(unsigned i=finite.first;i<finite.first+finite.count;++i) {
            const auto& p=group.vertices[i].position;const double z=p[2];
            const double x=200+frame.plan.focal_x*p[0]/z,y=120-frame.plan.focal_y*p[1]/z;
            const double plane_error=std::abs(z*(y-plane.centre-plane.slope*(x-200))-145*frame.plan.focal_y);
            // World vertices are float on PICA. Near the far clip, subtracting
            // two rolled horizon terms magnifies their representational ULP.
            const double rounding=4*std::numeric_limits<float>::epsilon()*(std::abs(z*(y-plane.centre))
                +std::abs(z*plane.slope*(x-200))+145*frame.plan.focal_y);
            require(plane_error<std::max(.2,rounding),
                "Atlas ground vertices departed from the original camera plane");
            require(z>=frame.plan.near_plane-.02 && z<=frame.plan.far_plane+.02,
                "Atlas ground omitted the near/far source clip");
        }
        for(unsigned y=1;y<224;y+=9) for(unsigned x=1;x<256;x+=11)
            require(mono_receiver_pixel(group,x,y)==mono_receiver_pixel(cpu.before_models,x,y),
                "Finite atlas changed mono indexed colours, UV registration, transparency or BG2 ownership");
        const auto saved_vertices=std::vector<PicaVertex>(group.vertices.begin(),group.vertices.end());
        const auto before=native.work();
        frame.plan=plan_frame(.5F,true,ScreenUse::world);
        const auto slider=native.prepare(frame,pica_vertex_limit);
        require(slider.before_models.vertices.size()==saved_vertices.size()
            && std::equal(saved_vertices.begin(),saved_vertices.end(),slider.before_models.vertices.begin())
            && native.work()[0].decodes==before[0].decodes,
            "Slider rebuilt source atlas geometry instead of changing eye matrices");
        const auto rejected=native.prepare(frame,5),reference=baseline.prepare(frame);
        require(rejected.before_models.textures[0].width==reference.before_models.textures[0].width,
            "Whole scene atlas overflow published truncated terrain instead of the complete raster");
        for(unsigned y:{3U,113U,219U}) for(unsigned x:{3U,127U,251U})
            require(mono_receiver_pixel(rejected.before_models,x,y)==mono_receiver_pixel(reference.before_models,x,y),
                "Terrain budget fallback changed original coverage");
        PicaBg2Tiles owner;GameScenery receiver;const auto policy=game_layer_plan(frame);
        const auto tiles=owner.prepare(ppu,policy.before_models,frame.plan,15,0,pica_vertex_limit,32,true);
        require(bool(tiles),"Receiver transaction fixture did not create an atlas");
        const auto accepted=receiver.prepare_tiles(frame,*tiles,owner.coverage_guard(),pica_vertex_limit);
        require(bool(accepted),"Receiver transaction fixture failed");
        const auto old=std::vector<PicaVertex>(accepted->vertices.begin(),accepted->vertices.end());
        require(!receiver.prepare_tiles(frame,*tiles,owner.coverage_guard(),unsigned(tiles->vertices.size())),
            "Finite geometry overflow was not rejected atomically");
        require(old.size()==accepted->vertices.size() && std::equal(old.begin(),old.end(),accepted->vertices.begin()),
            "Failed receiver preparation invalidated a previous borrowed complete frame");
    }
}
void unique_landscape_policy() {
    using enum simulation::GameFlowState;
    for(auto flow:{gameplay,training,stage_results,intro,title,ex_pregame_menu,controls_type}) for(bool right:{false,true}) {
        auto frame=source(flow,2);auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
        scene->background_landscape=true;scene->landscape_grid_height=-145;scene->landscape_atlas_origin=248;
        scene->background_landscape_unique_half=!right;scene->background_landscape_unique_right_half=right;
        frame.current=frame.previous=scene;
        const auto policy=game_layer_plan(frame);
        const bool world=flow==gameplay || flow==training || flow==stage_results;unsigned observed=0;
        for(const auto* batch:{&policy.before_models,&policy.after_models}) for(const auto& pass:batch->passes) {
            const bool expected=world && pass.layer==PpuLayer::bg2;
            require(pass.single_occurrence_sky_half.has_value()==expected,
                "Unique sky half ignored in native landscape or applied to menu/non-BG2 artwork");
            if(expected) {
                require(pass.single_occurrence_sky_half==PpuUniqueSkyHalf{right,360},
                    "Native unique sky half lost verified atlas orientation or horizon rows");
                ++observed;
            }
        }
        require(!world || observed>0,"Unique-half policy fixture had no native world pass");
    }
    auto frame=source(gameplay,2);auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
    scene->background_landscape=true;scene->landscape_grid_height=-145;
    scene->background_landscape_unique_half=scene->background_landscape_unique_right_half=true;
    frame.current=frame.previous=scene;bool failed=false;
    try {static_cast<void>(game_layer_plan(frame));} catch(const std::invalid_argument&) {failed=true;}
    require(failed,"Ambiguous source sky half silently substituted another atlas policy");
}
void results_landscape_priorities() {
    using enum simulation::GameFlowState;
    auto frame=source(stage_results,2);
    auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
    scene->background_landscape=true;scene->landscape_grid_height=-145;scene->landscape_atlas_origin=232;
    frame.current=frame.previous=scene;
    auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);
    ppu->cgram[33]=0; // Nonzero index: black ground must remain opaque.
    const auto tile=[&](unsigned row,unsigned col,unsigned word) {
        const unsigned at=0x6400+(row*32+col)*2;
        ppu->vram[at]=std::uint8_t(word);ppu->vram[at+1]=std::uint8_t(word>>8);
    };
    for(unsigned row=0;row<28;++row) for(unsigned col=0;col<32;++col)
        tile(row,col,0x0801|(row>=12 && row<=18 && col>=7 && col<=19?0x2000:0));
    for(unsigned priority=0;priority<4;++priority) {
        ppu->oam[priority*4]=std::uint8_t(56+priority*24);ppu->oam[priority*4+1]=128;
        ppu->oam[priority*4+2]=1;ppu->oam[priority*4+3]=std::uint8_t(priority<<4);
    }
    auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
    StereoSettings settings;settings.separation=64;settings.convergence=16;settings.strength=2;
    frame.plan=plan_frame(1,true,ScreenUse::world,settings);
    const auto policy=game_layer_plan(frame);
    require(native_landscape_scene(frame) && policy.before_model_groups.size()==4,
        "Results landscape was flattened or source BG2/OBJ boundaries were collected out of order");
    std::vector<PpuPass> flattened;
    for(const auto& batch:policy.before_model_groups) {
        for(const auto& pass:batch.passes) {
            require(batch.landscape_receiver==(pass.layer==PpuLayer::bg2)
                && (batch.space==PicaSpace::scenery)==batch.landscape_receiver,
                "Results text/sprites entered a terrain receiver or BG2 stayed at HUD depth");
            flattened.push_back(pass);
        }
    }
    require(flattened==policy.before_models.passes,"Results grouping changed authored priority sequence");
    PpuBatch raw=policy.before_models;raw.passes.insert(raw.passes.end(),policy.after_models.passes.begin(),policy.after_models.passes.end());
    PicaRaster oracle;const auto authored=oracle.prepare(ppu,raw,frame.plan,15);
    for(unsigned priority=0;priority<4;++priority)
        require(pixel(authored,56+priority*24,128).second==(priority<2?2U:16U),
            "Results fixture did not exercise both occluded and visible source OBJ priorities");
    GameLayers layers;PicaComposite composite;Canvas lower;
    for(unsigned budget:{0U,pica_vertex_limit,1U}) {
        auto prepared=layers.prepare(frame,budget);
        auto native=composite.prepare(frame.plan,std::array{prepared.before_models,prepared.after_models},lower.view());
        unsigned finite=0,screen=0;
        for(const auto& draw:native.draws) {
            if(draw.space==PicaSpace::world) {
                ++finite;require(draw.source_layer==2 && draw.projected_uv && draw.depth_test && draw.depth_write,
                    "Results finite ground lost homogeneous source UV or depth ownership");
            } else if(draw.space==PicaSpace::screen) {
                ++screen;require(!draw.depth_test && !draw.depth_write && (draw.source_layer==16
                    || (draw.texture!=pica_no_texture && validate_pica_layers(native.textures[draw.texture])==16)),
                    "Results screen text/sprite group acquired terrain depth");
            }
        }
        require(finite>0 && screen>0,"Results fixture lost its finite ground or screen-space sprites");
        for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x)
            require(mono_receiver_pixel(native,x,y)==pixel(authored,x,y),
                "Results receiver/atlas/fallback changed source pixels, black opacity or BG2/OBJ priority");
        const auto work=layers.work();
        for(float slider:{0.F,.5F,1.F}) {
            frame.plan=plan_frame(slider,true,ScreenUse::world,settings);prepared=layers.prepare(frame,budget);
            native=composite.prepare(frame.plan,std::array{prepared.before_models,prepared.after_models},lower.view());
            validate_pica_frame(native,lower.view());
            require(layers.work()[0].decodes==work[0].decodes && layers.work()[0].colour_updates==work[0].colour_updates,
                "Results slider reran source landscape/OBJ decoding or recolouring");
        }
    }
    raster->boss_roll=true;require(!native_landscape_scene(frame),"Boss-roll panels entered retained results terrain");
    raster->boss_roll=false;scene->flow=ex_pregame_menu;
    require(!native_landscape_scene(frame) && game_layer_plan(frame).before_model_groups.empty(),
        "Results terrain policy leaked into the EX pre-game menu");
}
void atlas_water_depth() {
    for(unsigned height:{64U,128U}) for(unsigned size:{0U,3U}) {
        auto frame=source(simulation::GameFlowState::gameplay,1);
        auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
        scene->background_water_surround=true;scene->camera.y=std::int16_t(-int(height));scene->shadow_height=0;
        frame.current=frame.previous=scene;frame.plan=plan_frame(1,true,ScreenUse::world);
        auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);
        ppu->main_screen=2;ppu->bg2_screen_size=std::uint8_t(size);
        ppu->bg2_scanline_scroll_enabled=ppu->bg2_horizontal_offsets_enabled=true;
        for(unsigned y=0;y<224;++y) {
            ppu->bg2_horizontal_offsets[y]=std::int16_t(-400+int(y/32)*9);
            ppu->bg2_scanline_scroll_y[y]=std::int16_t(y<112?-230:257);
        }
        for(unsigned y=0;y<8;++y) ppu->vram[0x2040+y*2]=std::uint8_t(y%2?0xaa:0x55);
        const unsigned pages=((size&1)?2:1)*((size&2)?2:1);
        for(unsigned i=0;i<pages*1024;++i) {
            const unsigned character=i%13==7?2:i%11==3?0:1;
            const unsigned tile=character|((i%8)<<10)|((i%2)<<13)|((i%4)<<14),at=0x6400+i*2;
            ppu->vram[at]=std::uint8_t(tile);ppu->vram[at+1]=std::uint8_t(tile>>8);
        }
        ppu->cgram[33]=0; // Opaque black is still source water, not a hole.
        auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
        GameLayers native,baseline;
        const auto gpu=native.prepare(frame,pica_vertex_limit),cpu=baseline.prepare(frame);
        require(!gpu.before_models.textures.empty() && gpu.before_models.textures[0].width==256,
            "Mode-1 source water did not use its compact atlas");
        for(const auto& draw:gpu.before_models.draws) {
            require(draw.source_layer==2,"Water atlas mixed another painter layer into its finite planes");
            if(draw.space==PicaSpace::world) {
                require(draw.projected_uv && draw.depth_test && draw.depth_write,"Water atlas omitted stereo/depth/UV projection");
                for(unsigned i=draw.first;i<draw.first+draw.count;++i) {
                    const auto p=gpu.before_models.vertices[i].position;
                    require(std::abs(std::abs(p[1])-height)<.02 && p[2]>=frame.plan.near_plane-.02
                        && p[2]<=frame.plan.far_plane+.02,"Water atlas changed its source planes or near/far bounds");
                }
            }
        }
        for(int y=-7;y<232;y+=7) for(int x=-71;x<328;x+=9)
            require(mono_receiver_pixel(gpu.before_models,x,y)==mono_receiver_pixel(cpu.before_models,x,y),
                "Water atlas changed wide source pixels, bridge edges, priority holes or opaque black");
        const auto fallback=native.prepare(frame,5);
        for(int y:{-7,1,111,219,231}) for(int x:{-71,1,127,251,327})
            require(mono_receiver_pixel(fallback.before_models,x,y)==mono_receiver_pixel(cpu.before_models,x,y),
                "Water vertex-budget fallback lost complete source coverage");
        PicaBg2Tiles tiles;GameScenery receiver;
        const auto policy=game_layer_plan(frame);
        auto batch=*std::find_if(policy.before_model_groups.begin(),policy.before_model_groups.end(),
            [](const auto& group){return group.water_receiver;});
        const auto atlas=tiles.prepare(ppu,batch,frame.plan,15,0,pica_vertex_limit,source_water_guard(frame),true);
        require(bool(atlas),"Water receiver transaction lacks its source atlas");
        const auto ready=receiver.prepare_water_tiles(frame,*atlas,tiles.coverage_guard(),pica_vertex_limit);
        require(bool(ready),"Water receiver transaction failed");
        const auto saved=std::vector<PicaVertex>(ready->vertices.begin(),ready->vertices.end());
        require(!receiver.prepare_water_tiles(frame,*atlas,tiles.coverage_guard(),1),"Water receiver overflow published partial geometry");
        require(saved.size()==ready->vertices.size() && std::equal(saved.begin(),saved.end(),ready->vertices.begin()),
            "Water receiver rejection invalidated its previous complete frame");
    }
}
void water_depth() {
    auto frame=source(simulation::GameFlowState::gameplay,1);
    auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
    scene->background_water_surround=true;scene->camera.y=-96;scene->shadow_height=0;
    frame.current=frame.previous=scene;frame.plan=plan_frame(1,true,ScreenUse::world);
    GameLayers layers;Canvas lower;
    const auto group=layers.prepare(frame).before_models;validate_pica_frame(group,lower.view());
    const auto finite=std::count_if(group.draws.begin(),group.draws.end(),[](const auto& draw) {
        return draw.space==PicaSpace::world && draw.projected_uv && draw.source_layer==2;
    });
    require(finite>0,"Open-water BG2 remains a flat screen-depth image instead of a finite source receiver");
    for(const auto& draw:group.draws) if(draw.space==PicaSpace::world) {
        require(draw.depth_test && draw.depth_write && draw.projected_uv,"Water discarded its source depth or homogeneous projector");
        for(unsigned i=draw.first;i<draw.first+draw.count;++i)
            require(std::abs(std::abs(group.vertices[i].position[1])-96)<.001,"Water height is an arbitrary stereo plane, not source camera-to-shadow distance");
    }
    using enum simulation::GameFlowState;
    for(auto flow:{gameplay,training,intro,planet_travel,stage_results,game_over,finished,credits}) {
        scene->flow=flow;require(native_water_scene(frame),"Source water depth was restricted to a gameplay-only shortcut");
        const auto water=layers.prepare(frame).before_models;
        require(std::any_of(water.draws.begin(),water.draws.end(),[](const auto& draw) {
            return draw.space==PicaSpace::world && draw.source_layer==2 && draw.projected_uv;
        }),"World transition restored water artwork at flat screen depth");
    }
    for(auto flow:{simulation::GameFlowState::title,simulation::GameFlowState::ex_pregame_menu,
        simulation::GameFlowState::controls_type,simulation::GameFlowState::planet_select}) {
        scene->flow=flow;require(!native_water_scene(frame),"Menu/map water was mistaken for a world receiver");
    }
    scene->flow=simulation::GameFlowState::gameplay;
    auto old=std::make_shared<vr::GameSceneSnapshot>(*scene);old->camera.y=-64;scene->camera.y=-128;frame.previous=old;
    for(double alpha:{0.,.25,.5,1.}) {
        frame.interpolation_alpha=alpha;
        require(source_water_height(frame)==std::lerp(64.,128.,alpha),"Water camera motion stayed locked to a source tick");
    }
    old->flow=simulation::GameFlowState::title;frame.interpolation_alpha=0;
    require(source_water_height(frame)==128,"Water interpolated through a scene change");
    frame.interpolation_alpha=std::numeric_limits<double>::quiet_NaN();bool rejected=false;
    try {static_cast<void>(source_water_height(frame));} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"Non-finite water interpolation was accepted");
}
void water_priority_pixels() {
    for(bool high_sky:{false,true}) for(int height:{48,96}) {
        scenario="Water priorities high-sky="+std::to_string(high_sky)+" height="+std::to_string(height)+": ";
        auto frame=source(simulation::GameFlowState::gameplay,1);
        auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
        scene->background_water_surround=true;scene->camera.y=std::int16_t(-height);scene->shadow_height=0;
        frame.current=frame.previous=scene;
        auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);ppu->bg3_high_priority=high_sky;
        ppu->bg3_screen_base=0x3800; // Full 32x32 maps need non-overlapping 2 KiB banks.
        ppu->cgram[33]=0; // Opaque black water belongs to BG2, never transparency.
        const auto tile=[&](unsigned map,unsigned row,unsigned col,unsigned word) {
            const unsigned at=map+(row*32+col)*2;ppu->vram[at]=std::uint8_t(word);ppu->vram[at+1]=std::uint8_t(word>>8);
        };
        for(unsigned row=0;row<28;++row) for(unsigned col=0;col<32;++col) {
            tile(0x6400,row,col,row<8 || row>=15?0x0801:0);
            tile(0x7000,row,col,0x0c01);
        }
        for(unsigned row=12;row<=16;++row) for(unsigned col=9;col<=19;++col) tile(0x6400,row,col,0x2801);
        for(unsigned row=8;row<=10;++row) for(unsigned col=22;col<=23;++col) tile(0x7000,row,col,0x2c01);
        for(unsigned i=0;i<4;++i) {
            ppu->oam[i*4]=std::uint8_t(56+i*24);ppu->oam[i*4+1]=80;
            ppu->oam[i*4+2]=1;ppu->oam[i*4+3]=std::uint8_t(i<<4);
        }
        auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
        const auto unchanged=*ppu;StereoSettings settings;settings.separation=64;settings.convergence=16;
        frame.plan=plan_frame(1,true,ScreenUse::world,settings);
        GameLayers layers;PicaRaster oracle;PicaComposite composite;Canvas lower;
        auto prepared=layers.prepare(frame);const auto policy=game_layer_plan(frame);
        unsigned sprite_groups=0;
        for(const auto* group:{&prepared.before_models,&prepared.after_models}) for(const auto& draw:group->draws)
            if(draw.space==PicaSpace::screen && draw.texture!=pica_no_texture) {
                const auto& image=group->textures[draw.texture];
                require(validate_pica_layers(image)==16,"Water screen group lost isolated OBJ ownership");
                require(image.width<=80 && image.height==8,"Tiny water-scene sprites retained full guarded pages");
                ++sprite_groups;
            }
        require(sprite_groups==(high_sky?3U:4U),"Water fixture lost a nonempty contiguous OBJ painter group");
        for(const auto& batches:{policy.before_model_groups,policy.after_model_groups}) for(const auto& batch:batches) {
            require(!batch.passes.empty(),"Water split inserted an empty source group");
            for(const auto& pass:batch.passes) {
                require(batch.water_receiver==(pass.layer==PpuLayer::bg2),"Water receiver contains sky/sprites or BG2 stayed planar");
                require((batch.space==PicaSpace::scenery)==(pass.layer==PpuLayer::bg2 || pass.layer==PpuLayer::bg3),
                    "Water sky became screen-depth or sprites were moved into world scenery");
            }
        }
        auto raw_policy=policy.before_models;raw_policy.passes.insert(raw_policy.passes.end(),policy.after_models.passes.begin(),policy.after_models.passes.end());
        const auto raw=oracle.prepare(ppu,raw_policy,frame.plan,15);
        for(unsigned i=0;i<4;++i) require(pixel(raw,56+i*24,80).second==16,
            "Water fixture did not exercise all four visible OBJ priorities");
        const auto native=composite.prepare(frame.plan,std::array{prepared.before_models,prepared.after_models},lower.view());
        for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x)
            require(mono_receiver_pixel(native,x,y)==pixel(raw,x,y),"Water finite surfaces/sky priorities changed canonical source pixels or opaque ownership");
        const auto work=layers.work();
        for(float slider:{0.F,.5F,1.F}) {
            frame.plan=plan_frame(slider,true,ScreenUse::world,settings);prepared=layers.prepare(frame);
            const auto current=layers.work();
            require(current[0].decodes==work[0].decodes && current[1].decodes==work[1].decodes
                && current[0].colour_updates==work[0].colour_updates && current[1].colour_updates==work[1].colour_updates,
                "Water slider reran the source raster/palette walker");
            validate_pica_frame(composite.prepare(frame.plan,std::array{prepared.before_models,prepared.after_models},lower.view()),lower.view());
        }
        raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->brightness=7;frame.raster=raster;
        prepared=layers.prepare(frame);
        require(layers.work()[0].decodes==work[0].decodes && layers.work()[1].decodes==work[1].decodes,
            "Water brightness fade redecoded its source tiles");
        require(ppu->vram==unchanged.vram && ppu->oam==unchanged.oam && ppu->cgram==unchanged.cgram,
            "Water preparation mutated cartridge VRAM, sprites or palette");
    }
    scenario.clear();
}
void water_eye_coverage() {
    // Independent inverse eye projection, not a comparison of the production
    // guard helper against itself. Check source UV registration and every LCD
    // row, including the overhead plane and the near/far transition.
    for(int height:{8,48,96}) for(float convergence:{16.F,1024.F}) for(float strength:{1.F,2.F}) {
        auto frame=source(simulation::GameFlowState::gameplay,1);
        auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
        scene->background_water_surround=true;scene->camera.y=std::int16_t(-height);
        frame.current=frame.previous=scene;
        auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);
        for(unsigned i=0;i<1024;++i) {ppu->vram[0x6400+i*2]=1;ppu->vram[0x6401+i*2]=8;}
        auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
        StereoSettings settings;settings.separation=64;settings.convergence=convergence;settings.strength=strength;
        PicaRaster artwork;GameScenery receiver;Canvas lower;
        PpuBatch batch{{{PpuLayer::bg2}},PicaSpace::scenery,true};batch.water_receiver=true;
        for(float slider:{1.F,.5F,0.F}) {
            frame.plan=plan_frame(slider,true,ScreenUse::world,settings);
            const auto decoded=artwork.prepare(ppu,batch,frame.plan,15,0,source_water_guard(frame),true);
            const auto group=receiver.prepare_water(frame,decoded,artwork.coverage_guard());
            validate_pica_frame(group,lower.view());
            struct Triangle {std::array<std::array<double,2>,3> p,uv;double area;unsigned image;};
            const auto cross=[](auto a,auto b,auto p){return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]);};
            for(unsigned eye=0;eye<frame.plan.eye_count;++eye) {
                std::vector<Triangle> triangles;
                for(const auto& draw:group.draws) if(draw.space==PicaSpace::world) {
                    const auto matrix=pica_draw_matrix(frame.plan,eye,draw);
                    for(unsigned i=draw.first;i<draw.first+draw.count;i+=3) {
                        Triangle triangle{};triangle.image=draw.texture;
                        const auto& image=group.textures[draw.texture];
                        const auto& origin=decoded.vertices[draw.texture*6].position;
                        for(unsigned k=0;k<3;++k) {
                            const auto& vertex=group.vertices[i+k];const auto& p=vertex.position;
                            std::array<double,4> clip{};
                            for(unsigned row=0;row<4;++row) {
                                clip[row]=matrix[row][3];
                                for(unsigned axis=0;axis<3;++axis) clip[row]+=double(matrix[row][axis])*p[axis];
                            }
                            triangle.p[k]={(1-clip[1]/clip[3])*200,(1-clip[0]/clip[3])*120};
                            triangle.uv[k]={origin[0]+vertex.uv[0]*image.width,origin[1]+vertex.uv[1]*image.height};
                            require(std::abs(triangle.uv[k][0]-(200+frame.plan.focal_x*double(p[0])/p[2]))<.03
                                && std::abs(triangle.uv[k][1]-(120-frame.plan.focal_y*double(p[1])/p[2]))<.03,
                                "Water homogeneous UV no longer registers with its source pixels");
                        }
                        triangle.area=cross(triangle.p[0],triangle.p[1],triangle.p[2]);
                        if(std::abs(triangle.area)>1.e-8) triangles.push_back(triangle);
                    }
                }
                for(unsigned y=0;y<240;++y) for(unsigned x=0;x<400;x+=3) {
                    const std::array<double,2> sample{x+.5,y+.5};
                    const double q=std::abs(sample[1]-120)/(height*double(frame.plan.focal_y));
                    if(q<1./frame.plan.far_plane || q>1./frame.plan.near_plane) continue;
                    const double expected_x=sample[0]-frame.plan.eyes[eye].projection_offset
                        +frame.plan.focal_x*double(frame.plan.eyes[eye].x)*q;
                    bool covered=false;
                    for(const auto& triangle:triangles) {
                        const std::array weights{cross(triangle.p[1],triangle.p[2],sample)/triangle.area,
                            cross(triangle.p[2],triangle.p[0],sample)/triangle.area,cross(triangle.p[0],triangle.p[1],sample)/triangle.area};
                        if(std::any_of(weights.begin(),weights.end(),[](double w){return w< -1.e-6;})) continue;
                        double sx=0,sy=0;
                        for(unsigned k=0;k<3;++k) {sx+=weights[k]*triangle.uv[k][0];sy+=weights[k]*triangle.uv[k][1];}
                        require(std::abs(sx-expected_x)<.03 && std::abs(sy-sample[1])<.03,
                            "Water eye samples wrong source pixels or perspective-stretches its artwork");
                        covered=true;
                    }
                    require(covered,"Finite water left an uncovered edge in an active eye");
                }
            }
        }
        require(artwork.work().decodes==1 && artwork.work().colour_updates==1,
            "Reducing water slider reran cartridge decoding or palette colour conversion");
    }
}
void corridor_source_symbols() {
    const std::string constants=
        "LTUNNEL_MINX $ffff88\nLTUNNEL_MAXX $000078\nLTUNNEL_MINY $ffff88\nLTUNNEL_MAXY $000000\n"
        "MTUNNEL_MINX $ffffa6\nMTUNNEL_MAXX $00005a\nMTUNNEL_MINY $ffff88\nMTUNNEL_MAXY $000000\n"
        "STUNNEL_MINX $ffffc4\nSTUNNEL_MAXX $00003c\nSTUNNEL_MINY $ffff88\nSTUNNEL_MAXY $000000\n"
        "COLONY_MINX $ffff56\nCOLONY_MAXX $000078\nCOLONY_MINY $ffff88\nCOLONY_MAXY $000000\n"
        "KTUNNEL_MINX $ffff38\nKTUNNEL_MAXX $0000c8\nKTUNNEL_MINY $fffeca\nKTUNNEL_MAXY $000000\n";
    const std::string backgrounds=
        "BGLISTS $108000\nBG_1_1I $108003\nBG_1_3B $108006\nBG_2_3C $108009\n"
        "BG_1_6B $10800c\nBG_1_7A $10800f\nBG_2_6B $108012\nBG_2_6C $108015\n"
        "BG_3_4C $108018\nBG_5_2A $10801b\nBG_5_2Z $10801e\nBG_2_6A $108021\n";
    const auto resolved=vr::source_corridors(assets::SymbolMap::parse(backgrounds+constants));
    for(unsigned i=0;i<10;++i) {
        const int width=i==1?90:i==2?60:120;
        require(resolved[i] && resolved[i]->background==(i+1)*3
            && resolved[i]->bounds==vr::SourceCorridorBounds{std::int16_t(-width),std::int16_t(width),-120,0},
            "Source-linked corridor dimensions changed or Gekkou incorrectly used unused KTUNNEL bounds");
    }
    require(resolved[10] && resolved[10]->background==33
        && resolved[10]->bounds==vr::SourceCorridorBounds{-170,120,-120,0,14},
        "Colony movement limit was incorrectly treated as a physical left wall");
    const auto incomplete=vr::source_corridors(assets::SymbolMap::parse(backgrounds+
        "KTUNNEL_MINX $ffff38\nKTUNNEL_MAXX $0000c8\nKTUNNEL_MINY $fffeca\nKTUNNEL_MAXY $000000\n"));
    for(const auto& entry:incomplete) require(!entry,"Missing authored tunnel symbols fabricated a receiver");
    const auto unrelated=vr::source_corridors(assets::SymbolMap::parse(constants+
        "BGLISTS $108000\nBG_5_2A $118003\nBG_5_2Z $107fff\nBG_6_6C $108009\n"));
    for(const auto& entry:unrelated) require(!entry,"Foreign-bank/pre-list/final-room symbols became closed corridors");
    bool rejected=false;
    try {static_cast<void>(vr::source_corridors(assets::SymbolMap::parse(backgrounds+
        "LTUNNEL_MINX $000078\nLTUNNEL_MAXX $000078\nLTUNNEL_MINY $ffff88\nLTUNNEL_MAXY $000000\n")));}
    catch(const std::runtime_error&) {rejected=true;}
    require(rejected,"Degenerate source corridor bounds were accepted");
}
void corridor_depth() {
    using enum simulation::GameFlowState;
    for(unsigned mode:{1U,2U}) for(int half_width:{60,90,120}) {
        auto frame=source(gameplay,mode);auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
        scene->camera.y=-60;scene->camera.x=10;scene->source_vanishing_point={112,96};
        scene->view_matrix={32767,0,0,0,32767,0,0,0,32767};
        scene->background_corridor=vr::SourceCorridorBounds{std::int16_t(-half_width),std::int16_t(half_width),-120,0};
        frame.current=frame.previous=scene;
        auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);ppu->tunnel_scene=true;
        ppu->bg2_scanline_scroll_enabled=true;
        for(unsigned y=0;y<224;++y) ppu->bg2_scanline_scroll_y[y]=std::int16_t(y<112?0:128);
        for(unsigned i=0;i<1024;++i) {
            const unsigned tile=1|((i%3+1)<<10)|(i%2?0x2000:0),at=0x6400+i*2;
            ppu->vram[at]=std::uint8_t(tile);ppu->vram[at+1]=std::uint8_t(tile>>8);
        }
        ppu->cgram[17]=0;
        for(unsigned i=0;i<4;++i) {
            ppu->oam[i*4]=std::uint8_t(56+i*24);ppu->oam[i*4+1]=80;
            ppu->oam[i*4+2]=1;ppu->oam[i*4+3]=std::uint8_t(i<<4);
        }
        auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
        const auto unchanged=*ppu;StereoSettings settings;settings.separation=64;settings.convergence=16;
        frame.plan=plan_frame(1,true,ScreenUse::world,settings);
        const auto policy=game_layer_plan(frame);
        require(native_corridor_scene(frame) && !policy.before_model_groups.empty(),"Authored enclosed corridor remained at screen depth");
        std::vector<PpuPass> flattened;
        for(const auto& batch:policy.before_model_groups) for(const auto& pass:batch.passes) {
            require(batch.corridor_receiver==(pass.layer==PpuLayer::bg2),"Corridor receiver contains OBJ/BG3 or missed a BG2 priority");
            flattened.push_back(pass);
        }
        require(flattened==policy.before_models.passes,"Corridor splitting changed source painter order");
        GameLayers layers;PicaRaster oracle;PicaComposite composite;Canvas lower;
        auto prepared=layers.prepare(frame);
        auto raw_policy=policy.before_models;raw_policy.passes.insert(raw_policy.passes.end(),policy.after_models.passes.begin(),policy.after_models.passes.end());
        const auto raw=oracle.prepare(ppu,raw_policy,frame.plan,15);
        const auto native=composite.prepare(frame.plan,std::array{prepared.before_models,prepared.after_models},lower.view());
        unsigned finite{};
        for(const auto& draw:prepared.before_models.draws) if(draw.space==PicaSpace::world) {
            ++finite;require(draw.projected_uv && draw.depth_test && draw.depth_write && draw.source_layer==2,
                "Corridor surface lost source projector, physical depth or CGADSUB ownership");
            require(prepared.before_models.textures[draw.texture].source_layers.empty(),"Isolated corridor retains redundant A8 residency");
        }
        require(finite>=4,"Corridor has no finite side walls, floor and ceiling");
        for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x)
            require(mono_receiver_pixel(native,x,y)==pixel(raw,x,y),"Corridor geometry changed canonical pixels, opaque black or source priority");
        const auto work=layers.work();
        for(float slider:{.5F,0.F,1.F}) {
            frame.plan=plan_frame(slider,true,ScreenUse::world,settings);prepared=layers.prepare(frame);
            const auto current=layers.work();
            require(current[0].decodes==work[0].decodes && current[1].decodes==work[1].decodes
                && current[0].colour_updates==work[0].colour_updates && current[1].colour_updates==work[1].colour_updates,
                "Corridor slider redecoded or recoloured source artwork");
            validate_pica_frame(composite.prepare(frame.plan,std::array{prepared.before_models,prepared.after_models},lower.view()),lower.view());
        }
        auto previous=std::make_shared<vr::GameSceneSnapshot>(*scene);previous->camera.x=-10;frame.previous=previous;frame.interpolation_alpha=.5;
        const auto interpolated=source_corridor_planes(frame);
        require(std::abs(interpolated[0][0]+32768./32767/(half_width*256.))<1.e-9,
            "Corridor receiver did not interpolate the same source camera as models");
        previous->scene_epoch=1;
        require(std::abs(source_corridor_planes(frame)[0][0]+32768./32767/((half_width+10)*256.))<1.e-9,
            "Corridor blended a source epoch discontinuity");
        frame.interpolation_alpha=std::numeric_limits<double>::quiet_NaN();bool rejected=false;
        try {static_cast<void>(source_corridor_planes(frame));} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected,"Corridor accepted a non-finite source interpolation");frame.interpolation_alpha=1;
        for(auto flow:{title,ex_pregame_menu,controls_type,controls_choice,planet_select,continue_choice}) {
            scene->flow=flow;require(!native_corridor_scene(frame),"Native menu/map was incorrectly put inside finite corridor geometry");
        }
        scene->flow=gameplay;scene->camera.x=half_width;
        require(native_corridor_scene(frame),"Authored camera-on-wall corridor was rejected");
        for(const auto q:source_corridor_planes(frame)) for(const double value:q)
            require(std::isfinite(value),"Camera-on-wall corridor divided by zero");
        scene->camera.x=half_width-20;previous->camera.x=half_width+20;previous->scene_epoch=scene->scene_epoch;
        frame.previous=previous;frame.interpolation_alpha=.5;
        require(source_corridor_planes(frame)[1]==std::array<double,3>{},
            "Continuous wall crossing cut the source camera instead of handling the on-wall midpoint");
        require(std::abs(source_corridor_planes(frame)[0][0]+32768./32767/(2*half_width*256.))<1.e-9,
            "Exterior-to-interior camera interpolation disagrees with source model timing");
        for(int x:{half_width,half_width+40,-half_width-40}) {
            scene->camera.x=x;frame.previous=scene;frame.interpolation_alpha=1;
            const auto outside=layers.prepare(frame);
            const auto canonical=composite.prepare(frame.plan,std::array{outside.before_models,outside.after_models},lower.view());
            for(unsigned y=0;y<224;++y) for(unsigned column=0;column<256;++column)
                require(mono_receiver_pixel(canonical,column,y)==pixel(raw,column,y),
                    "Exterior/on-wall receiver changed canonical source colour, opacity or priority");
        }
        scene->camera.x=10;scene->background_corridor.reset();
        require(!native_corridor_scene(frame),"Generic source tunnel flag fabricated physical boss-room dimensions");
        require(ppu->vram==unchanged.vram && ppu->oam==unchanged.oam && ppu->cgram==unchanged.cgram,"Corridor presentation mutated cartridge artwork");
    }
}
void corridor_eye_coverage(bool open_left=false,bool exterior=false,bool vertical=false) {
    // Independent slab intersections in world axes; never call the production
    // plane/guard helper for expected depth or source texture coordinates.
    const std::array camera_positions=vertical?std::array{-240,-120,0,120,1836}:exterior?std::array{-180,-120,60,120,180}:
        open_left?std::array{-4000,-170,-10,10,55}:std::array{-55,-10,0,10,55};
    for(int half_width:{60,90,120}) for(int camera_position:camera_positions) for(float convergence:{16.F,1024.F})
        for(float strength:{1.F,2.F}) for(int yaw:{-1,0,1}) {
        auto frame=source(simulation::GameFlowState::gameplay,open_left?1:2);
        auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
        const int camera_x=vertical?10:camera_position,camera_y=vertical?camera_position:-60;
        scene->camera.x=camera_x;scene->camera.y=camera_y;
        const std::int16_t sine=std::int16_t(yaw*12539),cosine=yaw?30273:32767;
        scene->view_matrix=vertical?simulation::MatrixQ15{32767,0,0,0,cosine,sine,0,std::int16_t(-sine),cosine}:
            simulation::MatrixQ15{cosine,0,sine,0,32767,0,std::int16_t(-sine),0,cosine};
        scene->background_corridor=vr::SourceCorridorBounds{std::int16_t(-half_width),std::int16_t(half_width),-120,0,
            std::uint8_t(open_left?14:15)};
        frame.current=frame.previous=scene;
        auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);ppu->tunnel_scene=!open_left;
        for(unsigned i=0;i<1024;++i) {ppu->vram[0x6400+i*2]=1;ppu->vram[0x6401+i*2]=8;}
        auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
        StereoSettings settings;settings.separation=64;settings.convergence=convergence;settings.strength=strength;
        PicaRaster artwork;GameScenery receiver;Canvas lower;
        PpuBatch batch{{{PpuLayer::bg2}},PicaSpace::scenery,true};batch.corridor_receiver=true;batch.corridor_open_left=open_left;
        for(float slider:{1.F,.5F,0.F}) {
            frame.plan=plan_frame(slider,true,ScreenUse::world,settings);
            const auto decoded=artwork.prepare(ppu,batch,frame.plan,15,0,source_corridor_guard(frame),true);
            const auto group=receiver.prepare_corridor(frame,decoded,artwork.coverage_guard());validate_pica_frame(group,lower.view());
            struct Triangle {std::array<std::array<double,2>,3> p,uv;std::array<double,3> q;double area;};
            const auto cross=[](auto a,auto b,auto p){return (b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]);};
            for(unsigned eye=0;eye<frame.plan.eye_count;++eye) {
                std::vector<Triangle> triangles;
                for(const auto& draw:group.draws) if(draw.space==PicaSpace::world) {
                    const auto matrix=pica_draw_matrix(frame.plan,eye,draw);const auto& image=group.textures[draw.texture];
                    const auto& origin=decoded.vertices[draw.texture*6].position;
                    for(unsigned i=draw.first;i<draw.first+draw.count;i+=3) {
                        Triangle triangle{};
                        for(unsigned k=0;k<3;++k) {
                            const auto& v=group.vertices[i+k];std::array<double,4> clip{};
                            for(unsigned row=0;row<4;++row) {
                                clip[row]=matrix[row][3];for(unsigned axis=0;axis<3;++axis) clip[row]+=double(matrix[row][axis])*v.position[axis];
                            }
                            triangle.p[k]={(1-clip[1]/clip[3])*200,(1-clip[0]/clip[3])*120};triangle.q[k]=1./v.position[2];
                            triangle.uv[k]={origin[0]+v.uv[0]*image.width,origin[1]+v.uv[1]*image.height};
                        }
                        triangle.area=cross(triangle.p[0],triangle.p[1],triangle.p[2]);
                        if(std::abs(triangle.area)>1.e-8) triangles.push_back(triangle);
                    }
                }
                for(unsigned y=0;y<240;++y) for(unsigned x=0;x<400;x+=3) {
                    const std::array<double,2> sample{x+.5,y+.5};
                    const double scale=32767./32768.,c=cosine/32768.,s=sine/32768.,norm=c*c+s*s;
                    const double ex=camera_x+frame.plan.eyes[eye].x*(vertical?1/scale:c/norm);
                    const double rx=vertical?(sample[0]-200-frame.plan.eyes[eye].projection_offset)/(256*scale):
                        (c*(sample[0]-200-frame.plan.eyes[eye].projection_offset)/256+s)/norm;
                    const double ry=vertical?(c*(sample[1]-120)/256+s)/norm:(sample[1]-120)/(256*scale);
                    // An eye outside the authored cross-section first sees an
                    // entry face, not an unbounded opposite floor/wall. Test
                    // all bounded faces, retaining only forward intersections.
                    double z=std::numeric_limits<double>::infinity();
                    if(rx!=0) for(double wall:{-double(half_width),double(half_width)}) {
                        if(open_left && wall<0) continue;
                        const double t=(wall-ex)/rx,world_y=camera_y+ry*t;
                        // A ray exactly through a corner belongs to both closed
                        // faces; floating evaluation must not exclude it twice.
                        if(t>=frame.plan.near_plane && t<=frame.plan.far_plane && world_y>=-120-1.e-9 && world_y<=1.e-9) z=std::min(z,t);
                    }
                    if(ry!=0) for(double wall:{-120.,0.}) {
                        const double t=(wall-camera_y)/ry,world_x=ex+rx*t;
                        if(t>=frame.plan.near_plane && t<=frame.plan.far_plane && (open_left || world_x>=-half_width-1.e-9) && world_x<=half_width+1.e-9) z=std::min(z,t);
                    }
                    const double expected_x=std::clamp(sample[0]-frame.plan.eyes[eye].projection_offset
                        +256.*frame.plan.eyes[eye].x/z,-double(artwork.coverage_guard()),400.+artwork.coverage_guard());
                    bool covered=false;double nearest_q=0,nearest_x=0,nearest_y=0;
                    for(const auto& triangle:triangles) {
                        const std::array weights{cross(triangle.p[1],triangle.p[2],sample)/triangle.area,
                            cross(triangle.p[2],triangle.p[0],sample)/triangle.area,cross(triangle.p[0],triangle.p[1],sample)/triangle.area};
                        if(std::any_of(weights.begin(),weights.end(),[](double w){return w< -1.e-9;})) continue;
                        double q=0,sx=0,sy=0;
                        for(unsigned k=0;k<3;++k) {q+=weights[k]*triangle.q[k];sx+=weights[k]*triangle.uv[k][0];sy+=weights[k]*triangle.uv[k][1];}
                        if(q>nearest_q) {nearest_q=q;nearest_x=sx;nearest_y=sy;}
                        covered=true;
                    }
                    if(covered!=std::isfinite(z)) scenario="corridor width="+std::to_string(half_width)+" camera="+
                        std::to_string(camera_x)+","+std::to_string(camera_y)+" open="+std::to_string(open_left)+
                        " vertical="+std::to_string(vertical)+" strength="+std::to_string(strength)+
                        " convergence="+std::to_string(convergence)+" rotation="+std::to_string(yaw)+
                        " slider="+std::to_string(slider)+" eye="+std::to_string(eye)+" pixel="+
                        std::to_string(x)+","+std::to_string(y)+" z="+std::to_string(z)+" q="+std::to_string(nearest_q)+
                        " eye-x="+std::to_string(frame.plan.eyes[eye].x)+" offset="+std::to_string(frame.plan.eyes[eye].projection_offset)+
                        " hit="+std::to_string(ex+rx/nearest_q)+","+std::to_string(camera_y+ry/nearest_q)+": ";
                    require(covered==std::isfinite(z),"Finite corridor missed a physical face or invented an unbounded wall");
                    if(!covered) continue;
                    require(std::abs(nearest_q-1./z)<1.e-5 && std::abs(nearest_x-expected_x)<.05 && std::abs(nearest_y-sample[1])<.05,
                        "Nearest bounded corridor face has wrong physical depth or source UV registration");
                }
            }
        }
        require(artwork.work().decodes==1 && artwork.work().colour_updates==1,"Corridor slider redecoded edge-clamped artwork");
    }
}
void colony_depth() {
    auto frame=source(simulation::GameFlowState::gameplay,1);
    auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
    scene->background_corridor=vr::SourceCorridorBounds{-170,120,-120,0,14};
    scene->camera.x=-400;scene->camera.y=-60;scene->view_matrix={32767,0,0,0,32767,0,0,0,32767};
    frame.current=frame.previous=scene;const auto unchanged=*frame.raster->ppu;
    require(native_corridor_scene(frame) && !native_panorama_scene(frame) && !native_water_scene(frame),
        "Source WATER colony became a flat panorama/water plane or rejected an open-side camera");
    require(source_corridor_planes(frame)[0]==std::array<double,3>{},"Colony's nonexistent left wall has finite depth");
    auto previous=std::make_shared<vr::GameSceneSnapshot>(*scene);previous->camera.x=-800;
    frame.previous=previous;frame.interpolation_alpha=.5;
    require(std::abs(source_corridor_planes(frame)[1][0]-32768./32767/(720*256.))<1.e-9,
        "Continuous open-side camera travel was not interpolated with the source model clock");
    ++previous->scene_epoch;
    require(std::abs(source_corridor_planes(frame)[1][0]-32768./32767/(520*256.))<1.e-9,
        "Open colony interpolated across a source scene handoff");
    frame.previous=scene;frame.interpolation_alpha=1;
    const auto policy=game_layer_plan(frame);GameLayers layers;PicaRaster before,after;Canvas lower;
    for(const auto& batch:policy.before_model_groups) for(const auto& pass:batch.passes)
        require(batch.corridor_receiver==(pass.layer==PpuLayer::bg2),"Open colony receiver reordered screen-space OBJ/BG3");
    for(float slider:{0.F,.5F,1.F}) {
        frame.plan=plan_frame(slider,true,ScreenUse::world);
        const auto actual=layers.prepare(frame);validate_pica_frame(actual.before_models,lower.view());
        const auto expected_before=before.prepare(frame.raster->ppu,policy.before_models,frame.plan,15);
        const auto expected_after=after.prepare(frame.raster->ppu,policy.after_models,frame.plan,15);
        for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x) {
            require(mono_receiver_pixel(actual.before_models,x,y)==pixel(expected_before,x,y),
                "Open colony changed canonical background/OBJ colors, black opacity or painter ownership");
            require(mono_receiver_pixel(actual.after_models,x,y)==pixel(expected_after,x,y),
                "Open colony changed canonical foreground ownership");
        }
    }
    require(*frame.raster->ppu==unchanged,"Open colony presentation changed the raw source WATER flag or palette");
    scene->camera.x=-170;require(native_corridor_scene(frame),"Camera on absent left wall was rejected");
    scene->camera.x=120;require(native_corridor_scene(frame),"Camera on physical right wall lost its bounded receiver policy");
    scene->camera.x=-400;scene->camera.y=0;require(native_corridor_scene(frame),"Camera on colony floor lost its signed-face receiver policy");
}
void panorama_depth() {
    using enum simulation::GameFlowState;
    Canvas lower;
    for(unsigned mode:{1U,2U}) for(auto flow:{intro,gameplay,training,planet_travel,stage_results,game_over,finished,credits}) {
        auto frame=source(flow,mode);
        auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);
        for(unsigned object=0;object<3;++object) {
            ppu->oam[object*4]=std::uint8_t(56+object*4);ppu->oam[object*4+1]=40;
            ppu->oam[object*4+2]=1;ppu->oam[object*4+3]=std::uint8_t(object<<4);
        }
        auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
        const auto unchanged=*ppu;const auto plan=game_layer_plan(frame);
        require(native_panorama_scene(frame) && !plan.before_model_groups.empty(),"Distant world artwork was left at screen depth");
        std::vector<PpuPass> flattened;
        for(const auto& batch:plan.before_model_groups) {
            for(const auto& pass:batch.passes) {
                require((batch.space==PicaSpace::scenery)==(pass.layer==PpuLayer::bg2),"Screen-space sprite/BG3 moved into the distant sky");
                require(batch.visible_scenery_only==(batch.space==PicaSpace::scenery),
                    "Only infinite panorama artwork may omit unseen disjoint-frustum gaps");
                flattened.push_back(pass);
            }
        }
        require(flattened==plan.before_models.passes,"Coordinate-space split reordered authored low/high OBJ/BG priorities");
        GameLayers layers;PicaRaster mono_oracle;
        auto prepared=layers.prepare(frame);
        const auto expected=mono_oracle.prepare(ppu,plan.before_models,frame.plan,15);
        validate_pica_frame(prepared.before_models,lower.view());
        for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x)
            require(pixel(prepared.before_models,x,y)==pixel(expected,x,y),"Split panorama changed native mono colour/opaque-black/priority pixels");
        const auto cached=layers.work();
        std::vector<std::vector<std::uint8_t>> pixels;
        for(const auto& image:prepared.before_models.textures) pixels.emplace_back(image.pixels.begin(),image.pixels.end());
        unsigned resident=512U*256U*4U;
        for(const auto& image:prepared.before_models.textures) resident+=pica_resident_texture_bytes(image);
        require(resident<=3U*1024U*1024U,"Contiguous panorama consumed more than its bounded LCD/artwork residency");
        for(float slider:{0.F,.5F,1.F}) {
            frame.plan=plan_frame(slider,true,ScreenUse::world);prepared=layers.prepare(frame);
            validate_pica_frame(prepared.before_models,lower.view());
            require(layers.work()[0].decodes==cached[0].decodes && layers.work()[0].colour_updates==cached[0].colour_updates,
                "3D slider reran source panorama raster/colour traversal");
            require(prepared.before_models.textures.size()==pixels.size(),"Slider rebuilt a different artwork sequence");
            for(unsigned i=0;i<pixels.size();++i)
                require(std::equal(pixels[i].begin(),pixels[i].end(),prepared.before_models.textures[i].pixels.begin()),"Slider changed source artwork bytes");
            for(const auto& draw:prepared.before_models.draws) {
                if(draw.space==PicaSpace::scenery) require(draw.source_layer==2
                    && prepared.before_models.textures[draw.texture].source_layers.empty(),"Isolated BG2 retained mixed ownership or redundant resident A8");
                std::array<float,2> xs{},ys{};
                for(unsigned eye=0;eye<frame.plan.eye_count;++eye) {
                    const auto matrix=pica_draw_matrix(frame.plan,eye,draw);
                    const auto& point=prepared.before_models.vertices[draw.first].position;
                    std::array<float,4> clip{};
                    for(unsigned row=0;row<4;++row) {
                        clip[row]=matrix[row][3];
                        for(unsigned axis=0;axis<3;++axis) clip[row]+=matrix[row][axis]*point[axis];
                    }
                    xs[eye]=(1-clip[1]/clip[3])*200;ys[eye]=(1-clip[0]/clip[3])*120;
                }
                if(frame.plan.eye_count==2) {
                    const float disparity=draw.space==PicaSpace::scenery?
                        background_offset(frame.plan,0)-background_offset(frame.plan,1):0;
                    require(std::abs(xs[0]-xs[1]-disparity)<.0001 && std::abs(ys[0]-ys[1])<.0001,
                        "Native eye matrices put distant artwork at screen depth or displaced screen sprites/vertical alignment");
                }
            }
        }
        raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->brightness=0;frame.raster=raster;
        prepared=layers.prepare(frame);
        require(layers.work()[0].decodes==cached[0].decodes,"Palette fade redecoded panorama priorities");
        for(const auto& image:prepared.before_models.textures) for(unsigned i=0;i<image.pixels.size();i+=4)
            require(image.pixels[i]==0 && image.pixels[i+1]==0 && image.pixels[i+2]==0,"Fade left stale illuminated panorama pixels");
        require(ppu->vram==unchanged.vram && ppu->oam==unchanged.oam && ppu->cgram==unchanged.cgram,"Panorama preparation mutated source PPU");
        const auto monotonic=layers.work()[0];
        frame=source(controls_type,1);prepared=layers.prepare(frame);
        require(layers.work()[0].decodes>=monotonic.decodes && game_layer_plan(frame).before_model_groups.empty(),
            "Leaving panorama retained split UI policy or reset cumulative work evidence");
    }
    for(unsigned mode:{1U,2U,3U}) for(auto flow:{title,ex_pregame_menu,planet_select,controls_type,controls_choice,continue_choice}) {
        const auto frame=source(flow,mode);
        require(!native_panorama_scene(frame) && game_layer_plan(frame).before_model_groups.empty(),"Menu/map artwork was incorrectly assigned world infinity");
    }
    auto frame=source(gameplay,1);auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
    frame.current=scene;scene->background_water_surround=true;
    require(!native_panorama_scene(frame),"Water cross-section was mistaken for a distant panorama");
    scene->background_water_surround=false;scene->background_landscape=true;
    require(!native_panorama_scene(frame),"Finite landscape was flattened into distant panorama");
    scene->background_landscape=false;
    auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);frame.raster=raster;raster->boss_roll=true;
    require(!native_panorama_scene(frame),"Boss-roll frame was treated as world scenery");
    raster->boss_roll=false;auto ppu=std::make_shared<simulation::SnesPpuState>(*raster->ppu);raster->ppu=ppu;ppu->tunnel_scene=true;
    require(!native_panorama_scene(frame),"Corridor artwork was projected at infinity");
}
void panorama_atlas_priority_pixels() {
    using enum simulation::GameFlowState;
    Canvas lower;
    for(unsigned mode:{1U,2U}) for(bool hdma:{false,true}) {
        auto frame=source(gameplay,mode);
        auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);
        ppu->bg2_horizontal_offsets_enabled=ppu->bg2_scanline_scroll_enabled=hdma;
        for(unsigned y=0;y<224;++y) {
            ppu->bg2_horizontal_offsets[y]=std::int16_t((y/32)*7-400);
            ppu->bg2_scanline_scroll_y[y]=std::int16_t(y<112?-231:257);
        }
        // Source OBJ crosses the low/high BG2 boundary, with opaque black
        // BG2 ink and transparent tile zero both present. Preserve priorities.
        for(unsigned object=0;object<3;++object) {
            ppu->oam[object*4]=std::uint8_t(56+object*4);ppu->oam[object*4+1]=40;
            ppu->oam[object*4+2]=1;ppu->oam[object*4+3]=std::uint8_t(object<<4);
        }
        ppu->cgram[33]=0;
        auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=ppu;frame.raster=raster;
        GameLayers native,reference;const auto expected=reference.prepare(frame);
        const auto prepared=native.prepare(frame,pica_vertex_limit);
        validate_pica_frame(prepared.before_models,lower.view());
        require(std::any_of(prepared.before_models.draws.begin(),prepared.before_models.draws.end(),[&](const auto& draw) {
            return draw.space==PicaSpace::scenery && prepared.before_models.textures[draw.texture].width==256;
        }),"Mode-1/2 panorama never selected a compact source tile atlas");
        for(int y=-8;y<232;++y) for(int x=-32;x<288;++x)
            require(mono_receiver_pixel(prepared.before_models,x,y)==mono_receiver_pixel(expected.before_models,x,y),
                "Panorama atlas changed source mono pixels, edges, priority or opaque black");
        require(prepared.after_models.vertices.size()==expected.after_models.vertices.size()
            && prepared.clear==expected.clear,"Panorama atlas changed foreground or backdrop policy");
        const auto work=native.work();
        raster=std::make_shared<GameRasterSnapshot>(*raster);raster->brightness=0;frame.raster=raster;
        const auto faded=native.prepare(frame,pica_vertex_limit);
        require(native.work()[0].decodes==work[0].decodes,"Panorama atlas fade decoded source geometry again");
        for(const auto& image:faded.before_models.textures) for(unsigned i=0;i<image.pixels.size();i+=4)
            require(image.pixels[i]==0 && image.pixels[i+1]==0 && image.pixels[i+2]==0,
                "Panorama atlas did not apply the native brightness fade");
        raster=std::make_shared<GameRasterSnapshot>(*raster);raster->brightness=15;frame.raster=raster;
        const auto fallback=native.prepare(frame,5);
        require(fallback.before_models.vertices.size()==expected.before_models.vertices.size()
            && fallback.before_models.textures.size()==expected.before_models.textures.size(),
            "Panorama budget rejection published incomplete painter groups");
    }
}
void offscreen_landscape_receiver() {
    Canvas lower;
    for(int horizon:{241,230,120,-7}) {
        auto frame=source(simulation::GameFlowState::gameplay,2);
        auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
        scene->background_landscape=true;scene->landscape_grid_height=-145;scene->landscape_atlas_origin=232;
        scene->background_scroll_override=std::array<std::int16_t,2>{0,std::int16_t(352-horizon)};
        frame.current=frame.previous=scene;
        require(source_landscape_plane(frame).centre==horizon,"Offscreen source horizon fixture was misregistered");
        GameLayers layers;PicaRaster oracle;
        for(auto optics:{StereoSettings{},StereoSettings{2,64,16}}) {
            frame.plan=plan_frame(1,true,ScreenUse::world,optics);
            const auto prepared=layers.prepare(frame);validate_pica_frame(prepared.before_models,lower.view());
            unsigned receivers=0;
            for(const auto& draw:prepared.before_models.draws) if(draw.space==PicaSpace::world) {
                ++receivers;require(draw.source_layer==2 && draw.projected_uv && draw.depth_test,
                    "Partially offscreen terrain lost finite source-owned geometry");
            }
            require((receivers!=0)==(horizon<240),"Terrain outside the LCD invented a receiver or visible terrain lost it");
            const auto expected=oracle.prepare(frame.raster->ppu,game_layer_plan(frame).before_models,frame.plan);
            for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x)
                require(mono_receiver_pixel(prepared.before_models,x,y)==pixel(expected,x,y),
                    "An offscreen/partially visible receiver changed canonical source pixels");
        }
    }
}
void ex_menu_panorama_depth() {
    Canvas lower;
    for(unsigned mode:{1U,2U}) for(unsigned kind=0;kind<5;++kind) {
        auto frame=source(simulation::GameFlowState::ex_pregame_menu,mode);
        auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
        scene->meters.extended=true;
        scene->background_orbital_planet=kind<3;
        scene->background_orbital_thin=kind==1;scene->background_orbital_entry=kind==2;
        scene->background_unique_space=kind==3;scene->background_star_sphere=kind==4;
        frame.current=frame.previous=scene;
        const auto policy=game_layer_plan(frame);
        require(native_panorama_scene(frame),"Verified EX menu surround stayed at screen depth");
        std::vector<PpuPass> flattened;
        for(const auto& group:policy.before_model_groups) for(const auto& pass:group.passes) {
            require((group.space==PicaSpace::scenery)==(pass.layer==PpuLayer::bg2),
                "EX orbital projection moved menu text or OBJ to world depth");
            require(group.visible_scenery_only==(group.space==PicaSpace::scenery),
                "EX menu infinity artwork lost bounded disjoint-eye coverage");
            require(pass.extend_horizontal==(pass.layer==PpuLayer::bg2),
                "EX menu widened screen text/OBJ or left scenery at native 256 columns");
            flattened.push_back(pass);
        }
        require(flattened==policy.before_models.passes,"EX menu depth split changed source painter priorities");
        PicaRaster oracle;GameLayers layers;
        const auto expected=oracle.prepare(frame.raster->ppu,policy.before_models,frame.plan);
        for(auto optics:{StereoSettings{},StereoSettings{2,64,16}}) for(float slider:{1.F,.123F,0.F,1.F}) {
            frame.plan=plan_frame(slider,true,ScreenUse::world,optics);const auto prepared=layers.prepare(frame);
            validate_pica_frame(prepared.before_models,lower.view());
            for(unsigned y=0;y<224;++y) for(unsigned x=0;x<256;++x)
                require(pixel(prepared.before_models,x,y)==pixel(expected,x,y),
                    "EX orbital menu changed canonical colour, text, priority or opaque black");
            for(const auto& draw:prepared.before_models.draws) {
                const auto point=prepared.before_models.vertices[draw.first].position;
                std::array<double,2> xs{};
                for(unsigned eye=0;eye<frame.plan.eye_count;++eye) {
                    const auto matrix=pica_draw_matrix(frame.plan,eye,draw);std::array<double,4> clip{};
                    for(unsigned row=0;row<4;++row) {
                        clip[row]=matrix[row][3];
                        for(unsigned axis=0;axis<3;++axis) clip[row]+=matrix[row][axis]*point[axis];
                    }
                    xs[eye]=(1-clip[1]/clip[3])*200;
                }
                if(frame.plan.eye_count==2) require(std::abs(xs[0]-xs[1]-(draw.space==PicaSpace::scenery
                    ?background_offset(frame.plan,0)-background_offset(frame.plan,1):0))<.001,
                    "EX menu eye projection moved UI or gave screen-depth disparity to the surround");
            }
        }
        scene->meters.extended=false;
        require(!native_panorama_scene(frame),"An Original menu inherited EX orbital projection");
        scene->meters.extended=true;scene->flow=simulation::GameFlowState::planet_select;
        require(!native_panorama_scene(frame),"An orbital menu metadata flag changed the planet-map projection");
        scene->flow=simulation::GameFlowState::controls_type;
        require(!native_panorama_scene(frame),"An orbital menu metadata flag changed Controls projection");
    }
}
void receiver_eye_coverage() {
    auto frame=source(simulation::GameFlowState::gameplay,2);
    auto scene=std::make_shared<vr::GameSceneSnapshot>(*frame.current);
    scene->background_landscape=true;scene->landscape_grid_height=-145;scene->landscape_atlas_origin=232;
    frame.current=frame.previous=scene;
    auto ppu=std::make_shared<simulation::SnesPpuState>(*frame.raster->ppu);
    ppu->bg2_scroll_y=232;ppu->bg2_screen_size=3;ppu->bg2_vertical_offsets_enabled=true;
    for(unsigned i=0;i<4096;++i) {ppu->vram[0x6400+i*2]=1;ppu->vram[0x6401+i*2]=8;}
    Canvas lower;
    for(int roll:{-3,0,3}) for(float convergence:{16.F,32.F,1024.F}) {
        for(unsigned i=0;i<32;++i) {
            const unsigned word=0x4000|((232-roll*16+roll*int(i+1))&8191),at=(0x2fa0+i)*2;
            ppu->vram[at]=std::uint8_t(word);ppu->vram[at+1]=std::uint8_t(word>>8);
        }
        auto raster=std::make_shared<GameRasterSnapshot>(*frame.raster);raster->ppu=std::make_shared<simulation::SnesPpuState>(*ppu);frame.raster=raster;
        StereoSettings settings;settings.strength=2;settings.separation=64;settings.convergence=convergence;
        frame.plan=plan_frame(1,true,ScreenUse::world,settings);GameLayers layers;
        const auto group=layers.prepare(frame).before_models;validate_pica_frame(group,lower.view());
        const auto plane=source_landscape_plane(frame);bool finite_started=false;
        GameLayers native;const auto atlas=native.prepare(frame,pica_vertex_limit).before_models;
        require(atlas.textures.size()==1 && atlas.textures[0].width==256,
            "Large source eye coverage incorrectly forced a flat/partial terrain atlas");
        validate_pica_frame(atlas,lower.view());
        for(unsigned eye=0;eye<2;++eye) for(unsigned y=0;y<240;y+=17) for(unsigned x=0;x<400;x+=23) {
            const double px=x+.5,py=y+.5,motion=256.*frame.plan.eyes[eye].x,offset=frame.plan.eyes[eye].projection_offset;
            const double sx=(px-offset+motion*(py-plane.centre+200*plane.slope)/(145*256))
                /(1+motion*plane.slope/(145*256));
            const double reciprocal=(py-plane.centre-plane.slope*(sx-200))/(145*256);
            if(reciprocal<1./65536 || reciprocal>1) continue;
            bool covered=false;
            for(const auto& draw:atlas.draws) if(draw.space==PicaSpace::world)
                for(unsigned i=draw.first;i<draw.first+draw.count;i+=3) {
                    std::array<std::array<double,2>,3> triangle;
                    for(unsigned k=0;k<3;++k) {
                        const auto p=atlas.vertices[i+k].position;
                        triangle[k]={200+256.*(p[0]-frame.plan.eyes[eye].x)/p[2]+offset,120-256.*p[1]/p[2]};
                    }
                    bool positive=true,negative=true;
                    for(unsigned k=0;k<3;++k) {
                        const auto a=triangle[k],b=triangle[(k+1)%3];
                        const double cross=(b[0]-a[0])*(py-a[1])-(b[1]-a[1])*(px-a[0]);
                        positive&=cross>=-.02;negative&=cross<=.02;
                    }
                    covered|=positive || negative;
                }
            require(covered,"Visible terrain pixel fell outside the guarded atlas in an eye");
        }
        for(const auto& draw:group.draws) {
            if(draw.space==PicaSpace::world) finite_started=true;
            else require(!finite_started,"Infinity strip was painted after finite ground");
        }
        for(unsigned eye=0;eye<2;++eye) for(unsigned y=0;y<240;++y) for(unsigned x=0;x<400;x+=3) {
            const double px=x+.5,py=y+.5,motion=256.*frame.plan.eyes[eye].x,offset=frame.plan.eyes[eye].projection_offset;
            const double sx=(px-offset+motion*(py-plane.centre+200*plane.slope)/(145*256))
                /(1+motion*plane.slope/(145*256));
            const double reciprocal=(py-plane.centre-plane.slope*(sx-200))/(145*256);
            if(reciprocal<1./65536 || reciprocal>1) continue;
            bool covered=false;
            for(const auto& draw:group.draws) if(draw.space==PicaSpace::world) {
                const auto& image=group.textures[draw.texture];const double left=(400.-image.pitch/4)/2+draw.texture*1024;
                for(unsigned i=draw.first;i<draw.first+draw.count;i+=3) {
                    std::array<std::array<double,2>,3> triangle;
                    for(unsigned k=0;k<3;++k) {
                        const auto& v=group.vertices[i+k];const auto& p=v.position;
                        triangle[k]={200+256.*(p[0]-frame.plan.eyes[eye].x)/p[2]+offset,120-256.*p[1]/p[2]};
                        require(std::abs(200+256.*p[0]/p[2]-left-v.uv[0]*image.width)<.02,
                            "Finite strip lost homogeneous source pixel registration");
                    }
                    bool positive=true,negative=true;
                    for(unsigned k=0;k<3;++k) {
                        const auto a=triangle[k],b=triangle[(k+1)%3];
                        const double cross=(b[0]-a[0])*(py-a[1])-(b[1]-a[1])*(px-a[0]);
                        positive&=cross>=-.02;negative&=cross<=.02;
                    }
                    covered|=positive || negative;
                }
            }
            require(covered,"Visible finite receiver pixel fell outside both-eye source coverage");
        }
    }
}
}
int main() try {
    priority_pixels();map_single_occurrence();policy_contracts();margins_and_cache();landscape_depth();atlas_landscape_depth();unique_landscape_policy();results_landscape_priorities();atlas_water_depth();water_depth();water_priority_pixels();water_eye_coverage();corridor_source_symbols();corridor_depth();corridor_eye_coverage();corridor_eye_coverage(true);corridor_eye_coverage(false,true);corridor_eye_coverage(true,true);corridor_eye_coverage(false,true,true);corridor_eye_coverage(true,true,true);colony_depth();panorama_depth();panorama_atlas_priority_pixels();offscreen_landscape_receiver();ex_menu_panorama_depth();receiver_eye_coverage();
    std::cout<<checks<<" 3DS actual source painter-policy checks passed; not full terrain/menu/hardware acceptance\n";
} catch(const std::exception& error) {std::cerr<<scenario<<error.what()<<'\n';return 1;}
