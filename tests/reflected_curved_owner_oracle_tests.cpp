#include "reflected_curved_visibility_domain.hpp"
#include <iostream>
#include <iomanip>
#include <string_view>

namespace o=reflected_curved_owner_oracle;
void require(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
void visibility_domain_tests() {
    o::Inputs in;in.liquid.width=8;in.liquid.height=6;in.previous.resize(1);in.mapping={0};
    std::array<unsigned,16> r{};std::fill_n(r.begin(),4,UINT32_MAX);r[4]=256;r[5]=0;
    const auto target=[&](float x,float y) {r[6]=std::bit_cast<unsigned>(x);r[7]=std::bit_cast<unsigned>(y);r[8]=0;};
    target(.123F,.234F);
    unsigned reads=0;bool missing=false;
    const auto linear=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {
        ++reads;if(missing && x==3 && y==4)return {};
        o::SourceTap t;t.identity=0;t.depth=20;std::copy_n(r.begin(),9,t.path.begin());
        t.path[6]=std::bit_cast<unsigned>(float(.1+.01*x));t.path[7]=std::bit_cast<unsigned>(float(.2+.01*y));return t;
    };
    const auto regions=o::old_visibility_domains(in,0,r,linear);
    require(reads==48,"Visibility domains reread the raw bank instead of bounded preprocessing");
    require(regions.size()==1 && regions[0].span_x==1 && regions[0].span_y==1
        && regions[0].x==2 && regions[0].y==3,"Literal linear feature domain did not isolate its one possible cell");
    const auto& region=regions[0];
    require(std::abs(region.minimum[0]-2.199866666666667)<1.e-5
        && std::abs(region.maximum[0]-2.533466666666667)<1.e-5
        && std::abs(region.minimum[1]-3.266533333333333)<1.e-5
        && std::abs(region.maximum[1]-3.600133333333333)<1.e-5,
        "Visibility half-plane intersection changed literal per-tap feature bounds");
    require(region.contains(2.3,3.4),"Necessary feature domain excluded the authored linear source");
    auto rgb_changed=r;for(unsigned c=9;c<16;++c)rgb_changed[c]=UINT32_MAX;
    const auto unchanged=o::old_visibility_domains(in,0,rgb_changed,linear);
    require(unchanged.size()==1 && unchanged[0].minimum==region.minimum && unchanged[0].maximum==region.maximum,
        "Visibility domain used incoming colour/response/base rather than geometric features");
    missing=true;
    require(o::old_visibility_domains(in,0,r,linear).empty(),"Missing nonzero source tap acquired visibility in a domain");
    missing=false;target(.12F,.23F);
    const auto boundary=o::old_visibility_domains(in,0,r,linear);
    require(std::any_of(boundary.begin(),boundary.end(),[](const auto& cell) {
        return cell.span_x==0 && cell.span_y==0 && cell.x==2 && cell.y==3 && cell.contains(2,3);
    }),"Visibility domains omitted zero-weight integer sources");
    target(.123F,.23F);
    const auto horizontal=o::old_visibility_domains(in,0,r,linear);
    require(std::any_of(horizontal.begin(),horizontal.end(),[](const auto& cell) {
        return cell.span_x==1 && cell.span_y==0 && cell.contains(2.3,3);
    }),"Visibility domains omitted zero-weight horizontal-edge sources");
    target(.12F,.234F);
    const auto vertical=o::old_visibility_domains(in,0,r,linear);
    require(std::any_of(vertical.begin(),vertical.end(),[](const auto& cell) {
        return cell.span_x==0 && cell.span_y==1 && cell.contains(2,3.4);
    }),"Visibility domains omitted zero-weight vertical-edge sources");
    // Two visible islands with identical geometric features. Neither colour
    // nor a local inverse/initializer may delete the nonlocal island.
    const auto islands=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {
        if((x!=1 || y!=1) && (x!=6 || y!=4))return {};
        o::SourceTap t;t.identity=0;t.depth=20;std::copy_n(r.begin(),9,t.path.begin());return t;
    };
    const auto separate=o::old_visibility_domains(in,0,r,islands);
    require(separate.size()==2 && separate[0].contains(1,1) && separate[1].contains(6,4),
        "Visibility domains silently selected the local of two old feature islands");
    // Noncontributing neighbouring depths may be invalid while their valid
    // matched features still provide the gradient used by source_guard.
    auto edge_input=in;edge_input.liquid.width=5;edge_input.liquid.height=1;target(.10003F,.2F);
    const auto edge_read=[&](unsigned x,unsigned)->std::optional<o::SourceTap> {
        o::SourceTap t;t.identity=0;t.depth=x==2 || x==3?20:INFINITY;std::copy_n(r.begin(),9,t.path.begin());
        t.path[6]=std::bit_cast<unsigned>(x==2 || x==3?.1F:.2F);return t;
    };
    const o::Solution edge_root{2.5,0,20,0,true,false};
    require(o::source_guard(edge_input,0,r,0,0,edge_root,edge_read)==o::SourceGuard::accepted,
        "Authored contributing-depth/adjacent-feature distinction no longer qualifies");
    const auto depth_edge=o::old_visibility_domains(edge_input,0,r,edge_read);
    require(std::any_of(depth_edge.begin(),depth_edge.end(),[](const auto& cell){return cell.contains(2.5,0);}),
        "Visibility domains incorrectly rejected a valid adjacent feature because of noncontributing depth");
    // A necessary polygon's bounding rectangle is not actual source coverage.
    // Independently trace a finite optical root, then author raw old features
    // whose diagonal admissible half-plane excludes that root but whose AABB
    // retains it. This is an actual predicate hole, not a hand-written GPU box.
    o::Inputs hole;hole.liquid.width=17;hole.liquid.height=9;hole.liquid.projection={256,272,8,4};
    hole.liquid.near=1;hole.liquid.far=65536;hole.current_projection=hole.liquid.projection;
    hole.previous={o::Plane{{-100,-100,400},{200,-100,400},{-100,200,400}},
        o::Plane{{-100,-100,100},{200,-100,100},{-100,200,100}}};hole.current=hole.previous;hole.mapping={0,1};
    o::Path optical;optical.receiver=hole.previous[0];optical.finite_terminal=true;optical.terminal_plane=hole.previous[1];
    const auto forward=o::forward(hole,optical,5.625,1.625);require(forward.valid,"Source-hole physical forward input failed");
    const double distance=(100-forward.origin[2])/forward.outgoing[2];
    const auto bary=o::bary(optical.terminal_plane,o::add(forward.origin,o::scale(forward.outgoing,distance)));
    std::array<unsigned,16> hr{};std::fill_n(hr.begin(),4,UINT32_MAX);hr[4]=256;hr[5]=1;
    hr[6]=std::bit_cast<unsigned>(float(bary[0]));hr[7]=std::bit_cast<unsigned>(float(bary[1]));
    optical.terminal=o::point(optical.terminal_plane,{std::bit_cast<float>(hr[6]),std::bit_cast<float>(hr[7])});
    const auto physical=o::solve_forward(hole,optical,5.625,1.625);require(physical.valid,"Source-hole independent root failed");
    const auto hole_read=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {
        if(x<4 || x>6 || y>2)return {};o::SourceTap tap;tap.identity=0;tap.depth=400;
        std::copy_n(hr.begin(),9,tap.path.begin());tap.path[6]=std::bit_cast<unsigned>(std::bit_cast<float>(hr[6])
            +(x==5 && y==1?.03F:0.F));return tap;
    };
    const o::Solution physical_source{physical.x-.5,physical.y-.5,physical.depth,physical.error,true,false};
    const auto hole_domains=o::old_visibility_domains(hole,0,hr,hole_read);
    require(std::any_of(hole_domains.begin(),hole_domains.end(),[&](const auto& cell) {
        return cell.x==5 && cell.y==1 && cell.span_x==1 && cell.span_y==1 && cell.contains(physical_source.x,physical_source.y);
    }),"Raw source-hole half-plane did not retain the physical root in its necessary bounding rectangle");
    require(o::source_guard(hole,0,hr,0,0,physical_source,hole_read)==o::SourceGuard::feature,
        "A root in a necessary bounding-box hole bypassed the independent actual source predicate");
    for(unsigned fault:{1U,2U}) {
        const auto fold_read=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {
            if(x<4 || x>6 || y>2)return {};o::SourceTap tap;tap.identity=0;tap.depth=400;
            std::copy_n(hr.begin(),9,tap.path.begin());const int dx=int(x)-5,dy=int(y)-1;
            const float feature_x=std::bit_cast<float>(hr[6])+.001F*float(fault==1?dx*(dy==1?-1:1):std::abs(dx));
            const float feature_y=std::bit_cast<float>(hr[7])+.001F*float(dy);
            require(feature_x>=0 && feature_y>=0 && feature_x+feature_y<=1,"Literal fold barycentrics malformed");
            tap.path[6]=std::bit_cast<unsigned>(feature_x);tap.path[7]=std::bit_cast<unsigned>(feature_y);return tap;
        };
        const auto regions=o::old_visibility_domains(hole,0,hr,fold_read);
        require(std::any_of(regions.begin(),regions.end(),[&](const auto& cell) {return cell.contains(physical_source.x,physical_source.y);}),
            "Literal fold fixture lost its independently traced physical source");
        require(o::source_guard(hole,0,hr,0,0,physical_source,fold_read)
            ==(fault==1?o::SourceGuard::cell_fold:o::SourceGuard::ring_fold),
            "Literal raw source cell/ring fold did not refuse at the intended actual guard");
    }
    auto maximum=in;maximum.liquid.width=maximum.liquid.height=256;
    const auto uniform=[&](unsigned,unsigned)->std::optional<o::SourceTap> {
        o::SourceTap t;t.identity=0;t.depth=20;std::copy_n(r.begin(),9,t.path.begin());return t;
    };
    const auto maximum_regions=o::old_visibility_domains(maximum,0,r,uniform);
    require(maximum_regions.size()==261121 && maximum_regions.back().contains(255,255),
        "Visibility-domain storage limit rejected a valid maximum bank or lost boundary supports");
    auto remapped=in;remapped.previous.resize(2);remapped.mapping={1,0};
    auto mr=r;mr[0]=1;mr[4]=257;mr[5]=0;bool wrong_hop=false,wrong_depth=false,wrong_feature=false;
    const auto mapped=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {
        if(x!=1 || y!=1)return {};
        o::SourceTap t;t.identity=1;t.depth=wrong_depth?INFINITY:20;
        std::copy_n(mr.begin(),9,t.path.begin());t.path[0]=wrong_hop?1:0;t.path[5]=1;
        if(wrong_feature)t.path[6]=std::bit_cast<unsigned>(std::numeric_limits<float>::quiet_NaN());
        return t;
    };
    require(o::old_visibility_domains(remapped,0,mr,mapped).size()==1,
        "Visibility domains failed to map receiver, ordered finite hop and terminal IDs");
    wrong_hop=true;require(o::old_visibility_domains(remapped,0,mr,mapped).empty(),"Unmapped hop entered a visibility domain");
    wrong_hop=false;wrong_depth=true;
    require(o::old_visibility_domains(remapped,0,mr,mapped).empty(),"Nonfinite contributing depth entered a visibility domain");
    wrong_depth=false;wrong_feature=true;
    require(o::old_visibility_domains(remapped,0,mr,mapped).empty(),"Nonfinite raw feature entered a visibility domain");
    wrong_feature=false;mr[0]=0xfffffffdU;mr[4]=273;
    const auto analytic=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {
        auto t=mapped(x,y);if(t)t->path[0]=0xfffffffdU;return t;
    };
    require(o::old_visibility_domains(remapped,0,mr,analytic).size()==1,
        "Visibility domain remapped an analytic liquid hop as finite geometry");
    for(unsigned malformed=0;malformed<3;++malformed) {
        auto bad=r;if(malformed==0)bad[4]=261;if(malformed==1)bad[4]=272;
        if(malformed==2)bad[6]=std::bit_cast<unsigned>(std::numeric_limits<float>::quiet_NaN());
        require(o::old_visibility_domains(in,0,bad,linear).empty(),"Malformed current path/feature entered a visibility domain");
    }
    auto unbounded=in;unbounded.liquid.width=65537;unbounded.liquid.height=1;bool rejected=false;
    try {(void)o::old_visibility_domains(unbounded,0,r,linear);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"Visibility domains accepted an unbounded raw bank");

    // Full independent source guard, real nonlinear old waves and raw feature
    // bank: every accepted source must remain in the NECESSARY query enclosure.
    o::Inputs water;auto& f=water.liquid;f.point={0,180,0};f.normal={0,-1,0};f.offset={29,0,17};
    f.projection={128,136,48,-80};f.width=96;f.height=64;f.near=1;f.far=65536;f.time=1.3;
    water.current_projection=f.projection;
    o::Path path;path.liquid_primary=true;
    std::array<unsigned,16> wr{};std::fill_n(wr.begin(),4,UINT32_MAX);wr[4]=512;wr[5]=UINT32_MAX;
    std::vector<o::SourceTap> bank(f.width*f.height);
    for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x) {
        const auto ray=o::forward(water,path,x+.5,y+.5);require(ray.valid,"Visibility-domain nonlinear raw bank has missing optics");
        auto& t=bank[y*f.width+x];t.identity=0xfffffffdU;t.depth=double(float(ray.depth));std::copy_n(wr.begin(),9,t.path.begin());
        for(unsigned c=0;c<3;++c)t.path[6+c]=std::bit_cast<unsigned>(float(ray.outgoing[c]));
    }
    const auto raw=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {return bank[y*f.width+x];};
    unsigned accepted=0,fractional=0;
    for(unsigned x:{7U,22U,64U})for(unsigned y:{12U,37U,54U})for(double fraction:{0.,.25,.75}) {
        const double px=x+fraction,py=y+fraction;const auto ray=o::forward(water,path,px+.5,py+.5);
        require(ray.valid,"Authored visibility-domain source has missing optics");
        for(unsigned c=0;c<3;++c)wr[6+c]=std::bit_cast<unsigned>(float(ray.outgoing[c]));
        const o::Solution root{px,py,double(float(ray.depth)),0,true,false};
        if(o::source_guard(water,0xfffffffdU,wr,0,0,root,raw)!=o::SourceGuard::accepted)continue;
        ++accepted;fractional+=fraction!=0;
        const auto domain=o::old_visibility_domains(water,0xfffffffdU,wr,raw);
        require(std::any_of(domain.begin(),domain.end(),[&](const auto& cell){return cell.contains(px,py);}),
            "Necessary visible-domain query excluded an independently qualified nonlinear old source");
    }
    require(accepted>=9 && fractional>0,"Visibility-domain enclosure test lost mandatory qualified nonlinear sources");
    const std::array<float,16> yaw{0,0,-1,0,0,1,0,0,1,0,0,0,23,-17,900,1},identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    o::eye_views(water,yaw,identity);
    const auto ray=o::forward(water,path,7.5,37.5);
    const o::V current_feature{-ray.outgoing[2],ray.outgoing[1],ray.outgoing[0]};
    for(unsigned c=0;c<3;++c)wr[6+c]=std::bit_cast<unsigned>(float(current_feature[c]));
    const auto rotated=o::old_visibility_domains(water,0xfffffffdU,wr,raw);
    require(std::any_of(rotated.begin(),rotated.end(),[](const auto& cell){return cell.contains(7,37);}),
        "Visibility domain used a stale environment eye basis");
}
void ordered_source_guard_tests() {
    o::Inputs inputs;inputs.liquid.width=17;inputs.liquid.height=9;inputs.liquid.projection={256,272,8,4};
    inputs.liquid.near=1;inputs.liquid.far=65536;inputs.current_projection=inputs.liquid.projection;
    inputs.previous={o::Plane{{-100,-100,400},{200,-100,400},{-100,200,400}}};
    for(double z:{100.,600.,50.,700.})inputs.previous.push_back({{-4096,-4096,z},{8192,-4096,z},{-4096,8192,z}});
    inputs.current=inputs.previous;inputs.mapping={0,1,2,3,4};
    for(unsigned lobes:{1U,8U})for(unsigned hops=1;hops<=4;++hops)for(unsigned lobe=0;lobe<lobes;++lobe) {
        o::Path path;path.receiver=inputs.previous[0];path.hops=hops;path.lobe=lobe;path.roughness=lobes==8?.5:0;
        for(unsigned hop=0;hop<hops;++hop)path.mirrors[hop]=inputs.previous[hop+1];
        const auto ray=o::forward(inputs,path,5.625,1.625);require(ray.valid,"Ordered finite old path did not trace all faces");
        std::array<unsigned,16> record{};
        for(unsigned hop=0;hop<4;++hop)record[hop]=hop<hops?hop+1:UINT32_MAX;
        record[4]=((hops==4?3U:2U)<<8)|hops;record[5]=UINT32_MAX;
        for(unsigned c=0;c<3;++c)record[6+c]=std::bit_cast<unsigned>(float(ray.outgoing[c]));
        path.terminal=o::unit({std::bit_cast<float>(record[6]),std::bit_cast<float>(record[7]),std::bit_cast<float>(record[8])});
        const auto root=o::solve_forward(inputs,path,5.625,1.625);require(root.valid,"Independent complete ordered source did not solve");
        const auto read=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {
            if(x<4 || x>6 || y>2)return {};
            const auto tap_ray=o::forward(inputs,path,x+.5,y+.5);require(tap_ray.valid,"Ordered source neighbour missed a complete face");
            o::SourceTap tap;tap.identity=0;tap.depth=float(tap_ray.depth);std::copy_n(record.begin(),9,tap.path.begin());
            for(unsigned c=0;c<3;++c)tap.path[6+c]=std::bit_cast<unsigned>(float(tap_ray.outgoing[c]));
            return tap;
        };
        const o::Solution source{root.x-.5,root.y-.5,root.depth,root.error,true,false};
        require(o::source_guard(inputs,0,record,path.roughness,lobe,source,read)==o::SourceGuard::accepted,
            "Independent complete ordered path failed old footprint/fold admission");
        const auto domains=o::old_visibility_domains(inputs,0,record,read);
        require(std::any_of(domains.begin(),domains.end(),[&](const auto& domain){return domain.contains(source.x,source.y);}),
            "Necessary old supports omitted a qualified ordered source");
        if(hops>=2) {
            auto reordered=record;std::swap(reordered[0],reordered[1]);
            require(o::source_guard(inputs,0,reordered,path.roughness,lobe,source,read)==o::SourceGuard::path,
                "Old source guard ignored finite-bounce order");
        }
    }
}
int main(int argc,char** argv) {
    try {
        const bool receiver_replay=argc==2 && std::string_view(argv[1])=="--receiver-replay-fixture";
        const bool replay_fixture=receiver_replay || (argc==2 && std::string_view(argv[1])=="--camera-replay-fixture");
        require(argc==1 || replay_fixture,"Unknown independent oracle test option");
        visibility_domain_tests();
        ordered_source_guard_tests();
        o::Inputs inputs;auto& f=inputs.liquid;
        f.point={0,180,0};f.normal={0,-1,0};f.offset={29,0,17};
        f.projection={128,136,48,-80};f.width=96;f.height=64;f.near=1;f.far=65536;f.time=1.3;
        inputs.current_projection=f.projection;
        std::array<unsigned,16> record{};
        std::fill(record.begin(),record.begin()+4,UINT32_MAX);
        record[4]=512;record[5]=UINT32_MAX;
        o::Path path;path.liquid_primary=true;
        const auto feature=[&](double x,double y) {
            const auto ray=o::forward(inputs,path,x,y);require(ray.valid,"Independent liquid forward sample failed");
            for(unsigned c=0;c<3;++c)record[6+c]=std::bit_cast<unsigned>(float(ray.outgoing[c]));
        };
        // An independently authored source just across a float/tap boundary.
        // Only the closed initializer is visible; no output RGB or GPU guide
        // participates in source selection.
        feature(7.5-.0000015,37.5-.000027);
        unsigned checks=0;
        const auto closed=[&](const o::Solution& s) {++checks;return s.x==7 && s.y==37;};
        const auto bounded=o::previous_source(inputs,0xfffffffdU,record,0,0,7,37,closed);
        require(bounded.valid && bounded.bounded_seed && checks>=2 && bounded.error<.003,
            "Owner oracle lost independently qualified subpixel/float-boundary initializer");
        const auto abi_neighbour=o::previous_source(inputs,0xfffffffdU,record,0,0,7,37,
            [](const o::Solution& s){return s.x<7 && s.x>=7-1.e-5 && s.y==37;});
        require(abi_neighbour.valid && abi_neighbour.bounded_seed && abi_neighbour.error<.003,
            "Owner oracle lost a representable ABI neighbour under the existing small-step exit");
        const auto hidden=o::previous_source(inputs,0xfffffffdU,record,0,0,7,37,[](const auto&){return false;});
        require(!hidden.valid,"Owner oracle turned missing old visibility into coverage");
        feature(7.5-.01,37.5-.01);
        const auto too_far=o::previous_source(inputs,0xfffffffdU,record,0,0,7,37,closed);
        require(!too_far.valid,"Owner oracle relaxed the existing 1/4096-pixel initializer bound");
        o::Path later=path;for(unsigned c=0;c<3;++c)later.terminal[c]=std::bit_cast<float>(record[6+c]);
        const auto later_root=o::solve_forward(inputs,later,7.5,37.5);
        require(later_root.valid,"Independent later-iteration optical root failed");
        const double later_x=double(float(later_root.x))-.5,later_y=double(float(later_root.y))-.5;
        float odd_exit=float(later_root.x);for(unsigned ulp=0;ulp<17;++ulp)odd_exit=std::nextafter(odd_exit,INFINITY);
        const auto later_neighbour=o::previous_source(inputs,0xfffffffdU,record,0,0,7,37,
            [&](const auto& s){return s.x==double(odd_exit)-.5 && s.y==later_y;});
        require(later_neighbour.valid && later_neighbour.bounded_seed && std::abs(later_x-7)>.001,
            "Owner oracle lost a bounded later-iteration ABI exit away from its initializer");
        // Captured actual optical INPUTS from the moving-water owner fixture.
        // No GPU inverse or expected RGB is part of this stalled-basin check.
        o::Inputs stalled;auto& water=stalled.liquid;
        water.point={8,double(179.44000244140625F),4};water.normal={0,-1,0};water.offset={21,-2,13};
        water.projection={double(104.02788543701172F),double(74.296066284179688F),
            double(56.830692291259766F),double(31.411874771118164F)};
        water.width=128;water.height=72;water.near=1;water.far=65536;water.time=double(1.3F);
        stalled.current_projection=water.projection;
        auto actual=record;actual[6]=1056744784U;actual[7]=3198965930U;actual[8]=1062031279U;
        o::Path moving;moving.liquid_primary=true;
        for(unsigned c=0;c<3;++c)moving.terminal[c]=std::bit_cast<float>(actual[6+c]);
        require(!o::solve_forward(stalled,moving,123.5,67.5).valid,
            "Captured water case no longer exercises the nonzero local minimum");
        unsigned recovery_checks=0;
        const auto recovered=o::previous_source(stalled,0xfffffffdU,actual,0,0,123,67,
            [&](const o::Solution& s){++recovery_checks;return s.error<1.e-8 && s.depth>0;});
        require(recovered.valid && !recovered.bounded_seed && recovery_checks>1,
            "Symmetric independent forward starts did not recover the unique water root");
        require(!o::previous_source(stalled,0xfffffffdU,actual,0,0,123,67,[](const auto&){return false;}).valid,
            "Independent restart fabricated visibility for a hidden old source");
        // A model-to-water return ray has distinct valid preimages. The
        // reference must keep this ambiguity open, not pick whichever RGB fits.
        auto ambiguous=stalled;auto& returning=ambiguous.liquid;
        returning.point[0]=-8;returning.offset[0]=37;returning.time=double(1.315F);
        ambiguous.current_projection[2]=56.830691814422607;
        ambiguous.current_projection[3]=31.411873877048492;
        ambiguous.current={{{61.1199951171875,150.48001098632812,149.51040649414062},
            {153.27999877929688,58.319999694824219,114.48960113525391},
            {61.1199951171875,58.319999694824219,117.25440216064453}}};
        ambiguous.previous={{{61.212833404541016,150.48001098632812,149.44354248046875},
            {153.44270324707031,58.319999694824219,114.60713195800781},
            {61.277347564697266,58.319999694824219,117.18760681152344}}};
        ambiguous.mapping={0};auto branch=actual;branch[0]=0xfffffffdU;branch[4]=529;
        branch[6]=1051955774U;branch[7]=3210728618U;branch[8]=3198926741U;
        unsigned branch_checks=0;
        const auto multiple=o::previous_source(ambiguous,0,branch,0,0,116,70,
            [&](const auto& s){++branch_checks;return s.error<1.e-8 && s.depth>0;});
        require(!multiple.valid && multiple.ambiguous && branch_checks>=2,
            "Independent reference hid distinct qualified optical branches");
        // Minimal raw optical inputs from the larger moving-eye fixture.
        // The old initializer is an authored visible pixel found by scanning
        // the complete old bank, NOT the separate native inverse coordinate.
        o::Inputs distant;auto& distant_water=distant.liquid;
        distant_water.point={8,179.44000244140625,4};distant_water.normal={0,-1,0};
        distant_water.offset={21,-2,13};
        distant_water.projection={104.02788543701172,74.296066284179688,56.830692291259766,31.411874771118164};
        distant_water.width=128;distant_water.height=72;distant_water.near=1;distant_water.far=65536;
        distant_water.time=double(1.3F);
        distant.current_projection={104.02788543701172,74.296066284179688,56.830691814422607,31.411873877048492};
        distant.current_liquid_transform=std::array<float,12>{.99992799758911133F,0,.011999712325632572F,21.219999313354492F,
            0,1,0,-2,-.011999712325632572F,0,.99992799758911133F,13};
        distant.current_eye_view={.99992799758911133F,0,.011999712325632572F,0,0,1,0,0,
            -.011999712325632572F,0,.99992799758911133F,0,.030575932934880257F,-.0078125F,-.015259196050465107F,1};
        distant.previous_eye_view={1,0,0,0,0,1,0,0,0,0,1,0,.03125F,-.0078125F,-.015625F,1};
        distant.previous.resize(2);distant.mapping={0,1};
        distant.previous[1]={{-273.60000610351562,283.60000610351562,735.64801025390625},
            {289.60000610351562,-279.60000610351562,296.35198974609375},
            {-273.60000610351562,-279.60000610351562,341.40802001953125}};
        std::array<unsigned,16> distant_record{1,0xfffffffdU,UINT32_MAX,UINT32_MAX,546,UINT32_MAX,
            3199339221U,3208016934U,3206280295U};
        const auto distant_visible=[](const auto& s) {
            return s.valid && s.error<1.e-8 && s.depth>0 && s.x>30 && s.x<45 && s.y>60 && s.y<70;
        };
        require(!o::previous_source(distant,0xfffffffdU,distant_record,0,0,25,63,distant_visible,415.47097778320312).valid,
            "Distant optical case no longer exercises missing local initializer coverage");
        const std::array<std::array<double,2>,1> distant_starts{{{28.5,58.5}}};
        const auto distant_source=o::previous_source(distant,0xfffffffdU,distant_record,0,0,25,63,distant_visible,
            415.47097778320312,distant_starts);
        require(distant_source.valid && !distant_source.bounded_seed && distant_visible(distant_source),
            "Independent old visible-pixel initializer failed to recover the distant optical branch");
        require(!o::previous_source(distant,0xfffffffdU,distant_record,0,0,25,63,[](const auto&){return false;},
            415.47097778320312,distant_starts).valid,"Distant initializer fabricated hidden old visibility");
        std::vector<std::array<double,2>> unbounded_starts(65537);bool unbounded_rejected=false;
        try {(void)o::previous_source(distant,0xfffffffdU,distant_record,0,0,25,63,distant_visible,
            415.47097778320312,unbounded_starts);}catch(const std::runtime_error&){unbounded_rejected=true;}
        require(unbounded_rejected,"Independent optical fallback accepted unbounded initializer storage");
        feature(7.5,37.5);
        const auto own_ray=o::forward(inputs,path,7.5,37.5);
        o::SourceTap own;own.identity=0xfffffffdU;own.depth=double(float(own_ray.depth));
        std::copy_n(record.begin(),9,own.path.begin());
        const o::Solution integral{7,37,own.depth,0,true,false};
        const auto own_only=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {
            return x==7 && y==37?std::optional{own}:std::nullopt;
        };
        const auto starts=o::old_visibility_initializers(inputs,0xfffffffdU,record,own_only);
        require(starts.size()==1 && starts[0]==std::array<double,2>{7.5,37.5},
            "Old visible-bank initializer scan lost the authored pixel centre");
        auto remapped=inputs;remapped.previous.resize(2);remapped.mapping={1,0};
        auto remapped_record=record;remapped_record[0]=1;remapped_record[4]=513;
        auto remapped_tap=own;remapped_tap.identity=1;std::copy_n(remapped_record.begin(),9,remapped_tap.path.begin());
        remapped_tap.path[0]=0;
        const auto remapped_read=[&](unsigned x,unsigned y)->std::optional<o::SourceTap> {
            return x==7 && y==37?std::optional{remapped_tap}:std::nullopt;
        };
        require(o::old_visibility_initializers(remapped,0,remapped_record,remapped_read).size()==1,
            "Old visible initializer scan failed to remap receiver and finite hop IDs");
        remapped_tap.path[0]=1;
        require(o::old_visibility_initializers(remapped,0,remapped_record,remapped_read).empty(),
            "Old visible initializer scan accepted an unmapped ordered hop");
        remapped_tap.path[0]=0;remapped_tap.depth=std::numeric_limits<double>::quiet_NaN();
        require(o::old_visibility_initializers(remapped,0,remapped_record,remapped_read).empty(),
            "Old visible initializer scan accepted nonfinite depth");
        remapped_tap.depth=own.depth;remapped_record[0]=0xfffffffdU;remapped_record[4]=529;
        std::copy_n(remapped_record.begin(),9,remapped_tap.path.begin());
        require(o::old_visibility_initializers(remapped,0,remapped_record,remapped_read).size()==1,
            "Old visible initializer scan tried to remap an analytic liquid hop as finite geometry");
        remapped_record[0]=1;remapped_record[4]=257;remapped_record[5]=0;
        std::copy_n(remapped_record.begin(),9,remapped_tap.path.begin());remapped_tap.path[0]=0;remapped_tap.path[5]=1;
        require(o::old_visibility_initializers(remapped,0,remapped_record,remapped_read).size()==1,
            "Old visible initializer scan failed to remap the finite terminal endpoint");
        remapped_tap.path[5]=0;
        require(o::old_visibility_initializers(remapped,0,remapped_record,remapped_read).empty(),
            "Old visible initializer scan accepted an unmapped finite terminal endpoint");
        require(o::old_visibility_initializers(remapped,2,remapped_record,remapped_read).empty(),
            "Old visible initializer scan fabricated an absent receiver mapping");
        auto unbounded_bank=inputs;unbounded_bank.liquid.width=65537;unbounded_bank.liquid.height=1;
        unbounded_rejected=false;
        try {(void)o::old_visibility_initializers(unbounded_bank,0xfffffffdU,record,own_only);}
        catch(const std::runtime_error&){unbounded_rejected=true;}
        require(unbounded_rejected,"Old initializer scan accepted unbounded visibility dimensions");
        const auto guard=[&](const o::Solution& s) {return o::source_guard(inputs,0xfffffffdU,record,0,0,s,own_only);};
        require(guard(integral)==o::SourceGuard::accepted,"Independent source guard rejected a visible integral source");
        own.identity=UINT32_MAX;
        require(guard(integral)==o::SourceGuard::path,"Independent source guard invented hidden old ownership");
        own.identity=0xfffffffdU;own.depth=std::numeric_limits<double>::quiet_NaN();
        require(guard(integral)==o::SourceGuard::depth,"Independent source guard accepted nonfinite old depth");
        own.depth=integral.depth;
        const auto displaced=o::unit({double(std::bit_cast<float>(record[6]))+.05,
            std::bit_cast<float>(record[7]),std::bit_cast<float>(record[8])});
        for(unsigned c=0;c<3;++c)own.path[6+c]=std::bit_cast<unsigned>(float(displaced[c]));
        require(guard(integral)==o::SourceGuard::feature,"Independent source guard ignored the ordered-feature footprint");
        std::copy_n(record.begin()+6,3,own.path.begin()+6);auto fractional=integral;fractional.x+=.25;fractional.y+=.25;
        require(guard(fractional)!=o::SourceGuard::accepted,"Independent source guard skipped nonzero missing bilinear taps");
        // Noncommuting, independently authored RH yaw/roll matrices. Literal
        // expected axes catch transpose/order/handedness errors; translating
        // either eye must not move the infinite environment.
        const std::array<float,16> yaw{0,0,-1,0, 0,1,0,0, 1,0,0,0, 23,-17,900,1};
        const std::array<float,16> roll{0,1,0,0, -1,0,0,0, 0,0,1,0, -700,31,5,1};
        auto turned=inputs;o::eye_views(turned,yaw,roll);
        const o::V axes[3]{{0,0,-1},{1,0,0},{0,-1,0}};
        for(unsigned axis=0;axis<3;++axis) {
            o::V d{};d[axis]=1;const auto transformed=o::old_environment_direction(turned,d);
            for(unsigned c=0;c<3;++c)require(std::abs(transformed[c]-axes[axis][c])<1.e-12,
                "Independent environment eye transform transposed or reversed its handedness/order");
        }
        auto translated=yaw;translated[12]+=10000;translated[13]-=20000;translated[14]+=30000;
        o::eye_views(turned,translated,roll);
        require(o::old_environment_direction(turned,{1,0,0})==axes[0],"Eye translation moved an infinite environment");
        // Old liquid path owns a visible integral source. Express its feature
        // in the CURRENT yawed eye; the independent inverse and old tap guard
        // must recover the original source rather than accepting a stale basis.
        const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        o::eye_views(turned,yaw,identity);
        auto rotated_record=record;
        const o::V current_feature{-own_ray.outgoing[2],own_ray.outgoing[1],own_ray.outgoing[0]};
        for(unsigned c=0;c<3;++c)rotated_record[6+c]=std::bit_cast<unsigned>(float(current_feature[c]));
        const auto rotated_root=o::previous_source(turned,0xfffffffdU,rotated_record,0,0,7,37,closed);
        require(rotated_root.valid,"Camera-rotated liquid reference lost a visible old source");
        require(o::source_guard(turned,0xfffffffdU,rotated_record,0,0,integral,own_only)==o::SourceGuard::accepted,
            "Camera-rotated feature did not match its actual old eye tap");
        require(o::source_guard(inputs,0xfffffffdU,rotated_record,0,0,integral,own_only)==o::SourceGuard::feature,
            "Unrotated stale environment feature falsely matched the old eye tap");
        for(unsigned malformed=0;malformed<3;++malformed) {
            auto bad=record;
            if(malformed==0)bad[6]=std::bit_cast<unsigned>(std::numeric_limits<float>::quiet_NaN());
            if(malformed==1)bad[6]=bad[7]=bad[8]=0;
            if(malformed==2)for(unsigned c=6;c<9;++c)bad[c]=std::bit_cast<unsigned>(2*std::bit_cast<float>(record[c]));
            require(!o::previous_source(inputs,0xfffffffdU,bad,0,0,7,37,closed).valid,
                "Independent inverse normalized an invalid current direction into a source");
            require(o::source_guard(inputs,0xfffffffdU,bad,0,0,integral,own_only)==o::SourceGuard::feature,
                "Independent tap guard accepted a nonfinite/zero/nonunit current feature");
        }
        // Literal nonrigid bases/offsets with a hand-computed point. The
        // initializer must invert the stored old basis, not assume that its
        // transpose is an inverse or that current and old pixels coincide.
        o::Inputs shifted_receiver;
        shifted_receiver.current_projection=shifted_receiver.liquid.projection={10,20,5,6};
        shifted_receiver.liquid.rotation={1,0,0,0,2,0,0,0,3};
        shifted_receiver.liquid.offset={-2,1,6};
        shifted_receiver.current_liquid_transform=std::array<float,12>{2,.5F,0,3,0,3,0,4,0,0,4,5};
        const auto transported=o::liquid_receiver_initializer(shifted_receiver,6,10,10);
        require(transported && std::abs((*transported)[0]-(5.+80./13))<1.e-12
            && std::abs((*transported)[1]-(6.+105./13))<1.e-12,
            "Independent liquid initializer lost literal scale/shear/translation point transport");
        for(unsigned malformed=0;malformed<7;++malformed) {
            auto bad=shifted_receiver;double depth=10;
            if(malformed==0)bad.liquid.rotation[8]=0;
            if(malformed==1)(*bad.current_liquid_transform)[0]=std::numeric_limits<float>::quiet_NaN();
            if(malformed==2)depth=0;
            if(malformed==3)depth=-1;
            if(malformed==4)depth=std::numeric_limits<double>::infinity();
            if(malformed==5)bad.current_projection[0]=0;
            if(malformed==6)bad.liquid.offset[2]=1000;
            require(!o::liquid_receiver_initializer(bad,6,10,depth),
                "Independent liquid initializer accepted invalid basis/depth/projection/behind-eye point");
        }
        o::Inputs nominal_input;nominal_input.liquid.projection={10,20,5,6};
        nominal_input.liquid.point={0,5,0};nominal_input.liquid.normal={0,-1,0};
        nominal_input.liquid.width=30;nominal_input.liquid.height=50;
        o::Path nominal_path;nominal_path.liquid_primary=true;nominal_path.terminal={1,-1,2};
        const auto direction_seed=o::nominal_receiver_initializer(nominal_input,nominal_path);
        require(direction_seed && *direction_seed==std::array<double,2>{10,16},
            "Independent nominal liquid initializer lost literal direction reflection/projection");
        nominal_path.finite_terminal=true;nominal_path.terminal={2,3,4};
        const auto point_seed=o::nominal_receiver_initializer(nominal_input,nominal_path);
        require(point_seed && *point_seed==std::array<double,2>{10,41},
            "Independent nominal liquid initializer lost translated-plane point reflection");
        if(replay_fixture) {
            // Complete synthetic old visibility, raw located views and optical
            // metadata only. Exercise the V3 offline parser without a GPU,
            // external assets, inverse guide or an expected-output RGB plane.
            const auto vector=[](const auto& values){for(auto value:values)std::cout<<value<<' ';std::cout<<'\n';};
            std::cout<<std::setprecision(17)<<(receiver_replay?"BEGIN_CURVED_OWNER_SOURCE_V4\n":"BEGIN_CURVED_OWNER_SOURCE_V3\n")
                <<f.width<<' '<<f.height<<" 4294967293 0 0 7 37\n";
            vector(turned.current_projection);vector(turned.current_eye_view);vector(turned.previous_eye_view);
            vector(f.point);vector(f.normal);vector(f.rotation);vector(f.offset);vector(f.projection);
            std::cout<<f.width<<' '<<f.height<<' '<<f.material<<' '<<f.near<<' '<<f.far<<' '<<f.time<<'\n';
            if(receiver_replay) {
                std::array<float,12> transform{};
                for(unsigned r=0;r<3;++r) {
                    for(unsigned c=0;c<3;++c)transform[r*4+c]=float(f.rotation[r*3+c]);
                    transform[r*4+3]=float(f.offset[r]);
                }
                vector(transform);std::cout<<o::forward(inputs,path,7.5,37.5).depth<<'\n';
            }
            std::cout<<"0\n";
            vector(rotated_record);std::cout<<f.width*f.height<<'\n';
            for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x) {
                const auto ray=o::forward(inputs,path,x+.5,y+.5);
                require(ray.valid,"Synthetic V3 replay has missing old optical coverage");
                auto words=record;
                for(unsigned c=0;c<3;++c)words[6+c]=std::bit_cast<unsigned>(float(ray.outgoing[c]));
                std::cout<<x<<' '<<y<<" 4294967293 "<<double(float(ray.depth))<<' ';
                vector(std::span<const unsigned,9>(words.data(),9));
            }
            std::cout<<(receiver_replay?"END_CURVED_OWNER_SOURCE_V4\n":"END_CURVED_OWNER_SOURCE_V3\n");return 0;
        }
        for(unsigned malformed=0;malformed<4;++malformed) {
            auto bad=identity;
            if(malformed==0)bad[0]=std::numeric_limits<float>::quiet_NaN();
            if(malformed==1)bad[3]=1;
            if(malformed==2)bad[0]=2;
            if(malformed==3)bad[0]=-1;
            bool refused=false;try{o::eye_views(turned,bad,identity);}catch(const std::runtime_error&){refused=true;}
            require(refused,"Independent camera reference accepted a malformed eye basis");
        }
        // Native geometry packing and accepted-old clock, independent of SDL.
        std::vector<unsigned> words(25);
        const float positions[3][3]{{-100,-100,200},{100,-100,200},{0,100,200}};
        for(unsigned bank=0;bank<2;++bank)for(unsigned v=0;v<3;++v) {
            for(unsigned c=0;c<3;++c)words[bank*12+v*4+c]=std::bit_cast<unsigned>(positions[v][c]+(bank==0 && c==0?.5F:0.F));
            words[bank*12+v*4+3]=std::bit_cast<unsigned>(1.F);
        }
        words[24]=0;
        std::array<float,32> frame{};frame[1]=180;frame[5]=-1;
        frame[8]=frame[13]=frame[18]=1;
        frame[20]=128;frame[21]=136;frame[22]=48;frame[23]=-80;
        frame[24]=96;frame[25]=64;frame[26]=1;frame[27]=65536;frame[28]=1.3F;
        const auto decoded=o::decode(words,48,3,frame,inputs.current_projection);
        require(decoded.previous.size()==1 && decoded.mapping[0]==0
            && decoded.current[0].a[0]!=decoded.previous[0].a[0] && decoded.liquid.time==double(1.3F),
            "Owner oracle changed current/previous geometry order or old clock packing");
        words[15]=0;bool rejected=false;
        try {(void)o::decode(words,48,3,frame,inputs.current_projection);}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"Owner oracle accepted malformed finite W instead of rejecting its geometry");
        std::cout<<"Independent owner oracle: necessary full-bank feature domains with literal half-plane/boundary/island/remapping/rotated-eye guards and nonlinear-source enclosure, float-boundary initializer/ABI neighbour, stalled-basin and distant visible-bank recovery, optical-ambiguity refusal, bounded old-bank/mapped receiver/hop/terminal and nonfinite-depth guards, depth/feature/missing-tap guards, rotated eye/order/translation and malformed-basis guards, literal affine liquid receiver transport/invalid-input refusal, radius/visibility refusal, finite geometry packing and accepted-old clock passed.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
