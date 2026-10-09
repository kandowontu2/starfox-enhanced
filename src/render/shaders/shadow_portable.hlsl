struct Node {
    float3 low; uint begin;
    float3 high; uint count;
    uint left; uint right; uint axis; uint reserved;
};
struct Triangle { float4 origin; float4 edge1; float4 edge2; };
StructuredBuffer<Node> nodes : register(t0, space0);
StructuredBuffer<Triangle> triangles : register(t1, space0);
RWStructuredBuffer<uint> outputMask : register(u0, space1);
cbuffer Settings : register(b0, space2) {
    float4 camera;
    float4 options;
    float4 groundPoint;
    float4 groundNormal;
    float4 lights[16];
};
bool bounds_hit(Node n, float3 origin, float3 direction, float nearT, float farT) {
    for (uint axis=0; axis<3; ++axis) {
        if (abs(direction[axis])<1e-15) {
            if (origin[axis]<n.low[axis] || origin[axis]>n.high[axis]) return false;
        } else {
            float a=(n.low[axis]-origin[axis])/direction[axis];
            float b=(n.high[axis]-origin[axis])/direction[axis];
            nearT=max(nearT,min(a,b));farT=min(farT,max(a,b));
            if (nearT>farT) return false;
        }
    }
    return true;
}
bool triangle_hit(Triangle t, float3 origin, float3 direction, float nearT, inout float farT) {
    float3 p=cross(direction,t.edge2.xyz);
    float determinant=dot(t.edge1.xyz,p);
    if (abs(determinant)<=t.origin.w*1e-10) return false;
    float inverse=1/determinant;
    float3 offset=origin-t.origin.xyz;
    float u=dot(offset,p)*inverse;
    if (u<0 || u>1) return false;
    float3 q=cross(offset,t.edge1.xyz);
    float v=dot(direction,q)*inverse;
    if (v<0 || u+v>1) return false;
    float distance=dot(t.edge2.xyz,q)*inverse;
    if (!isfinite(distance) || distance<=nearT || distance>farT) return false;
    farT=distance;return true;
}
bool trace(float3 origin, float3 direction, float nearT, inout float farT, bool anyHit) {
    uint stack[64];uint size=1;stack[0]=0;bool found=false;
    while (size) {
        Node n=nodes[stack[--size]];
        if (!bounds_hit(n,origin,direction,nearT,farT)) continue;
        if (n.count) {
            for (uint i=n.begin;i<n.begin+n.count;++i)
                if (triangle_hit(triangles[i],origin,direction,nearT,farT)) {
                    if (anyHit) return true;
                    found=true;
                }
        } else {
            bool forward=direction[n.axis]>=0;
            stack[size++]=forward?n.right:n.left;
            stack[size++]=forward?n.left:n.right;
        }
    }
    return found;
}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    uint width=(uint)camera.x;
    if (id.x>=width || id.y>=(uint)camera.y) return;
    float3 ray=float3((id.x+.5-camera.w)/camera.z,(id.y+.5-options.x)/options.z,1);
    float depth=65536;bool receiver=false;
    if (options.y!=0) {
        float denominator=dot(ray,groundNormal.xyz);
        if (abs(denominator)>1e-10) {
            float d=dot(groundPoint.xyz,groundNormal.xyz)/denominator;
            if (d>1 && d<depth) {depth=d;receiver=true;}
        }
    }
    // A revealed world underlay needs its own receiver, not the model that
    // currently covers it. Secondary rays still visit the complete scene.
    if (options.w==0) receiver=trace(0,ray,1,depth,false) || receiver;
    uint blocked=0;
    if (receiver) for (uint sample=0;sample<uint(lights[0].w);++sample) {
        float distance=65536;
        blocked+=trace(ray*depth,lights[sample].xyz,max(.1,depth*1e-5),distance,true)?1:0;
    }
    outputMask[id.y*width+id.x]=160*blocked/uint(lights[0].w);
}
