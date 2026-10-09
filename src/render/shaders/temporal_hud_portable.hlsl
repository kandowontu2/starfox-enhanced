Texture2D<float4> original : register(t0,space0);
Texture2D<float4> reconstructed : register(t1,space0);
StructuredBuffer<uint> packedPixels : register(t2,space0);
[[vk::image_format("rgba8")]] RWTexture2D<float4> result : register(u0,space1);
cbuffer Settings : register(b0,space2) {uint width,height,preserveArtwork,protectHud;};
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=width || id.y>=height) return;
    uint packed=packedPixels[id.y*width+id.x];
    uint tag=(packed>>8)&255u;
    bool hud=protectHud!=0 && tag==1u && (packed&0x10000000u)==0;
    // Tilemap artwork has no pinhole-camera correspondence. Keep its native
    // samples instead of asking temporal reconstruction to invent movement.
    // Explicit terrain ownership stays in the reconstructed world layer.
    bool artwork=(preserveArtwork&1u)!=0 && ((tag==2u && (packed&0x08000000u)==0)
        || (tag==1u && (packed&0x10000000u)!=0));
    if(artwork && tag==2u && (preserveArtwork&2u)!=0) {
        // The reference compositor still has the current sample's model
        // silhouette. Using that single jittered mask to restore the sky
        // clips the reconstructed edge differently every frame. Leave a small
        // reconstruction guard band; distant artwork and HUD stay native.
        for(int y=-2;y<=2;++y) for(int x=-2;x<=2;++x) {
            int2 at=clamp(int2(id.xy)+int2(x,y),0,int2(width-1,height-1));
            uint neighbor=packedPixels[at.y*width+at.x];
            uint neighborTag=(neighbor>>8)&255u;
            // Bit 25 records earlier native coverage, not visible ownership:
            // a later tile/text pixel may retain it over an occluded model.
            // Such artwork must not become a neural edge or blur its neighbours.
            bool visibleModel=neighborTag==0u || neighborTag==3u || neighborTag==4u || neighborTag==5u;
            if((neighbor&0x02000000u)!=0 && visibleModel && (neighbor&0x08000000u)==0)
                artwork=false;
        }
    }
    result[id.xy]=(hud || artwork)?original.Load(int3(id.xy,0)):reconstructed.Load(int3(id.xy,0));
}
