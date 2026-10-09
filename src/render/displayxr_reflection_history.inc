// Independent of primary AA: no jitter, centre raster, primary motion MRTs or
// whole-image TAA allocations are introduced when AA is disabled/spatial.
struct ReflectionImages {
    std::array<std::array<std::unique_ptr<GpuCalibratedReflectionHistory>,8>,2> resolve;
    CalibratedReflectionTimeline timeline;
    unsigned pending_samples{1};
    void settle(bool ok) noexcept {
        for(auto& eye:resolve) for(unsigned sample=0;sample<eye.size();++sample) if(eye[sample]) {
            if(ok) {
                eye[sample]->commit();
                // Retain old sample banks until the COMPLETE presentation
                // accepts the new sample count. Rejection keeps them intact.
                if(sample>=pending_samples) eye[sample].reset();
            } else eye[sample]->discard();
        }
        timeline.settle(ok);
    }
};
std::unique_ptr<ReflectionImages> reflected;
bool prepare_reflection_history(const std::array<vr::EyeCamera,2>& cameras,std::array<float,2> jitter,unsigned bytes_per_pixel=52,unsigned samples=1) {
    const CalibratedReflectionTimeline::Extents sizes{scene_extent(0),scene_extent(1)};
    const auto pixels=std::uint64_t(sizes[0][0])*sizes[0][1]+std::uint64_t(sizes[1][0])*sizes[1][1];
    // Existing resident effects may retain caches after selection changes.
    // Include two history banks and the native ray output (52/88 bytes/pixel,
    // 104/108 with the unchanged liquid surface/hidden-world prefix, or
    // 124 for the compact eight-lobe model record, or 80/444 for the
    // complete one/eight-lobe ordered model path, or 96/460 for the mixed
    // analytic-ground/model path with separate CURRENT additive base light);
    // explicit ordered-liquid validation also accounts for the canonical
    // surface/hidden-world prefix plus all one/eight 64-byte lobe records.
    // AS/geometry/driver storage is separate, not promised available VRAM.
    const auto& settings=frame->settings;
    if((samples!=1 && samples!=2 && samples!=4 && samples!=8)
        || (bytes_per_pixel!=52 && bytes_per_pixel!=88 && bytes_per_pixel!=104 && bytes_per_pixel!=108
        && bytes_per_pixel!=124 && bytes_per_pixel!=80 && bytes_per_pixel!=444 && bytes_per_pixel!=96 && bytes_per_pixel!=460
        && bytes_per_pixel!=92 && bytes_per_pixel!=128 && bytes_per_pixel!=540 && bytes_per_pixel!=556
        && bytes_per_pixel!=572 && bytes_per_pixel!=576))
        throw std::invalid_argument("Invalid native reflection working layout");
    auto total=pixels*samples*(336+(settings.volumetric_fog?32:0)+3*bytes_per_pixel)
        +(settings.volumetric_fog?64ULL*1024*1024*samples:0);
    if(source_validation) {
        // Reserve a complete third image and all lazy bounded stage scratch per
        // native eye/raster owner, including cold frames. Existing retained
        // caches below are additionally counted (conservative, never free VRAM).
        // This validation never samples only part of a frame to fit the budget.
        for(unsigned eye=0;eye<2;++eye) {
            if(!reflection_source_index_layout(sizes[eye][0],sizes[eye][1],bytes_per_pixel))return false;
            total+=samples*(std::uint64_t(sizes[eye][0])*sizes[eye][1]*bytes_per_pixel
                +std::uint64_t(sizes[eye][0])*sizes[eye][1]*4 // Third diagnostic admission bank.
                +GpuReflectionSourceQueries::working_bytes+GpuReflectionSourceOptical::working_bytes
                +GpuReflectionSourceOptical::stream_working_bytes
                +GpuReflectionSourceLocalRoots::working_bytes);
        }
    }
    if(dlss) {
        // SDK guides use its input grid even with SSAA; output/ink images use
        // the physical eye grid. Include the five owner MRTs (44), packed SDK
        // input (17), optional two phase witnesses (8), two SDK output banks
        // (8), retained panel/ink targets (48), exposure and empty-ray buffers.
        // Opaque vendor scratch/AS/driver storage is still not available-VRAM
        // acceptance. Exceeding this cache preflight preserves full-quality
        // CURRENT rays and the selected SDK mode, not lower resolution/RT.
        std::uint64_t inputs=0,outputs=0;
        for(unsigned eye=0;eye<2;++eye) {
            inputs+=std::uint64_t(extent[eye][0])*extent[eye][1];
            outputs+=std::uint64_t(output_extent[eye][0])*output_extent[eye][1];
        }
        total+=inputs*69+outputs*56+40;
    }
    // A count change constructs the new hardware raster owner before releasing
    // its old one. Its actually retained sample images/meters/native outputs
    // are additional to the new layout, just like inactive secondary banks.
    // Extent changes already retire msaa in prepare_samples at this boundary.
    if(msaa && msaa->count!=samples) total+=msaa->working_image_bytes(sizes);
    // A bank transition retains the accepted banks until whole-presentation
    // acceptance. Count those allocations before tracing metadata or preparing
    // the timeline. Do NOT lower ray quality, resize the eye or turn RT off.
    for(unsigned eye=0;eye<2;++eye) for(unsigned sample=0;sample<8;++sample) {
        const auto* bank=reflected?reflected->resolve[eye][sample].get():nullptr;
        if(sample<samples) {
            if(bank) {
                if(!bank->can_allocate(sizes[eye][0],sizes[eye][1],bytes_per_pixel,source_validation,source_validation)) return false;
                total+=*bank->allocation_bytes(sizes[eye][0],sizes[eye][1],bytes_per_pixel,source_validation,source_validation)
                    -std::uint64_t(sizes[eye][0])*sizes[eye][1]*bytes_per_pixel*2;
            } else {
                const auto index=source_validation?reflection_source_index_layout(sizes[eye][0],sizes[eye][1],bytes_per_pixel):std::nullopt;
                const auto additional=(index?std::uint64_t(index->storage_bytes)*2:0)
                    +(source_validation?std::uint64_t(sizes[eye][0])*sizes[eye][1]*8:0);
                if(!GpuCalibratedReflectionHistory::working_bound_fits(
                    std::uint64_t(sizes[eye][0])*sizes[eye][1]*bytes_per_pixel*2+additional,0,false))return false;
                total+=additional;
            }
        } else if(bank) total+=bank->working_image_bytes();
    }
    if(total>4ULL*1024*1024*1024) {
        if(source_validation)throw std::runtime_error("Complete source-reflection validation exceeds native working bound; no partial-eye/lobe publication");
        return false;
    }
    if(!reflected) reflected=std::make_unique<ReflectionImages>();
    for(auto& eye:reflected->resolve) for(unsigned sample=0;sample<samples;++sample) if(!eye[sample]) {
        auto next=std::make_unique<GpuCalibratedReflectionHistory>();
        if(!next->initialize(device)) throw std::runtime_error(next->status());
        eye[sample]=std::move(next);
    }
    if(!reflected->timeline.prepare(frame,cameras,sizes,jitter,samples))
        throw std::runtime_error("Invalid/pending native reflection presentation");
    reflected->pending_samples=samples;
    return true;
}
vr::EyeCamera reflection_previous_camera(unsigned eye,std::optional<unsigned> sample={}) const {
    auto camera=reflected->timeline.cameras()[eye];
    const auto& size=reflected->timeline.extents()[eye];
    const auto& jitter=reflected->timeline.jitter();
    camera.projection[8]-=2*jitter[0]/size[0];camera.projection[9]+=2*jitter[1]/size[1];
    if(sample) {
        const auto shifted=calibrated_msaa_sample_camera(camera,size[0],size[1],reflected->timeline.samples(),*sample);
        if(!shifted) throw std::runtime_error("Invalid accepted native reflection sample camera");
        camera=*shifted;
    }
    return camera;
}
std::optional<CalibratedGroundRayReceiver> reflection_previous_receiver(unsigned eye,bool liquid) const {
    const auto* old=reflected->timeline.previous();
    if(!old || !old->current || !frame->current) return {};
    const auto& a=*frame->current;const auto& b=*old->current;
    if(a.background_id!=b.background_id || a.scene_epoch!=b.scene_epoch || a.flow!=b.flow) return {};
    const auto find=[](const CalibratedGameFrame& source)->std::optional<std::size_t> {
        std::optional<std::size_t> found;
        for(std::size_t i=0;i<source.draws.size();++i) if(source.draws[i].source_key==calibrated_ground_source_key) {
            if(found) return {};
            found=i;
        }
        return found;
    };
    const auto current=find(*frame),previous=find(*old);
    if(!current || !previous) return {};
    const auto& now=frame->draws[*current];const auto& before=old->draws[*previous];
    const auto material=calibrated_ground_material(now.packet);
    if((liquid?(material!=5 && material!=9):(material!=6 && material!=7)) || calibrated_ground_material(before.packet)!=material
        || now.layer!=CalibratedGameLayer::world || before.layer!=now.layer || before.blend!=now.blend
        || before.depth_test!=now.depth_test || now.blend!=vr::SceneBlend::opaque
        || now.ray_caster || before.ray_caster || now.after_rays || before.after_rays
        || now.packet.preserve_native_colour || before.packet.preserve_native_colour
        || !calibrated_ground_motion_source(now.packet) || !calibrated_ground_motion_source(before.packet)) return {};
    const auto packets=old->packets();const auto& p=packets[*previous];
    if(!p.ground_receiver) return {};
    const auto receiver=calibrated_ground_ray_receiver(before.packet,reflection_previous_camera(eye),
        float(old->settings.reflections)/3,false,p.model_override?&*p.model_override:nullptr);
    return receiver;
}
std::optional<shadows::RayReflectionGround> reflection_previous_ground(unsigned eye) const {
    const auto old=reflection_previous_receiver(eye,false);if(!old) return {};
    return shadows::RayReflectionGround{{old->plane.point.x,old->plane.point.y,old->plane.point.z},
        {old->plane.normal.x,old->plane.normal.y,old->plane.normal.z}};
}
std::optional<shadows::RayReflectionLiquid> reflection_previous_liquid(unsigned eye) const {
    const auto old=reflection_previous_receiver(eye,true);if(!old) return {};
    shadows::RayReflectionLiquid result;
    result.ground={{old->plane.point.x,old->plane.point.y,old->plane.point.z},
        {old->plane.normal.x,old->plane.normal.y,old->plane.normal.z}};
    std::copy(old->transport.world_to_view.begin(),old->transport.world_to_view.end(),result.world_to_view.begin());
    std::copy(old->transport.camera_position.begin(),old->transport.camera_position.end(),result.offset.begin());
    result.time=old->transport.time;result.material=old->transport.material;
    return result.valid()?std::optional(result):std::nullopt;
}
