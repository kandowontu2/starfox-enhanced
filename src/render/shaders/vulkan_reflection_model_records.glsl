// Hooks in the ORIGINAL native model loop, not a second colour renderer.
// 0 sharp guide, 1 compact finite lobes, 2 MODEL paths, 3 MODEL/planar paths,
// 4 separated planar, 5 water, 6 lava motion (not MODEL path records).
layout(constant_id=3) const uint HISTORY_MODEL_RECORDS=0u;
struct ModelHistoryPath {uvec4 mirrors;uint control,terminal;vec3 feature;uint incoming;vec3 response;};
ModelHistoryPath history_path_empty() {
    ModelHistoryPath path;path.mirrors=uvec4(0xffffffffu);path.control=0u;path.terminal=0xffffffffu;
    path.feature=vec3(0);path.incoming=0u;path.response=vec3(0);return path;
}
void history_path_store(uint at,ModelHistoryPath path) {
    for(uint i=0u;i<4u;++i)output_image.pixels[at+i]=path.mirrors[i];
    output_image.pixels[at+4u]=path.control;output_image.pixels[at+5u]=path.terminal;
    output_image.pixels[at+6u]=floatBitsToUint(path.feature.x);output_image.pixels[at+7u]=floatBitsToUint(path.feature.y);
    output_image.pixels[at+8u]=floatBitsToUint(path.feature.z);output_image.pixels[at+9u]=path.incoming;
    output_image.pixels[at+10u]=floatBitsToUint(path.response.x);output_image.pixels[at+11u]=floatBitsToUint(path.response.y);
    output_image.pixels[at+12u]=floatBitsToUint(path.response.z);
}
ModelHistoryPath history_current_path;
uint history_record_base,history_record_stride,history_record_lobe;
vec3 history_primary_base,history_primary_response;
uint history_primary_incoming,history_secondary;
vec2 history_secondary_bary;
void history_record_terminal(uint kind,uint count,uint terminal,vec3 feature,uint incoming,vec3 response) {
    if(HISTORY_MODEL_RECORDS>=4u) {
        // Only a finite one-bounce witness can enter single-hit reuse. Current
        // multi-hop colour still uses the unchanged original transport loop.
        if(kind==1u && count==0u) {history_secondary=terminal;history_secondary_bary=feature.xy;}
        return;
    }
    history_current_path.control=(kind<<8u)|count;history_current_path.terminal=terminal;
    history_current_path.feature=feature;history_current_path.incoming=incoming;history_current_path.response=response;
    uint at=history_record_base+history_record_lobe*history_record_stride;
    if(HISTORY_MODEL_RECORDS>=2u)history_path_store(at,history_current_path);
    else {
        bool finite_hit=kind==1u;uvec2 bary=uvec2(floor(clamp(feature.xy,vec2(0),vec2(1))*65535.0+.5));
        bary.y=min(bary.y,65535u-bary.x);
        output_image.pixels[at]=finite_hit?terminal:0xffffffffu;
        output_image.pixels[at+1u]=finite_hit?bary.x|(bary.y<<16u):0u;output_image.pixels[at+2u]=incoming;
    }
    ++history_record_lobe;
}
