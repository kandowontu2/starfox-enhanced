// Native solid/dither and affine texture spans. Each polygon owns height
// command slots; empty rows are zero-area commands. Repeated-row EX effects
// accumulate a coverage mask rather than overwrite earlier row spans.
// Input is GpuClip's 129-int4 block.
#include "scaled_round.hlsli"
struct Command {
    int left,top,right,bottom;
    uint even,odd,dither,tag;
    uint texture_offset,u_mask,v_mask,colour_base;
    int u,v,du,dv;
    float4 surface;
    uint has_surface,textured,scroll_x,scroll_y;
};
[[vk::binding(0,0)]] StructuredBuffer<int4> clipped : register(t0,space0);
[[vk::binding(1,0)]] StructuredBuffer<Command> materials : register(t1,space0);
[[vk::binding(2,0)]] StructuredBuffer<uint> order : register(t2,space0);
[[vk::binding(3,0)]] StructuredBuffer<uint2> orderResults : register(t3,space0);
[[vk::binding(0,1)]] RWStructuredBuffer<Command> commands : register(u0,space1);
[[vk::binding(1,1)]] RWByteAddressBuffer masks : register(u1,space1);
[[vk::binding(0,2)]] cbuffer Settings : register(b0,space2) {
    uint count; int width; int height; uint windingIndependent;
    uint renderScale;uint fractional;uint orderedMode;uint polygonCount;
    uint orderFirst;uint orderTree;uint lineThickness;uint padding;
    uint maskEnabled;uint maskOffset;uint maskStride;uint maskPadding;
    float2 rasterScale;uint2 rasterPadding;
};
int word(int v){return (v<<16)>>16;}
int fixedX(int x){return width>224?x<<8:word(x<<8);}
int integerX(int x){return width>224?x>>8:int((uint(x)&65535U)>>8);}
int roundedX(int x){return width>224?(x+127)>>8:integerX(word(x+127));}
int advanceX(int x,int increment){
    if(width>224) return x+increment;
    int value=word(x+increment);
    if(value<0 && word(value+2048)>=0) value=0;
    return value;
}
int roundAway(float value){return value<0?-int(floor(-value+0.5)):int(floor(value+0.5));}
int2 pointAt(uint base,uint vertex){
    int4 raw=clipped[base+1+vertex];
    float2 scale=maskPadding!=0?rasterScale:float2(renderScale,renderScale);
    float2 value=asfloat(raw.xy)*scale;
    int2 xy=fractional!=0?int2(roundAway(value.x),roundAway(value.y)):raw.xy*int(renderScale);
    // Above 4x the float product can itself round onto a half pixel.
    if(fractional!=0 && scale.x>4) xy.x=sr_scaled_round(asfloat(raw.x),scale.x);
    if(fractional!=0 && scale.y>4) xy.y=sr_scaled_round(asfloat(raw.y),scale.y);
    return clamp(xy,int2(0,0),int2(width,height));
}
int2 uvAt(uint base,uint vertex){
    int2 raw=clipped[base+1+vertex].zw;float2 value=asfloat(raw);
    return fractional!=0?int2(roundAway(value.x),roundAway(value.y)):raw;
}
int textureAdvance(int value,int increment) {
    value=word(value+increment);
    return value<0 && word(value+2048)>=0?0:value;
}
struct Tracer {uint vertex;int direction;int x;int increment;int remaining;int2 uv;int2 uvIncrement;};
bool beginSegment(inout Tracer t,uint base,uint size,int y) {
    int rounded=roundedX(t.x);
    int2 startUV=int2((uint2(t.uv)&65535U)>>8);
    for(uint guard=0;guard<size;++guard) {
        t.vertex=t.direction>0?(t.vertex+1)%size:(t.vertex+size-1)%size;
        int2 endpoint=pointAt(base,t.vertex);int lines=endpoint.y-y;
        if(lines<0) return false;
        int2 endUV=uvAt(base,t.vertex);
        if(lines==0) {
            rounded=endpoint.x;t.x=fixedX(rounded);startUV=endUV;
            t.uv=int2(word(startUV.x<<8),word(startUV.y<<8));continue;
        }
        t.x=fixedX(rounded);
        int reciprocal=lines==1?32767:32768/lines;
        t.increment=((endpoint.x-rounded)*reciprocal)>>7;
        if(width<=224) t.increment=word(t.increment);
        t.uv=int2(word(startUV.x<<8),word(startUV.y<<8));
        int2 uvStep=((endUV-startUV)*reciprocal)>>7;
        t.uvIncrement=int2(word(uvStep.x),word(uvStep.y));
        t.remaining=lines;return true;
    }
    return false;
}
[numthreads(32,1,1)]
void main(uint3 id:SV_DispatchThreadID) {
    // GPU FAST (padding bit 0): clear every span record in its own pass,
    // eight per thread, coalesced across the group. Bit 1 tells the span
    // pass the records are already clear.
    if((padding&1U)!=0) {
        uint records=count*uint(height),group=id.x/32,lane=id.x%32;
        for(uint j=0;j<8;++j) {
            uint index=(group*8+j)*32+lane;
            if(index<records) commands[index]=(Command)0;
        }
        return;
    }
    if(id.x>=count) return;
    uint commandBase=id.x*uint(height);
    Command empty=(Command)0;
    if((padding&2U)==0) for(int row=0;row<height;++row) commands[commandBase+uint(row)]=empty;
    uint maskBase=maskOffset+id.x*uint(height)*maskStride;
    if(maskEnabled!=0) for(uint at=0;at<uint(height)*maskStride;at+=4) masks.Store(maskBase+at,0);
    uint polygon=id.x;
    if(orderedMode!=0) {
        uint2 result=orderResults[orderTree];
        if(result.y!=0 || result.x>count || id.x>=result.x) return;
        // Expanded warp geometry/materials are indexed by occurrence, not
        // authored face. Keep traversal validation without remapping twice.
        if(orderedMode==1) polygon=order[orderFirst+id.x];
    }
    if(polygon>=polygonCount) return;
    uint base=polygon*129U;
    Command material=materials[polygon];
    int4 header=clipped[base];
    if(header.y==0 && header.x==1 && header.z==2 && material.textured==1) {
        int4 raw=clipped[base+1];float3 position=asfloat(raw.xyz);
        int3 centre=fractional!=0?int3(roundAway(position.x),roundAway(position.y),roundAway(position.z)):raw.xyz;
        int depth=int(uint(centre.z)&65535U),sourceWidth=int(material.u_mask+1);
        int increment=clamp((depth*(sourceWidth==64?128:256))>>8,1,32767);
        int extent=sourceWidth*128/increment,scale=int(renderScale);
        if(maskPadding!=0) {
            int left=max(0,int(ceil((centre.x-extent)*rasterScale.x)));
            int right=min(width,int(ceil((centre.x+extent+1)*rasterScale.x)));
            int top=max(0,int(ceil((centre.y-extent)*rasterScale.y)));
            int bottom=min(height,int(ceil((centre.y+extent+1)*rasterScale.y)));
            for(int y=top;y<bottom;++y) {
                Command span=material;span.left=left;span.right=right;span.top=y;span.bottom=y+1;
                span.textured=1;span.has_surface=0;
                span.u=sourceWidth/2*256+roundAway((left/rasterScale.x-centre.x)*increment);
                span.v=word(sourceWidth/2*256+roundAway((y/rasterScale.y-centre.y)*increment));
                span.du=roundAway(increment/rasterScale.x);span.dv=0;
                commands[commandBase+uint(y)]=span;
            }
            return;
        }
        int left=max(0,centre.x-extent),right=min(width/scale-1,centre.x+extent);
        int top=max(0,centre.y-extent),bottom=min(height/scale-1,centre.y+extent);
        if(left>right || top>bottom) return;
        for(int y=top*scale;y<(bottom+1)*scale;++y) {
            Command span=material;span.left=left*scale;span.right=(right+1)*scale;span.top=y;span.bottom=y+1;
            span.textured=3;span.has_surface=0;span.u=sourceWidth/2*256+(left-centre.x)*increment;
            span.v=word(sourceWidth/2*256+(y/scale-centre.y)*increment);span.du=increment;span.dv=scale;
            commands[commandBase+uint(y)]=span;
        }
        return;
    }
    if(header.y==0 && header.x==2 && header.z==1) {
        int2 a=pointAt(base,0),b=pointAt(base,1),delta=abs(b-a),step=int2(a.x<b.x?1:-1,a.y<b.y?1:-1);
        bool majorX=delta.x>=delta.y;int major=majorX?delta.x:delta.y,minor=majorX?delta.y:delta.x;
        int error=major>>1,thickness=int(renderScale*clamp(lineThickness,1U,4U)),offset=(thickness-1)/2;
        if(maskPadding!=0) {thickness=max(1,roundAway(min(rasterScale.x,rasterScale.y)*clamp(lineThickness,1U,4U)));offset=(thickness-1)/2;}
        material.textured=0;material.has_surface=0;material.scroll_x=renderScale;material.scroll_y=0;
        for(int i=0;i<=major;++i) {
            int left=max(0,a.x-offset),right=min(width,a.x-offset+thickness);
            for(int y=max(0,a.y-offset);y<min(height,a.y-offset+thickness);++y) if(left<right) {
                uint at=commandBase+uint(y);Command previous=commands[at],span=material;
                span.left=previous.right>previous.left?min(left,previous.left):left;
                span.right=max(right,previous.right);span.top=y;span.bottom=y+1;commands[at]=span;
            }
            error-=minor;
            if(error<0){error+=major;if(majorX) a.y+=step.y;else a.x+=step.x;}
            if(majorX) a.x+=step.x;else a.y+=step.y;
        }
        return;
    }
    if(header.y!=0 || header.x<3 || header.x>128 || material.textured>1) return;
    uint size=uint(header.x),minimum=0;int maximumY=pointAt(base,0).y;
    for(uint i=1;i<size;++i) {
        int y=pointAt(base,i).y;
        if(y<pointAt(base,minimum).y) minimum=i;
        maximumY=max(maximumY,y);
    }
    int y=pointAt(base,minimum).y;
    Tracer left,right;
    left.vertex=right.vertex=minimum;left.direction=1;right.direction=-1;
    left.x=right.x=fixedX(pointAt(base,minimum).x);
    left.increment=right.increment=left.remaining=right.remaining=0;
    int2 uv=uvAt(base,minimum);
    left.uv=right.uv=int2(word(uv.x<<8),word(uv.y<<8));
    left.uvIncrement=right.uvIncrement=0;
    bool mode2Continuation=false;
    bool sparseWobble=material.textured==0 && (material.scroll_y&65536U)!=0,havePrevious=false;
    bool repeatedRow=material.textured==0 && (material.scroll_y&262144U)!=0;
    if(repeatedRow && maskEnabled==0) return;
    int previousLeft=0;
    while(y<maximumY) {
        bool leftStarts=left.remaining==0,rightStarts=right.remaining==0;
        if(left.remaining==0 && !beginSegment(left,base,size,y)) return;
        if(right.remaining==0 && !beginSegment(right,base,size,y)) return;
        int x1=integerX(left.x),x2=integerX(right.x);
        int2 nextLeft=int2(textureAdvance(left.uv.x,left.uvIncrement.x),textureAdvance(left.uv.y,left.uvIncrement.y));
        int2 nextRight=int2(textureAdvance(right.uv.x,right.uvIncrement.x),textureAdvance(right.uv.y,right.uvIncrement.y));
        int2 spanUV=left.uv,spanRight=nextRight;
        if(windingIndependent!=0 && x2<x1) {
            int swap=x1;x1=x2;x2=swap;spanUV=right.uv;spanRight=nextLeft;
        }
        if(x2>=x1 && (sparseWobble || (x2>=0 && x1<width))) {
            Command span=material;
            span.left=max(0,x1);span.right=min(width,x2+1);span.top=y;span.bottom=y+1;
            if(material.textured==1) {
                int distance=x2-x1;
                int reciprocal=distance==0?0:(distance==1?32767:32768/distance);
                int2 difference=int2(word(spanRight.x-spanUV.x),word(spanRight.y-spanUV.y));
                int2 step=(difference*reciprocal)>>15;
                span.left=x1;span.right=x2+1;
                span.u=int(uint(spanUV.x)&65535U);span.v=int(uint(spanUV.y)&65535U);
                span.du=word(step.x);span.dv=word(step.y);
            } else {
                uint wireframe=(material.scroll_y>>1)&255U;
                bool edges=(wireframe==1 || (wireframe==2 && mode2Continuation)) && !leftStarts && !rightStarts;
                span.scroll_x=1;span.scroll_y=edges?1U:0U;
                if((material.scroll_y&131072U)!=0) span.scroll_y|=2U;
                span.u=x1;span.v=x2;
                if(wireframe==0 && (material.scroll_y&1U)!=0) {span.left=max(0,x1+1);span.right=min(width,x2);}
                if(sparseWobble) {
                    span.scroll_y=0;span.left=previousLeft;span.right=previousLeft+1;
                    if(rightStarts || !havePrevious) span.right=span.left;
                }
            }
            if(repeatedRow) {
                int first=max(0,span.left),last=min(width,span.right);
                if((span.scroll_y&1U)!=0) {
                    for(uint edge=0;edge<2;++edge) {
                        int x=edge==0?span.u:span.v;
                        if(x<first || x>=last)continue;
                        uint at=maskBase+uint(y)*maskStride+uint(x/32)*4;
                        masks.Store(at,masks.Load(at)|(1U<<(uint(x)&31U)));
                    }
                } else for(int x=first;x<last;) {
                    uint bit=uint(x)&31U;
                    uint covered=min(uint(last-x),32U-bit);
                    uint bits=(0xffffffffU>>(32U-covered))<<bit;
                    uint at=maskBase+uint(y)*maskStride+uint(x/32)*4;
                    masks.Store(at,masks.Load(at)|bits);
                    x+=int(covered);
                }
                Command previous=commands[commandBase+uint(y)];
                if(first<last) {
                    span.left=previous.right>previous.left?min(first,previous.left):first;
                    span.right=previous.right>previous.left?max(last,previous.right):last;
                } else {
                    span.left=previous.left;span.right=previous.right;
                }
                // Repeated-row coverage is indexed before wave displacement.
                // Keep the wave flag so the consumer can invert that mapping.
                span.scroll_y=4U|(span.scroll_y&2U);
                span.texture_offset=maskBase;span.u_mask=maskStride;span.v_mask=uint(height);
            }
            commands[commandBase+uint(y)]=span;
        }
        previousLeft=x1;havePrevious=true;
        if(rightStarts) mode2Continuation=false;
        if(leftStarts && !rightStarts) mode2Continuation=true;
        left.x=advanceX(left.x,left.increment);right.x=advanceX(right.x,right.increment);
        left.uv=nextLeft;right.uv=nextRight;
        --left.remaining;--right.remaining;
        if(repeatedRow) {
            if(left.remaining!=0 && right.remaining!=0)continue;
            ++y;
        }
        ++y;
    }
}
