// Source 15x15 lattice projection. Keep offscreen points until eye projection.
[[vk::binding(0,1)]] RWStructuredBuffer<int4> points : register(u0,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    int4 origin;
    int4 xStep;
    int4 zStep;
    uint width,height;
    float eyeX,convergence;
};
int word(int x) {return (x<<16)>>16;}
int projectSource(int x,int depth) {
    int reciprocal=word((32767*256)/(depth&~1));
    return word((x*reciprocal)>>15);
}
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=225) return;
    int3 p=origin.xyz+xStep.xyz*int(id.x%15)+zStep.xyz*int(id.x/15);
    p=int3(word(p.x),word(p.y),word(p.z));
    points[id.x]=int4(0,0,p.z,0);
    if(p.z<=256) return;
    int depth=min(p.z,12287);
    int x=word(projectSource(p.x,depth)+int(width/2));
    int y=word(projectSource(p.y,depth)+int(height/2));
    if(eyeX!=0 && convergence>0)
        x=int(floor(float(x)+256.0*eyeX*(1.0/convergence-1.0/float(depth))+.5));
    if(x<0 || y<0 || x>=int(width) || y>=int(height)) return;
    points[id.x]=int4(x,y,p.z,1);
}
