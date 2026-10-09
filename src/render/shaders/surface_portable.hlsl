struct Command {
    int4 bounds;uint4 colour;uint4 texture;int4 uv;
    float4 surface;uint4 flags;
};
struct CameraPoint {int4 camera;int4 screen;};
[[vk::binding(0,0)]] StructuredBuffer<CameraPoint> points : register(t0,space0);
[[vk::binding(1,0)]] StructuredBuffer<uint4> corners : register(t1,space0);
[[vk::binding(2,0)]] StructuredBuffer<uint4> polygons : register(t2,space0);
[[vk::binding(3,0)]] StructuredBuffer<Command> materials : register(t3,space0);
[[vk::binding(4,0)]] StructuredBuffer<int4> normals : register(t4,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<Command> outputMaterials : register(u0,space1);
[[vk::binding(1,1)]] RWStructuredBuffer<float4> geometryPlanes : register(u1,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint count,pointCount,cornerCount,fractionalCamera;
    int4 row0,row1,row2;
    uint fractionalNormal,wantGeometry;uint2 padding;
};
int word(int value){return (value<<16)>>16;}
int q15(int a,int b){return word((word(a)*word(b))>>15);}
[numthreads(32,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=count) return;
    Command material=materials[id.x];material.flags.x=0;material.surface=float4(0,0,1,0);
    outputMaterials[id.x]=material;
    if(wantGeometry!=0) geometryPlanes[id.x]=0;
    uint4 face=polygons[id.x];
    if(face.y<3 || face.y>32 || face.x>cornerCount || face.y>cornerCount-face.x) return;
    // Compensated accumulation keeps mean depth stable for fractional models.
    precise float sum=0,correction=0;
    float3 positions[32];
    bool validPositions=true;
    for(uint i=0;i<face.y;++i) {
        uint index=corners[face.x+i].x;if(index>=pointCount) return;
        int4 raw=points[index].camera;
        float depth=fractionalCamera!=0?asfloat(raw.z):float(raw.z);
        if(!isfinite(depth) || (fractionalCamera!=0?asfloat(raw.w)<0:raw.w!=0)) return;
        if(wantGeometry!=0) {
            positions[i]=fractionalCamera!=0?asfloat(raw.xyz):float3(raw.xyz);
            validPositions=validPositions && all(isfinite(positions[i]));
        }
        precise float next=sum+depth;
        precise float b=next-sum;
        correction+=(sum-(next-b))+(depth-b);sum=next;
    }
    int3 normal=normals[id.x].xyz;
    float3 rotated;
    if(fractionalNormal!=0) {
        precise float3 value=float(normal.x)*asfloat(row0.xyz)+float(normal.y)*asfloat(row1.xyz)+float(normal.z)*asfloat(row2.xyz);
        rotated=value;
    } else {
        for(uint c=0;c<3;++c) rotated[c]=float(word(q15(normal.x,row0[c])+q15(normal.y,row1[c])+q15(normal.z,row2[c])));
    }
    float lengthNormal=length(rotated);
    if(!isfinite(lengthNormal)) return;
    if(lengthNormal>0.0001) material.surface.xyz=rotated/lengthNormal;
    material.surface.w=(sum+correction)/float(face.y);material.flags.x=1;
    if(wantGeometry!=0) {
        // Preserve the original face ID through BSP ordering and row-span
        // expansion without changing the 96-byte command ABI. Low bit remains
        // the legacy surface flag. Zero plane means no trustworthy depth.
        material.flags.x|=(id.x+1)<<1;
        float3 normalPlane=0;
        float largest=0;
        for(uint j=1;validPositions && j+1<face.y;++j) {
            float3 candidate=cross(positions[j]-positions[0],positions[j+1]-positions[0]);
            float area=dot(candidate,candidate);
            if(isfinite(area) && area>largest) {largest=area;normalPlane=candidate;}
        }
        if(largest>1e-12) {
            normalPlane/=sqrt(largest);
            float distance=dot(normalPlane,positions[0]);
            bool planar=isfinite(distance);
            for(uint k=1;k<face.y;++k) {
                float3 delta=positions[k]-positions[0];
                // Only float roundoff is tolerated, not genuinely folded faces.
                planar=planar && abs(dot(normalPlane,delta))<=.001+length(delta)*2e-6;
            }
            if(planar) geometryPlanes[id.x]=float4(normalPlane,distance);
        }
    }
    outputMaterials[id.x]=material;
}
