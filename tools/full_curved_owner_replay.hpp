#pragma once
// Bounded raw optical/visibility INPUT replay only. Never parses the separate
// downloaded GPU inverse/colour diagnostics after the marked input block.
#include "../tests/reflected_curved_visibility_domain.hpp"
#include <fstream>
#include <string>
struct FullCurvedOwnerReplay {
    reflected_curved_owner_oracle::Inputs inputs;
    unsigned width{},height{},primary{},lobe{},x{},y{};double roughness{},current_depth{};
    std::array<unsigned,16> record{};
    std::vector<reflected_curved_owner_oracle::SourceTap> taps;
};
inline reflected_curved_owner_oracle::Path full_replay_optical_path(const FullCurvedOwnerReplay& replay) {
    namespace o=reflected_curved_owner_oracle;
    const auto& input=replay.inputs;const auto& record=replay.record;
    o::Path path;path.hops=record[4]&7U;path.liquid_mask=(record[4]>>4)&15U;path.liquid_primary=replay.primary==0xfffffffdU;
    const unsigned kind=record[4]>>8;
    if(path.hops>4 || (path.liquid_mask>>path.hops)!=0 || kind<1 || kind>3
        || (kind==3)!=(path.hops==4) || replay.lobe>=8 || !std::isfinite(replay.roughness)
        || replay.roughness<0 || replay.roughness>1)
        throw std::runtime_error("Malformed bounded replay optical path");
    path.lobe=replay.lobe;path.roughness=replay.roughness;path.finite_terminal=kind==1;
    if(!path.liquid_primary)path.receiver=input.previous.at(input.mapping.at(replay.primary));
    for(unsigned h=0;h<path.hops;++h)if(!(path.liquid_mask&(1U<<h)))path.mirrors[h]=input.previous.at(input.mapping.at(record[h]));
    for(unsigned c=0;c<3;++c)path.terminal[c]=std::bit_cast<float>(record[6+c]);
    if(!o::valid_feature(path.terminal,kind))throw std::runtime_error("Malformed raw replay terminal feature");
    if(path.finite_terminal) {
        path.terminal_plane=input.previous.at(input.mapping.at(record[5]));path.terminal=o::point(path.terminal_plane,{path.terminal[0],path.terminal[1]});
        for(auto& value:path.terminal)value=float(value);
    }else path.terminal=o::old_environment_direction(input,path.terminal);
    return path;
}
inline FullCurvedOwnerReplay load_full_curved_owner_replay(const char* path) {
    namespace o=reflected_curved_owner_oracle;
    std::ifstream stream(path);if(!stream)throw std::runtime_error("Cannot read bounded optical input replay");
    std::string line;unsigned version=0;
    while(std::getline(stream,line)) {
        for(unsigned v=2;v<=4;++v)if(line.find("BEGIN_CURVED_OWNER_SOURCE_V"+std::to_string(v))!=std::string::npos)version=v;
        if(version)break;
    }
    if(!version)throw std::runtime_error("Resident index requires a complete V2/V3/V4 old input bank");
    FullCurvedOwnerReplay r;auto& i=r.inputs;auto& f=i.liquid;
    const auto read=[&](auto& values){for(auto& value:values)stream>>value;};
    stream>>r.width>>r.height>>r.primary>>r.lobe>>r.roughness>>r.x>>r.y;read(i.current_projection);
    if(version>=3) {std::array<float,16> current{},previous{};read(current);read(previous);o::eye_views(i,current,previous);}
    read(f.point);read(f.normal);read(f.rotation);read(f.offset);read(f.projection);
    stream>>f.width>>f.height>>f.material>>f.near>>f.far>>f.time;
    if(version==4) {i.current_liquid_transform.emplace();read(*i.current_liquid_transform);stream>>r.current_depth;}
    unsigned count{};stream>>count;
    if(!stream || count>8192 || !r.width || !r.height || r.width>8192 || r.height>8192
        || std::uint64_t(r.width)*r.height>65536 || r.lobe>=8 || f.width!=r.width || f.height!=r.height)
        throw std::runtime_error("Unbounded or inconsistent full optical replay");
    i.current.resize(count);i.previous.resize(count);i.mapping.resize(count);
    for(unsigned n=0;n<count;++n) {read(i.current[n].a);read(i.current[n].b);read(i.current[n].c);
        read(i.previous[n].a);read(i.previous[n].b);read(i.previous[n].c);stream>>i.mapping[n];}
    read(r.record);unsigned taps{};stream>>taps;
    if(!stream || taps!=r.width*r.height)throw std::runtime_error("Incomplete old optical bank");
    r.taps.resize(taps);std::vector<bool> seen(taps);
    for(unsigned n=0;n<taps;++n) {
        unsigned x{},y{};o::SourceTap tap;stream>>x>>y>>tap.identity>>tap.depth;read(tap.path);
        if(!stream || x>=r.width || y>=r.height || seen[y*r.width+x])throw std::runtime_error("Invalid old optical input coordinate");
        seen[y*r.width+x]=true;r.taps[y*r.width+x]=tap;
    }
    stream>>line;
    if(!stream || line!="END_CURVED_OWNER_SOURCE_V"+std::to_string(version))throw std::runtime_error("Truncated full optical replay");
    return r;
}
