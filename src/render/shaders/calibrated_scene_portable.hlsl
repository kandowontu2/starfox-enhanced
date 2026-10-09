// Shared scene behavior, different SDL resource binding convention only.
// Compile each graphics stage with its matching STARFOX_SDL_SCENE_* define.
#define STARFOX_CALIBRATED_PALETTE
#include "calibrated_colour.hlsli"
#if defined(STARFOX_CALIBRATED_GROUND_SURFACE)
#include "../../../include/starfox/render/lava_surface.inc"
#include "calibrated_water_surface.hlsli"
#endif
#define fragment_textured_raw calibrated_source_fragment_raw
#define fragment_textured_main calibrated_source_fragment_main
#include "../../vr/shaders/scene.hlsl"
#undef fragment_textured_raw
#undef fragment_textured_main

// Native-only motion interface. Nest the original varying ABI without changing
// the shared headset/source shader. Legacy rigid guides use this same entry
// with the accepted-vertex mode disabled, so ordinary colour remains identical.
struct CalibratedMotionVertex {
    Fragment original;
#if defined(STARFOX_CALIBRATED_MSAA_GUIDES)
    [[vk::location(17)]] sample float3 previous_eye : TEXCOORD15;
#else
    [[vk::location(17)]] float3 previous_eye : TEXCOORD15;
#endif
    [[vk::location(18)]] nointerpolation uint previous_visible : TEXCOORD16;
    // Only native motion stages use this ABI. Hardware lines interpolate the
    // actual source segment; a covered pixel centre need not lie on that line.
#if defined(STARFOX_CALIBRATED_MSAA_GUIDES)
    [[vk::location(19)]] sample float3 current_eye : TEXCOORD17;
#else
    [[vk::location(19)]] float3 current_eye : TEXCOORD17;
#endif
};
#if defined(STARFOX_SDL_SCENE_VERTEX)
ByteAddressBuffer previousVertices : register(t1,space0);
cbuffer PreviousGeometry : register(b1,space1) {
    column_major float4x4 previousModelView;uint previousVertexCount;uint3 previousGeometryPadding;
};
float4x4 previous_view_matrix() {return previousModelView;}
// Reuse the exact source equations a second time with a different camera.
// There is no second, approximate destruction/billboard implementation.
#define view_matrix previous_view_matrix
#define source_billboard_visible previous_source_billboard_visible
#define source_visible previous_source_visible
#define explosion_round previous_explosion_round
#define explosion_word previous_explosion_word
#define explosion_q15 previous_explosion_q15
#define explosion_rotate previous_explosion_rotate
#define scene_eye_position previous_scene_eye_position
#include "../../vr/shaders/scene_geometry.hlsli"
#undef scene_eye_position
#undef explosion_rotate
#undef explosion_q15
#undef explosion_word
#undef explosion_round
#undef source_visible
#undef source_billboard_visible
#undef view_matrix
Vertex previous_vertex(uint index) {
    uint at=index*160;Vertex v;
    v.position=asfloat(previousVertices.Load3(at));v.color=asfloat(previousVertices.Load4(at+12));
    v.odd_color=asfloat(previousVertices.Load4(at+28));v.dither_scale=previousVertices.Load(at+44);
    v.visibility_a=asfloat(previousVertices.Load3(at+48));v.visibility_b=asfloat(previousVertices.Load3(at+60));
    v.visibility_c=asfloat(previousVertices.Load3(at+72));v.visibility_enabled=previousVertices.Load(at+84);
    v.group_a=asfloat(previousVertices.Load3(at+88));v.group_b=asfloat(previousVertices.Load3(at+100));
    v.group_c=asfloat(previousVertices.Load3(at+112));v.group_enabled=previousVertices.Load(at+124);
    v.uv=asfloat(previousVertices.Load2(at+128));v.texture=previousVertices.Load4(at+136);
    v.billboard=asfloat(previousVertices.Load2(at+152));return v;
}
CalibratedMotionVertex vertex_motion_main(Vertex input,uint index:SV_VertexID) {
    CalibratedMotionVertex output;output.original=vertex_textured_main(input);
    output.current_eye=0;
    [branch] if(previousGeometryPadding.x!=0) {
        Vertex now=input;source_billboard_visible(now);float3 currentPosition;uint currentVisible;
        output.current_eye=scene_eye_position(now,currentPosition,currentVisible).xyz;
    }
    output.previous_eye=0;output.previous_visible=0;
    if(index<previousVertexCount) {
        Vertex old=previous_vertex(index);uint billboardVisible=previous_source_billboard_visible(old);
        float3 position;uint visible;
        output.previous_eye=previous_scene_eye_position(old,position,visible).xyz;
        output.previous_visible=visible&billboardVisible;
    }
    return output;
}
#endif

