struct Command {
    int left,top,right,bottom;
    uint even,odd,dither,tag;
    uint texture_offset,u_mask,v_mask,colour_base;
    int u,v,du,dv;
    float4 surface;
    uint has_surface,textured,scroll_x,scroll_y;
};
StructuredBuffer<Command> commands:register(t0,space0);
RWStructuredBuffer<uint> rows:register(u0,space1);
// First tile_count entries are scatter cursors, followed by sparse IDs.
RWStructuredBuffer<uint> indices:register(u1,space1);
// Keep indirect arguments separate from shader-readable occupied IDs so their
// simultaneous native driver read states do not have to alias.
RWStructuredBuffer<uint> dispatchArgs:register(u2,space1);
cbuffer Settings:register(b0,space2) {uint width,height,command_count,stage;};
groupshared uint sums[64];
groupshared uint occupiedCount,occupiedBase;
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID,uint lane:SV_GroupIndex,uint3 group:SV_GroupID) {
    uint tiles_x=(width+63)/64, tile_count=tiles_x*height;
    if(stage==8) {
        if(id.x==0) {
            rows[0]=0;
            dispatchArgs[0]=dispatchArgs[1]=1;
            dispatchArgs[2]=1;dispatchArgs[3]=0;
        }
        return;
    }
    if(stage==6 || stage==7) {
        uint count=0;
        if(id.x<tile_count) {
            uint y=id.x/tiles_x,x=(id.x%tiles_x)*64;
            uint base=id.x*(command_count+1);
            for(uint polygon=0;polygon<command_count;++polygon) {
                uint index=polygon*height+y;
                Command c=commands[index];
                if(c.left<c.right && c.right>int(x) && c.left<int(min(x+64,width)))
                    indices[base+1+count++]=index;
            }
            indices[base]=count;
        }
        if(stage==7) {
            // Reserve once per workgroup, not once per live tile. Argument
            // generation shares the binning pass; no final full-device pass.
            if(lane==0) occupiedCount=0;
            GroupMemoryBarrierWithGroupSync();
            uint slot=0;
            if(count!=0) InterlockedAdd(occupiedCount,1,slot);
            GroupMemoryBarrierWithGroupSync();
            if(lane==0 && occupiedCount!=0) {
                InterlockedAdd(rows[0],occupiedCount,occupiedBase);
                uint end=occupiedBase+occupiedCount;
                InterlockedMax(dispatchArgs[0],min(end,65535u));
                InterlockedMax(dispatchArgs[1],(end+65534u)/65535u);
            }
            GroupMemoryBarrierWithGroupSync();
            if(count!=0) rows[1+occupiedBase+slot]=id.x;
        }
        return;
    }
    if(stage==0) {if(id.x<=tile_count) rows[id.x]=0;return;}
    if(stage==2) {
        uint count=id.x<tile_count?rows[id.x]:0;
        sums[lane]=count;GroupMemoryBarrierWithGroupSync();
        for(uint distance=1;distance<64;distance*=2) {
            uint addend=lane>=distance?sums[lane-distance]:0;
            GroupMemoryBarrierWithGroupSync();
            sums[lane]+=addend;GroupMemoryBarrierWithGroupSync();
        }
        if(id.x<tile_count) rows[id.x]=sums[lane]-count;
        if(lane==63) rows[tile_count+1+group.x]=sums[lane];
        return;
    }
    if(stage==3) {
        if(id.x!=0) return;
        uint sum=0;
        for(uint block=0;block<(tile_count+63)/64;++block) {
            uint slot=tile_count+1+block,count=rows[slot];rows[slot]=sum;sum+=count;
        }
        rows[tile_count]=sum;return;
    }
    if(stage==4) {
        if(id.x<tile_count) {
            uint offset=rows[id.x]+rows[tile_count+1+group.x];
            rows[id.x]=offset;indices[id.x]=offset;
        }
        return;
    }
    uint command_index=id.x/8,worker=id.x%8;
    if(command_index>=command_count) return;
    Command c=commands[command_index];
    int left=max(0,c.left),right=min(int(width),c.right);
    int top=max(0,c.top),bottom=min(int(height),c.bottom);
    if(left>=right || top>=bottom) return;
    uint first=uint(left)/64,last=(uint(right)+63)/64;
    uint columns=last-first,coverage=uint(bottom-top)*columns;
    for(uint item=worker;item<coverage;item+=8) {
        uint tile=(uint(top)+item/columns)*tiles_x+first+item%columns,slot;
        if(stage==1) InterlockedAdd(rows[tile],1,slot);
        else {InterlockedAdd(indices[tile],1,slot);indices[tile_count+slot]=command_index;}
    }
}
