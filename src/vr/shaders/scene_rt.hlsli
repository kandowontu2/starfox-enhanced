// MODERN's ray-traced sun shadows and coloured point lights, fragment side,
// shared by the flat renderer (scene_sdl.hlsl) and the headset
// (scene.hlsl's STARFOX_SCENE_RT variant). Moved from scene_sdl.hlsl.
// The includer declares, under these names: shadow_params, shadow_light,
// shadow_target, light_params and point_lights (layouts in scene_sdl.hlsl's
// SdlFog); shadow_triangles and shadow_objects (ray_shadow.hlsl's outputs);
// texels; and three accessors: float4 ray_clip(float4 position) (SV_Position
// to clip space), float4 ray_unproject_row(uint row) (rows of the inverse of
// the clip transform: clip to the space the casters were expanded in) and
// float3 ray_eye() (the eye in that space, native units).
float shadow_unordered(uint bits) {return asfloat((bits&0x80000000U)!=0?bits&0x7fffffffU:~bits);}
float3 shadow_native_position(float4 position) {
    float4 clip=ray_clip(position);
    float4 eye=float4(dot(ray_unproject_row(0),clip),dot(ray_unproject_row(1),clip),
        dot(ray_unproject_row(2),clip),dot(ray_unproject_row(3),clip));
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
    float bias=max(.5,length(surface-ray_eye())*shadow_light.w);
    float3 origin=surface+normal*bias;
    [loop] for(uint i=0;i<count;++i) {
        float4 placed=point_lights[i*4],tint=point_lights[i*4+1],shape=point_lights[i*4+2],look=point_lights[i*4+3];
        float amount;
        if(look.y!=0) {
            if(!background) continue;
            float3 offset=surface-placed.xyz;
            // How far the light is above, along the sun (the traced point
            // sits look.w further up).
            float height=-dot(offset,shadow_light.xyz);
            float climb=(height-look.w)/look.z;
            if(climb<=0 || climb>=1) continue;
            float3 lateral=offset+shadow_light.xyz*height;
            float along=dot(lateral,shape.xyz);
            float3 across=lateral-shape.xyz*along;
            float distance=sqrt(dot(across,across)+along*along/(shape.w*shape.w));
            // The pool widens (look.y times at the top) and fades as it climbs.
            float radius=placed.w*lerp(1.,look.y,climb);
            if(distance>=radius) continue;
            // Nearly full while low, then gone quickly.
            amount=point_light_edge(distance/radius,look.x)*(1.-smoothstep(0.,1.,climb));
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
// Lighting for one receiver pixel (both renderers). `mode` 2: the
// background, i.e. the landscape's ground plane, the 3D corridor (tile layer
// marked 2048) and the Space Armada boss room's wall, floor and ceiling
// (flat vertices; texels[7] 4096/4097); 1: the world's models, every face
// except billboards and source lines. `sun`: sun shadows on these receivers.
// `surface` and `normal` come from shadow_native_position and its screen
// derivatives, taken before the fragment may discard.
float4 ray_lighting(Fragment input,float4 colour,float3 surface,float3 normal,uint mode,bool sun) {
    bool ground=false,receiver=false;
    if(mode==2 && colour.a!=0) {
        if((input.texture.w&8U)!=0) {
            uint flags=texels[input.texture.x+7];
            ground=(flags&4096U)!=0 && (flags&16384U)!=0;
            receiver=ground || flags==2048U;
        } else receiver=input.texture.w==0 && input.texture.x==0 && (texels[7]==4096U || texels[7]==4097U);
    } else if(mode==1 && colour.a!=0) receiver=(input.texture.w&4U)==0;
    float length_squared=dot(normal,normal);
    if(receiver && length_squared>1e-12) {
        normal*=rsqrt(length_squared);
        if(dot(normal,surface-ray_eye())>0) normal=-normal; // the side the eye sees
        float shade=0;
        if(sun) {
            float bias=max(ground?1.:.5,length(surface-ray_eye())*shadow_light.w);
            if(ground)
                // Start the ray just off the ground, relative to its distance.
                shade=shadow_coverage(surface+shadow_light.xyz*bias,true);
            // A face turned from the sun is already shaded by its palette
            // (walls, ceilings, a ship's underside): only faces the sun
            // lights take cast shadows (including their own object's).
            else if(dot(normal,shadow_light.xyz)>.02) shade=shadow_coverage(surface+(normal+shadow_light.xyz)*bias);
        }
        colour.rgb*=1.-shade*(1.-shadow_params.z);
        if(light_params.x>0) colour.rgb=point_lighting(colour.rgb,surface,normal,ground,mode==2);
    }
    return colour;
}
