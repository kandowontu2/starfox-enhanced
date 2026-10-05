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
// list_length: the compact list buffer's length in uints (stages 10-14).
cbuffer Settings:register(b0,space2) {uint width,height,command_count,stage;uint list_length;uint3 settings_padding;};
groupshared uint sums[64];
groupshared uint box[4];
groupshared int2 spans[64];
// Compact ordered row-span tile lists (GPU FAST, stages 10-14). Tiles are
// indexed within the screen box from stage 8 (rows[11..14]). indices[] holds
// CSR offsets [0..box tiles], the box-tile total at [box tiles], block sums
// from compact_blocks(), and entries from compact_entries(). Each tile lists
// its covering polygons in ascending order, exactly like bin_tile.
uint compact_tiles() {return ((width+63)/64)*height;}
uint compact_blocks() {return compact_tiles()+1;}
uint compact_entries() {return compact_blocks()+(compact_tiles()+63)/64+1;}
// One 64-lane group per box row. Lane l owns box tile columns l and l+64
// (a box is at most 8192 pixels = 128 tiles wide). Polygons are staged
// through groupshared memory 64 at a time.
void compact_row(uint row,uint lane,bool fill) {
    uint y=rows[13]+row,columns=rows[11];
    uint2 column=uint2(lane,lane+64);
    uint2 x=rows[12]+column*64;
    uint2 tile=row*columns+column,count=0,base=0;
    bool2 owned=column<columns;
    uint capacity=0;
    if(fill) {
        base=compact_entries()+uint2(owned.x?indices[tile.x]:0,owned.y?indices[tile.y]:0);
        capacity=list_length;
    }
    for(uint first=0;first<command_count;first+=64) {
        uint polygon=first+lane;
        int2 span=int2(0,0);
        if(polygon<command_count) {Command c=commands[polygon*height+y];span=int2(c.left,c.right);}
        spans[lane]=span;
        GroupMemoryBarrierWithGroupSync();
        uint chunk=min(64U,command_count-first);
        for(uint i=0;i<chunk;++i) {
            int2 s=spans[i];
            if(s.x>=s.y) continue;
            [unroll] for(uint k=0;k<2;++k) {
                if(owned[k] && s.y>int(x[k]) && s.x<int(min(x[k]+64,width))) {
                    if(fill && base[k]+count[k]<capacity) indices[base[k]+count[k]]=(first+i)*height+y;
                    ++count[k];
                }
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if(!fill) {if(owned.x) indices[tile.x]=count.x;if(owned.y) indices[tile.y]=count.y;}
}
// Row-span tile list: every polygon row that overlaps one 64-pixel tile.
void bin_tile(uint tile,uint tiles_x) {
    uint y=tile/tiles_x,x=(tile%tiles_x)*64;
    uint base=tile*(command_count+1),count=0;
    for(uint polygon=0;polygon<command_count;++polygon) {
        uint index=polygon*height+y;
        Command c=commands[index];
        if(c.left<c.right && c.right>int(x) && c.left<int(min(x+64,width)))
            indices[base+1+count++]=index;
    }
    indices[base]=count;
}
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID,uint lane:SV_GroupIndex,uint3 group:SV_GroupID) {
    uint tiles_x=(width+63)/64, tile_count=tiles_x*height;
    if(stage==6) {
        if(id.x>=tile_count) return;
        bin_tile(id.x,tiles_x);return;
    }
    // Bounded row spans (GPU FAST). rows[] holds the model's screen box:
    // [0..3] min x, min y, max x, max y (exclusive; identity while unused),
    // [11] box width in tiles, [12..13] 64-aligned origin, [14] box tiles.
    if(stage==7) {
        // Reduce every covered span row into the box.
        if(lane==0) {box[0]=0xffffffffU;box[1]=0xffffffffU;box[2]=0;box[3]=0;}
        GroupMemoryBarrierWithGroupSync();
        if(id.x<command_count*height) {
            Command c=commands[id.x];
            int left=max(0,c.left),right=min(int(width),c.right);
            if(left<right) {
                uint y=id.x%height;
                InterlockedMin(box[0],uint(left));InterlockedMin(box[1],y);
                InterlockedMax(box[2],uint(right));InterlockedMax(box[3],y+1);
            }
        }
        GroupMemoryBarrierWithGroupSync();
        if(lane==0 && box[2]!=0) {
            InterlockedMin(rows[0],box[0]);InterlockedMin(rows[1],box[1]);
            InterlockedMax(rows[2],box[2]);InterlockedMax(rows[3],box[3]);
        }
        return;
    }
    if(stage==8) {
        // indices[] is the indirect argument buffer here: raster groups at
        // [0..2], bounded binning groups at [4..6]. An empty box dispatches
        // nothing, leaving the background untouched. Reset for the next use.
        // Stage 7 only lowers minima and raises maxima, so stale or
        // uninitialized contents can only widen the box; clamp to the frame.
        if(id.x!=0) return;
        uint max_x=min(rows[2],width),max_y=min(rows[3],height);
        bool empty=max_x<=rows[0] || max_y<=rows[1];
        uint x0=empty?0:(rows[0]&~63U),y0=empty?0:rows[1];
        uint groups_x=empty?0:(max_x-x0+63)/64,groups_y=empty?0:max_y-y0;
        indices[0]=groups_x;indices[1]=groups_y;indices[2]=1;indices[3]=0;
        indices[4]=(groups_x*groups_y+63)/64;indices[5]=1;indices[6]=1;indices[7]=0;
        // Compact list count/fill passes: one group per box row.
        indices[8]=groups_y;indices[9]=1;indices[10]=1;indices[11]=0;
        rows[11]=groups_x;rows[12]=x0;rows[13]=y0;rows[14]=groups_x*groups_y;
        rows[0]=0xffffffffU;rows[1]=0xffffffffU;rows[2]=0;rows[3]=0;
        return;
    }
    if(stage==9) {
        // Stage 6 restricted to the box's tiles.
        if(id.x>=rows[14]) return;
        bin_tile((rows[13]+id.x/rows[11])*tiles_x+rows[12]/64+id.x%rows[11],tiles_x);
        return;
    }
    if(stage==10 || stage==14) {compact_row(group.x,lane,stage==14);return;}
    if(stage==11) {
        // Exclusive scan of the box tile counts within 64-tile blocks.
        uint total=rows[14];
        uint count=id.x<total?indices[id.x]:0;
        sums[lane]=count;GroupMemoryBarrierWithGroupSync();
        for(uint distance=1;distance<64;distance*=2) {
            uint addend=lane>=distance?sums[lane-distance]:0;
            GroupMemoryBarrierWithGroupSync();
            sums[lane]+=addend;GroupMemoryBarrierWithGroupSync();
        }
        if(id.x<total) indices[id.x]=sums[lane]-count;
        if(lane==63) indices[compact_blocks()+group.x]=sums[lane];
        return;
    }
    if(stage==12) {
        if(id.x!=0) return;
        uint total=rows[14],sum=0;
        for(uint block=0;block<(total+63)/64;++block) {
            uint slot=compact_blocks()+block,count=indices[slot];indices[slot]=sum;sum+=count;
        }
        indices[total]=sum;return;
    }
    if(stage==13) {
        if(id.x<rows[14]) indices[id.x]+=indices[compact_blocks()+group.x];
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
