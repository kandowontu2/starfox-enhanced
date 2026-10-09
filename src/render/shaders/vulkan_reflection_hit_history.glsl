// Accepted finite sharp secondary radiance, not primary receiver motion.
// The old virtual feature is reflected through its actual old finite mirror.
// The consumer still checks accepted ownership/depth/IDs before colour reuse.
bool history_finite(float v) {return !isnan(v) && !isinf(v);}
bool history_finite(vec2 v) {return all(not(isnan(v))) && all(not(isinf(v)));}
bool history_finite(vec3 v) {return all(not(isnan(v))) && all(not(isinf(v)));}
bool history_finite(vec4 v) {return all(not(isnan(v))) && all(not(isinf(v)));}
vec4 history_vertex(uint primitive,uint corner) {
    uint at=p.history_info.x+primitive*12u+corner*4u;
    if(at>uint(materials.word.length()) || uint(materials.word.length())-at<4u)return vec4(0);
    return uintBitsToFloat(uvec4(materials.word[at],materials.word[at+1u],materials.word[at+2u],materials.word[at+3u]));
}
vec4 reflected_feature_motion(uint receiver,uint secondary,vec2 feature_bary) {
    vec4 a=history_vertex(receiver,0u),b=history_vertex(receiver,1u),c=history_vertex(receiver,2u);
    vec4 qa=history_vertex(secondary,0u),qb=history_vertex(secondary,1u),qc=history_vertex(secondary,2u);
    if(a.w!=1.0 || b.w!=1.0 || c.w!=1.0 || qa.w!=1.0 || qb.w!=1.0 || qc.w!=1.0
        || !history_finite(a) || !history_finite(b) || !history_finite(c)
        || !history_finite(qa) || !history_finite(qb) || !history_finite(qc)
        || !history_finite(feature_bary) || any(lessThan(feature_bary,vec2(0)))
        || feature_bary.x+feature_bary.y>1.00001)return vec4(0);
    vec3 ab=b.xyz-a.xyz,ac=c.xyz-a.xyz,normal=cross(ab,ac),hit_normal=cross(qb.xyz-qa.xyz,qc.xyz-qa.xyz);
    float length2=dot(normal,normal);
    if(!history_finite(length2) || length2<=1e-20 || !history_finite(hit_normal) || dot(hit_normal,hit_normal)<=1e-20)return vec4(0);
    normal*=inversesqrt(length2);
    vec3 feature=qa.xyz*(1.0-feature_bary.x-feature_bary.y)+qb.xyz*feature_bary.x+qc.xyz*feature_bary.y;
    vec3 virtual_feature=feature-2.0*normal*dot(feature-a.xyz,normal);
    float denominator=dot(virtual_feature,normal);
    if(!history_finite(virtual_feature) || virtual_feature.z<=0.0 || !history_finite(denominator) || abs(denominator)<=1e-12)return vec4(0);
    float fraction=dot(a.xyz,normal)/denominator;
    if(!history_finite(fraction) || fraction<=0.0 || fraction>=1.0)return vec4(0);
    vec3 old_receiver=virtual_feature*fraction,relative=old_receiver-a.xyz;
    if(!history_finite(old_receiver) || old_receiver.z<p.history_clip.x || old_receiver.z>p.history_clip.y)return vec4(0);
    float aa=dot(ab,ab),bb=dot(ab,ac),cc=dot(ac,ac),ra=dot(relative,ab),rc=dot(relative,ac),determinant=aa*cc-bb*bb;
    if(!history_finite(determinant) || determinant<=1e-20)return vec4(0);
    vec2 bary=vec2(cc*ra-bb*rc,aa*rc-bb*ra)/determinant;
    if(!history_finite(bary) || any(lessThan(bary,vec2(-.00001))) || bary.x+bary.y>1.00001)return vec4(0);
    vec2 pixel=p.history_projection.xy*virtual_feature.xy/virtual_feature.z+p.history_projection.zw;
    if(!history_finite(pixel) || any(lessThan(pixel,vec2(0))) || any(greaterThanEqual(pixel,vec2(p.history_info.zw))))return vec4(0);
    return vec4(pixel,old_receiver.z,1);
}
void history_store4(uint at,vec4 value) {
    uvec4 words=floatBitsToUint(value);
    output_image.pixels[at]=words.x;output_image.pixels[at+1u]=words.y;
    output_image.pixels[at+2u]=words.z;output_image.pixels[at+3u]=words.w;
}
void history_sharp() {
    uint id=gl_GlobalInvocationID.x,count=p.dimensions.x*p.dimensions.y;
    if(id>=count)return;
    // Initialize every plane, including clear/sky/invalid accepted features.
    uint motion=count+id*4u,identity=count*5u+id*4u,witness=count*9u+id*4u;
    output_image.pixels[id]=0u;history_store4(motion,vec4(0));history_store4(witness,vec4(0));
    for(uint c=0u;c<4u;++c)output_image.pixels[identity+c]=0xffffffffu;
    uvec2 pixel=uvec2(id%p.dimensions.x,id/p.dimensions.x);
    vec3 primary_direction=vec3((float(pixel.x)+.5-p.camera.z)/p.camera.x,(float(pixel.y)+.5-p.camera.w)/p.camera.y,1);
    float near_depth=p.primary_range.z>.5?p.primary_range.x:1.0;
    float far_depth=p.primary_range.z>.5?p.primary_range.y:65536.0;
    rayQueryEXT primary;
    rayQueryInitializeEXT(primary,scene,gl_RayFlagsNoOpaqueEXT,0xff,vec3(0),near_depth,primary_direction,far_depth);
    while(rayQueryProceedEXT(primary)) {
        if(rayQueryGetIntersectionTypeEXT(primary,false)==gl_RayQueryCandidateIntersectionTriangleEXT
            && material_visible(rayQueryGetIntersectionPrimitiveIndexEXT(primary,false),rayQueryGetIntersectionBarycentricsEXT(primary,false),pixel))
            rayQueryConfirmIntersectionEXT(primary);
    }
    if(rayQueryGetIntersectionTypeEXT(primary,true)!=gl_RayQueryCommittedIntersectionTriangleEXT)return;
    uint receiver=rayQueryGetIntersectionPrimitiveIndexEXT(primary,true),first=receiver*3u;
    float depth=rayQueryGetIntersectionTEXT(primary,true);
    vec3 incoming=normalize(primary_direction),normal=normalize(cross(vertices.vertex[first+1u].xyz-vertices.vertex[first].xyz,
        vertices.vertex[first+2u].xyz-vertices.vertex[first].xyz));
    if(dot(normal,incoming)>0.0)normal=-normal;
    float bias=max(.01,depth*length(primary_direction)*1e-5);
    vec3 origin=primary_direction*depth+normal*bias,direction=reflect(incoming,normal);
    rayQueryEXT secondary;
    rayQueryInitializeEXT(secondary,scene,gl_RayFlagsNoOpaqueEXT,0xff,origin,bias,direction,65536.0);
    while(rayQueryProceedEXT(secondary)) {
        if(rayQueryGetIntersectionTypeEXT(secondary,false)==gl_RayQueryCandidateIntersectionTriangleEXT
            && material_visible(rayQueryGetIntersectionPrimitiveIndexEXT(secondary,false),rayQueryGetIntersectionBarycentricsEXT(secondary,false),pixel))
            rayQueryConfirmIntersectionEXT(secondary);
    }
    if(rayQueryGetIntersectionTypeEXT(secondary,true)!=gl_RayQueryCommittedIntersectionTriangleEXT) {
        output_image.pixels[id]=reflected_accumulated(reflected_backdrop(direction),vec3(1),vec3(0));return;
    }
    uint hit=rayQueryGetIntersectionPrimitiveIndexEXT(secondary,true);
    vec2 bary=rayQueryGetIntersectionBarycentricsEXT(secondary,true);
    output_image.pixels[id]=reflected_accumulated(material_colour(hit,bary,pixel),vec3(1),vec3(0));
    history_store4(motion,reflected_feature_motion(receiver,hit,bary));
    output_image.pixels[identity]=receiver;output_image.pixels[identity+1u]=hit;
    if(p.history_info.y!=0xffffffffu) {
        output_image.pixels[identity+2u]=materials.word[p.history_info.y+receiver];
        output_image.pixels[identity+3u]=materials.word[p.history_info.y+hit];
    }
    history_store4(witness,vec4(bary,depth,1));
}

