// Offline replay of a single actual-owner optical/old-visibility failure.
// The separate GPU inverse trace is never parsed or used as oracle input.
#include "../tests/reflected_curved_visibility_domain.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <unordered_map>
namespace o=reflected_curved_owner_oracle;
int main(int argc,char** argv) {
    try {
        bool global_audit=false,domain_audit=false;
        if(argc<2 || argc>4)throw std::runtime_error("Usage: starfox_curved_owner_source_check owner-failure.log [--global-source-audit] [--visibility-domain-audit]");
        for(int option=2;option<argc;++option) {
            const std::string name=argv[option];
            if(name=="--global-source-audit" && !global_audit)global_audit=true;
            else if(name=="--visibility-domain-audit" && !domain_audit)domain_audit=true;
            else throw std::runtime_error("Unknown or duplicate optical audit option");
        }
        std::ifstream stream(argv[1]);if(!stream)throw std::runtime_error("Cannot read owner failure log");
        std::string line;bool found=false,complete_bank=false,camera_views=false,receiver_transform=false;
        while(std::getline(stream,line)) {
            receiver_transform=line.find("BEGIN_CURVED_OWNER_SOURCE_V4")!=std::string::npos;
            camera_views=receiver_transform || line.find("BEGIN_CURVED_OWNER_SOURCE_V3")!=std::string::npos;
            complete_bank=camera_views || line.find("BEGIN_CURVED_OWNER_SOURCE_V2")!=std::string::npos;
            if(complete_bank || line.find("BEGIN_CURVED_OWNER_SOURCE_V1")!=std::string::npos){found=true;break;}
        }
        if(!found)throw std::runtime_error("No bounded owner-source replay in this log");
        const auto read=[&](auto& values){for(auto& value:values)stream>>value;};
        unsigned width{},height{},primary{},lobe{},x{},y{};double roughness{};o::Inputs in;
        stream>>width>>height>>primary>>lobe>>roughness>>x>>y;
        auto& f=in.liquid;read(in.current_projection);
        if(camera_views) {
            std::array<float,16> current{},previous{};read(current);read(previous);o::eye_views(in,current,previous);
        }
        read(f.point);read(f.normal);read(f.rotation);read(f.offset);read(f.projection);
        stream>>f.width>>f.height>>f.material>>f.near>>f.far>>f.time;
        double current_depth{};
        if(receiver_transform) {
            in.current_liquid_transform.emplace();read(*in.current_liquid_transform);stream>>current_depth;
        }
        unsigned count{};stream>>count;
        if(count>8192 || !width || !height || width>8192 || height>8192 || lobe>=8
            || f.width!=width || f.height!=height)throw std::runtime_error("Unbounded or inconsistent owner replay input");
        in.current.resize(count);in.previous.resize(count);in.mapping.resize(count);
        for(unsigned n=0;n<count;++n) {
            read(in.current[n].a);read(in.current[n].b);read(in.current[n].c);
            read(in.previous[n].a);read(in.previous[n].b);read(in.previous[n].c);stream>>in.mapping[n];
        }
        std::array<unsigned,16> record{};read(record);
        using Tap=o::SourceTap;
        std::unordered_map<unsigned,Tap> taps;unsigned tap_count{};stream>>tap_count;
        if(tap_count>(complete_bank?65536U:49U) || (complete_bank && tap_count!=width*height))
            throw std::runtime_error("Unbounded or incomplete old visibility bank");
        for(unsigned n=0;n<tap_count;++n) {
            unsigned tx{},ty{};Tap tap;stream>>tx>>ty>>tap.identity>>tap.depth;
            for(unsigned c=0;c<(complete_bank?9U:6U);++c)stream>>tap.path[c];
            if(tx>=width || ty>=height || !taps.emplace(ty*width+tx,tap).second)throw std::runtime_error("Invalid old visibility coordinate");
        }
        stream>>line;
        if(!stream || line!=(receiver_transform?"END_CURVED_OWNER_SOURCE_V4":camera_views?"END_CURVED_OWNER_SOURCE_V3":complete_bank?"END_CURVED_OWNER_SOURCE_V2":"END_CURVED_OWNER_SOURCE_V1"))
            throw std::runtime_error("Truncated bounded owner replay");
        std::cout<<std::setprecision(17)<<"Actual submitted optical replay: pixel="<<x<<','<<y
            <<" primary="<<primary<<" control="<<record[4]<<" old-time="<<f.time<<" material="<<f.material<<'\n';
        o::Path diagnostic;diagnostic.hops=record[4]&7U;diagnostic.liquid_mask=(record[4]>>4)&15U;
        diagnostic.liquid_primary=primary==0xfffffffdU;diagnostic.lobe=lobe;diagnostic.roughness=roughness;
        if(!diagnostic.liquid_primary && primary<in.mapping.size())diagnostic.receiver=in.previous.at(in.mapping[primary]);
        for(unsigned h=0;h<diagnostic.hops && h<4;++h)if(!(diagnostic.liquid_mask&(1U<<h)))
            diagnostic.mirrors[h]=in.previous.at(in.mapping.at(record[h]));
        for(unsigned c=0;c<3;++c)diagnostic.terminal[c]=std::bit_cast<float>(record[6+c]);
        diagnostic.finite_terminal=(record[4]>>8)==1;
        if(diagnostic.finite_terminal) {
            diagnostic.terminal_plane=in.previous.at(in.mapping.at(record[5]));
            diagnostic.terminal=o::point(diagnostic.terminal_plane,{diagnostic.terminal[0],diagnostic.terminal[1]});
            for(auto& value:diagnostic.terminal)value=float(value);
        } else diagnostic.terminal=o::old_environment_direction(in,diagnostic.terminal);
        const unsigned old_primary=primary==0xfffffffdU?primary:in.mapping.at(primary);
        std::array<unsigned,6> old_path;std::copy_n(record.begin(),6,old_path.begin());
        for(unsigned h=0;h<diagnostic.hops && h<4;++h)if(!(diagnostic.liquid_mask&(1U<<h)))
            old_path[h]=in.mapping.at(record[h]);
        if(diagnostic.finite_terminal)old_path[5]=in.mapping.at(record[5]);
        bool incomplete_visibility=false,report_candidate=true;
        const auto visible=[&](const o::Solution& root) {
            if(report_candidate) {
            std::cout<<"Independent candidate xy="<<root.x<<','<<root.y<<" depth="<<root.depth<<" residual="<<root.error
                <<" initializer="<<root.bounded_seed<<'\n';
            if((record[4]>>8)==2) {
                bool positive=false,negative=false,complete=true;
                const double bx=std::floor(root.x)+.5,by=std::floor(root.y)+.5;
                o::V grid[9];
                for(unsigned gy=0;gy<3;++gy)for(unsigned gx=0;gx<3;++gx) {
                    const auto ray=o::forward(in,diagnostic,bx+gx*.5,by+gy*.5);
                    complete=complete && ray.valid;grid[gy*3+gx]=ray.outgoing;
                }
                if(complete)for(unsigned gy=0;gy<2;++gy)for(unsigned gx=0;gx<2;++gx) {
                    const unsigned a=gy*3+gx;
                    const auto dx0=o::sub(grid[a+1],grid[a]),dx1=o::sub(grid[a+4],grid[a+3]),
                        dy0=o::sub(grid[a+3],grid[a]),dy1=o::sub(grid[a+4],grid[a+1]);
                    for(unsigned tap=0;tap<4;++tap) {
                        const auto dx=(tap&2U)?dx1:dx0,dy=(tap&1U)?dy1:dy0;
                        const double orientation=o::dot(grid[a+(tap&1U)+(tap>>1)*3],o::cross(dx,dy));
                        positive|=orientation>1.e-20;negative|=orientation<-1.e-20;
                    }
                }
                std::cout<<"Independent half-cell forward orientation complete="<<complete<<" positive="<<positive<<" negative="<<negative<<'\n';
            }
            }
            if(root.x<0 || root.y<0 || root.x>width-1 || root.y>height-1){if(report_candidate)std::cout<<"REJECT outside old viewport\n";return false;}
            double reciprocal=0;
            for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx) {
                const double fx=root.x-std::floor(root.x),fy=root.y-std::floor(root.y);
                const double share=(dx?fx:1-fx)*(dy?fy:1-fy);if(!share)continue;
                const unsigned tx=std::min(unsigned(std::floor(root.x))+dx,width-1),ty=std::min(unsigned(std::floor(root.y))+dy,height-1);
                const auto old=taps.find(ty*width+tx);
                if(old==taps.end()) {incomplete_visibility=true;if(report_candidate)std::cout<<"REJECT missing bounded tap "<<tx<<','<<ty<<'\n';return false;}
                const auto& tap=old->second;
                if(tap.identity!=old_primary || !std::isfinite(tap.depth) || tap.depth<=0
                    || !std::equal(old_path.begin(),old_path.end(),tap.path.begin())) {
                    if(report_candidate)std::cout<<"REJECT nonzero old tap "<<tx<<','<<ty<<" share="<<share<<" id="<<tap.identity
                        <<" control="<<tap.path[4]<<" depth="<<tap.depth<<'\n';return false;
                }
                reciprocal+=share/tap.depth;
            }
            const bool valid=std::isfinite(reciprocal) && reciprocal>0 && std::abs(1/reciprocal-root.depth)<=std::max(.01,root.depth*.005);
            if(report_candidate)std::cout<<(valid?"ACCEPT":"REJECT")<<" interpolated old-depth="<<1/reciprocal<<'\n';
            if(!valid || !complete_bank)return valid;
            const auto guard=o::source_guard(in,primary,record,roughness,lobe,root,[&](unsigned tx,unsigned ty)->std::optional<o::SourceTap> {
                const auto old=taps.find(ty*width+tx);
                return old==taps.end()?std::optional<o::SourceTap>{}:old->second;
            });
            if(report_candidate)std::cout<<"Independent complete optical source guard="<<unsigned(guard)<<'\n';
            return guard==o::SourceGuard::accepted;
        };
        auto solution=o::previous_source(in,primary,record,roughness,lobe,x,y,visible,current_depth);
        std::vector<std::array<double,2>> old_starts;
        if(complete_bank && (!solution.valid || global_audit)) {
            old_starts=o::old_visibility_initializers(in,primary,record,[&](unsigned tx,unsigned ty)->std::optional<Tap> {
                const auto old=taps.find(ty*width+tx);return old==taps.end()?std::optional<Tap>{}:old->second;
            });
            if(!solution.valid && !solution.ambiguous) {
                report_candidate=false;
                solution=o::previous_source(in,primary,record,roughness,lobe,x,y,visible,current_depth,old_starts);
                report_candidate=true;
                std::cout<<"Independent old visible-bank fallback starts="<<old_starts.size()<<'\n';
                if(solution.valid)std::cout<<"Independent recovered source xy="<<solution.x<<','<<solution.y
                    <<" depth="<<solution.depth<<" residual="<<solution.error<<'\n';
            }
        }
        std::cout<<(incomplete_visibility?"Replay INCOMPLETE for independent branches":
            solution.valid?"Independent old source QUALIFIED":"Independent old source NOT QUALIFIED")<<'\n';
        if(solution.ambiguous)std::cout<<"Distinct independent optical branches remain qualified\n";
        std::vector<o::VisibilityRegion> domains;std::vector<o::Solution> domain_roots;
        const auto domain_contains=[&](const o::Solution& root) {
            return std::any_of(domains.begin(),domains.end(),[&](const auto& region){return region.contains(root.x,root.y);});
        };
        if(domain_audit) {
            if(!complete_bank)throw std::runtime_error("Visibility domain audit requires the complete bounded old bank");
            const auto begin=std::chrono::steady_clock::now();
            domains=o::old_visibility_domains(in,primary,record,[&](unsigned tx,unsigned ty)->std::optional<Tap> {
                const auto old=taps.find(ty*width+tx);return old==taps.end()?std::optional<Tap>{}:old->second;
            });
            std::array<unsigned,4> supports{};
            for(const auto& region:domains)++supports[region.span_x+2*region.span_y];
            std::cout<<"Necessary old feature domains="<<domains.size()<<" vertices/horizontal/vertical/cells="
                <<supports[0]<<'/'<<supports[1]<<'/'<<supports[2]<<'/'<<supports[3]
                <<" raw-bank taps="<<tap_count<<" preprocessing-ms="
                <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<'\n';
            if(solution.valid && !domain_contains(solution))throw std::runtime_error("Necessary domain excluded the independently qualified local source");
            // Start only inside the independently derived necessary feature
            // domains. This can reduce forward-solve work but does not prove
            // absence of multiple roots INSIDE a region or complete convergence.
            unsigned converged=0,accepted=0;report_candidate=false;
            for(const auto& region:domains) {
                const auto root=o::solve_forward(in,diagnostic,region.initializer[0],region.initializer[1]);
                if(!root.valid)continue;
                ++converged;
                o::Solution candidate{double(float(root.x))-.5,double(float(root.y))-.5,double(float(root.depth)),root.error,true,false};
                if(!visible(candidate))continue;
                ++accepted;
                if(!domain_contains(candidate))throw std::runtime_error("Necessary domain excluded an independently qualified domain-initialized source");
                if(std::any_of(domain_roots.begin(),domain_roots.end(),[&](const auto& other) {
                    return std::abs(other.x-candidate.x)<=1./4096 && std::abs(other.y-candidate.y)<=1./4096;
                }))continue;
                domain_roots.push_back(candidate);
                std::cout<<"Domain-initialized independently visible branch xy="<<candidate.x<<','<<candidate.y
                    <<" depth="<<candidate.depth<<" source-support="<<region.x<<','<<region.y
                    <<'+'<<region.span_x<<','<<region.span_y<<'\n';
            }
            report_candidate=true;
            std::cout<<"Necessary-domain optical search starts="<<domains.size()<<" converged="<<converged
                <<" visible="<<accepted<<" distinct="<<domain_roots.size()<<"; necessary enclosure, NOT optical uniqueness\n";
        }
        if(global_audit) {
            if(!complete_bank)throw std::runtime_error("Global source audit requires the complete bounded old visibility bank");
            // Every eligible authored old pixel is an INITIALIZER for an
            // independent binary64 forward solve. This does not parse the GPU
            // inverse or select a root using RGB. Multiple visible branches
            // remain ambiguous; a finite grid is not a proof of global uniqueness.
            std::vector<o::Solution> roots;unsigned starts=0,converged=0,accepted=0;
            report_candidate=false;
            for(const auto& start:old_starts) {
                ++starts;
                const auto root=o::solve_forward(in,diagnostic,start[0],start[1]);if(!root.valid)continue;
                ++converged;
                o::Solution candidate{double(float(root.x))-.5,double(float(root.y))-.5,
                    double(float(root.depth)),root.error,true,false};
                if(!visible(candidate))continue;
                ++accepted;
                if(domain_audit && !domain_contains(candidate))throw std::runtime_error("Necessary domain excluded an independently qualified whole-bank branch");
                if(std::any_of(roots.begin(),roots.end(),[&](const auto& other) {
                    return std::abs(other.x-candidate.x)<=1./4096 && std::abs(other.y-candidate.y)<=1./4096;
                }))continue;
                roots.push_back(candidate);
                std::cout<<"Whole-bank independently visible branch xy="<<candidate.x<<','<<candidate.y
                    <<" depth="<<candidate.depth<<" residual="<<candidate.error<<" authored-start="<<start[0]-.5<<','<<start[1]-.5<<'\n';
            }
            report_candidate=true;
            std::cout<<"Whole-bank optical search starts="<<starts<<" converged="<<converged
                <<" visible="<<accepted<<" distinct="<<roots.size()
                <<"; bounded initializer coverage, not a global uniqueness proof\n";
            if(roots.size()>1) {
                std::cout<<"Whole-bank audit AMBIGUOUS: local qualification cannot certify colour reuse\n";
                return incomplete_visibility?2:1;
            }
        }
        if(domain_roots.size()>1) {
            std::cout<<"Necessary-domain audit AMBIGUOUS: multiple independently visible old optical roots\n";
            return incomplete_visibility?2:1;
        }
        if(!solution.valid && primary==0xfffffffdU && (record[4]&7U)==0 && (record[4]>>8)==2) {
            // Independently authored symmetric starts, not a GPU-source seed.
            // Report every qualified branch; never select by expected colour.
            for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx) {
                if((dx==0 && dy==0) || int(x)+dx<0 || int(y)+dy<0
                    || int(x)+dx>=int(width) || int(y)+dy>=int(height))continue;
                const auto branch=o::previous_source(in,primary,record,roughness,lobe,unsigned(int(x)+dx),unsigned(int(y)+dy),visible);
                if(branch.valid)std::cout<<"Independent restart "<<dx<<','<<dy<<" qualified source="
                    <<branch.x<<','<<branch.y<<" depth="<<branch.depth<<'\n';
            }
            // Binary64 local convergence trace, built from the authored input
            // pixel and stored terminal direction, not the separate GPU guide.
            o::Path path;path.liquid_primary=true;
            path.terminal=diagnostic.terminal;
            double sx=x+.5,sy=y+.5;
            for(unsigned iteration=0;iteration<48;++iteration) {
                const auto e=o::residual(in,path,sx,sy);if(!e){std::cout<<"Invalid independent forward sample\n";break;}
                std::cout<<"Local iteration "<<iteration<<" pixel="<<sx<<','<<sy<<" error="<<(*e)[0]<<','<<(*e)[1]<<'\n';
                const double measure=std::hypot((*e)[0],(*e)[1]);if(measure<1.e-9)break;
                constexpr double h=1.e-4;
                const auto ax=o::residual(in,path,sx-h,sy),bx=o::residual(in,path,sx+h,sy),
                    ay=o::residual(in,path,sx,sy-h),by=o::residual(in,path,sx,sy+h);
                if(!ax || !bx || !ay || !by){std::cout<<"Invalid independent derivative sample\n";break;}
                const double a=((*bx)[0]-(*ax)[0])/(2*h),b=((*by)[0]-(*ay)[0])/(2*h),
                    c=((*bx)[1]-(*ax)[1])/(2*h),d=((*by)[1]-(*ay)[1])/(2*h),det=a*d-b*c;
                double dx=(d*(*e)[0]-b*(*e)[1])/det,dy=(a*(*e)[1]-c*(*e)[0])/det;
                const double radius=std::hypot(dx,dy);if(radius>8){dx*=8/radius;dy*=8/radius;}
                bool advanced=false;
                for(unsigned trial=0;trial<14;++trial) {
                    const double scale=std::ldexp(1.,-int(trial));const auto next=o::residual(in,path,sx-dx*scale,sy-dy*scale);
                    if(next && std::hypot((*next)[0],(*next)[1])<measure){sx-=dx*scale;sy-=dy*scale;advanced=true;break;}
                }
                if(!advanced){std::cout<<"Independent local solve stalled: determinant="<<det<<" step="<<dx<<','<<dy<<'\n';break;}
            }
        }
        return incomplete_visibility?2:solution.valid?0:1;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 2;}
}
