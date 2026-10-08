// Native screen clipping, preserving MCLIP's inside-anchored signed division.
// Corners: projected point index, signed-word U/V, reserved. Descriptor:
// first corner, corner count, visibility index, reserved. Result per polygon:
// header int4(count,status,0,0), followed by up to 128 int4(x,y,u,v).
// Status 1 is invalid input, 2 capacity exceeded. Caller must not render either.
#ifndef STARFOX_CLIP_SOURCE_CORNERS
#define STARFOX_CLIP_SOURCE_CORNERS 32
#endif
#ifndef STARFOX_CLIP_CAPACITY
#define STARFOX_CLIP_CAPACITY 128
#endif
[[vk::binding(0,0)]] StructuredBuffer<int4> points : register(t0,space0);
[[vk::binding(1,0)]] StructuredBuffer<uint4> corners : register(t1,space0);
[[vk::binding(2,0)]] StructuredBuffer<uint4> polygons : register(t2,space0);
[[vk::binding(3,0)]] StructuredBuffer<uint> visibility : register(t3,space0);
struct CameraPoint {int4 coordinate;int4 vanish;};
[[vk::binding(4,0)]] StructuredBuffer<CameraPoint> cameraPoints : register(t4,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<int4> clipped : register(u0,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint polygonCount; uint pointCount; uint cornerCount; uint visibilityCount;
    int width; int height; uint cameraCount;uint padding;
};
int word(int v) {return (v<<16)>>16;}
int midpoint(int a,int b) {int sum=word(a+b);return sum==-1?0:sum>>1;}
int3 nearIntersection(int3 a,int3 b) {
    if(a.z<0 && b.z>=0) {int3 swap=a;a=b;b=swap;}
    for(uint iteration=0;iteration<32;++iteration) {
        int3 m=int3(midpoint(a.x,b.x),midpoint(a.y,b.y),midpoint(a.z,b.z));
        if(m.z==0) return m;
        if(m.z<0) {if(all(m==b)) break;b=m;}
        else {if(all(m==a)) break;a=m;}
    }
    if(a.z==0) return a;if(b.z==0) return b;
    int denominator=a.z-b.z;
    if(denominator==0) return int3(a.xy,0);
    return int3((b.x*a.z-a.x*b.z)/denominator,(b.y*a.z-a.y*b.z)/denominator,0);
}
int2 projectWord(int3 p,int2 vanish) {
    uint2 magnitude=uint2(abs(p.xy));uint depth=max(1U,uint(abs(p.z)));
    uint dominant=max(magnitude.x,magnitude.y);uint2 xy=0;
    if(dominant!=0) {
        if((dominant<<8)/depth>=16384U) {
            uint minor=min(magnitude.x,magnitude.y)*16383U/dominant;
            xy=magnitude.x>=magnitude.y?uint2(16383,minor):uint2(minor,16383);
        } else xy=(magnitude<<8)/depth;
    }
    int x=((p.x<0)!=(p.z<0))?-int(xy.x):int(xy.x);
    int y=((p.y<0)!=(p.z<0))?-int(xy.y):int(xy.y);
    return int2(word(x+word(vanish.x)),word(y+word(vanish.y)));
}
int source_value(int anchor,int outside,int axis,int other,int boundary) {
    int denominator=word(other-axis);
    if(denominator==0) return word(anchor);
    int numerator=word(boundary-axis),difference=word(outside-anchor);
    return word(anchor+(numerator*difference)/denominator);
}
[numthreads(32,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    if(id.x>=polygonCount) return;
    uint base=id.x*129U;
    clipped[base]=0;
    uint4 descriptor=polygons[id.x];
    if(descriptor.z>=visibilityCount || descriptor.x>cornerCount
        || descriptor.y>cornerCount-descriptor.x || descriptor.y>STARFOX_CLIP_SOURCE_CORNERS) {
        clipped[base]=int4(0,1,0,0);return;
    }
    bool isLine=(descriptor.w&2U)!=0 && descriptor.y==2;
    if((descriptor.w&5U)==5U && descriptor.y==1 && visibility[descriptor.z]!=0) {
        uint index=corners[descriptor.x].x;
        if(index>=pointCount) {clipped[base]=int4(0,1,0,0);return;}
        int4 p=points[index];
        if(p.w>=0 && p.z>=32) {clipped[base+1]=p;clipped[base]=int4(1,0,2,0);}
        return;
    }
    if(visibility[descriptor.z]==0 || (descriptor.y<3 && !isLine)) return;
    int4 work[STARFOX_CLIP_CAPACITY],scratch[STARFOX_CLIP_CAPACITY];uint size=descriptor.y;bool behind=false;uint frontCount=0;
    for(uint i=0;i<size;++i) {
        uint4 corner=corners[descriptor.x+i];
        if(corner.x>=pointCount) {clipped[base]=int4(0,1,0,0);return;}
        int4 p=points[corner.x];
        if(p.w<0) {clipped[base]=int4(0,1,0,0);return;}
        behind=behind || p.z<0;frontCount+=p.z>=0?1U:0U;
        work[i]=int4(word(p.x),word(p.y),word(asint(corner.y)),word(asint(corner.z)));
    }
    if(behind) {
        if((descriptor.w&1U)!=0 || frontCount==0) return;
        int3 camera[STARFOX_CLIP_SOURCE_CORNERS];int2 vanish=0;
        for(uint i=0;i<size;++i) {
            uint index=corners[descriptor.x+i].x;
            if(index>=cameraCount) {clipped[base]=int4(0,3,0,0);return;}
            CameraPoint p=cameraPoints[index];
            if(p.coordinate.w!=0) {clipped[base]=int4(0,1,0,0);return;}
            camera[i]=int3(word(p.coordinate.x),word(p.coordinate.y),word(p.coordinate.z));
            if(i==0) vanish=p.vanish.xy;
        }
        uint nextSize=0;
        if(isLine) {
            scratch[0]=int4(camera[0],0);scratch[1]=int4(camera[1],0);nextSize=2;
            if(camera[0].z<0) scratch[0]=int4(nearIntersection(camera[0],camera[1]),0);
            if(camera[1].z<0) scratch[1]=int4(nearIntersection(camera[0],camera[1]),0);
        } else for(uint i=0;i<size;++i) {
            int3 a=camera[i],b=camera[(i+1)%size];
            if(a.z>=0) scratch[nextSize++]=int4(a,0);
            if((a.z>=0)!=(b.z>=0)) scratch[nextSize++]=int4(nearIntersection(a,b),0);
        }
        size=nextSize;
        for(uint i=0;i<size;++i) work[i]=int4(projectWord(scratch[i].xyz,vanish),0,0);
    }
    if((descriptor.w&8U)!=0 && !isLine) {
        // Exact signed 64-bit area using two 32-bit words. Each source-word
        // coordinate product fits int32; the polygon sum need not.
        uint2 area=0;
        for(uint i=0;i<size;++i) {
            int2 a=work[i].xy,b=work[(i+1)%size].xy;
            int terms[2]={a.x*b.y,-b.x*a.y};
            for(uint term=0;term<2;++term) {
                uint previous=area.x;area.x+=asuint(terms[term]);
                area.y+=(terms[term]<0?0xffffffffU:0U)+uint(area.x<previous);
            }
        }
        if(asint(area.y)>=0)return;
    }
    if(isLine) {
        for(uint plane=0;plane<4;++plane) {
            uint axis=plane/2;int boundary=(plane&1U)==0?0:(axis==0?width:height);
            bool less=(plane&1U)!=0;
            bool a=less?work[0][axis]<boundary:work[0][axis]>=boundary;
            bool b=less?work[1][axis]<boundary:work[1][axis]>=boundary;
            if(!a && !b) return;
            if(a!=b) {
                uint outside=a?1:0,anchor=1-outside;uint other=1-axis;
                work[outside][other]=source_value(work[anchor][other],work[outside][other],work[anchor][axis],work[outside][axis],boundary);
                work[outside][axis]=boundary;
            }
        }
        clipped[base+1]=work[0];clipped[base+2]=work[1];clipped[base]=int4(2,0,1,0);return;
    }
    for(uint plane=0;plane<4 && size>0;++plane) {
        uint axis=plane<2?0:1;
        int boundary=(plane&1U)==0?0:(axis==0?width:height);
        bool keepLess=(plane&1U)!=0;
        uint nextSize=0;int4 previous=work[size-1];
        bool previousInside=keepLess?previous[axis]<boundary:previous[axis]>=boundary;
        for(uint i=0;i<size;++i) {
            int4 current=work[i];
            bool inside=keepLess?current[axis]<boundary:current[axis]>=boundary;
            if(inside!=previousInside) {
                int4 anchor=previousInside?previous:current;
                int4 outside=previousInside?current:previous;
                int4 intersection;
                for(uint component=0;component<4;++component)
                    intersection[component]=component==axis?boundary:source_value(
                        anchor[component],outside[component],anchor[axis],outside[axis],boundary);
                if(nextSize>=STARFOX_CLIP_CAPACITY) {clipped[base]=int4(0,2,0,0);return;}
                scratch[nextSize++]=intersection;
            }
            if(inside) {
                if(nextSize>=STARFOX_CLIP_CAPACITY) {clipped[base]=int4(0,2,0,0);return;}
                scratch[nextSize++]=current;
            }
            previous=current;previousInside=inside;
        }
        size=nextSize;
        for(uint i=0;i<size;++i) work[i]=scratch[i];
    }
    for(uint i=0;i<size;++i) clipped[base+1+i]=work[i];
    clipped[base]=int4(size,0,0,0);
}