float4 fragment_textured_raw(Fragment input) {
    // Only the existing physical lower-hemisphere receiver is replaced. Sky,
    // models and all later wipes/portraits/HUD keep their ordinary source path.
    if((input.texture.w&0x20000008U)==0x20000008U && input.surface_position.y<0) {
        // A mirror is a finite ray receiver, never baked base-colour scenery.
        // Keep this packet's sky in the cube, but exclude its reflective floor.
        if((camera.effects.w&0x00400000U)!=0) discard;
        uint start=input.texture.x+272+16384+1;
        float3 farColour=float3(asfloat(texels[start]),asfloat(texels[start+1]),asfloat(texels[start+2]));
        float3 nearColour=float3(asfloat(texels[start+3]),asfloat(texels[start+4]),asfloat(texels[start+5]));
        float row=112-input.surface_position.y*512/max(length(input.surface_position.xz),.0001);
        float fraction=saturate((row-asfloat(texels[start+6]))/asfloat(texels[start+7]));
        float3 encoded=lerp(farColour,nearColour,fraction);
#if defined(STARFOX_CALIBRATED_GROUND_SURFACE)
        uint material=texels[start+8],motion=texels[start+13];
        if(material!=0 || motion==3) {
            float2 origin=asfloat(uint2(texels[start+9],texels[start+10]));
            float seconds=asfloat(texels[start+11]);
            float2 world=input.surface_position.xz*float2(256,-256)+origin;
            float footprint=max(length(ddx_fine(world)),length(ddy_fine(world)));
            if(material==5) {
                // Optical rays replace this exact receiver in the RT finish.
                // Do not pay for disposable decorative waves underneath it.
                [branch] if((camera.effects.w&0x04000000U)==0)
                    encoded=calibrated_water_ripples(encoded,world,seconds,motion,
                        max(0.,row-asfloat(texels[start+6])),footprint);
            } else if(material==9) {
                // The ray finish supplies the displaced lava surface. Avoid
                // shading a second, disposable wave beneath that receiver.
                [branch] if((camera.effects.w&0x04000000U)==0) {
                LavaSample wave=lava_surface(world.x,world.y,seconds,footprint);
                // The actual per-eye view is already combined with the source
                // receiver's bank/rotation. Recover that eye in receiver-local
                // space; no centre-eye ray or screen-coordinate texture warp.
                float3 t=float3(camera.view_rows[0].w,camera.view_rows[1].w,camera.view_rows[2].w);
                float3 eye=-float3(dot(float3(camera.view_rows[0].x,camera.view_rows[1].x,camera.view_rows[2].x),t),
                    dot(float3(camera.view_rows[0].y,camera.view_rows[1].y,camera.view_rows[2].y),t),
                    dot(float3(camera.view_rows[0].z,camera.view_rows[1].z,camera.view_rows[2].z),t));
                float3 towardEye=(eye-input.surface_position)*float3(1,-1,-1);
                LavaColour molten=lava_shade(wave,towardEye.x,towardEye.y,towardEye.z);
                float edge=saturate((row-asfloat(texels[start+6]))/8.);
                edge=edge*edge*(3-2*edge);
                encoded=lerp(encoded,saturate(float3(molten.r,molten.g,molten.b)*asfloat(texels[start+12])),edge);
                }
            } else {
                float2 uv=world/64.;
                if(motion==1) uv.x+=sin(uv.y*.17+seconds)*.6;
                if(motion==2) uv.y+=sin(uv.x*.2+seconds*1.5)*.8;
                float detail=material==8?(lava_noise(uv.x*.10,uv.y*.10)*2-1)*.025:0;
                if(motion==3) detail+=sin(seconds*1.5+uv.y*.1)*.08;
                encoded=saturate(encoded*(1+detail*saturate((row-asfloat(texels[start+6]))/32.)));
            }
        }
#endif
        return float4((camera.effects.z&1U)!=0?calibrated_decode_srgb(encoded):encoded,1);
    }
    return calibrated_source_fragment_raw(input);
}
float4 fragment_textured_main(Fragment input):SV_Target {
    return styled_colour(fragment_textured_raw(input));
}

