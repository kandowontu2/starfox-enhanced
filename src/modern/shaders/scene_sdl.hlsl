// MODERN: the VR scene shader under SDL_GPU's binding model.
// Vertex stage: storage buffers t#/space0, uniform buffers b#/space1.
// Fragment stage: storage buffers t#/space2, uniform buffers b#/space3.
// SPIR-V takes set = space, binding = register; DXIL uses the registers as-is.
// The camera arrives as a uniform block (SDL_PushGPU*UniformData, slot 0)
// instead of a Vulkan push constant. The caller flips projection Y because
// SDL_GPU presents a Y-up clip space on every backend.
// scene.hlsl declares those bindings for STARFOX_SDL_SCENE_VERTEX/FRAGMENT
// (shared with the calibrated DisplayXR renderer).
#if defined(STARFOX_SDL_FRAGMENT)
#define STARFOX_SDL_SCENE_FRAGMENT
#else
#define STARFOX_SDL_SCENE_VERTEX
#endif
#define STARFOX_SCENE_RESOLVE_DITHER
#include "../../vr/shaders/scene.hlsl"

// SDL_GPU's D3D12 backend names every vertex attribute TEXCOORD<location>.
struct SdlVertex {
    [[vk::location(0)]] float3 position : TEXCOORD0;
    [[vk::location(1)]] float4 color : TEXCOORD1;
    [[vk::location(2)]] float4 odd_color : TEXCOORD2;
    [[vk::location(3)]] uint dither_scale : TEXCOORD3;
    [[vk::location(4)]] float3 visibility_a : TEXCOORD4;
    [[vk::location(5)]] float3 visibility_b : TEXCOORD5;
    [[vk::location(6)]] float3 visibility_c : TEXCOORD6;
    [[vk::location(7)]] uint visibility_enabled : TEXCOORD7;
    [[vk::location(8)]] float3 group_a : TEXCOORD8;
    [[vk::location(9)]] float3 group_b : TEXCOORD9;
    [[vk::location(10)]] float3 group_c : TEXCOORD10;
    [[vk::location(11)]] uint group_enabled : TEXCOORD11;
    [[vk::location(12)]] float2 uv : TEXCOORD12;
    [[vk::location(13)]] uint4 texture : TEXCOORD13;
    [[vk::location(14)]] float2 billboard : TEXCOORD14;
};
Vertex sdl_vertex(SdlVertex input) {
    Vertex output;
    output.position=input.position;output.color=input.color;output.odd_color=input.odd_color;
    output.dither_scale=input.dither_scale;
    output.visibility_a=input.visibility_a;output.visibility_b=input.visibility_b;output.visibility_c=input.visibility_c;
    output.visibility_enabled=input.visibility_enabled;
    output.group_a=input.group_a;output.group_b=input.group_b;output.group_c=input.group_c;
    output.group_enabled=input.group_enabled;
    output.uv=input.uv;output.texture=input.texture;output.billboard=input.billboard;
    return output;
}
#if !defined(STARFOX_SDL_FRAGMENT)
// Vertex uniform slot 1 (SdlScenePipelines::push_line_brush): x = brush
// pixels, yz = clip offset per target pixel, w = enabled (line draws only).
cbuffer SdlLineBrush : register(b1, space1) { float4 line_brush; };
Fragment sdl_vertex_textured_main(SdlVertex input,uint instance : SV_InstanceID) {
    Fragment output=vertex_textured_main(sdl_vertex(input));
    // Thick source lines: instance i < N shifts i pixels right, later ones
    // shift down, sweeping the desktop's N x N square brush along the line.
    if(line_brush.w!=0 && instance!=0) {
        uint pixels=uint(line_brush.x);
        float2 shift=instance<pixels?float2(instance,0):float2(0,instance-pixels+1);
        output.position.xy+=shift*line_brush.yz*output.position.w;
    }
    return output;
}
#endif
#if defined(STARFOX_SDL_FRAGMENT)
// Fragment uniform slot 1 (SdlFrameRenderer): distance haze. colour in the
// target encoding (a = strength); params x/y = start/end distance (source
// units), z = near plane x 256 (reversed depth: distance = z / depth),
// w = 0 off, 1 every fragment, 2 the landscape: its ground by distance and
// its sky dome by height above the horizon (full at it, clear 56 art rows
// up), so the haze hangs over the horizon as well as in front of it.
// The same block carries the ray-traced sun shadow (SdlRayShadows):
// shadow_params x = receivers (0 none, 1 world models, 2 the landscape
// ground plane), y = caster objects, z = brightness kept in shadow, w = sun
// samples (1 hard, more for soft); shadow_light xyz = toward the sun in
// native eye space (+Y down, +Z forward, source units), w = ray start
// offset per unit of distance; shadow_target xy = target size, z = the
// sun's angular radius (soft shadows); shadow_unproject = inverse
// projection rows (SDL clip space, Y up).
// Emissive point lights (SdlRayShadows::lights): light_params x = count,
// y = brightness steps (0 smooth), z = strength, w = 1 when the sun shadows
// these receivers; point_lights, four per light (SdlRayShadows::PointLight):
// position (native eye space) and radius; colour and its own caster object
// (asuint, ~0 none); pool axis and aspect; hardness, projected, fade height,
// background only (lights the ground, corridor and room, not models).
cbuffer SdlFog : register(b1, space3) {
    float4 fog_colour; float4 fog_params;
    float4 shadow_params; float4 shadow_light; float4 shadow_target; float4 shadow_unproject[4];
    float4 light_params; float4 point_lights[64];
};
// Casters (ray_shadow.hlsl): three float4 corners per triangle and its
// bounds across the sun's rays (min u,v, max u,v on shadow_basis); per object
// ordered-bits bounds plus its triangle range (count bit 31: large scenery).
StructuredBuffer<float4> shadow_triangles : register(t1, space2);
ByteAddressBuffer shadow_objects : register(t2, space2);
float shadow_unordered(uint bits) {return asfloat((bits&0x80000000U)!=0?bits&0x7fffffffU:~bits);}
float3 shadow_native_position(float4 position) {
    float4 clip=float4(position.x/shadow_target.x*2.-1.,1.-position.y/shadow_target.y*2.,position.z,1);
    float4 eye=float4(dot(shadow_unproject[0],clip),dot(shadow_unproject[1],clip),
        dot(shadow_unproject[2],clip),dot(shadow_unproject[3],clip));
    eye.xyz/=eye.w;
    return float3(eye.x,-eye.y,-eye.z)*256.;
}
// Moller-Trumbore distance along the ray, or -1 for a miss.
float shadow_triangle_distance(uint t,float3 origin,float3 direction) {
    float3 p0=shadow_triangles[t*4].xyz,e1=shadow_triangles[t*4+1].xyz-p0,e2=shadow_triangles[t*4+2].xyz-p0;
    float3 p=cross(direction,e2);
    float determinant=dot(e1,p);
    if(abs(determinant)<1e-12) return -1.;
    float inverse_determinant=1./determinant;
    float3 offset=origin-p0;
    float u=dot(offset,p)*inverse_determinant;
    if(u<0 || u>1) return -1.;
    float3 q=cross(offset,e1);
    float v=dot(direction,q)*inverse_determinant;
    if(v<0 || u+v>1) return -1.;
    return dot(e2,q)*inverse_determinant;
}
// Moller-Trumbore, hits in front of the origin only.
bool shadow_triangle_hit(uint t,float3 origin,float3 direction) {
    float3 p0=shadow_triangles[t*4].xyz,e1=shadow_triangles[t*4+1].xyz-p0,e2=shadow_triangles[t*4+2].xyz-p0;
    float3 p=cross(direction,e2);
    float determinant=dot(e1,p);
    if(abs(determinant)<1e-12) return false;
    float inverse_determinant=1./determinant;
    float3 offset=origin-p0;
    float u=dot(offset,p)*inverse_determinant;
    if(u<0 || u>1) return false;
    float3 q=cross(offset,e1);
    float v=dot(direction,q)*inverse_determinant;
    if(v<0 || u+v>1) return false;
    return dot(e2,q)*inverse_determinant>0;
}
// Fixed directions across the sun's disc (as GPU FAST's soft shadows): no
// per-pixel noise, so the penumbra is still at any frame rate and steps in
// even shades. Probes are four rays on the disc's rim; samples follow a
// spiral from its centre.
float3 shadow_sample_direction(uint sample,uint samples,bool probes,float3 tangent,float3 bitangent) {
    if(samples<=1) return shadow_light.xyz;
    float radius=probes?shadow_target.z:shadow_target.z*sqrt((float(sample)+.5)/float(samples));
    float angle=probes?float(sample)*1.5707963+.7853982:float(sample)*2.399963229728653;
    return normalize(shadow_light.xyz+(tangent*cos(angle)+bitangent*sin(angle))*radius);
}
// Whether a point at `across` (on the plane facing the sun) lies within
// triangle t's bounds there, widened by `spread` source units.
bool shadow_across(uint t,float2 across,float spread) {
    float4 bounds=shadow_triangles[t*4+3];
    return all(across>=bounds.xy-spread) && all(across<=bounds.zw+spread);
}
// Bit mask of the blocked rays between the point and the sun. Objects are
// culled once for the whole cone (their bounds widened by the cone's
// spread), then each still-open ray tests that object's triangles.
uint shadow_trace(float3 origin,uint samples,bool probes,bool ground) {
    float3 light=shadow_light.xyz;
    float3 tangent=normalize(cross(light,abs(light.y)<.9?float3(0,1,0):float3(1,0,0)));
    float3 bitangent=cross(light,tangent);
    float3 inverse=1./light;
    // The point's position across the sun's rays (ray_shadow.hlsl bounds).
    float2 across=float2(dot(origin,tangent),dot(origin,bitangent));
    uint all_blocked=(1U<<samples)-1U,blocked=0;
    uint objects=uint(shadow_params.y);
    [loop] for(uint o=0;o<objects && blocked!=all_blocked;++o) {
        uint4 low_bits=shadow_objects.Load4(o*32),high_bits=shadow_objects.Load4(o*32+16);
        uint first=low_bits.w,count=high_bits.w&0x7fffffffU;
        if(count==0 || any(low_bits.xyz>high_bits.xyz)) continue;
        float3 low=float3(shadow_unordered(low_bits.x),shadow_unordered(low_bits.y),shadow_unordered(low_bits.z));
        float3 high=float3(shadow_unordered(high_bits.x),shadow_unordered(high_bits.y),shadow_unordered(high_bits.z));
        float margin=samples>1?shadow_target.z*1.05*(length((low+high)*.5-origin)+length(high-low)*.5):0.;
        float3 a=(low-margin-origin)*inverse,b=(high+margin-origin)*inverse;
        float3 closest=min(a,b),farthest=max(a,b);
        float enter=max(max(closest.x,closest.y),max(closest.z,0.)),leave=min(min(farthest.x,farthest.y),farthest.z);
        if(enter>leave) continue;
        // Large scenery (tunnels, hangars, arches) never shades its own
        // inside: walls, doors and floors within its bounds stay as painted.
        // The landscape ground still takes its shadow (under an arch).
        if(!ground && (high_bits.w&0x80000000U)!=0 && all(origin>=low) && all(origin<=high)) continue;
        // Rays off the sun's centre drift across by up to the cone margin.
        float spread=margin+.5;
        [loop] for(uint t=first;t<first+count && blocked!=all_blocked;++t) {
            if(!shadow_across(t,across,spread)) continue;
            [loop] for(uint sample=0;sample<samples;++sample) {
                if((blocked>>sample)&1U) continue;
                if(shadow_triangle_hit(t,origin,shadow_sample_direction(sample,samples,probes,tangent,bitangent))) blocked|=1U<<sample;
            }
        }
    }
    return blocked;
}
// Fraction of the sun hidden from the point. Soft shadows first trace the
// four rim probes: when they agree (fully lit or fully shadowed, most
// pixels) that is the answer; only penumbra pixels trace every sample.
float shadow_coverage(float3 origin,bool ground=false) {
    uint samples=clamp(uint(shadow_params.w),1U,16U);
    if(samples==1) return float(shadow_trace(origin,1,false,ground));
    uint probes=shadow_trace(origin,4,true,ground);
    if(probes==0) return 0.;
    if(probes==15) return 1.;
    return float(countbits(shadow_trace(origin,samples,false,ground)))/float(samples);
}
// Whether any caster lies between the point and a point light `range`
// away, other than the light's own object. Large scenery is ignored for
// points within its bounds, as for the sun.
bool light_blocked(float3 origin,float3 direction,float range,uint own,bool ground) {
    float3 inverse=1./direction;
    uint objects=uint(shadow_params.y);
    [loop] for(uint o=0;o<objects;++o) {
        if(o==own) continue;
        uint4 low_bits=shadow_objects.Load4(o*32),high_bits=shadow_objects.Load4(o*32+16);
        uint first=low_bits.w,count=high_bits.w&0x7fffffffU;
        if(count==0 || any(low_bits.xyz>high_bits.xyz)) continue;
        float3 low=float3(shadow_unordered(low_bits.x),shadow_unordered(low_bits.y),shadow_unordered(low_bits.z));
        float3 high=float3(shadow_unordered(high_bits.x),shadow_unordered(high_bits.y),shadow_unordered(high_bits.z));
        if(!ground && (high_bits.w&0x80000000U)!=0 && all(origin>=low) && all(origin<=high)) continue;
        float3 a=(low-origin)*inverse,b=(high-origin)*inverse;
        float3 closest=min(a,b),farthest=max(a,b);
        float enter=max(max(closest.x,closest.y),max(closest.z,0.)),leave=min(min(farthest.x,farthest.y),min(farthest.z,range));
        if(enter>leave) continue;
        [loop] for(uint t=first;t<first+count;++t) {
            float distance=shadow_triangle_distance(t,origin,direction);
            if(distance>0 && distance<range) return true;
        }
    }
    return false;
}
// Coloured point lights on a receiver: full brightness out to the light's
// hardness, then a smooth fall to its radius, times facing, with
// ray-traced visibility (light_params.y > 0 bands it into even steps). A
// projected light (an engine's glow) is a pool on background surfaces where
// the sun casts it, stretched along its axis and fading with its height. The
// light tints the surface's own colour and adds its own, so blue reads blue
// on green ground; near-black surfaces (space, voids) stay black.
float point_light_edge(float x,float hardness) {float edge=1.-smoothstep(hardness,1.,x);return edge*edge;}
float3 point_lighting(float3 colour,float3 surface,float3 normal,bool ground,bool background) {
    float3 total=0;
    uint count=min(uint(light_params.x),16U);
    float steps=light_params.y;
    float bias=max(.5,length(surface)*shadow_light.w);
    float3 origin=surface+normal*bias;
    [loop] for(uint i=0;i<count;++i) {
        float4 placed=point_lights[i*4],tint=point_lights[i*4+1],shape=point_lights[i*4+2],look=point_lights[i*4+3];
        float amount;
        if(look.y!=0) {
            if(!background) continue;
            float3 offset=surface-placed.xyz;
            float height=-dot(offset,shadow_light.xyz); // How far the light is above, along the sun.
            if(height<=0 || height>=look.z) continue;
            float3 lateral=offset+shadow_light.xyz*height;
            float along=dot(lateral,shape.xyz);
            float3 across=lateral-shape.xyz*along;
            float distance=sqrt(dot(across,across)+along*along/(shape.w*shape.w));
            if(distance>=placed.w) continue;
            amount=point_light_edge(distance/placed.w,look.x)*(1.-height/look.z);
            if(amount<=0) continue;
            // Traced to the light itself: an engine's own hull cuts its pool.
            float3 toward=placed.xyz-origin;
            float reach=length(toward);
            if(reach>1e-3 && light_blocked(origin,toward/reach,reach,asuint(tint.w),ground)) continue;
        } else {
            if(look.w!=0 && !background) continue;
            float3 offset=placed.xyz-origin;
            float distance=length(offset);
            if(distance>=placed.w || distance<1e-3) continue;
            float3 direction=offset/distance;
            float facing=dot(normal,direction);
            if(facing<=0) continue;
            amount=point_light_edge(distance/placed.w,look.x)*facing;
            if(steps>0) amount=floor(amount*steps+.5)/steps;
            if(amount<=0) continue;
            if(light_blocked(origin,direction,distance,asuint(tint.w),ground)) continue;
        }
        total+=tint.rgb*amount;
    }
    // The small added glow fades out on near-black surfaces: what the
    // cartridge paints black (space, voids) stays black.
    float peak=max(max(colour.r,colour.g),colour.b);
    return saturate(colour+total*(colour*.5+.35*saturate(peak*8.))*light_params.z);
}
float4 sdl_fragment_main(Fragment input) : SV_Target0 {
    // Model receivers need the face's plane before anything may discard:
    // flat faces, so screen derivatives of the rebuilt position give it.
    float3 surface=0,normal=0;
    if(shadow_params.x!=0) {
        surface=shadow_native_position(input.position);
        normal=cross(ddx(surface),ddy(surface));
    }
    float4 colour=fragment_textured_main(input);
    // Receivers. Background (mode 2): the landscape's ground plane, the 3D
    // corridor (tile layer marked 2048) and the Space Armada boss room's
    // wall, floor and ceiling (flat vertices; texels[7] 4096/4097). Models
    // (mode 1): every face except billboards and source lines.
    bool ground=false,receiver=false;
    if(shadow_params.x==2 && colour.a!=0) {
        if((input.texture.w&8U)!=0) {
            uint flags=texels[input.texture.x+7];
            ground=(flags&4096U)!=0 && (flags&16384U)!=0;
            receiver=ground || flags==2048U;
        } else receiver=input.texture.w==0 && input.texture.x==0 && (texels[7]==4096U || texels[7]==4097U);
    } else if(shadow_params.x==1 && colour.a!=0) receiver=(input.texture.w&4U)==0;
    float length_squared=dot(normal,normal);
    if(receiver && length_squared>1e-12) {
        normal*=rsqrt(length_squared);
        if(dot(normal,surface)>0) normal=-normal; // the side the camera sees
        float shade=0;
        if(light_params.w!=0) {
            float bias=max(ground?1.:.5,length(surface)*shadow_light.w);
            if(ground)
                // Start the ray just off the ground, relative to its distance.
                shade=shadow_coverage(surface+shadow_light.xyz*bias,true);
            // A face turned from the sun is already shaded by its palette
            // (walls, ceilings, a ship's underside): only faces the sun
            // lights take cast shadows (including their own object's).
            else if(dot(normal,shadow_light.xyz)>.02) shade=shadow_coverage(surface+(normal+shadow_light.xyz)*bias);
        }
        colour.rgb*=1.-shade*(1.-shadow_params.z);
        if(light_params.x>0) colour.rgb=point_lighting(colour.rgb,surface,normal,ground,shadow_params.x==2);
    }
    if(fog_params.w==0 || colour.a==0) return colour;
    float amount;
    if(fog_params.w==2) {
        uint flags=texels[input.texture.x+7];
        if((input.texture.w&8U)==0 || (flags&4096U)==0) return colour;
        if((flags&16384U)==0) {
            // The sky dome (landscape_world_sample's art rows above the horizon).
            float3 d=input.surface_position;
            float rows=d.y/max(-d.z,length(d)*.02)*256.;
            amount=pow(1.-saturate(rows/56.),1.5)*fog_colour.a;
            colour.rgb=lerp(colour.rgb,fog_colour.rgb,amount);
            return colour;
        }
    }
    float distance=fog_params.z/max(input.position.z,1e-9);
    amount=saturate((distance-fog_params.x)/max(fog_params.y-fog_params.x,1.))*fog_colour.a;
    colour.rgb=lerp(colour.rgb,fog_colour.rgb,amount);
    return colour;
}
#endif
