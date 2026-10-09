// Same storage ABI and exact geometry/UV arithmetic as the reference tracer.
// One group owns a polygon; the leader produces bounded, ordered row inputs
// and adjacent lanes write adjacent row commands. No cross-group cooperation.
#define STARFOX_SPANS_SERIAL_HELPER 1
#include "spans_portable.hlsl"

struct RowInput {
    int4 xy; // sorted left/right, prior sorted left, authored row
    int4 uv; // source left UV, next opposite UV (reference order)
    uint flags; // writable, edge-only, sparse prior allowed
};
groupshared RowInput rowInputs[32];
groupshared uint rowCount;
groupshared uint rowsFinished;

[numthreads(32,1,1)]
void main(uint3 group:SV_GroupID,uint3 lane:SV_GroupThreadID) {
    uint slot=group.x+group.y*65535U;
    if(slot>=count) return;
    uint polygon=slot;
    bool orderedValid=true;
    if(orderedMode!=0) {
        uint2 result=orderResults[orderTree];
        orderedValid=result.y==0 && result.x<=count && slot<result.x;
        if(orderedValid && orderedMode==1) polygon=order[orderFirst+slot];
    }
    bool valid=orderedValid && polygon<polygonCount;
    Command material=(Command)0;
    int4 header=0;
    uint base=polygon*129U;
    if(valid) {material=materials[polygon];header=clipped[base];}
    bool cooperative=valid && rasterPadding.y==0 && header.y==0
        && header.x>=3 && header.x<=128 && material.textured<=1
        && !(material.textured==0 && (material.scroll_y&262144U)!=0);
    // Lines, sprites, repeated-row accumulations and rejected descriptors keep
    // the entire original function, including its clear/order/mask behavior.
    if(!cooperative) {
        if(lane.x==0) traceSerial(uint3(slot,0,0));
        return;
    }

    uint commandBase=slot*uint(height);
    if((padding&2U)==0) {
        Command empty=(Command)0;
        for(uint row=lane.x;row<uint(height);row+=32) {
            if((padding&1U)!=0) {
                commands[commandBase+row].left=0;
                commands[commandBase+row].top=0;
                commands[commandBase+row].right=0;
                commands[commandBase+row].bottom=0;
            } else commands[commandBase+row]=empty;
        }
    }
    uint maskBase=maskOffset+slot*uint(height)*maskStride*(rasterPadding.x+1);
    if(maskEnabled!=0 && rasterPadding.x==0)
        for(uint at=lane.x*4;at<uint(height)*maskStride;at+=32*4)
            masks.Store(maskBase+at,0);
    // Initial clear writes can be owned by a different lane than live rows.
    // A groupshared-only barrier would not order these UAV writes.
    DeviceMemoryBarrierWithGroupSync();

    uint size=uint(header.x),minimum=0;
    int maximumY=0,y=0,previousLeft=0;
    Tracer left=(Tracer)0,right=(Tracer)0;
    bool mode2Continuation=false,havePrevious=false,running=true;
    bool sparseWobble=material.textured==0 && (material.scroll_y&65536U)!=0;
    if(lane.x==0) {
        maximumY=pointAt(base,0).y;
        for(uint i=1;i<size;++i) {
            int pointY=pointAt(base,i).y;
            if(pointY<pointAt(base,minimum).y) minimum=i;
            maximumY=max(maximumY,pointY);
        }
        y=pointAt(base,minimum).y;
        left.vertex=right.vertex=minimum;left.direction=1;right.direction=-1;
        left.x=right.x=fixedX(pointAt(base,minimum).x);
        int2 uv=uvAt(base,minimum);
        left.uv=right.uv=int2(word(uv.x<<8),word(uv.y<<8));
    }
    [loop] while(true) {
        if(lane.x==0) {
            rowCount=0;
            for(uint index=0;index<32;++index) {
                if(y>=maximumY) {running=false;break;}
                bool leftStarts=left.remaining==0,rightStarts=right.remaining==0;
                if(left.remaining==0 && !beginSegment(left,base,size,y)) {running=false;break;}
                if(right.remaining==0 && !beginSegment(right,base,size,y)) {running=false;break;}
                int x1=integerX(left.x),x2=integerX(right.x);
                int2 nextLeft=int2(textureAdvance(left.uv.x,left.uvIncrement.x),textureAdvance(left.uv.y,left.uvIncrement.y));
                int2 nextRight=int2(textureAdvance(right.uv.x,right.uvIncrement.x),textureAdvance(right.uv.y,right.uvIncrement.y));
                int2 spanUV=left.uv,spanRight=nextRight;
                if(windingIndependent!=0 && x2<x1) {
                    int swap=x1;x1=x2;x2=swap;spanUV=right.uv;spanRight=nextLeft;
                }
                uint wireframe=(material.scroll_y>>1)&255U;
                bool edges=(wireframe==1 || (wireframe==2 && mode2Continuation)) && !leftStarts && !rightStarts;
                uint flags=(x2>=x1 && (sparseWobble || (x2>=0 && x1<width)))?1U:0U;
                if(edges) flags|=2U;
                if(!rightStarts && havePrevious) flags|=4U;
                RowInput input;
                input.xy=int4(x1,x2,previousLeft,y);
                input.uv=int4(spanUV,spanRight);input.flags=flags;
                rowInputs[index]=input;++rowCount;
                previousLeft=x1;havePrevious=true;
                if(rightStarts) mode2Continuation=false;
                if(leftStarts && !rightStarts) mode2Continuation=true;
                left.x=advanceX(left.x,left.increment);right.x=advanceX(right.x,right.increment);
                left.uv=nextLeft;right.uv=nextRight;
                --left.remaining;--right.remaining;++y;
            }
            rowsFinished=running?0U:1U;
        }
        GroupMemoryBarrierWithGroupSync();
        if(lane.x<rowCount) {
            RowInput input=rowInputs[lane.x];
            if((input.flags&1U)!=0) {
                int x1=input.xy.x,x2=input.xy.y,row=input.xy.w;
                Command span=material;
                span.left=max(0,x1);span.right=min(width,x2+1);span.top=row;span.bottom=row+1;
                if(material.textured==1) {
                    int distance=x2-x1;
                    int reciprocal=distance==0?0:(distance==1?32767:32768/distance);
                    int2 difference=int2(word(input.uv.z-input.uv.x),word(input.uv.w-input.uv.y));
                    int2 step=(difference*reciprocal)>>15;
                    span.left=x1;span.right=x2+1;
                    span.u=int(uint(input.uv.x)&65535U);span.v=int(uint(input.uv.y)&65535U);
                    span.du=word(step.x);span.dv=word(step.y);
                } else {
                    uint wireframe=(material.scroll_y>>1)&255U;
                    span.scroll_x=1;span.scroll_y=(input.flags&2U)!=0?1U:0U;
                    if((material.scroll_y&131072U)!=0) span.scroll_y|=2U;
                    span.u=x1;span.v=x2;
                    if(wireframe==0 && (material.scroll_y&1U)!=0) {span.left=max(0,x1+1);span.right=min(width,x2);}
                    if(sparseWobble) {
                        span.scroll_y=0;span.left=input.xy.z;span.right=span.left+1;
                        if((input.flags&4U)==0) span.right=span.left;
                    }
                }
                commands[commandBase+uint(row)]=span;
            }
        }
        // All readers finish before the leader reuses the bounded 32-row bank.
        GroupMemoryBarrierWithGroupSync();
        if(rowsFinished!=0) break;
    }
}