// Optional MRT entry: the ordinary scene pipeline and VR shaders are unchanged.
// Invoke the same raw fragment function, including every discard, alpha hole,
// source visibility, destruction and specialized background operation.
struct CalibratedReceiverFragment {
    float4 colour : SV_Target0;
    float4 receiver : SV_Target1;
};
CalibratedReceiverFragment fragment_receiver_main(Fragment input) {
    CalibratedReceiverFragment output;
    output.colour=styled_colour(fragment_textured_raw(input));
    output.receiver=0;
    if((camera.effects.w&0x04000000U)!=0 && output.colour.a==1
        && (input.texture.w&0x20000008U)==0x20000008U && input.surface_position.y<0)
        output.receiver=float4(1./255.,1,0,1); // Ground transport, not a model material.
    if((camera.effects.w&0x80000000U)!=0 && output.colour.a==1) {
        float3 position=mul(view_matrix(),float4(input.surface_position,1)).xyz;
        float3 normal=cross(ddx(position),ddy(position));
        float lengthSquared=dot(normal,normal),distanceSquared=dot(position,position);
        float facing=lengthSquared>1.e-20 && distanceSquared>1.e-20
            ?saturate(abs(dot(normal,position))*rsqrt(lengthSquared*distanceSquared)):1;
        // Dielectric rocks remain opaque. Explicit mirror/conductor materials
        // replace the receiver with traced radiance; Fresnel is evaluated by
        // the ray shader, not multiplied by a second dielectric Fresnel here.
        float fresnel=(camera.effects.w&0x40000000U)!=0?1:min(.3,.08+.92*pow(1-facing,5));
        output.receiver=float4(1,fresnel,0,1);
    }
    // Independent ownership channel: world effects need no ray receiver or RT.
    // Every visible fragment (also excluded foreground) writes its own class,
    // with exactly the same depth/discard/alpha-cutout path as source colour.
    output.receiver.b=float((camera.effects.w>>24)&3U)/255.;
    if((camera.effects.w&0x08000000U)!=0) {
        // Native AA is opt-in, independent of ray receivers. Preserve texture
        // samples, special bitmap/billboard art and all protected overlays.
        uint artwork=1U|8U|64U|128U|8388608U|67108864U|268435456U;
        // An analytic metal floor is physical geometry, even though its source
        // packet also carries bitmap sky. Restore only that lower receiver from
        // the ray-shaded sample; centre ink has no ray transport. Its absent
        // correspondence guide keeps changing reflections reactive (no invented
        // temporal history). The same packet's sky remains protected artwork.
        bool transportedGround=(camera.effects.w&0x04000000U)!=0
            && (input.texture.w&0x20000008U)==0x20000008U && input.surface_position.y<0;
        bool model=((camera.effects.w>>24)&3U)==2 && (input.texture.w&artwork)==0;
        output.receiver.a=(transportedGround || model) && output.colour.a==1?1:0;
    }
    return output;
}
#if defined(STARFOX_SDL_SCENE_FRAGMENT)
cbuffer MotionSettings : register(b2,space3) {
    column_major float4x4 currentToPrevious;
    float4 previousProjection; // P00, P11, P02, P12; canonical asymmetric perspective.
    float2 previousExtent;uint motionValid;uint motionPadding;
};
struct CalibratedMotionFragment {
    float4 colour : SV_Target0;
    float4 receiver : SV_Target1;
    float4 surface : SV_Target2;
    float4 motion : SV_Target3;
};
#endif
#if defined(STARFOX_CALIBRATED_MSAA_GUIDES)
float2 calibrated_sample_position(float2 pixel,uint sampleIndex) {
    const int2 offsets[14]={int2(4,4),int2(-4,-4),int2(-2,-6),int2(6,-2),int2(-6,2),int2(2,6),
        int2(1,-3),int2(-1,3),int2(5,1),int2(-3,-5),int2(-5,5),int2(-7,-1),int2(3,7),int2(7,-7)};
    uint count=1U<<((camera.effects.w>>28)&3U);
    return floor(pixel)+.5+float2(offsets[count-2+sampleIndex])/16.;
}
CalibratedReceiverFragment fragment_receiver_msaa_main(Fragment input,uint sampleIndex:SV_SampleIndex) {
    // SV_SampleIndex and sample-qualified source coordinates make texture
    // holes and exact labels belong to each covered sample, not its centre.
    if(sampleIndex>=8) discard;
    return fragment_receiver_main(input);
}
#endif

