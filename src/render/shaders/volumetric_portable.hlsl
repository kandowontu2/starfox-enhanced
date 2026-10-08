// Geometry-occluded homogeneous fog. Output is linear in-scattering RGB and
// transmittance A, not a display colour. The compositor owns HUD protection.
struct Node { float3 low; uint begin; float3 high; uint count; uint left; uint right; uint axis; uint reserved; };
struct Triangle { float4 origin; float4 edge1; float4 edge2; };
StructuredBuffer<Node> nodes : register(t0, space0);
StructuredBuffer<Triangle> triangles : register(t1, space0);
RWStructuredBuffer<float4> fogOutput : register(u0, space1);
cbuffer Settings : register(b0, space2) {
    float4 extent; // width, height, node count, sample count
    float4 projection; // focal x/y, centre x/y
    float4 medium; // extinction, anisotropy, maximum distance, ground enabled
    float4 albedo;
    float4 ambient;
    float4 sunlight;
    float4 light;
    float4 groundPoint;
    float4 groundNormal;
    float4 eyeOrigin; // Shared source coordinates; keep both eyes' BVH immutable.
};
bool bounds_hit(Node n,float3 origin,float3 direction,float nearT,float farT) {
    for(uint axis=0;axis<3;++axis) {
        if(abs(direction[axis])<1e-15) {
            if(origin[axis]<n.low[axis]||origin[axis]>n.high[axis]) return false;
        } else {
            float a=(n.low[axis]-origin[axis])/direction[axis];
            float b=(n.high[axis]-origin[axis])/direction[axis];
            nearT=max(nearT,min(a,b));farT=min(farT,max(a,b));
            if(nearT>farT) return false;
        }
    }
    return true;
}
bool triangle_hit(Triangle t,float3 origin,float3 direction,float nearT,inout float farT) {
    float3 p=cross(direction,t.edge2.xyz);
    float determinant=dot(t.edge1.xyz,p);
    if(abs(determinant)<=t.origin.w*1e-10) return false;
    float inverse=1/determinant;
    float3 offset=origin-t.origin.xyz;
    float u=dot(offset,p)*inverse;
    // Float projection/packed vertices can move an authored edge by a few
    // ULPs. A barycentric tolerance keeps surface depth conservative there;
    // it is dimensionless and does not inflate the ray distance or bias.
    const float edgeTolerance=2e-6;
    if(u< -edgeTolerance||u>1+edgeTolerance) return false;
    float3 q=cross(offset,t.edge1.xyz);
    float v=dot(direction,q)*inverse;
    if(v< -edgeTolerance||u+v>1+edgeTolerance) return false;
    float distance=dot(t.edge2.xyz,q)*inverse;
    if(!isfinite(distance)||distance<=nearT||distance>farT) return false;
    farT=distance;return true;
}
bool trace(float3 origin,float3 direction,float nearT,inout float farT,bool anyHit) {
    if(extent.z==0) return false;
    uint stack[64];uint size=1;stack[0]=0;bool found=false;
    while(size) {
        Node n=nodes[stack[--size]];
        if(!bounds_hit(n,origin,direction,nearT,farT)) continue;
        if(n.count) {
            for(uint i=n.begin;i<n.begin+n.count;++i)
                if(triangle_hit(triangles[i],origin,direction,nearT,farT)) {
                    if(anyHit) return true;
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
float opacity(float opticalDepth) {
    // Match -expm1 for optically thin cells without cancellation in float.
    return opticalDepth<.001?opticalDepth*(1-opticalDepth*(.5-opticalDepth/6)):1-exp(-opticalDepth);
}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=uint(extent.x)||id.y>=uint(extent.y)) return;
    uint index=id.y*uint(extent.x)+id.x;
    if(medium.x==0) {fogOutput[index]=float4(0,0,0,1);return;}
    float3 axialRay=float3((float2(id.xy)+.5-projection.zw)/projection.xy,1);
    float rayLength=length(axialRay);
    float3 ray=axialRay/rayLength;
    float depth=medium.z/rayLength;
    // Underlay fog omits model hits only for the view ray, not light rays.
    if(light.w==0) trace(eyeOrigin.xyz,axialRay,0,depth,false);
    float distance=depth*rayLength;
    if(medium.w!=0) {
        float denominator=dot(ray,groundNormal.xyz);
        if(abs(denominator)>1e-12) {
            float planeDistance=dot(groundPoint.xyz-eyeOrigin.xyz,groundNormal.xyz)/denominator;
            if(planeDistance>0) distance=min(distance,planeDistance);
        }
    }
    float g=medium.y;
    float phase=(1-g*g)/pow(1+g*g-2*g*clamp(dot(ray,light.xyz),-1,1),1.5);
    float transmittance=exp(-medium.x*distance);
    float3 scattering=0;
    if(extent.z==0||all(sunlight.xyz==0)) {
        scattering=albedo.xyz*(ambient.xyz+sunlight.xyz*phase)*opacity(medium.x*distance);
    } else {
        float step=distance/extent.w;
        float attenuation=exp(-medium.x*step),weight=opacity(medium.x*step),T=1;
        for(uint i=0;i<uint(extent.w);++i) {
            float farT=65536;
            bool blocked=trace(eyeOrigin.xyz+ray*((i+.5)*step),light.xyz,.01,farT,true);
            scattering+=albedo.xyz*(ambient.xyz+(blocked?0:sunlight.xyz*phase))*T*weight;
            T*=attenuation;
        }
    }
    fogOutput[index]=float4(scattering,transmittance);
}
