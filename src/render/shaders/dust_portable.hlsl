// Wrapped source-relative binary64 XYZ, before matrix/projection. Source point
// recycling and palette order remain simulation-owned, not presentation-owned.
#include "geometry_fp64.hlsli"
struct Point {uint2 x,y,z,padding;};
[[vk::binding(0,0)]] StructuredBuffer<Point> sourcePoints:register(t0,space0);
[[vk::binding(1,0)]] StructuredBuffer<uint> colours:register(t1,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<int4> points:register(u0,space1);
[[vk::binding(0,2)]] cbuffer Settings:register(b0,space2) {
    int4 rowX,rowY,rowZ;
    int4 viewport; // width,height,offsetX,offsetY
    uint count; float eyeX,convergence; uint padding;
};
Sf64 number(float x) {return sf_from_float_bits(asuint(x));}
Sf64 dotSource(Point p,int3 row) {
    Sf64 x=sf_mul(sf_make(p.x.x,p.x.y),number(float(row.x)));
    Sf64 y=sf_mul(sf_make(p.y.x,p.y.y),number(float(row.y)));
    Sf64 z=sf_mul(sf_make(p.z.x,p.z.y),number(float(row.z)));
    return sf_div(sf_add(sf_add(x,y),z),number(32768));
}
int truncSource(Sf64 x) {
    int exponent=int((x.hi>>20)&2047)-1023;
    if(exponent<0) return 0;
    if(exponent>30) return (x.hi>>31)!=0?-2147483647:2147483647;
    uint magnitude=sf_right(sf_make(x.lo,(x.hi&0xfffff)|0x100000),uint(52-exponent)).lo;
    return (x.hi>>31)!=0?-int(magnitude):int(magnitude);
}
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    points[id.x]=0;
    Point p=sourcePoints[id.x];
    Sf64 z=dotSource(p,rowZ.xyz);
    if(!sf_valid(z) || (z.hi>>31)!=0 || sf_less(z,number(256))) return;
    Sf64 depth=z;if(sf_less(number(4095),z)) depth=number(4095);
    Sf64 x=sf_div(sf_mul(dotSource(p,rowX.xyz),number(256)),depth);
    Sf64 y=sf_div(sf_mul(dotSource(p,rowY.xyz),number(256)),depth);
    if(eyeX!=0 && convergence>0) {
        Sf64 disparity=sf_mul(number(256*eyeX),
            sf_sub(sf_div(number(1),number(convergence)),sf_div(number(1),depth)));
        x=sf_add(x,disparity);
    }
    int sx=viewport.x/2+viewport.z+truncSource(x);
    int sy=viewport.y/2+viewport.w+truncSource(y);
    if(sx<0 || sy<0 || sx>=viewport.x || sy>=viewport.y) return;
    uint shade=uint(clamp(truncSource(depth)>>8,0,15));
    int colour=int((112+colours[((count-id.x)&3)*16+shade])&255);
    points[id.x]=int4(sx,sy,colour,sf_less(z,number(1024))?3:1);
}
