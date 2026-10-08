void check_prepared_faces() {
    using namespace starfox;
    assets::Shape shape;shape.visibilities.resize(1);
    shape.faces={{0,0,{0,0,100},{0,1,2}}, {-1,1,{0,0,100},{0,1}},
        {-1,0,{0,0,100},{0},true}};
    shape.colour_words={0x4000,0x3e01};
    assets::TextureImage texture;texture.descriptor=0x4000;texture.u_mask=texture.v_mask=1;
    texture.coordinates={{{0,0},{1,0},{0,1},{1,1}}};texture.texels={1,2,3,4};
    shape.textures={texture};
    render::PreparedBspSource topology(shape,false);
    render::RenderPose pose;pose.use_source_lighting_state=true;pose.source_depth=200;
    pose.use_rotation_matrix=true;pose.source_lighting_matrix={32767,0,0,0,32767,0,0,0,32767};
    pose.rotation_matrix=pose.source_lighting_matrix;
    pose.has_depth_colour_tables=true;pose.depth_colour_tables[0][1]=0x31;
    render::RenderSettings settings;settings.colour_index_base=128;
    const render::PreparedFacesSource source(shape,topology,pose,settings);
    const auto exact=[&](const render::PackedFaces& a,const render::PackedFaces& b) {
        require(a.corners==b.corners && a.polygons==b.polygons && a.texels==b.texels
            && a.primitives==b.primitives && a.polygon_only==b.polygon_only
            && a.materials.size()==b.materials.size());
        require(a.materials.empty() || std::memcmp(a.materials.data(),b.materials.data(),a.materials.size()*sizeof(render::RasterCommand))==0);
    };
    exact(source.faces(),render::pack_faces(shape,topology.graph(),pose,settings));
    require(source.storage_bytes()>=sizeof(source)+4);
    const render::PreparedRayTopology rays(source,true),shadows(source,false);
    const std::array<std::uint32_t,4> expected_triangle{0,1,2,0};
    require(rays.matches(source,true) && shadows.matches(source,false) && !shadows.matches(source,true));
    require(rays.triangles().size()==1 && rays.triangles()[0]==expected_triangle);
    require(rays.material_topology().size()==1 && rays.material_topology()[0]==expected_triangle);
    require(shadows.triangles().size()==1 && shadows.triangles()[0]==expected_triangle && shadows.material_topology().empty());
    require(rays.storage_bytes()==sizeof(rays)+32 && shadows.storage_bytes()==sizeof(shadows)+16);
    // A source object at a different identity must not accidentally borrow its
    // predecessor's corner mapping, even if this fixture has equal bytes.
    const render::PreparedFacesSource duplicate(shape,topology,pose,settings);
    require(!rays.matches(duplicate,true));
    auto eye=pose;eye.x=90;eye.y=-70;eye.z=6000;eye.vanish_x=200;eye.vanish_y=120;
    eye.pitch=20;eye.roll=-13;eye.yaw=72;eye.scale=2;
    eye.rotation_matrix={0,32767,0,-32767,0,0,0,0,32767};
    eye.continuous_geometry=eye.subpixel_projection=true;eye.explosion_phase=.5;
    auto raster=settings;raster.render_scale=4;raster.focal_length=400;
    require(source.matches(shape,topology,eye,raster));
    exact(source.faces(),render::pack_faces(shape,topology.graph(),eye,raster));
    const auto reject=[&](auto mutate) {auto changed=pose;mutate(changed);require(!source.matches(shape,topology,changed,settings));};
    reject([](auto& p){++p.colour_frame;});
    reject([](auto& p){p.source_depth=3000;});
    reject([](auto& p){p.depth_thresholds[0]=100;});
    reject([](auto& p){p.source_lighting_matrix[0]=-32767;});
    reject([](auto& p){p.use_source_lighting_state=false;p.z=5000;});
    reject([](auto& p){++p.depth_colour_tables[0][1];});
    reject([](auto& p){p.has_depth_colour_tables=false;});
    reject([](auto& p){p.palette_override=200;});
    reject([](auto& p){p.force_colour=true;});
    reject([](auto& p){++p.forced_colour;});
    reject([](auto& p){++p.texture_scroll_x;});
    reject([](auto& p){--p.texture_scroll_y;});
    reject([](auto& p){p.terrain_geometry=true;});
    reject([](auto& p){p.world_geometry=true;});
    reject([](auto& p){p.cel_mode=true;});
    reject([](auto& p){p.wave_mode=true;});
    reject([](auto& p){++p.wave_offset;});
    reject([](auto& p){++p.animation_frame;});
    reject([](auto& p){++p.wireframe_mode;});
    reject([](auto& p){++p.wobble_mode;});
    reject([](auto& p){p.explosion_progress=1;});
    reject([](auto& p){p.simple_scaled_sprite=true;});
    reject([](auto& p){p.collapse_to_axis_line=true;});
    reject([](auto& p){p.colour_warp=true;});
    for(unsigned kind=0;kind<2;++kind) {auto changed=settings;
        if(kind) changed.backface_culling=true;else ++changed.colour_index_base;
        require(!source.matches(shape,topology,pose,changed));}
    auto copy=shape;render::PreparedBspSource other(copy,false),same_shape(shape,false),flat(shape,true);
    require(!source.matches(copy,other,pose,settings) && !source.matches(shape,same_shape,pose,settings));
    bool rejected=false;try{render::PreparedFacesSource bad(shape,flat,pose,settings);}
    catch(const std::runtime_error&){rejected=true;}require(rejected);
    auto disabled=pose;disabled.has_depth_colour_tables=false;
    render::PreparedFacesSource no_tables(shape,topology,disabled,settings);
    ++disabled.depth_colour_tables[0][1];require(no_tables.matches(shape,topology,disabled,settings));
    auto forced=pose;forced.colour_warp=forced.force_colour=true;forced.forced_colour=0x91;
    render::PreparedFacesSource forced_source(shape,topology,forced,settings);
    require(forced_source.matches(shape,topology,forced,settings));
    exact(forced_source.faces(),render::pack_faces(shape,topology.graph(),forced,settings));
    std::cout<<"Prepared faces: complete material-state guards, independent eye geometry, mixed textured/line/sprite sources and unsupported mutable paths passed\n";
}
