// Native MDO_PROJECT word arithmetic. No floating-point division or hardware
// perspective convention: saturation and signed-word seams match the source.
struct SourcePoint { int4 coordinate; int4 vanish; };
[[vk::binding(0,0)]] StructuredBuffer<SourcePoint> points : register(t0, space0);
[[vk::binding(0,1)]] RWStructuredBuffer<int4> projected : register(u0, space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0, space2) { uint count; uint3 padding; };
int word(int value) { return (value << 16) >> 16; }
[numthreads(64,1,1)]
void main(uint3 id : SV_DispatchThreadID) {
    if(id.x>=count) return;
    SourcePoint source=points[id.x];
    if(source.coordinate.w!=0) { projected[id.x]=int4(0,0,0,-1); return; }
    int3 p=int3(word(source.coordinate.x),word(source.coordinate.y),word(source.coordinate.z));
    uint2 magnitude=uint2(abs(p.xy));
    uint depth=max(1U,uint(abs(p.z)));
    uint dominant=max(magnitude.x,magnitude.y);
    uint2 xy=0;
    if(dominant!=0) {
        if((dominant<<8)/depth>=16384U) {
            uint minor=min(magnitude.x,magnitude.y)*16383U/dominant;
            xy=magnitude.x>=magnitude.y?uint2(16383,minor):uint2(minor,16383);
        } else xy=(magnitude<<8)/depth;
    }
    int x=((p.x<0)!=(p.z<0))?-int(xy.x):int(xy.x);
    int y=((p.y<0)!=(p.z<0))?-int(xy.y):int(xy.y);
    projected[id.x]=int4(word(x+word(source.vanish.x)),word(y+word(source.vanish.y)),p.z,p.z>=0?1:0);
}
