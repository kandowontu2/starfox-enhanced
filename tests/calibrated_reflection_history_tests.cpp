#include "starfox/render/calibrated_game_motion.hpp"
#include "starfox/render/ray_reflection_history.hpp"
#include "starfox/render/dxr_shadows.hpp"
#include "starfox/render/calibrated_reflection_timeline.hpp"
#include "starfox/render/calibrated_ground_receiver.hpp"
#include "starfox/render/gpu_calibrated_reflection_history.hpp"
#include "starfox/vr/background_tiles.hpp"
#include <iostream>
#include <stdexcept>
using namespace starfox;
int main() try {
    const auto check=[](bool value,const char* message){if(!value) throw std::runtime_error(message);};
    using History=render::GpuCalibratedReflectionHistory;
    constexpr std::uint64_t gib=1024ULL*1024*1024;
    check(History::working_bound_fits(gib,0,false) && !History::working_bound_fits(gib+1,0,false)
        && History::working_bound_fits(gib/2,gib/2,false) && !History::working_bound_fits(gib/2+1,gib/2,false)
        && History::working_bound_fits(gib,gib,true) && !History::working_bound_fits(0,0,false)
        && !History::working_bound_fits(1,UINT64_MAX,false),"Reflection working bound overflowed retained banks or rejected exact same-bank reuse");
    History forecast;
    bool stage_called=false;
    const auto stage_observer=[](void* context,const render::ReflectionSourceStageEvent&) {
        *static_cast<bool*>(context)=true;
    };
    check(!forecast.enqueue_source_frame(nullptr,nullptr,nullptr,stage_observer,&stage_called)
        && !stage_called && !forecast.working_image_bytes(),
        "Cold source stage diagnostics ran or allocated without a pending native command");
    check(!forecast.source_index().buffer && !forecast.accepted_source_index().buffer,
        "Uninitialized reflection owner exposed a resident source index");
    using Query=render::GpuReflectionSourceQueries;
    check(Query::working_bytes==414720 && Query::capacity==64 && Query::query_stride==48
        && Query::leaf_stride==272 && Query::region_stride==6160
        && forecast.source_query_allocation_bytes()==Query::working_bytes
        && !forecast.working_image_bytes() && !forecast.source_queries().queries
        && !forecast.enqueue_source_queries(nullptr,0,1),
        "Query preflight allocated/exposed scratch or changed its bounded native ABI");
    check(render::ReflectionSourceQueryLimits{}.valid(),"Default bounded source-query limits invalid");
    using Optical=render::GpuReflectionSourceOptical;
    check(Optical::frame_stride==512 && Optical::result_stride==4096 && Optical::working_bytes==294912
        && Optical::stream_task_capacity==8192 && Optical::stream_working_bytes==65548
        && forecast.source_optical_allocation_bytes()==709632 && !forecast.source_optical().frames
        && !forecast.enqueue_source_optical(nullptr),"Native optical preflight allocated/exposed unbounded or stale state");
    check(Optical::stream_lanes==256,"Stream optical pipeline and lane partition contract changed");
    check(!Optical::stream_lanes_supported(128,256) && !Optical::stream_lanes_supported(256,128)
        && !Optical::stream_lanes_supported(0,0) && Optical::stream_lanes_supported(256,256)
        && Optical::stream_lanes_supported(1024,1024),
        "Wide optical dispatch ignored either physical-device group-size limit");
    // Integer scheduling/reduction proof only, not a floating optical oracle.
    // Exhaust every permitted cell count, including zero and partial groups.
    // Native every-batch raw equality still has to prove the compiled writer.
    for(unsigned cells=0;cells<=8192;++cells) {
          const auto receipt=[&](unsigned lanes,bool lane_reduction=false) {
              std::array<unsigned,6> result{0,0,UINT32_MAX,UINT32_MAX,0,0};
              std::vector<unsigned char> visits(cells);
              for(unsigned lane=0;lane<lanes;++lane) {
                std::array<unsigned,6> local{0,0,UINT32_MAX,UINT32_MAX,0,0};
                auto& target=lane_reduction?local:result;
                for(unsigned cell=lane;cell<cells;cell+=lanes) {
                  check(++visits[cell]==1,"Optical lane partition visited a cell more than once");
                  if(cell%5==0)++target[1];
                  else {
                      ++target[0];
                      const unsigned x=cell*13+17,y=cell*7+23;
                      target[2]=std::min(target[2],x);target[3]=std::min(target[3],y);
                      target[4]=std::max(target[4],x+11);target[5]=std::max(target[5],y+19);
                  }
                }
                if(lane_reduction) {
                    result[0]+=local[0];result[1]+=local[1];
                    if(local[0]) {
                        result[2]=std::min(result[2],local[2]);result[3]=std::min(result[3],local[3]);
                        result[4]=std::max(result[4],local[4]);result[5]=std::max(result[5],local[5]);
                    }
                }
              }
            check(std::all_of(visits.begin(),visits.end(),[](auto n){return n==1;}),
                "Optical lane partition omitted a closed source cell");
            if(!result[0])result[2]=result[3]=result[4]=result[5]=0;
            return result;
        };
          const auto original_receipt=receipt(64);
          check(original_receipt==receipt(Optical::stream_lanes),
              "Stream lane scheduling changed complete integer counts/hull receipts");
          check(original_receipt==receipt(Optical::stream_lanes,true),
              "Private integer lane reduction changed complete count/hull receipts");
    }
    check(render::ReflectionSourceOpticalLimits{}.valid() && render::ReflectionSourceOpticalLimits{1,0}.valid()
        && !render::ReflectionSourceOpticalLimits{0,12}.valid()
        && !render::ReflectionSourceOpticalLimits{8193,12}.valid()
        && !render::ReflectionSourceOpticalLimits{8192,13}.valid(),"Native optical quotas changed conservative bounded refusal");
    using LocalRoots=render::GpuReflectionSourceLocalRoots;
    check(LocalRoots::region_capacity==128 && LocalRoots::record_stride==192
        && LocalRoots::result_stride==24576 && LocalRoots::working_bytes==1572864
        && forecast.source_local_root_allocation_bytes()==2282496
        && !forecast.source_local_roots().results && !forecast.working_image_bytes()
        && !forecast.enqueue_source_local_roots(nullptr),
        "Native local-root preflight allocated/exposed scratch or changed the bounded ABI");
    using RootHull=render::GpuReflectionSourceRootHull;
    check(RootHull::record_stride==192 && RootHull::result_stride==24576
        && !forecast.source_root_hull().results && !forecast.enqueue_source_root_hull(nullptr)
        && !forecast.source_local_roots().results && !forecast.working_image_bytes()
        && forecast.source_local_root_allocation_bytes()==2282496,
        "Whole-source root hull exposed a descriptor or allocated a separate scratch bank");
    using Witness=render::GpuReflectionSourceWitness;
    check(Witness::record_offset==192 && Witness::record_bytes==64 && Witness::result_stride==24576
        && !forecast.source_witness().results && !forecast.enqueue_source_witness(nullptr)
        && !forecast.source_root_hull().results && !forecast.source_local_roots().results && !forecast.working_image_bytes(),
        "Source-footprint witness cold API/ABI or allocation changed");
    using Folds=render::GpuReflectionSourceFolds;
    check(Folds::record_offset==256 && Folds::record_bytes==64 && Folds::result_stride==24576
        && !forecast.source_folds().results && !forecast.enqueue_source_folds(nullptr)
        && !forecast.source_witness().results && !forecast.source_root_hull().results
        && !forecast.source_local_roots().results && !forecast.working_image_bytes()
        && forecast.source_local_root_allocation_bytes()==2282496,
        "Source fold cold API/ABI or reusable bounded scratch changed");
    // A proof-only native guide must remain cold and allocation-free.
    using Guide=render::GpuReflectionSourceGuide;
    check(Guide::record_offset==320 && Guide::record_bytes==64 && Guide::result_stride==24576
        && !forecast.source_guide().results && !forecast.enqueue_source_guide(nullptr)
        && !forecast.source_folds().results && !forecast.source_witness().results
        && !forecast.source_root_hull().results && !forecast.source_local_roots().results
        && !forecast.working_image_bytes() && forecast.source_local_root_allocation_bytes()==2282496,
        "Source finite-face/angular guide cold API/ABI or reusable scratch changed");
    using Colour=render::GpuReflectionSourceColour;
    check(Colour::record_offset==384 && Colour::record_bytes==96 && Colour::result_stride==24576
        && !forecast.source_colour().results && !forecast.enqueue_source_colour(nullptr)
        && !forecast.source_guide().results && !forecast.source_folds().results && !forecast.source_witness().results
        && !forecast.source_root_hull().results && !forecast.source_local_roots().results
        && !forecast.working_image_bytes() && forecast.source_local_root_allocation_bytes()==2282496,
        "Source incident colour cold API/ABI or reusable scratch changed");
    using Composition=render::GpuReflectionSourceComposition;
    check(Composition::record_offset==512 && Composition::record_bytes==64 && Composition::result_stride==24576
        && !forecast.source_composition().results && !forecast.enqueue_source_composition(nullptr)
        && !forecast.source_colour().results && !forecast.source_guide().results && !forecast.source_folds().results
        && !forecast.source_witness().results && !forecast.source_root_hull().results && !forecast.source_local_roots().results
        && !forecast.working_image_bytes() && forecast.source_local_root_allocation_bytes()==2282496,
        "Whole-pixel composition cold API/ABI or reusable scratch changed");
    check(!forecast.fresh_current_output().buffer && !forecast.source_publication_complete()
        && !forecast.source_frame_allocation_bytes()
        && !forecast.source_publication_allocation_bytes() && !forecast.enqueue_source_publication(nullptr)
        && !forecast.enqueue_source_frame(nullptr)
        && !forecast.enqueue_source_frame(nullptr,nullptr,&forecast)
        && !forecast.source_publication_allocation_bytes() && !forecast.working_image_bytes(),
        "Complete-frame publication cold API exposed a partial image or allocated storage");
    for(unsigned fault=0;fault<8;++fault) {
        render::ReflectionSourceQueryLimits limits;
        if(fault==0)limits.leaves=0;
        if(fault==1)limits.leaves=65;
        if(fault==2)limits.nodes=0;
        if(fault==3)limits.nodes=4097;
        if(fault==4)limits.regions=0;
        if(fault==5)limits.regions=129;
        if(fault==6)limits.supports=0;
        if(fault==7)limits.supports=16385;
        check(!limits.valid(),"Source-query limits accepted zero or an unbounded shader workspace");
    }
    for(unsigned stride:{80U,96U,92U,108U,124U,128U,444U,460U,540U,556U,572U,576U}) {
        const auto planned=render::reflection_source_index_layout(17,9,stride);
        check(planned && planned->level_count==3 && planned->levels[0]==std::array<unsigned,4>{0,3,2,0}
            && planned->levels[1]==std::array<unsigned,4>{6,2,1,0}
            && planned->levels[2]==std::array<unsigned,4>{8,1,1,0} && planned->total_nodes==9
            && planned->storage_bytes==9*32*planned->lobes,
            "Non-power-of-two source index omitted an edge or hierarchy level");
        check(forecast.allocation_bytes(17,9,stride,true)==(17ULL*9*stride+planned->storage_bytes)*2,
            "Reflection preflight omitted one of the paired source-index banks");
        check(forecast.allocation_bytes(17,9,stride,true,true)==(17ULL*9*stride+planned->storage_bytes)*2+17ULL*9*8,
            "Diagnostic admission preflight omitted a paired per-pixel mask bank");
    }
    check(!render::reflection_source_index_layout(0,9,576)
        && !render::reflection_source_index_layout(17,16385,576)
        && !render::reflection_source_index_layout(17,9,88)
        && !forecast.can_allocate(17,9,88,true)
        && !forecast.can_allocate(16384,16384,576,true),
        "Source index accepted an unknown/unbounded/non-ordered allocation");
    check(!forecast.can_allocate(17,9,576,false,true) && !forecast.accepted_source_admission().buffer,
        "Admission mask bypassed explicit staging or invented accepted data");
    const auto maximum=render::reflection_source_index_layout(16384,16384,576);
    check(maximum && maximum->level_count==12 && maximum->levels[11][1]==1 && maximum->levels[11][2]==1,
        "Maximum native source extent exceeded the bounded hierarchy ABI");
    check(forecast.can_allocate(128,72,444) && forecast.allocation_bytes(128,72,444)==128ULL*72*444*2
        && !forecast.can_allocate(16384,16384,444) && !forecast.can_allocate(0,72,80)
        && !forecast.can_allocate(128,72,443),"Reflection allocation preflight changed its bound or accepted an unknown layout");
    for(unsigned stride:{92U,108U,124U,128U,540U,556U,572U,576U})
        check(forecast.allocation_bytes(128,72,stride)==128ULL*72*stride*2
            && forecast.can_allocate(128,72,stride) && !forecast.can_allocate(16384,16384,stride),
            "Curved reflection preflight omitted full canonical/lobe records or overflowed its bound");
    {
        render::shadows::RayReflectionHistory h{48,{17,9},{256,272,8,4},1,65536,96,true};
        h.model_lobes=1;h.model_paths=h.curved_paths=h.curved_receivers=true;
        h.previous_liquid=render::shadows::RayReflectionLiquid{{{0,180,0},{0,-1,0}}};
        render::CalibratedReflectionLobeGeometry geometry{reinterpret_cast<void*>(1),100,3,h,0};
        check(!geometry.valid(),"Curved cache accepted absent CURRENT native liquid calibration");
        geometry.current_liquid=h.previous_liquid;geometry.current_projection={256,272,8,4};geometry.current_clip={1,65536};
        check(geometry.valid(),"Curved cache rejected exact finite CURRENT/accepted liquid inputs");
        for(unsigned fault=0;fault<12;++fault) {
            auto bad=geometry;
            if(fault==0)bad.current_liquid.reset();
            if(fault==1)bad.current_liquid->material=3;
            if(fault==2)bad.current_liquid->world_to_view={};
            if(fault==3)bad.current_liquid->time=-1;
            if(fault==4)bad.current_liquid->offset[0]=std::numeric_limits<double>::quiet_NaN();
            if(fault==5)bad.current_projection[0]=0;
            if(fault==6)bad.current_projection[1]=std::numeric_limits<double>::infinity();
            if(fault==7)bad.current_ground=render::shadows::RayReflectionGround{{0,180,0},{0,-1,0}};
            if(fault==8)bad.history.curved_paths=bad.history.curved_receivers=false;
            if(fault==9)bad.current_clip[0]=0;
            if(fault==10)bad.current_clip[1]=1;
            if(fault==11)bad.current_clip[1]=std::numeric_limits<double>::quiet_NaN();
            check(!bad.valid(),"Curved cache accepted stale material, absent calibration or conflicting planar metadata");
        }
        auto moved=geometry;moved.current_liquid->time+=.2;moved.current_liquid->offset[0]+=.3;
        check(moved.valid() && *moved.current_liquid!=*geometry.current_liquid,
            "Curved current/accepted metadata froze a legitimately advancing liquid");
    }
    auto snapshot=std::make_shared<vr::GameSceneSnapshot>();
    for(unsigned key=1;key<=4;++key) {
        auto& object=snapshot->transforms[simulation::ObjectHandle(key)];object.generation=42;
        object.shape=10;object.strategy_address=20;
    }
    const auto actor=[](unsigned key,unsigned triangles,unsigned lines=0) {
        render::CalibratedGameDraw draw;draw.source_key=key;draw.layer=render::CalibratedGameLayer::model;draw.ray_caster=true;
        draw.packet.geometry.vertices.resize(triangles*3);draw.packet.geometry.line_vertices.resize(lines*2);
        for(auto& v:draw.packet.geometry.vertices) {v.position[2]=-2;v.color[3]=v.odd_color[3]=1;}
        for(auto& v:draw.packet.geometry.line_vertices) {v.position[2]=-2;v.color[3]=v.odd_color[3]=1;}
        return draw;
    };
    render::CalibratedGameFrame old,now;old.current=now.current=snapshot;
    old.draws={actor(1,1,1),actor(2,2),actor(0,4),actor(3,1)};old.draws[2].ray_caster=false;
    now.draws={old.draws[1],actor(4,1),old.draws[3],old.draws[0]};
    vr::EyeCamera camera{};camera.view={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    camera.projection={1,0,0,0,0,1,0,0,0,0,-1.01F,-1,0,0,-.101F,0};
    const auto first=render::calibrated_game_motion_draws(now,&old,camera,camera);
    if(first.size()!=5 || !first[0].valid || first[0].previous_ray_first_triangle!=3) {
        for(const auto& entry:first) std::cerr<<"match valid="<<entry.valid<<" accepted primitive="<<entry.previous_ray_first_triangle<<'\n';
    }
    check(first.size()==5 && first[0].valid && first[0].previous_ray_first_triangle==3
        && !first[1].valid && first[1].previous_ray_first_triangle==UINT32_MAX
        && first[2].valid && first[2].previous_ray_first_triangle==5
        && first[3].valid && first[3].previous_ray_first_triangle==0
        && first[4].valid && first[4].previous_ray_first_triangle==1,
        "Reordered/inserted actors or source lines borrowed current primitive indices");
    auto recycled=std::make_shared<vr::GameSceneSnapshot>(*snapshot);recycled->transforms[simulation::ObjectHandle(2)].generation=43;
    now.current=recycled;
    const auto second=render::calibrated_game_motion_draws(now,&old,camera,camera);
    check(!second[0].valid && second[0].previous_ray_first_triangle==UINT32_MAX,"Recycled actor retained accepted reflection identity");
    now.current=snapshot;old.draws.push_back(old.draws[0]);
    const auto ambiguous=render::calibrated_game_motion_draws(now,&old,camera,camera);
    check(!ambiguous[3].valid && !ambiguous[4].valid && ambiguous[3].previous_ray_first_triangle==UINT32_MAX
        && ambiguous[4].previous_ray_first_triangle==UINT32_MAX,"Ambiguous source occurrence retained reflection identity");
    old.draws.pop_back();auto invalid=camera;invalid.projection[11]=0;
    const auto singular=render::calibrated_game_motion_draws(now,&old,invalid,camera);
    for(const auto& item:singular) check(!item.valid && item.previous_ray_first_triangle==UINT32_MAX,
        "Invalid camera cleared geometric motion but kept reflection identity");
    const auto layout=render::shadows::native_reflection_history(12,8,{17,9});
    check(layout && layout->motion_offset==384 && layout->identity_offset==1920 && layout->witness_offset==3456
        && layout->storage_bytes==4992,"Reflected history planes overlap or omit their visibility witnesses");
    const auto separated=render::shadows::native_reflection_history(12,8,{17,9},true);
    check(separated && separated->motion_offset==layout->motion_offset && separated->identity_offset==layout->identity_offset
        && separated->witness_offset==layout->witness_offset && separated->incoming_offset==4992
        && separated->base_offset==5376 && separated->weight_offset==6912 && separated->storage_bytes==8448,
        "Separated incident/base/response planes overlap or alter the legacy layout");
    check(!render::shadows::native_reflection_history(16384,16384,{17,9},true)
        && !render::shadows::native_reflection_history(0,8,{17,9},true),
        "Separated reflected-light allocation exceeded its bounded address space");
    for(unsigned lobes:{1U,8U}) {
        const auto compact=render::shadows::native_reflection_history(12,8,{17,9},true,{},lobes);
        check(compact && compact->storage_bytes==12*8*(28+12*lobes) && compact->model_lobes==lobes
            && compact->motion_offset==12*8*4 && compact->identity_offset==12*8*4
            && compact->witness_offset==12*8*8 && compact->weight_offset==12*8*12
            && compact->incoming_offset==12*8*28 && !compact->base_offset,
            "Compact reflection records overlap or acquired per-lobe motion images");
        check(!render::shadows::native_reflection_history(12,8,{17,9},false,{},lobes)
            && !render::shadows::native_reflection_history(16384,16384,{17,9},true,{},lobes)
            && !render::shadows::native_reflection_history(12,8,{17,9},true,*render::shadows::native_water_layers(12,8),lobes),
            "Compact model history borrowed whole-radiance/liquid planes or overflowed");
        render::shadows::RayReflectionHistory compact_frame{64,{17,9},{100,110,8,4},1,65536,128,true};
        compact_frame.model_lobes=lobes;
        check(compact_frame.valid() && render::shadows::reflection_history_receiver_valid(compact_frame,nullptr,false),
            "Compact model history declined its own finite receiver calibration");
        for(unsigned metallic=0;metallic<=3;++metallic) {
            const float rough=lobes==8?.35F:0.F;
            check(render::shadows::reflection_history_transport_valid(compact_frame,rough,metallic,false)
                && !render::shadows::reflection_history_transport_valid(compact_frame,rough,metallic,true)
                && !render::shadows::reflection_history_transport_valid(compact_frame,lobes==8?0.F:.35F,metallic,false),
                "Compact history confused ray quadrature or accepted unvalidated multiple bounces");
            auto path_frame=compact_frame;path_frame.model_paths=true;
            check(render::shadows::reflection_history_transport_valid(path_frame,rough,metallic,true)
                && render::shadows::reflection_history_transport_valid(path_frame,rough,metallic,false),
                "Explicit ordered witness producer refused bounded specular transport");
        }
        const auto paths=render::shadows::native_reflection_history(12,8,{17,9},true,{},lobes,true);
        check(paths && paths->model_paths && paths->storage_bytes==12*8*(28+52*lobes)
            && paths->incoming_offset==compact->incoming_offset && paths->weight_offset==compact->weight_offset
            && *paths!=*compact,"Ordered path witnesses aliased legacy single-hit ABI");
        check(!render::shadows::native_reflection_history(16384,16384,{17,9},true,{},lobes,true)
            && !render::shadows::native_reflection_history(12,8,{17,9},false,{},lobes,true),
            "Ordered path witness allocation overflowed or lacked separated radiance");
        const auto curved=render::shadows::native_reflection_history(12,8,{17,9},true,{},lobes,true,false,true);
        check(curved && curved->curved_paths && curved->model_paths && !curved->scene_paths
            && curved->storage_bytes==12*8*(28+64*lobes) && curved->incoming_offset==12*8*28
            && *curved!=*paths && *curved!=*compact,
            "Ordered liquid/current-light records aliased the 52-byte or single-hit ABI");
        check(!render::shadows::native_reflection_history(12,8,{17,9},true,{},lobes,false,false,true)
            && !render::shadows::native_reflection_history(12,8,{17,9},true,{},lobes,true,true,true)
            && !render::shadows::native_reflection_history(12,8,{17,9},true,{},0,true,false,true)
            && !render::shadows::native_reflection_history(16384,16384,{17,9},true,{},lobes,true,false,true)
            && !render::shadows::native_reflection_history(12,8,{17,9},true,
                *render::shadows::native_water_layers(12,8),lobes,true,false,true),
            "Staged curved MODEL records borrowed liquid prefix planes or accepted unbounded/incompatible modes");
        for(unsigned layer=0;layer<3;++layer) {
            const auto liquid=layer?*render::shadows::native_water_layers(12,8,layer==2)
                :render::shadows::NativeWaterLayers{};
            const unsigned prefix=layer==2?24:layer==1?20:4;
            const auto full=render::shadows::native_reflection_history(12,8,{17,9},true,liquid,lobes,true,false,true,true);
            check(full && full->curved_receivers && full->curved_paths && !full->scene_paths
                && full->storage_bytes==12*8*(prefix+40+64*lobes)
                && full->motion_offset==12*8*prefix && full->identity_offset==12*8*prefix
                && full->witness_offset==12*8*(prefix+4) && full->weight_offset==12*8*(prefix+8)
                && full->base_offset==12*8*(prefix+24) && full->incoming_offset==12*8*(prefix+40)
                && *full!=*curved,"Full curved receiver aliases current liquid prefix or MODEL records");
            check(!render::shadows::native_reflection_history(12,8,{17,9},true,liquid,lobes,true,false,false,true)
                && !render::shadows::native_reflection_history(12,8,{17,9},false,liquid,lobes,true,false,true,true)
                && !render::shadows::native_reflection_history(16384,16384,{17,9},true,liquid,lobes,true,false,true,true),
                "Full curved receiver accepted unmarked/unseparated/overflowing allocation");
            if(layer) {
                auto bad=liquid;++bad.surface_offset;
                check(!render::shadows::native_reflection_history(12,8,{17,9},true,bad,lobes,true,false,true,true),
                    "Full curved receiver accepted noncanonical water prefix");
            }
        }
        check(!render::shadows::native_reflection_history(12,8,{17,9},true,{},0,false,false,false,true),
            "Full curved flag accepted legacy/no ordered lobe records");
        for(unsigned material:{0U,3U}) {
            auto curved_frame=compact_frame;curved_frame.model_paths=curved_frame.curved_paths=true;
            curved_frame.previous_liquid=render::shadows::RayReflectionLiquid{};
            curved_frame.previous_liquid->ground={{0,256,0},{0,-1,0}};
            curved_frame.previous_liquid->material=material;
            render::shadows::RayWater liquid;liquid.material=material;liquid.mirror_models=true;
            if(material==0) liquid.source_colour=std::array<float,3>{.1F,.2F,.3F};
            check(curved_frame.valid() && render::shadows::reflection_history_receiver_valid(curved_frame,&liquid,true)
                && render::shadows::reflection_history_transport_valid(curved_frame,lobes==8?.35F:0.F,2,true),
                "Explicit actual-liquid ordered producer refused its own contract");
            auto full_frame=curved_frame;full_frame.curved_receivers=true;
            check(full_frame.valid() && render::shadows::reflection_history_receiver_valid(full_frame,&liquid,true),
                "Full curved producer refused current primary ownership");
            for(unsigned layer=0;layer<2;++layer) {
                auto layered=liquid;layered.auxiliary_layers=layer==0;layered.surface_layers=layer==1;
                check(render::shadows::reflection_history_receiver_valid(full_frame,&layered,true)==(material==0),
                    "Full curved producer confused water layers with lava");
            }
            auto unmarked=full_frame;unmarked.curved_paths=false;
            check(!unmarked.valid(),"Full curved primary forgot ordered liquid opt-in");
            for(unsigned fault=0;fault<6;++fault) {
                auto bad=curved_frame;
                if(fault==0) bad.model_paths=false;
                if(fault==1) bad.scene_paths=true;
                if(fault==2) bad.previous_liquid.reset();
                if(fault==3) bad.previous_ground=bad.previous_liquid->ground;
                if(fault==4) bad.model_lobes=2;
                if(fault==5) bad.curved_paths=false;
                check(!bad.valid(),"Curved ordered records accepted malformed or unmarked liquid metadata");
            }
            for(unsigned fault=0;fault<4;++fault) {
                auto bad=liquid;
                if(fault==0) bad.material=material==0?3:0;
                if(fault==1) bad.auxiliary_layers=true;
                if(fault==2) bad.surface_layers=true;
                if(fault==3) {if(material==0) bad.source_colour.reset();
                    else bad.source_colour=std::array<float,3>{.1F,.2F,.3F};}
                check(!render::shadows::reflection_history_receiver_valid(curved_frame,&bad,true),
                    "Curved ordered producer confused liquid kind/ownership/current source colour");
            }
        }
        compact_frame.previous_ground=render::shadows::RayReflectionGround{{0,256,0},{0,-1,0}};
        check(!compact_frame.valid(),"Compact model history accepted analytic ground metadata");
        auto scene_frame=compact_frame;scene_frame.model_paths=scene_frame.scene_paths=true;
        const auto scene=render::shadows::native_reflection_history(12,8,{17,9},true,{},lobes,true,true);
        check(scene && scene->scene_paths && scene->model_paths && scene->storage_bytes==12*8*(44+52*lobes)
            && scene->base_offset==12*8*28 && scene->incoming_offset==12*8*44 && *scene!=*paths,
            "Mixed scene paths omitted CURRENT base light or aliased model-only records");
        check(scene_frame.valid() && render::shadows::reflection_history_transport_valid(scene_frame,lobes==8?.35F:0.F,3,true),
            "Explicit analytic scene paths rejected valid model/ground transport");
        for(unsigned material:{1U,2U}) {
            render::shadows::RayWater floor;floor.material=material;floor.mirror_models=true;
            check(render::shadows::reflection_history_receiver_valid(scene_frame,&floor,true),
                "Mixed mirror/gold paths rejected reflective model receivers");
            for(unsigned fault=0;fault<5;++fault) {
                auto bad=floor;if(fault==0) bad.material=0;if(fault==1) bad.material=3;
                if(fault==2) bad.source_colour=std::array<float,3>{.1F,.2F,.3F};
                if(fault==3) bad.auxiliary_layers=true;if(fault==4) bad.surface_layers=true;
                check(!render::shadows::reflection_history_receiver_valid(scene_frame,&bad,true),
                    "Mixed planar paths accepted curved/liquid or incompatible layered receiver");
            }
        }
        check(!render::shadows::native_reflection_history(12,8,{17,9},true,{},lobes,false,true)
            && !render::shadows::native_reflection_history(12,8,{17,9},true,{},0,false,true)
            && !render::shadows::native_reflection_history(16384,16384,{17,9},true,{},lobes,true,true),
            "Mixed scene records lacked ordered witnesses or overflowed their allocation");
    }
    check(!render::shadows::native_reflection_history(12,8,{17,9},true,{},2)
        && !render::shadows::native_reflection_history(12,8,{17,9},true,{},UINT32_MAX),
        "Compact history accepted an unbounded/arbitrary quadrature count");
    check(!render::shadows::native_reflection_history(12,8,{17,9},true,{},0,true),
        "Ordered path witnesses accepted legacy motion-plane storage");
    {
        render::shadows::RayReflectionHistory metadata{64,{17,9},{100,110,8,4},1,65536,128,true};
        metadata.model_lobes=1;metadata.model_paths=true;
        render::CalibratedReflectionLobeGeometry guide{reinterpret_cast<void*>(1),132,3,metadata,0};
        check(guide.valid(),"Ordered path geometry rejected identity camera rotations");
        for(unsigned fault=0;fault<5;++fault) {
            auto invalid=guide;
            if(fault==0) invalid.current_cube[0]=std::numeric_limits<float>::quiet_NaN();
            if(fault==1) invalid.previous_cube[4]=std::numeric_limits<float>::infinity();
            if(fault==2) invalid.current_cube[0]=2;
            if(fault==3) invalid.previous_cube[3]=1;
            if(fault==4) invalid.bytes=131;
            check(!invalid.valid(),"Ordered path geometry accepted malformed rotation or truncated mapping");
        }
        const float c=std::cos(.37F),s=std::sin(.37F);
        guide.current_cube={c,0,s,0,1,0,-s,0,c};
        check(guide.valid(),"Ordered path geometry rejected a rigid environment camera rotation");
    }
    for(bool world:{false,true}) {
        const auto liquid=*render::shadows::native_water_layers(12,8,world);
        const auto combined=render::shadows::native_reflection_history(12,8,{17,9},true,liquid);
        const unsigned prefix=world?24:20,count=12*8;
        check(combined && combined->motion_offset==count*prefix && combined->identity_offset==count*(prefix+16)
            && combined->witness_offset==count*(prefix+32) && combined->incoming_offset==count*(prefix+48)
            && combined->base_offset==count*(prefix+52) && combined->weight_offset==count*(prefix+68)
            && combined->storage_bytes==count*(prefix+84) && combined->motion_offset==liquid.storage_bytes,
            "Combined reflection allocation overlapped or moved the original liquid ABI");
        check(!render::shadows::native_reflection_history(12,8,{17,9},false,liquid),
            "Liquid RGB history accepted a whole-radiance layout");
        auto bad=liquid;bad.surface_offset+=4;
        check(!render::shadows::native_reflection_history(12,8,{17,9},true,bad),"Invalid surface prefix accepted");
        bad=liquid;bad.storage_bytes+=4;
        check(!render::shadows::native_reflection_history(12,8,{17,9},true,bad),"Invalid liquid prefix size accepted");
        bad=liquid;bad.world_offset=world?0:4;
        check(!render::shadows::native_reflection_history(12,8,{17,9},true,bad),"Ambiguous liquid prefix mode accepted");
        const auto large=*render::shadows::native_water_layers(8192,6000,world);
        check(!render::shadows::native_reflection_history(8192,6000,{17,9},true,large),
            "Combined reflected/liquid allocation overflowed although the prefix fit");
    }
    render::shadows::RayReflectionHistory ground_history{64,{17,9},{100,110,8,4},1,65536,128,true};
    ground_history.previous_ground=render::shadows::RayReflectionGround{{0,256,0},{.1,-1,.02}};
    check(ground_history.valid(),"Accepted analytic ground reflection calibration was rejected");
    ground_history.separated=false;
    check(!ground_history.valid(),"Old ground plane borrowed the ordinary whole-radiance layout");
    ground_history.separated=true;ground_history.previous_ground->normal={};
    check(!ground_history.valid(),"Degenerate old analytic ground plane was accepted");
    ground_history.previous_ground->normal={0,-1,0};
    ground_history.previous_ground->point[0]=std::numeric_limits<double>::quiet_NaN();
    check(!ground_history.valid(),"Nonfinite old analytic ground plane was accepted");
    render::shadows::RayReflectionHistory liquid_history{64,{17,9},{100,110,8,4},1,65536,128,true};
    render::shadows::RayWater liquid;liquid.source_colour=std::array<float,3>{.2F,.3F,.4F};
    check(render::shadows::reflection_history_receiver_valid(liquid_history,&liquid,true),
        "First native water frame cannot seed separated radiance");
    liquid_history.previous_liquid=render::shadows::RayReflectionLiquid{};
    liquid_history.previous_liquid->ground={{0,256,0},{0,-1,0}};
    liquid_history.previous_liquid->time=4.5;
    {
        auto packed=liquid_history;
        packed.previous_liquid->offset={1.25,-2.5,3.75};packed.previous_liquid->material=3;
        packed.previous_liquid->world_to_view={1,.02,.03,.04,1,.06,.07,.08,1};
        const auto frame=render::shadows::reflection_liquid_frame_words(packed);
        for(unsigned i=0;i<3;++i) {
            check(frame[i]==float(packed.previous_liquid->ground.point[i])
                && frame[4+i]==float(packed.previous_liquid->ground.normal[i]),"Native old-liquid plane packing changed");
            for(unsigned j=0;j<3;++j) check(frame[8+i*4+j]==float(packed.previous_liquid->world_to_view[i*3+j]),
                "Native old-liquid matrix packing changed");
            check(frame[11+i*4]==float(packed.previous_liquid->offset[i]),"Native old-liquid offset packing changed");
        }
        for(unsigned i=0;i<4;++i) check(frame[20+i]==float(packed.projection[i]),"Native old-liquid projection packing changed");
        check(frame[24]==17 && frame[25]==9 && frame[26]==1 && frame[27]==65536
            && frame[28]==4.5F && frame[29]==3 && frame[3]==0 && frame[7]==0 && frame[30]==0 && frame[31]==0,
            "Native old-liquid extent/clip/clock/material/reserved packing changed");
        packed.previous_liquid.reset();
        check(render::shadows::reflection_liquid_frame_words(packed)==std::array<float,32>{},"Absent old liquid does not pack zero");
    }
    check(render::shadows::reflection_history_receiver_valid(liquid_history,&liquid,true),
        "Accepted old native wave frame was rejected");
    for(unsigned bad=0;bad<11;++bad) {
        auto malformed=liquid_history;
        if(bad==0) malformed.previous_liquid->ground.normal={};
        if(bad==1) malformed.previous_liquid->ground.point[0]=std::numeric_limits<double>::infinity();
        if(bad==2) malformed.previous_liquid->time=-1;
        if(bad==3) malformed.previous_liquid->time=std::numeric_limits<double>::quiet_NaN();
        if(bad==4) malformed.previous_liquid->material=1;
        if(bad==5) malformed.previous_liquid->world_to_view={};
        if(bad==6) malformed.previous_liquid->world_to_view[4]=std::numeric_limits<double>::infinity();
        if(bad==7) malformed.previous_liquid->offset[2]=std::numeric_limits<double>::quiet_NaN();
        if(bad==8) malformed.previous_liquid->offset[0]=1.e13;
        if(bad==9) malformed.previous_ground=render::shadows::RayReflectionGround{{0,256,0},{0,-1,0}};
        if(bad==10) malformed.separated=false;
        check(!malformed.valid(),"Malformed or conflicting old wave metadata was accepted");
    }
    liquid.material=3;
    check(!render::shadows::reflection_history_receiver_valid(liquid_history,&liquid,true),
        "Old water metadata was borrowed by current lava");
    liquid_history.previous_liquid->material=3;liquid.source_colour.reset();
    check(render::shadows::reflection_history_receiver_valid(liquid_history,&liquid,true),
        "Native lava accepted-wave frame was rejected");
    liquid.auxiliary_layers=true;
    check(!render::shadows::reflection_history_receiver_valid(liquid_history,&liquid,true),
        "Lava acquired the translucent water prefix");
    render::CalibratedReflectionTimeline timeline;
    const std::array eyes{camera,camera};
    const render::CalibratedReflectionTimeline::Extents extents{{{12,8},{17,9}}};
    auto frame=std::make_shared<render::CalibratedGameFrame>(now);frame->presentation_seconds=1;
    check(timeline.prepare(frame,eyes,extents) && !timeline.previous() && !timeline.held(),
        "First reflection presentation borrowed a nonexistent AA timeline");
    const auto first_epoch=timeline.epoch();
    check(!timeline.prepare(frame,eyes,extents),"Pending reflection presentation was overwritten");
    timeline.settle(true);
    check(timeline.prepare(frame,eyes,extents) && timeline.previous()==frame.get() && timeline.held()
        && timeline.epoch()==first_epoch,"AA-OFF held secondary RGB did not retain exact acceptance");
    timeline.settle(true);
    auto moved=std::make_shared<render::CalibratedGameFrame>(*frame);moved->draws[0].packet.model[12]+=.1F;
    moved->presentation_seconds+=.01;
    check(timeline.prepare(moved,eyes,extents) && timeline.previous()==frame.get() && !timeline.held(),
        "Moving reflection reused a held sample or lost accepted primitive correspondence");
    timeline.settle(false);
    for(unsigned material:{5U,6U,7U,8U,9U}) {
        simulation::SnesPpuState ppu;ppu.background_mode=2;ppu.main_screen=2;
        auto floor=vr::landscape_sphere_packet(ppu,{},112,false,false,232);
        vr::place_landscape_ground(floor,-.5F,true);
        render::CalibratedGroundGradient gradient{{.2F,.3F,.1F},{.4F,.5F,.2F}};
        gradient.material=material;gradient.seconds=2;
        check(render::apply_calibrated_ground(floor,gradient),"Native liquid clock fixture did not retain its floor");
        const auto receiver=render::calibrated_ground_ray_receiver(floor,camera,.8F);
        check(receiver.has_value()==(material!=8),"Red sand became a ray liquid or native lava lost its ray receiver");
        if(receiver) check(receiver->transport.material==(material==9?3:material-5)
            && receiver->transport.source_colour.has_value()==(material==5),"Native ground lost its water/mirror/gold/lava transport identity");
        if(material!=5 && material!=9) continue;
        render::CalibratedReflectionTimeline liquid_timeline;
        check(liquid_timeline.prepare(frame,eyes,extents),"Liquid timeline seed failed");liquid_timeline.settle(true);
        auto source=std::make_shared<render::CalibratedGameFrame>(*frame);
        source->draws.push_back({floor,render::CalibratedGameLayer::world,vr::SceneBlend::opaque,false,render::calibrated_ground_source_key});
        check(liquid_timeline.prepare(source,eyes,extents) && !liquid_timeline.previous(),"Liquid layout insertion reused ordinary history");
        liquid_timeline.settle(true);
        check(liquid_timeline.prepare(source,eyes,extents) && liquid_timeline.held(),"Unchanged accepted wave did not retain its RGB");liquid_timeline.settle(false);
        auto wave=std::make_shared<render::CalibratedGameFrame>(*source);
        wave->draws.back().packet.geometry.texels[render::calibrated_ground_offset+11]=std::bit_cast<std::uint32_t>(2.2F);
        check(liquid_timeline.prepare(wave,eyes,extents) && liquid_timeline.previous()==source.get() && !liquid_timeline.held(),
            "Advancing retained wave froze RGB or borrowed a host effect clock");liquid_timeline.settle(true);
        check(liquid_timeline.prepare(source,eyes,extents) && !liquid_timeline.previous() && !liquid_timeline.held(),
            "Retained wave clock rewind kept reflected RGB history");liquid_timeline.settle(false);
        auto switched=std::make_shared<render::CalibratedGameFrame>(*wave);
        switched->draws.back().packet.geometry.texels[render::calibrated_ground_offset+8]=material==5?9:5;
        check(liquid_timeline.prepare(switched,eyes,extents) && !liquid_timeline.previous(),"Water/lava transition borrowed old optical history");liquid_timeline.settle(false);
        check(liquid_timeline.prepare(frame,eyes,extents),"Liquid timeline could not restore ordinary source");liquid_timeline.settle(true);
    }
    check(timeline.prepare(frame,eyes,extents) && timeline.previous()==frame.get() && timeline.held(),
        "Rejected reflection presentation replaced the accepted source");
    timeline.settle(false);
    auto cut=std::make_shared<render::CalibratedGameFrame>(*frame);cut->settings.history_epoch=9;
    check(timeline.prepare(cut,eyes,extents) && !timeline.previous() && !timeline.held()
        && timeline.epoch()==first_epoch+1,"Reflection settings cut retained incompatible RGB");
    timeline.settle(false);
    auto resized=extents;resized[1][0]+=1;
    check(timeline.prepare(frame,eyes,resized) && !timeline.previous(),"Reflection extent change retained incompatible RGB");
    timeline.settle(false);
    auto backwards=std::make_shared<render::CalibratedGameFrame>(*frame);backwards->presentation_seconds=.5;
    check(timeline.prepare(backwards,eyes,extents) && !timeline.previous(),"Reflection time rewind retained history");
    timeline.settle(false);
    auto shifted=eyes;shifted[1].view[12]+=.04F;
    check(timeline.prepare(frame,shifted,extents) && timeline.previous() && !timeline.held(),
        "Changed tracked eye reused held reflected RGB");
    timeline.settle(false);
    check(timeline.prepare(frame,eyes,extents,{.25F,-.25F}) && timeline.previous() && !timeline.held(),
        "Changed TAA jitter reused held secondary RGB");
    timeline.settle(false);
    auto malformed=extents;malformed[0][0]=0;
    check(!timeline.prepare(frame,eyes,malformed) && !timeline.prepare(frame,eyes,extents,{2,0}),
        "Malformed reflection extent/jitter was accepted");
    shifted=eyes;shifted[0].projection[11]=0;
    check(!timeline.prepare(frame,shifted,extents),"Invalid reflection projection was accepted");
    for(unsigned samples:{2U,4U,8U}) {
        render::CalibratedReflectionTimeline sampled;
        check(sampled.prepare(frame,eyes,extents,{},samples),"MSAA reflection timeline failed to seed its sample count");
        sampled.settle(true);
        check(sampled.samples()==samples && sampled.prepare(frame,eyes,extents,{},samples)
            && sampled.held() && sampled.previous()==frame.get(),"Held reflection lost independent native sample banks");
        sampled.settle(false);
        check(sampled.prepare(frame,eyes,extents,{},1) && !sampled.previous() && !sampled.held(),
            "MSAA count transition borrowed another pattern's accepted reflection footprint");
        sampled.settle(false);
        check(sampled.samples()==samples && sampled.prepare(frame,eyes,extents,{},samples) && sampled.held(),
            "Rejected MSAA count transition replaced accepted sample identities");
        sampled.settle(false);
        check(!sampled.prepare(frame,eyes,extents,{},3) && !sampled.prepare(frame,eyes,extents,{},16),
            "Unsupported native reflection sample count was accepted");
    }
    auto changed_tick=std::make_shared<vr::GameSceneSnapshot>(*frame->current);changed_tick->scene_epoch+=1;
    auto changed_scene=std::make_shared<render::CalibratedGameFrame>(*frame);changed_scene->current=changed_tick;
    check(timeline.prepare(changed_scene,eyes,extents) && !timeline.previous(),"Reflection scene cut retained history");
    timeline.settle(false);
    changed_tick->scene_epoch=frame->current->scene_epoch;changed_tick->flow=simulation::GameFlowState::gameplay;
    check(timeline.prepare(changed_scene,eyes,extents) && !timeline.previous(),"Reflection flow change retained history");
    timeline.settle(false);
    auto clocked=std::make_shared<render::CalibratedGameFrame>(*frame);
    clocked->settings.extra_effects[1]=unsigned(render::Effect::energy_shield);clocked->settings.effect_seconds=2;
    check(timeline.prepare(clocked,eyes,extents),"Clocked reflection source failed to prepare");timeline.settle(true);
    auto rewound=std::make_shared<render::CalibratedGameFrame>(*clocked);rewound->settings.effect_seconds=1;
    check(timeline.prepare(rewound,eyes,extents) && !timeline.previous(),"Animated effect clock rewind retained secondary RGB");
    timeline.settle(false);
    std::cout<<"Reflection allocation ABI: unchanged 52/88-byte layouts and 104/108-byte liquid prefixes; compact 40/124-byte, ordered 80/444-byte model and mixed 96/460-byte ground/model records, exact offsets, malformed prefixes and full-allocation overflow rejection passed\n";
    std::cout<<"Native liquid metadata: water/lava seed and accepted wave frames, finite/invertible transforms, clocks, material matching and conflicting flat/layer layouts passed\n";
    std::cout<<"Native ground owner metadata: correct water/mirror/gold/lava ray mapping, red-sand exclusion, actual retained-wave held/advancing/rewound clocks and water/lava cuts passed\n";
    std::cout<<"Accepted ray identity: reordered/inserted/recycled/ambiguous actors, line expansion, noncasters, invalid camera and explicit resident layout passed\n";
    std::cout<<"Independent reflection timeline: AA OFF, held/moving/cut/resized/rewound/tracked/jittered presentations, 2/4/8 native sample identities, rejected count transitions, pending/rejected ownership and malformed inputs passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
