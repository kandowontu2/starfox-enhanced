// Average already-transformed extrema in authored order. Averaging before
// source-word rotation is not equivalent because each vertex wraps separately.
#include "geometry_fp64.hlsli"
struct Point { uint4 camera; uint4 other; };
struct Result { float4 camera; float4 screen; };
precise float2 sum2(float a,float b) {precise float s=a+b,v=s-a;return float2(s,(a-(s-v))+(b-v));}
precise float2 add2(float2 a,float2 b) {precise float2 s=sum2(a.x,b.x);return sum2(s.x,s.y+a.y+b.y);}
precise float2 product2(float a,float b) {
    precise float ca=4097*a,cb=4097*b,ah=ca-(ca-a),bh=cb-(cb-b),al=a-ah,bl=b-bh;
    precise float p=a*b;return float2(p,((ah*bh-p)+ah*bl+al*bh)+al*bl);
}
precise float2 multiply2(float2 a,float2 b) {precise float2 p=product2(a.x,b.x);return sum2(p.x,p.y+a.x*b.y+a.y*b.x+a.y*b.y);}
precise float2 divide2(float2 a,float2 b) {precise float q=a.x/b.x;precise float2 r=add2(a,-multiply2(float2(q,0),b));return sum2(q,(r.x+r.y)/b.x);}
[[vk::binding(0,0)]] StructuredBuffer<Point> points : register(t0,space0);
[[vk::binding(1,0)]] StructuredBuffer<uint> indices : register(t1,space0);
[[vk::binding(2,0)]] StructuredBuffer<Result> residuals : register(t2,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<Result> centres : register(u0,space1);
[[vk::binding(1,1)]] RWStructuredBuffer<Result> centreResiduals : register(u1,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint pointCount,indexCount,fractional,hasResiduals;
    uint4 ranges;
    float4 projection;
};
int word(int value){return (value<<16)>>16;}
int meanWord(int total,uint count){
    uint magnitude=total<0?0U-asuint(total):uint(total);
    uint whole=magnitude/count,remainder=magnitude%count;
    if(remainder*2U>=count)++whole;
    return word(total<0?-int(whole):int(whole));
}
// Native mode returns camera/vanish records for the existing word projector.
// A single lane handles both endpoints so near clipping happens before output.
void nativeAxis(){
    Result invalid;invalid.camera=asfloat(int4(0,0,0,1));invalid.screen=0;
    centres[0]=centres[1]=invalid;centreResiduals[0]=centreResiduals[1]=invalid;
    int3 means[2];bool behind[2];
    for(uint group=0;group<2;++group){
        uint first=ranges[group*2],count=ranges[group*2+1];
        if(count==0 || count>65536 || first>indexCount || count>indexCount-first)return;
        int3 total=0;
        for(uint i=0;i<count;++i){
            uint index=indices[first+i];if(index>=pointCount)return;
            int4 camera=asint(points[index].camera);if(camera.w!=0)return;
            if(any(camera.xyz < -32768) || any(camera.xyz > 32767))return;
            total+=camera.xyz;
        }
        behind[group]=total.z<0;
        means[group]=int3(meanWord(total.x,count),meanWord(total.y,count),meanWord(total.z,count));
    }
    if(behind[0] && behind[1])return;
    if(behind[0] || behind[1]){
        uint changed=behind[0]?0U:1U;
        int3 a=means[0],b=means[1];
        if(a.z<0 && b.z>=0){int3 swap=a;a=b;b=swap;}
        int3 intersection=0;bool found=false;
        for(uint iteration=0;iteration<32;++iteration){
            int3 sum=int3(word(a.x+b.x),word(a.y+b.y),word(a.z+b.z));
            int3 mid=int3(sum.x==-1?0:sum.x>>1,sum.y==-1?0:sum.y>>1,sum.z==-1?0:sum.z>>1);
            if(mid.z==0){intersection=mid;found=true;break;}
            if(mid.z<0){if(all(mid==b))break;b=mid;}
            else {if(all(mid==a))break;a=mid;}
        }
        if(!found){
            if(a.z==0)intersection=a;
            else if(b.z==0)intersection=b;
            else {
                float2 amount=divide2(float2(a.z,0),float2(a.z-b.z,0));
                for(uint c=0;c<2;++c){float2 value=add2(float2(a[c],0),multiply2(float2(b[c]-a[c],0),amount));intersection[c]=int(value.x+value.y);}
                intersection.z=0;
            }
        }
        means[changed]=intersection;
    }
    for(uint group=0;group<2;++group){
        Result output;output.camera=asfloat(int4(means[group],0));
        int vx=word(int(projection.x>=0?floor(projection.x+.5):ceil(projection.x-.5)));
        int vy=word(int(projection.y>=0?floor(projection.y+.5):ceil(projection.y-.5)));
        output.screen=asfloat(int4(vx,vy,0,0));centres[group]=output;
    }
}
[numthreads(2,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=2) return;
    if(fractional==2){if(id.x==0)nativeAxis();return;}
    Result result;result.camera=result.screen=float4(0,0,0,-1);centres[id.x]=result;
    centreResiduals[id.x]=result;
    uint first=ranges[id.x*2],count=ranges[id.x*2+1];
    if(count==0 || count>65536 || first>indexCount || count>indexCount-first) return;
    precise float3 sum=0,tail=0;
    Sf64 exactSum[3];for(uint c=0;c<3;++c)exactSum[c]=sf_make(0,0);
    bool exactInputs=false;
    for(uint i=0;i<count;++i) {
        uint index=indices[first+i];if(index>=pointCount) return;
        uint4 raw=points[index].camera;
        float3 value=fractional!=0?asfloat(raw.xyz):float3(asint(raw.xyz));
        if((fractional!=0?asfloat(raw.w)<0:raw.w!=0) || !all(isfinite(value))) return;
        precise float3 next=sum+value,b=next-sum;
        tail+=(sum-(next-b))+(value-b);sum=next;
        if(hasResiduals!=0) {
            float4 low=residuals[index].camera;
            bool rawCamera=low.w==3;
            if(i==0)exactInputs=rawCamera;
            if(rawCamera!=exactInputs)return;
            if(rawCamera) {
                for(uint c=0;c<3;++c) {
                    Sf64 exact=sf_make(asuint(low[c]),asuint(residuals[index].screen[c]));
                    if(!sf_valid(exact))return;
                    exactSum[c]=sf_add(exactSum[c],exact);
                }
                continue;
            }
            if(low.w<0 || !all(isfinite(low))) return;
            for(uint c=0;c<3;++c) {
                precise float2 combined=add2(float2(sum[c],tail[c]),float2(low[c],0));
                sum[c]=combined.x;tail[c]=combined.y;
            }
        }
    }
    precise float3 mean=sum/float(count)+tail/float(count);
    // Projection of a mean equals projection of the summed coordinates, so
    // retain accumulation precision rather than re-projecting narrowed means.
    precise float depth=sum.z+tail.z;
    precise float divisor=depth==0?float(count):depth;
    precise float2 screen=projection.xy+(sum.xy+tail.xy)*projection.z/divisor;
    Result lows;lows.camera=float4(0,0,0,1);lows.screen=float4(screen,0,0);
    if(hasResiduals!=0) {
        Sf64 exactMean[3];
        for(uint c=0;c<3;++c) {
            precise float2 value=divide2(float2(sum[c],tail[c]),float2(float(count),0));
            exactMean[c]=sf_add(sf_from_float_bits(asuint(value.x)),sf_from_float_bits(asuint(value.y)));
            if(exactInputs) {
                exactMean[c]=sf_div(exactSum[c],sf_from_float_bits(asuint(float(count))));
                if(!sf_valid(exactMean[c]))return;
                value.x=asfloat(sf_to_float_bits(exactMean[c]));
                value.y=asfloat(sf_to_float_bits(sf_sub(exactMean[c],sf_from_float_bits(asuint(value.x)))));
            }
            mean[c]=value.x;lows.camera[c]=value.y;
        }
        Sf64 z=exactMean[2];
        if(sf_zero(z)) z=sf_from_float_bits(asuint(1.f));
        // Keep every projected binary64 bit through the clipper handoff.
        // A high/low float pair loses boundary-significant low bits.
        uint4 rawScreen=0;
        for(uint c=0;c<2;++c) {
            Sf64 coordinate=exactMean[c];
            Sf64 value=sf_add(sf_from_float_bits(asuint(projection[c])),sf_div(sf_mul(coordinate,sf_from_float_bits(asuint(projection.z))),z));
            if(!sf_valid(value)) return;
            screen[c]=asfloat(sf_to_float_bits(value));
            rawScreen[c*2]=value.lo;rawScreen[c*2+1]=value.hi;
        }
        lows.camera.w=2;lows.screen=asfloat(rawScreen);
    }
    if(all(isfinite(mean)) && all(isfinite(screen))) {
        result.camera=float4(mean,1);result.screen=float4(screen,mean.z,mean.z>=0?1:0);
        centres[id.x]=result;
        centreResiduals[id.x]=lows;
    }
}
