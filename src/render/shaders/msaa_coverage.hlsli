static const float2 msaaOffsets[14]={
#define SF_MSAA_SAMPLE(x,y) float2(x,y)/16.0,
#include "../../../include/starfox/render/msaa_samples.inc"
#undef SF_MSAA_SAMPLE
};
float msaaEdge(float2 a,float2 b,float2 p) {
    bool flip=a.y>b.y || (a.y==b.y && a.x>b.x);
    if(flip) {float2 swap=a;a=b;b=swap;}
    precise float value=(b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
    return flip?-value:value;
}
bool msaaTopLeft(float2 a,float2 b) {return b.y<a.y || (b.y==a.y && b.x>a.x);}
uint msaaCoverage(float2 a,float2 b,float2 c,uint2 pixel,uint samples) {
    if(samples!=2 && samples!=4 && samples!=8) return 0;
    if(!all(isfinite(a)) || !all(isfinite(b)) || !all(isfinite(c))) return 0;
    // Every sample lies inside its pixel. Reject disjoint pixel/triangle boxes
    // before winding and per-sample edge work; inclusive bounds retain ties.
    float2 lower=min(a,min(b,c)),upper=max(a,max(b,c));
    if(any(float2(pixel)+1<lower) || any(float2(pixel)>upper)) return 0;
    float area=msaaEdge(a,b,c);if(area==0 || !isfinite(area)) return 0;
    if(area<0) {float2 swap=b;b=c;c=swap;}
    uint mask=0;
    for(uint sample=0;sample<samples;++sample) {
        float2 p=float2(pixel)+.5+msaaOffsets[samples-2+sample];
        float3 e=float3(msaaEdge(a,b,p),msaaEdge(b,c,p),msaaEdge(c,a,p));
        bool3 owns=bool3(msaaTopLeft(a,b),msaaTopLeft(b,c),msaaTopLeft(c,a));
        if(all((e>0) | ((e==0)&owns))) mask|=1U<<sample;
    }
    return mask;
}