// Native-only optional third MRT. Do not extend the shared VR vertex/varying
// ABI or treat source-span UV numerators / billboard centres as geometry.
// SV_Position contains the actually rasterized point after all source/model,
// destruction, billboard, calibrated eye and projection operations.
#if defined(STARFOX_SDL_SCENE_FRAGMENT)
cbuffer SurfaceSettings : register(b1,space3) {float2 surfaceExtent;float sourceUnits;uint surfaceEligible;};
struct CalibratedSurfaceFragment {
    float4 colour : SV_Target0;
    float4 receiver : SV_Target1;
    float4 surface : SV_Target2;
};
CalibratedSurfaceFragment fragment_surface_main(Fragment input) {
    CalibratedReceiverFragment original=fragment_receiver_main(input);
    CalibratedSurfaceFragment output;output.colour=original.colour;output.receiver=original.receiver;output.surface=0;
    // DirectX fragment SV_Position.w is clip W; the SPIR-V surface variant
    // uses -fvk-use-dx-position-w to reciprocate Vulkan's raw 1/W input once.
    // Canonical perspective has clip W=-eye Z. Unlike normalized Z, this keeps
    // far-floor precision and ignores the source decal's intentional Z bias.
    float forward=input.position.w;
    float2 ndc=float2(input.position.x*2/surfaceExtent.x-1,1-input.position.y*2/surfaceExtent.y);
    float3 position=float3((ndc.x+camera.projection[0][2])*forward/camera.projection[0][0],
        (ndc.y+camera.projection[1][2])*forward/camera.projection[1][1],-forward);
    float3 normal=cross(ddx(position),ddy(position));float lengthSquared=dot(normal,normal);
    // World scenery is at infinity except the source's explicit flattened
    // physical floor. A sky sphere, screen bitmap or cloud must not become a
    // finite AO occluder just because it has triangles in the native renderer.
    uint layer=(camera.effects.w>>24)&3U;
    bool floor=false;
    [branch] if(layer==1 && (input.texture.w&8U)!=0 && input.surface_position.y<0)
        floor=(texels[input.texture.x+15]&0x10000000U)!=0;
    [branch] if(floor) {
        // The known physical floor is a plane, not a sampled sky sphere.
        // Near its horizon, subpixel vertex snapping magnifies even clip-W
        // interpolation error. Intersect the true eye ray with that plane,
        // using the full affine model/eye transform (also bank/nonuniform scale).
        float3 tangentX=float3(camera.view_rows[0].x,camera.view_rows[1].x,camera.view_rows[2].x);
        float3 tangentZ=float3(camera.view_rows[0].z,camera.view_rows[1].z,camera.view_rows[2].z);
        normal=cross(tangentX,tangentZ);lengthSquared=dot(normal,normal);
        float3 floorPoint=mul(view_matrix(),float4(0,input.surface_position.y,0,1)).xyz;
        float3 ray=float3((ndc.x+camera.projection[0][2])/camera.projection[0][0],
            (ndc.y+camera.projection[1][2])/camera.projection[1][1],-1);
        forward=dot(normal,floorPoint)/dot(normal,ray);position=ray*forward;
    }
    if(surfaceEligible!=0 && (layer==2 || floor) && output.colour.a==1 && forward>0
        && isfinite(forward*sourceUnits) && lengthSquared>1.e-20 && isfinite(lengthSquared)) {
        normal*=rsqrt(lengthSquared);
        if(dot(normal,position)>0) normal=-normal;
        // Match desktop surfaces: +Y down/+Z forward, cartridge units.
        output.surface=float4(normal*float3(1,-1,-1),forward*sourceUnits);
    }
    // Every visible excluded overlay writes zero, with the same depth, alpha
    // holes and discard as colour. HUD/emissive ink cannot inherit a surface.
    return output;
}
CalibratedMotionFragment fragment_motion_main(CalibratedMotionVertex varying) {
    Fragment input=varying.original;
    CalibratedSurfaceFragment original=fragment_surface_main(input);
    CalibratedMotionFragment output;
    output.colour=original.colour;output.receiver=original.receiver;
    output.surface=original.surface;output.motion=0;
    bool sourceLine=surfaceEligible==2 && ((camera.effects.w>>24)&3U)==2 && output.colour.a==1;
    if(sourceLine) {
        // No fictitious planar normal for a one-dimensional primitive. Depth
        // is nevertheless well defined on its perspective-interpolated segment.
        float forward=-varying.current_eye.z;
        output.surface=all(isfinite(varying.current_eye)) && forward>0 && isfinite(forward*sourceUnits)
            ?float4(0,0,0,forward*sourceUnits):float4(0,0,0,0);
    }
    if(motionValid!=0 && original.surface.w>0) {
        float forward=original.surface.w/sourceUnits;
        float2 ndc=float2(input.position.x*2/surfaceExtent.x-1,1-input.position.y*2/surfaceExtent.y);
        float4 eye=float4((ndc.x+camera.projection[0][2])*forward/camera.projection[0][0],
            (ndc.y+camera.projection[1][2])*forward/camera.projection[1][1],-forward,1);
        float3 prior=motionValid==2?varying.previous_eye:mul(currentToPrevious,eye).xyz;
        float distance=-prior.z;
        float2 projected=float2(prior.x*previousProjection.x/distance-previousProjection.z,
            prior.y*previousProjection.y/distance-previousProjection.w);
        float2 pixel=float2((projected.x+1)*previousExtent.x/2,(1-projected.y)*previousExtent.y/2);
        if((motionValid!=2 || varying.previous_visible!=0) && distance>0 && all(isfinite(prior))
            && all(isfinite(pixel)) && isfinite(distance*sourceUnits))
            output.motion=float4(pixel-(sourceLine?float2(
                (varying.current_eye.x*camera.projection[0][0]/forward-camera.projection[0][2]+1)*surfaceExtent.x/2,
                (1-varying.current_eye.y*camera.projection[1][1]/forward+camera.projection[1][2])*surfaceExtent.y/2)
                :input.position.xy),distance*sourceUnits,1);
    }
    return output;
}
#if defined(STARFOX_CALIBRATED_MSAA_GUIDES)
CalibratedMotionFragment fragment_motion_msaa_main(CalibratedMotionVertex input,uint sampleIndex:SV_SampleIndex) {
    if(sampleIndex>=8) discard;
    input.original.position.xy=calibrated_sample_position(input.original.position.xy,sampleIndex);
    input.original.position.w=input.original.forward_depth;
    return fragment_motion_main(input);
}
CalibratedSurfaceFragment fragment_surface_msaa_main(Fragment input,uint sampleIndex:SV_SampleIndex) {
    if(sampleIndex>=8) discard;
    // Normalize XY explicitly; both native backends use the standard sample
    // positions, while their fragment-position conventions can differ.
    input.position.xy=calibrated_sample_position(input.position.xy,sampleIndex);
    input.position.w=input.forward_depth;
    return fragment_surface_main(input);
}
#endif
#endif
