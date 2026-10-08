struct Pose { int4 row0; int4 row1; int4 row2; int4 translation; int4 vanish; };
struct SourcePoint { int4 coordinate; int4 vanish; };
[[vk::binding(0,0)]] StructuredBuffer<int4> vertices : register(t0,space0);
#ifndef SF_INLINE_MODEL_POSES
[[vk::binding(1,0)]] StructuredBuffer<Pose> poses : register(t1,space0);
#endif
[[vk::binding(0,1)]] RWStructuredBuffer<SourcePoint> transformed : register(u0,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint count; uint poseCount; uint2 padding;
#ifdef SF_INLINE_MODEL_POSES
    Pose poses[1];
#endif
};
int word(int value) { return (value << 16) >> 16; }
int q15(int a,int b) { return word((word(a)*word(b)) >> 15); }
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    int4 v=vertices[id.x];
    SourcePoint result;
    result.coordinate=int4(0,0,0,1);result.vanish=0;
    if(asuint(v.w)<poseCount) {
        Pose p=poses[asuint(v.w)];
        int3 rotated;
        for(uint c=0;c<3;++c)
            rotated[c]=word(q15(v.x,p.row0[c])+q15(v.y,p.row1[c])+q15(v.z,p.row2[c])+word(p.translation[c]));
        if(p.vanish.w==0x455850 && p.vanish.z>=0 && p.vanish.z<=255) {
            int3 direction;
            for(uint axis=0;axis<3;++axis)
                direction[axis]=word(q15(-p.row0.w,p.row0[axis])+q15(p.row1.w,p.row1[axis])+q15(-p.row2.w,p.row2[axis]));
            direction.y=-abs(direction.y);
            // Do not word-wrap the fragment offset here: near clipping consumes
            // the expanded camera coordinates before source projection wraps.
            rotated+=(direction*p.vanish.z)>>2;
        }
        result.coordinate=int4(rotated,0);result.vanish=p.vanish;
    }
    transformed[id.x]=result;
}
