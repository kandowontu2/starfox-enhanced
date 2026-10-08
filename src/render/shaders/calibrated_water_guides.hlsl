Texture2D<float4> ownership : register(t0,space2);SamplerState ownershipSampler : register(s0,space2);
Texture2D<float4> surfaces : register(t1,space2);SamplerState surfaceSampler : register(s1,space2);
#if defined(STARFOX_WATER_PANEL_GUIDES)
cbuffer Settings : register(b0,space3) {
    uint width,height,padding0,padding1;
    float4 projection; // focal X/Y, centre X/Y
    float4 planePoint,planeNormal;
    float4 worldRow0,worldRow1,worldRow2;
    float4 depthTime; // projection Z/W, retained wave time
};
float panel_water_band(float footprint,float frequency) {
    float f=footprint*frequency;return 1./(1.+f*f*f*f);
}
// Reuse the native axis shader's compensated-float arithmetic: a rounded
// normalized ray loses depth precision where the plane denominator nearly
// cancels at the horizon. This needs neither shaderFloat64 nor a CPU image.
precise float2 panel_sum(float a,float b) {precise float s=a+b,v=s-a;return float2(s,(a-(s-v))+(b-v));}
precise float2 panel_add(float2 a,float2 b) {precise float2 s=panel_sum(a.x,b.x);return panel_sum(s.x,s.y+a.y+b.y);}
precise float2 panel_product(float a,float b) {
    precise float ca=4097*a,cb=4097*b,ah=ca-(ca-a),bh=cb-(cb-b),al=a-ah,bl=b-bh;
    precise float p=a*b;return float2(p,((ah*bh-p)+ah*bl+al*bh)+al*bl);
}
precise float2 panel_multiply(float2 a,float2 b) {
    precise float2 p=panel_product(a.x,b.x);return panel_sum(p.x,p.y+a.x*b.y+a.y*b.x+a.y*b.y);
}
precise float2 panel_divide(float2 a,float2 b) {
    precise float q=a.x/b.x;precise float2 r=panel_add(a,-panel_multiply(float2(q,0),b));
    return panel_sum(q,(r.x+r.y)/b.x);
}
#else
ByteAddressBuffer water : register(t2,space2);
cbuffer Settings : register(b0,space3) {uint width,height,surfaceOffset,hasSurface;};
#endif
struct Fullscreen {float4 position : SV_Position;};
struct Guides {float4 ownership : SV_Target0;float4 surface : SV_Target1;};
Guides water_guides_fragment_main(Fullscreen input) {
    uint2 p=uint2(input.position.xy);if(p.x>=width || p.y>=height) discard;
    Guides output;output.ownership=ownership.Load(int3(p,0));
    #if defined(STARFOX_WATER_PANEL_GUIDES)
    output.surface=surfaces.Load(int3(p,0));
    uint layer=uint(round(output.ownership.b*255));
    if((layer!=1 && layer!=2) || output.ownership.r<=0) return output;
    precise float2 x=panel_divide(panel_sum(float(p.x)+.5,-projection.z),float2(projection.x,0));
    precise float2 y=panel_divide(panel_sum(float(p.y)+.5,-projection.w),float2(projection.y,0));
    float3 direction=float3(x.x+x.y,y.x+y.y,1);
    float rayLength=length(direction);float3 ray=direction/rayLength;
    precise float2 denominatorPair=panel_add(panel_add(panel_multiply(x,float2(planeNormal.x,0)),
        panel_multiply(y,float2(planeNormal.y,0))),float2(planeNormal.z,0));
    float denominator=(denominatorPair.x+denominatorPair.y)/rayLength;
    if(abs(denominator)<=1.e-8) return output;
    precise float2 numerator=panel_add(panel_add(panel_product(planePoint.x,planeNormal.x),
        panel_product(planePoint.y,planeNormal.y)),panel_product(planePoint.z,planeNormal.z));
    precise float2 forward=panel_divide(numerator,denominatorPair);
    float z=forward.x+forward.y;float distance=z*rayLength;
    float ndc=-depthTime.x+depthTime.y*256./z;
    if(!isfinite(distance) || distance<=0 || z>=65536 || !isfinite(ndc) || ndc<0 || ndc>=1
        || (output.ownership.r>1./255. && output.surface.w>0 && z>output.surface.w)) return output;
    precise float2 hitX=panel_multiply(x,forward),hitY=panel_multiply(y,forward);
    float3 hit=float3(hitX.x+hitX.y,hitY.x+hitY.y,z);
    float3x3 rotation=float3x3(worldRow0.xyz,worldRow1.xyz,worldRow2.xyz);
    float3 world=mul(hit,rotation)+float3(worldRow0.w,worldRow1.w,worldRow2.w);
    float footprint=distance/max(projection.x,1.)/max(abs(denominator),.04);
    float t=depthTime.z;
    float dx=.055*cos(world.x*.018+world.z*.011-t*.8)*panel_water_band(footprint,.022)
        +.025*cos(world.x*.047-world.z*.025+t*1.2)*panel_water_band(footprint,.054);
    float dz=.045*cos(world.z*.022-world.x*.009-t*.65)*panel_water_band(footprint,.024)
        -.020*cos(world.x*.047-world.z*.025+t*1.2)*panel_water_band(footprint,.054);
    float3 normal=normalize(mul(rotation,float3(dx,-1,dz)));
    if(dot(normal,ray)>0) normal=-normal;
    output.ownership=float4(1./255.,1,1./255.,output.ownership.a);
    output.surface=float4(normal,z);
    #else
    output.surface=hasSurface!=0?surfaces.Load(int3(p,0)):float4(0,0,0,0);
    uint layer=uint(round(output.ownership.b*255));uint index=p.y*width+p.x;
    if((layer==1 || layer==2) && output.ownership.r>0 && (water.Load(index*4)>>24)==253U) {
        float4 liquid=asfloat(water.Load4(surfaceOffset+index*16));
        float lengthSquared=dot(liquid.xyz,liquid.xyz);
        if(all(isfinite(liquid)) && liquid.w>0 && lengthSquared>.5 && lengthSquared<1.5) {
            // Physical world receiver, not the submerged model's material or
            // style. Preserve AA eligibility, including protected source ink.
            output.ownership=float4(1./255.,1,1./255.,output.ownership.a);
            output.surface=liquid;
        }
    }
    #endif
    return output;
}
