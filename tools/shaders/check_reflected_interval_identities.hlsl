// Actual shared interval helper contracts, not optical/source/RGB acceptance.
#include "../../src/render/shaders/reflection_source_optical_interval.hlsli"
RWByteAddressBuffer output:register(u0,space1);
[numthreads(16,1,1)]
void interval_identity_main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=16)return;
    const uint raw[16]={0,0x80000000U,0x00800000U,0x80800000U,1,0x80000001U,
        0x3f800000U,0xbf800000U,0x3dcccccdU,0xbdcccccdU,0x2f800000U,0xaf800000U,
        0x501502f9U,0xd01502f9U,0x5d800000U,0xdd800000U};
    const Interval x=ip(asfloat(raw[id.x]));const uint at=id.x*144;
    Interval values[16];
    values[0]=iadd(x,ip(0));values[1]=iadd(ip(0),x);
    values[2]=imul(x,ip(0));values[3]=imul(ip(0),x);
    values[4]=imul(x,ip(1));values[5]=imul(ip(1),x);
    values[6]=imul(x,ip(-1));values[7]=imul(ip(-1),x);
    values[8]=idiv(x,ip(1));values[9]=idiv(x,ip(-1));
    values[10]=idiv(ip(0),iv(2,4));values[11]=idiv(ip(0),iv(-4,-2));
    values[12]=iadd(iv(-2,3),ip(0));values[13]=imul(iv(-2,3),ip(1));
    values[14]=imul(iv(-2,3),ip(0));values[15]=idiv(iv(-2,3),ip(-1));
    const uint validFailed=uint(intervalFailed);
    [unroll] for(uint n=0;n<16;++n)output.Store2(at+16+n*8,asuint(float2(values[n].lo,values[n].hi)));
    uint invalidMask=0;intervalFailed=false;
    Interval bad=iadd(ip(asfloat(0x7f800000U)),ip(0));invalidMask|=uint(intervalFailed);
    intervalFailed=false;bad=imul(ip(asfloat(0x7fc00000U)),ip(0));invalidMask|=uint(intervalFailed)<<1;
    intervalFailed=false;bad=imul(ip(1),ip(asfloat(0x7f800000U)));invalidMask|=uint(intervalFailed)<<2;
    intervalFailed=false;bad=idiv(ip(asfloat(0x7f800000U)),ip(1));invalidMask|=uint(intervalFailed)<<3;
    intervalFailed=false;bad=idiv(ip(0),iv(-1,1));const uint dividedByZero=uint(intervalFailed);
    output.Store4(at,uint4(id.x,validFailed,invalidMask,dividedByZero));
}