struct ModelHistoryHit {bool found;uint primitive;float distance;vec2 bary;};
ModelHistoryHit history_model_hit(vec3 origin,vec3 direction,float minimum,float maximum,uvec2 pixel) {
    rayQueryEXT query;
    rayQueryInitializeEXT(query,scene,gl_RayFlagsNoOpaqueEXT,0xff,origin,minimum,direction,maximum);
    while(rayQueryProceedEXT(query)) {
        if(rayQueryGetIntersectionTypeEXT(query,false)==gl_RayQueryCandidateIntersectionTriangleEXT
            && material_visible(rayQueryGetIntersectionPrimitiveIndexEXT(query,false),rayQueryGetIntersectionBarycentricsEXT(query,false),pixel))
            rayQueryConfirmIntersectionEXT(query);
    }
    ModelHistoryHit hit;hit.found=rayQueryGetIntersectionTypeEXT(query,true)==gl_RayQueryCommittedIntersectionTriangleEXT;
    hit.primitive=0u;hit.distance=0.0;hit.bary=vec2(0);
    if(hit.found) {hit.primitive=rayQueryGetIntersectionPrimitiveIndexEXT(query,true);hit.distance=rayQueryGetIntersectionTEXT(query,true);
        hit.bary=rayQueryGetIntersectionBarycentricsEXT(query,true);}
    return hit;
}
vec3 history_model_normal(uint primitive,vec3 incoming) {
    uint first=primitive*3u;
    vec3 normal=normalize(cross(vertices.vertex[first+1u].xyz-vertices.vertex[first].xyz,vertices.vertex[first+2u].xyz-vertices.vertex[first].xyz));
    return dot(normal,incoming)>0.0?-normal:normal;
}
void history_model() {
    uint id=gl_GlobalInvocationID.x,count=p.dimensions.x*p.dimensions.y;if(id>=count)return;
    bool scene_paths=HISTORY_MODEL_RECORDS==3u;
    uint lobes=uint(p.history_clip.z),stride=HISTORY_MODEL_RECORDS>=2u?13u:3u,prefix=scene_paths?11u:7u;
    output_image.pixels[id]=0u;output_image.pixels[count+id]=0xffffffffu;output_image.pixels[count*2u+id]=0u;
    history_store4(count*3u+id*4u,vec4(0));
    if(scene_paths)history_store4(count*7u+id*4u,vec4(0));
    for(uint lobe=0u;lobe<lobes;++lobe) {
        uint at=count*prefix+(id*lobes+lobe)*stride;
        if(HISTORY_MODEL_RECORDS>=2u)history_path_store(at,history_path_empty());
        else {output_image.pixels[at]=0xffffffffu;output_image.pixels[at+1u]=0u;output_image.pixels[at+2u]=0u;}
    }
    uvec2 pixel=uvec2(id%p.dimensions.x,id/p.dimensions.x);
    vec3 raw=vec3((float(pixel.x)+.5-p.camera.z)/p.camera.x,(float(pixel.y)+.5-p.camera.w)/p.camera.y,1);
    float near_depth=p.primary_range.z>.5?p.primary_range.x:1.0,far_depth=p.primary_range.z>.5?p.primary_range.y:65536.0;
    ModelHistoryHit primary=history_model_hit(vec3(0),raw,near_depth,far_depth,pixel);
    history_record_base=count*prefix+id*lobes*stride;history_record_stride=stride;history_record_lobe=0u;
    if(scene_paths && p.settings.y>.5) {
        float denominator=dot(raw,p.ground_normal.xyz);
        float ground=abs(denominator)>1.e-10?dot(p.ground_point.xyz,p.ground_normal.xyz)/denominator:far_depth;
        if(ground>near_depth && ground<(primary.found?primary.distance:far_depth)) {
            // The floor uses its one physical sharp ray even for rough MODEL
            // history. Copy that record; do not invent seven extra floor rays.
            output_image.pixels[id]=shade_water(pixel,normalize(raw),ground*length(raw));
            output_image.pixels[count+id]=0xfffffffeu;output_image.pixels[count*2u+id]=floatBitsToUint(ground);
            history_store4(count*3u+id*4u,vec4(history_primary_response,1));
            history_store4(count*7u+id*4u,vec4(history_primary_base,1));
            for(uint lobe=1u;lobe<lobes;++lobe)for(uint c=0u;c<stride;++c)
                output_image.pixels[history_record_base+lobe*stride+c]=output_image.pixels[history_record_base+c];
            return;
        }
    }
    if(!primary.found)return;
    vec3 incoming=normalize(raw),normal=history_model_normal(primary.primitive,raw),response=vec3(1);
    if(p.dimensions.w!=0u) {vec3 f0=conductor_f0(material_colour(primary.primitive,primary.bary,pixel),p.dimensions.w);
        float grazing=pow(1.0-clamp(dot(-incoming,normal),0.0,1.0),5.0);response=f0+(1.0-f0)*grazing;}
    output_image.pixels[count+id]=primary.primitive;output_image.pixels[count*2u+id]=floatBitsToUint(primary.distance);
    history_store4(count*3u+id*4u,vec4(response,1));
    if(scene_paths)history_store4(count*7u+id*4u,vec4(0,0,0,1));
    output_image.pixels[id]=shade_native_model(pixel,incoming,raw*primary.distance,normal,primary.distance*length(raw),primary.primitive,primary.bary);
}
#ifdef STARFOX_VULKAN_GROUND_HISTORY
vec4 reflected_planar_feature_motion(uint secondary,vec2 bary) {
    if(p.history_clip.w<.5)return vec4(0);
    vec3 point=p.previous_ground_point.xyz,normal=p.previous_ground_normal.xyz;
    vec4 a=history_vertex(secondary,0u),b=history_vertex(secondary,1u),c=history_vertex(secondary,2u);
    if(!history_finite(point) || !history_finite(normal) || dot(normal,normal)<=1e-20
        || a.w!=1.0 || b.w!=1.0 || c.w!=1.0 || !history_finite(a) || !history_finite(b) || !history_finite(c)
        || !history_finite(bary) || any(lessThan(bary,vec2(0))) || bary.x+bary.y>1.00001)return vec4(0);
    vec3 hit_normal=cross(b.xyz-a.xyz,c.xyz-a.xyz);
    if(!history_finite(hit_normal) || dot(hit_normal,hit_normal)<=1e-20)return vec4(0);
    normal=normalize(normal);
    vec3 feature=a.xyz*(1.0-bary.x-bary.y)+b.xyz*bary.x+c.xyz*bary.y;
    vec3 virtual_feature=feature-2.0*normal*dot(feature-point,normal);
    float denominator=dot(virtual_feature,normal);
    if(!history_finite(virtual_feature) || virtual_feature.z<=0.0 || !history_finite(denominator) || abs(denominator)<=1e-12)return vec4(0);
    float fraction=dot(point,normal)/denominator;
    vec3 receiver=virtual_feature*fraction;
    if(!history_finite(fraction) || fraction<=0.0 || fraction>=1.0 || !history_finite(receiver)
        || receiver.z<p.history_clip.x || receiver.z>p.history_clip.y)return vec4(0);
    vec2 pixel=p.history_projection.xy*virtual_feature.xy/virtual_feature.z+p.history_projection.zw;
    if(!history_finite(pixel) || any(lessThan(pixel,vec2(0))) || any(greaterThanEqual(pixel,vec2(p.history_info.zw))))return vec4(0);
    return vec4(pixel,receiver.z,1);
}
void history_separated_planar() {
    uint id=gl_GlobalInvocationID.x,count=p.dimensions.x*p.dimensions.y;if(id>=count)return;
    // Canonical 88-byte SoA layout. Every clear/off/invalid lane is written;
    // old lighting, old tint and primary screen velocity are never stored.
    uint motion=count+id*4u,identity=count*5u+id*4u,witness=count*9u+id*4u;
    uint incoming_at=count*13u+id,base_at=count*14u+id*4u,weight_at=count*18u+id*4u;
    output_image.pixels[id]=0u;output_image.pixels[incoming_at]=0u;
    history_store4(motion,vec4(0));history_store4(witness,vec4(0));
    history_store4(base_at,vec4(0));history_store4(weight_at,vec4(0));
    for(uint c=0u;c<4u;++c)output_image.pixels[identity+c]=0xffffffffu;
    history_secondary=0xffffffffu;history_secondary_bary=vec2(0);
    history_primary_base=vec3(0);history_primary_response=vec3(0);history_primary_incoming=0u;
    uvec2 pixel=uvec2(id%p.dimensions.x,id/p.dimensions.x);
    vec3 raw=vec3((float(pixel.x)+.5-p.camera.z)/p.camera.x,(float(pixel.y)+.5-p.camera.w)/p.camera.y,1);
    float near_depth=p.primary_range.z>.5?p.primary_range.x:1.0,far_depth=p.primary_range.z>.5?p.primary_range.y:65536.0;
    ModelHistoryHit primary=history_model_hit(vec3(0),raw,near_depth,far_depth,pixel);
    float denominator=dot(raw,p.ground_normal.xyz);
    float floor_depth=abs(denominator)>1e-10?dot(p.ground_point.xyz,p.ground_normal.xyz)/denominator:far_depth;
    bool floor_hit=floor_depth>near_depth && floor_depth<(primary.found?primary.distance:far_depth);
    float depth=floor_hit?floor_depth:primary.distance;
    if(floor_hit)output_image.pixels[id]=shade_water(pixel,normalize(raw),floor_depth*length(raw));
    else {
        if(!primary.found)return;
        vec3 normal=history_model_normal(primary.primitive,raw);
        uint colour=shade_native_model(pixel,normalize(raw),raw*primary.distance,normal,primary.distance*length(raw),primary.primitive,primary.bary);
        output_image.pixels[id]=colour;history_primary_incoming=colour;history_primary_response=vec3(1);
    }
    output_image.pixels[incoming_at]=history_primary_incoming;
    history_store4(base_at,vec4(history_primary_base,1));
    history_store4(weight_at,vec4(history_primary_response,0));
    if(history_secondary==0xffffffffu)return;
    uint receiver=floor_hit?0xfffffffeu:primary.primitive;
    history_store4(motion,floor_hit?reflected_planar_feature_motion(history_secondary,history_secondary_bary)
        :reflected_feature_motion(receiver,history_secondary,history_secondary_bary));
    output_image.pixels[identity]=receiver;output_image.pixels[identity+1u]=history_secondary;
    if(p.history_info.y!=0xffffffffu && (!floor_hit || p.history_clip.w>.5)) {
        output_image.pixels[identity+2u]=floor_hit?0xfffffffeu:materials.word[p.history_info.y+receiver];
        output_image.pixels[identity+3u]=materials.word[p.history_info.y+history_secondary];
    }
    history_store4(witness,vec4(history_secondary_bary,depth,1));
}
void main() {history_separated_planar();}
#elif defined(STARFOX_VULKAN_LIQUID_HISTORY)
void history_separated_liquid() {
    uint id=gl_GlobalInvocationID.x,count=p.dimensions.x*p.dimensions.y;if(id>=count)return;
    // Canonical current water prefix is 4/20/24 bytes, lava is 4 bytes.
    // Append the shared separated-history SoA; never reinterpret MODEL paths.
    uint prefix=p.liquid_layers.w!=0u?p.liquid_layers.w:count;
    uint motion=prefix+id*4u,identity=prefix+count*4u+id*4u,witness=prefix+count*8u+id*4u;
    uint incoming_at=prefix+count*12u+id,base_at=prefix+count*13u+id*4u,weight_at=prefix+count*17u+id*4u;
    output_image.pixels[id]=0u;output_image.pixels[incoming_at]=0u;
    history_store4(motion,vec4(0));history_store4(witness,vec4(0));
    history_store4(base_at,vec4(0));history_store4(weight_at,vec4(0));
    for(uint c=0u;c<4u;++c)output_image.pixels[identity+c]=0xffffffffu;
    store_liquid_surface(id,vec4(0));
    history_secondary=0xffffffffu;history_secondary_bary=vec2(0);
    history_primary_base=vec3(0);history_primary_response=vec3(0);history_primary_incoming=0u;
    uvec2 pixel=uvec2(id%p.dimensions.x,id/p.dimensions.x);
    vec3 raw=vec3((float(pixel.x)+.5-p.camera.z)/p.camera.x,(float(pixel.y)+.5-p.camera.w)/p.camera.y,1);
    float near_depth=p.primary_range.z>.5?p.primary_range.x:1.0,far_depth=p.primary_range.z>.5?p.primary_range.y:65536.0;
    uint world_colour=0u,world_secondary=0xffffffffu,world_incoming=0u;
    vec4 world_surface=vec4(0);vec2 world_bary=vec2(0);vec3 world_base=vec3(0),world_response=vec3(0);
    bool world_valid=false;
    if(NATIVE_WATER && (p.liquid_layers.z&1u)!=0u) {
        float ground=native_ground_depth(pixel);
        if(ground>near_depth && ground<far_depth) {
            world_colour=shade_native_water(pixel,normalize(raw),ground*length(raw),world_surface);world_valid=true;
            world_secondary=history_secondary;world_bary=history_secondary_bary;world_incoming=history_primary_incoming;
            world_base=history_primary_base;world_response=history_primary_response;
        }
        output_image.pixels[p.liquid_layers.x+id]=world_colour;
        // Hidden water does not own visible model guides. Reuse its shading
        // only if the same physical floor wins primary visibility below.
        history_secondary=0xffffffffu;history_secondary_bary=vec2(0);
        history_primary_base=vec3(0);history_primary_response=vec3(0);history_primary_incoming=0u;
    }
    ModelHistoryHit primary=history_model_hit(vec3(0),raw,near_depth,far_depth,pixel);
    float denominator=dot(raw,p.ground_normal.xyz);
    float floor_depth=abs(denominator)>1e-10?(NATIVE_WATER?native_ground_depth(pixel)
        :dot(p.ground_point.xyz,p.ground_normal.xyz)/denominator):far_depth;
    bool floor_hit=floor_depth>near_depth && floor_depth<(primary.found?primary.distance:far_depth);
    float depth=floor_hit?floor_depth:primary.distance;
    if(floor_hit) {
        if(NATIVE_WATER) {
            vec4 surface;
            if(world_valid) {
                output_image.pixels[id]=world_colour;surface=world_surface;
                history_secondary=world_secondary;history_secondary_bary=world_bary;history_primary_incoming=world_incoming;
                history_primary_base=world_base;history_primary_response=world_response;
            } else output_image.pixels[id]=shade_native_water(pixel,normalize(raw),floor_depth*length(raw),surface);
            store_liquid_surface(id,surface);
        } else output_image.pixels[id]=shade_water(pixel,normalize(raw),floor_depth*length(raw));
    } else {
        if(!primary.found)return;
        uint colour=shade_native_model(pixel,normalize(raw),raw*primary.distance,history_model_normal(primary.primitive,raw),
            primary.distance*length(raw),primary.primitive,primary.bary);
        output_image.pixels[id]=colour;history_primary_incoming=colour;history_primary_response=vec3(1);
    }
    output_image.pixels[incoming_at]=history_primary_incoming;
    history_store4(base_at,vec4(history_primary_base,1));history_store4(weight_at,vec4(history_primary_response,0));
    if(history_secondary==0xffffffffu)return;
    uint receiver=floor_hit?0xfffffffeu:primary.primitive;
    // Curved floor motion is solved AFTER the ray-query pass. Analytic depth
    // remains distinct from displaced lava's reflection origin/surface depth.
    if(!floor_hit)history_store4(motion,reflected_feature_motion(receiver,history_secondary,history_secondary_bary));
    output_image.pixels[identity]=receiver;output_image.pixels[identity+1u]=history_secondary;
    if(p.history_info.y!=0xffffffffu && (!floor_hit || p.history_clip.w>.5)) {
        output_image.pixels[identity+2u]=floor_hit?0xfffffffeu:materials.word[p.history_info.y+receiver];
        output_image.pixels[identity+3u]=materials.word[p.history_info.y+history_secondary];
    }
    history_store4(witness,vec4(history_secondary_bary,depth,1));
}
void main() {history_separated_liquid();}
#else
void main() {if(HISTORY_MODEL_RECORDS==0u)history_sharp();else history_model();}
#endif
