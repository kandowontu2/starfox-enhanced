#include "raster_jitter.hlsli"
#include "native_pixel.hlsli"
struct Command {
    int left,top,right,bottom;
    uint even,odd,dither,tag;
    uint texture_offset,u_mask,v_mask,colour_base;
    int u,v,du,dv;
    float4 surface;
    uint has_surface,textured,scroll_x,scroll_y;
};
StructuredBuffer<Command> commands:register(t0,space0);
StructuredBuffer<uint> rows:register(t1,space0);
StructuredBuffer<uint> indices:register(t2,space0);
ByteAddressBuffer texels:register(t3,space0);
StructuredBuffer<uint> back_pixels:register(t4,space0);
#if !defined(STARFOX_PIXEL_ONLY_RASTER)
StructuredBuffer<float4> back_surfaces:register(t5,space0);
StructuredBuffer<float4> geometry_planes:register(t6,space0);
StructuredBuffer<float> back_depth:register(t7,space0);
#endif
RWStructuredBuffer<uint> pixels:register(u0,space1);
#if !defined(STARFOX_PIXEL_ONLY_RASTER)
RWStructuredBuffer<float4> surfaces:register(u1,space1);
RWStructuredBuffer<float> geometry_depth:register(u2,space1);
#endif
cbuffer Settings:register(b0,space2) {
    uint width,height,want_surface,reserved;
    uint has_back,has_back_surface,take_surface,padding;
    uint texel_bytes,reserved1,reserved2,painterFlags;
    uint want_depth,plane_count,has_back_depth,depth_padding;
    float4 depth_projection; // focal x/y, center x/y in output pixels.
    float2 rasterJitter;uint2 jitterPadding;
};
// Source wave arithmetic uses signed 16-bit wrapping before both phase steps.
int waveShift(int x,int offset,uint frame) {
    int phase=(int(uint(offset+x)<<16))>>16;
    phase=(int(uint((phase>>1)+int(frame&15U)-1)<<16))>>16;
    static const int sine[16]={0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1};
    return sine[uint(phase)&15U];
}
#if defined(STARFOX_ROW_TILE_RASTER)
groupshared Command tileCommands[64];
groupshared uint tileMask[2];
#endif
[numthreads(64,1,1)]
#if defined(STARFOX_ROW_TILE_RASTER)
void main(uint3 id:SV_DispatchThreadID,uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    // The owner guarantees <=64 ordinary row spans, no sample remapping/wave
    // rows. Every lane, including a partial tile's out-of-range lanes, reaches
    // the barrier before the ordinary pixel bounds check below.
    uint count=reserved&0x3fffffffU;
    if(lane<2) tileMask[lane]=0;
    GroupMemoryBarrierWithGroupSync();
    uint candidate=0;
    if(lane<count && group.y<height) {
        Command c=commands[lane*height+group.y];
        candidate=c.left<c.right && c.right>int(group.x*64)
            && c.left<int(min(group.x*64+64,width));
        if(candidate!=0) {
            tileCommands[lane]=c;
            InterlockedOr(tileMask[lane/32],1u<<(lane%32));
        }
    }
    GroupMemoryBarrierWithGroupSync();
    uint2 remaining=uint2(tileMask[0],tileMask[1]);
#elif defined(STARFOX_OCCUPIED_RASTER)
void main(uint3 id:SV_DispatchThreadID,uint3 group:SV_GroupID,uint lane:SV_GroupIndex) {
    if((depth_padding&2u)!=0) {
        uint entry=group.x+group.y*65535u;
        if(entry>=rows[0]) return;
        uint tile=rows[1+entry],tiles_x=(width+63)/64;
        id.xy=uint2((tile%tiles_x)*64+lane,tile/tiles_x);
    }
#else
void main(uint3 id:SV_DispatchThreadID) {
#endif
    uint outputWidth=reserved1!=0?reserved1:width,outputHeight=reserved2!=0?reserved2:height;
    if(id.x>=outputWidth || id.y>=outputHeight) return;
    uint outputIndex=id.y*outputWidth+id.x;
    if(any(rasterJitter!=0)) {
        int2 sampleAt=int2(jitterFloor(id.x,width,outputWidth,rasterJitter.x,true),jitterFloor(id.y,height,outputHeight,rasterJitter.y,true));
        if(any(sampleAt<0) || any(sampleAt>=int2(width,height))) {
            pixels[outputIndex]=0;
#if !defined(STARFOX_PIXEL_ONLY_RASTER)
            if(want_surface) surfaces[outputIndex]=0;
            if(want_depth) geometry_depth[outputIndex]=0;
#endif
            return;
        }
        id.xy=uint2(sampleAt);
    } else if(reserved1!=0) id.xy=id.xy*uint2(width,height)/uint2(outputWidth,outputHeight);
    bool have_pixel=false;
    uint packed=0;
#if !defined(STARFOX_PIXEL_ONLY_RASTER)
    bool have_surface=take_surface==0;
    float4 surface=float4(0,0,1,0);
    float depth=0;
    uint surface_owner=0;
#endif
    uint pixel_owner=0;
#if defined(STARFOX_SINGLE_FACE_RASTER) || defined(STARFOX_MASK_TILE_RASTER)
    // Specialized owners guarantee ordinary row spans, not wave/scatter input.
    const bool row_spans=true,wave_rows=false,sparse=false,tiled_spans=false;
#else
    bool row_spans=(reserved&0x80000000U)!=0;
    bool wave_rows=row_spans && (padding&1U)!=0;
    bool sparse=!row_spans && (reserved&1U)!=0;
    bool tiled_spans=row_spans && (padding&0x80000000U)!=0;
#endif
    int wave_delta=wave_rows?waveShift(int(id.x),int((padding>>1)&65535U),(padding>>17)&15U):0;
    bool in_place=depth_padding!=0;
    // Last covering command wins, as in the source painter. Surface metadata
    // survives later non-surface lines/sprites, matching SurfaceBuffer::set.
    uint row=id.y*((width+63)/64)+id.x/64;
#if defined(STARFOX_MASK_TILE_RASTER)
    uint2 remaining=uint2(indices[row*2],indices[row*2+1]);
    if(in_place && all(remaining==0)) return;
#endif
    uint begin=row_spans?0:rows[row];
    if(tiled_spans) begin=row*((reserved&0x3fffffffU)+1)+1;
    // The ordered owner explicitly lends a fully initialized canonical target.
    // Empty tiles need neither a background fetch nor any output write.
    if(in_place && tiled_spans && indices[begin-1]==0) return;
#if defined(STARFOX_SINGLE_FACE_RASTER)
    [unroll] for(uint cursor=1;cursor>0;) {
#else
    for(uint cursor=tiled_spans?begin+indices[begin-1]:row_spans?(reserved&0x3fffffffU)*(wave_rows?2U:1U):rows[row+1];cursor>begin;) {
#endif
#if defined(STARFOX_ROW_TILE_RASTER) || defined(STARFOX_MASK_TILE_RASTER)
        // Walk only live commands, in exact descending painter order. Empty
        // tiles do not scan all 64 face slots for every one of their pixels.
        if(remaining.y!=0) {
            uint bit=firstbithigh(remaining.y);remaining.y&=~(1u<<bit);cursor=bit+33;
        } else if(remaining.x!=0) {
            uint bit=firstbithigh(remaining.x);remaining.x&=~(1u<<bit);cursor=bit+1;
        } else break;
#endif
        --cursor;
        bool wave_candidate=wave_rows && (cursor&1U)!=0;
        int source_y=int(id.y)-(wave_candidate?wave_delta:0);
        if(source_y<0 || source_y>=int(height)) continue;
        uint polygon=wave_rows?cursor/2U:cursor;
        uint command_index=tiled_spans?indices[cursor]:row_spans?polygon*height+uint(source_y):indices[cursor+(sparse?height*((width+63)/64):0)];
        uint owner=command_index+1;
        // Sparse atomic scatter is unordered. Once both independent owners
        // outrank this command, neither coverage nor texture transparency
        // can change the result; avoid fetching its material altogether.
#if defined(STARFOX_PIXEL_ONLY_RASTER)
        if(sparse && have_pixel && owner<=pixel_owner) continue;
#else
        if(sparse && have_pixel && owner<=pixel_owner && have_surface
            && (take_surface==0 || owner<=surface_owner)) continue;
#endif
#if defined(STARFOX_ROW_TILE_RASTER)
        Command c=tileCommands[polygon];
#else
        Command c=commands[command_index];
#endif
        if(wave_rows) {
            bool wave=c.textured==0 && (c.scroll_y&2U)!=0;
            if(wave!=wave_candidate) continue;
        }
        if(int(id.x)<c.left || int(id.x)>=c.right) continue;
        if(c.textured==0 && (c.scroll_y&1U)!=0 && int(id.x)!=c.u && int(id.x)!=c.v) continue;
        if(c.textured==0 && (c.scroll_y&4U)!=0) {
            uint bytes=texel_bytes;
            if(c.u_mask==0 || (c.u_mask&3U)!=0 || (c.texture_offset&3U)!=0
                || uint(source_y)>=c.v_mask || id.x/32>=c.u_mask/4 || c.texture_offset>bytes) continue;
            if(c.v_mask>(bytes-c.texture_offset)/c.u_mask) continue;
            uint bits=texels.Load(c.texture_offset+uint(source_y)*c.u_mask+(id.x/32)*4);
            if((bits&(1U<<(id.x&31)))==0) continue;
        }
        uint dither_scale=max(1U,c.scroll_x);
        uint colour=c.dither && (((id.x/dither_scale)^(id.y/dither_scale))&1)!=0?c.odd:c.even;
        uint pixelTag=c.tag;
        uint materialPair=0;
        if(c.textured==8) {
            if(c.du<=0 || c.texture_offset>texel_bytes || texel_bytes-c.texture_offset<640) continue;
            int px=c.dv!=0?((int(id.x)-c.u)*6+2)/(7*c.du):(int(id.x)-c.u)/c.du;
            int py=(source_y-c.v)/c.du;
            if(px<0 || px>=32 || py<0 || py>=40) continue;
            uint at=c.texture_offset+uint((px/8)*5+py/8)*32+uint(py%8)*2,ink=0;
            for(uint plane=0;plane<4;++plane) {
                uint offset=at+(plane/2)*16+plane%2;
                uint bits=(texels.Load(offset&~3U)>>((offset&3U)*8))&255;
                ink|=((bits>>uint(7-px%8))&1U)<<plane;
            }
            colour=(c.colour_base+ink)&255;
        } else if(c.textured==7) {
            if(c.du<=0 || (c.dv!=8 && c.dv!=12) || c.texture_offset>texel_bytes || texel_bytes-c.texture_offset<8) continue;
            int column=(int(id.x)-c.u)/c.du,row=(source_y-c.v)/c.du;
            if(column<0 || column>=c.dv || row<0 || row>=c.dv) continue;
            uint at=c.texture_offset+uint(row*8/c.dv);
            uint bits=(texels.Load(at&~3U)>>((at&3U)*8))&255;
            if((bits&(0x80U>>uint(column*8/c.dv)))==0) continue;
        } else if(c.textured==6) {
            if(c.du<=0 || c.dv<2 || c.texture_offset>texel_bytes || texel_bytes-c.texture_offset<24) continue;
            int column=(int(id.x)-c.u)/c.du,row=(source_y-c.v)/c.du;
            if(column<0 || column>=16 || row<0 || row>=c.dv) continue;
            uint at=c.texture_offset+uint(row*11/(c.dv-1))*2;
            uint low=(texels.Load(at&~3U)>>((at&3U)*8))&255;
            ++at;
            uint high=(texels.Load(at&~3U)>>((at&3U)*8))&255;
            if(((low|(high<<8))&(0x8000U>>uint(column)))==0) continue;
        } else if(c.textured==5) {
            if(c.du<=0 || c.dv<=0 || c.scroll_x==0 || c.u_mask==0 || c.v_mask==0) continue;
            if(c.texture_offset>texel_bytes || c.v_mask>(texel_bytes-c.texture_offset)/c.u_mask) continue;
            if(c.dither!=0 && (c.scroll_y>texel_bytes || c.v_mask>(texel_bytes-c.scroll_y)/c.u_mask)) continue;
            int2 origin=int2(c.even,c.odd),delta=int2(id.xy)/c.dv-origin;
            int step=int(c.scroll_x);
            int2 snapped=int2(delta.x<0?-((-delta.x+step-1)/step):delta.x/step,
                delta.y<0?-((-delta.y+step-1)/step):delta.y/step)*step+origin-int2(c.u,c.v);
            if(any(snapped<0) || any(snapped>=int2(c.u_mask,c.v_mask)/c.du)) continue;
            uint2 sub=((id.xy%uint(c.dv)*2+1)*uint(c.du))/(uint(c.dv)*2);
            uint at=(uint(snapped.y)*uint(c.du)+sub.y)*c.u_mask+uint(snapped.x)*uint(c.du)+sub.x;
            uint offset=c.texture_offset+at;
            colour=(texels.Load(offset&~3U)>>((offset&3U)*8))&255;
            if(c.colour_base!=0) {
                uint pairBase=c.colour_base-1;
                if(pairBase>texel_bytes || c.u_mask>(texel_bytes-pairBase)/2
                    || c.v_mask>(texel_bytes-pairBase)/2/c.u_mask) continue;
                uint pairAt=pairBase+at*2;
                materialPair=(texels.Load(pairAt&~3U)>>((pairAt&3U)*8))&255;
                ++pairAt;
                materialPair|=((texels.Load(pairAt&~3U)>>((pairAt&3U)*8))&255)<<8;
            }
            if(colour==0 && (materialPair&256)==0) continue;
            if(c.dither!=0) {offset=c.scroll_y+at;pixelTag=(texels.Load(offset&~3U)>>((offset&3U)*8))&255;}
        } else if(c.textured==4) {
            if(c.du<=0 || c.dv<=0 || c.texture_offset>texel_bytes || texel_bytes-c.texture_offset<65536) continue;
            int u=(int(id.x)-c.u)/c.du,v=(int(id.y)-c.v)/c.du;
            if(u<0 || v<0 || u>=c.dv || v>=c.dv) continue;
            if(c.scroll_y&1U) u=c.dv-1-u;
            if(c.scroll_y&2U) v=c.dv-1-v;
            uint address=(c.scroll_x+uint(v/8)*512+uint(u/8)*32+uint(v&7)*2)&65535U;
            uint bit=7-uint(u&7),texel=0;
            for(uint plane=0;plane<4;++plane) {
                uint offset=c.texture_offset+((address+(plane/2)*16+(plane&1))&65535U);
                uint value=(texels.Load(offset&~3U)>>((offset&3U)*8))&255;
                texel|=((value>>bit)&1U)<<plane;
            }
            if(texel==0) continue;
            colour=(c.colour_base+texel)&255;
        } else if(c.textured!=0) {
            uint dx=id.x-uint(c.left);
            uint u,v;
            if(c.textured==1) {
                u=((((uint(c.u)+uint(c.du)*dx)&65535)>>8)+c.scroll_x)&c.u_mask;
                v=((((uint(c.v)+uint(c.dv)*dx)&65535)>>8)+c.scroll_y)&c.v_mask;
            } else if(c.textured==2) {
                u=((id.x-uint(c.u))/uint(c.dv))*(c.u_mask+1)/uint(c.du);
                v=((id.y-uint(c.v))/uint(c.dv))*(c.v_mask+1)/uint(c.du);
            } else {
                u=((uint(c.u)+uint(c.du)*(dx/uint(c.dv)))&65535)>>8;
                v=((uint(c.v)+uint(c.du)*((id.y-uint(c.top))/uint(c.dv)))&65535)>>8;
                if(u>c.u_mask || v>c.v_mask) continue;
            }
            uint offset=c.texture_offset+v*(c.u_mask+1)+u;
            uint texel=(texels.Load(offset&~3U)>>((offset&3U)*8))&255;
            if(texel==0) continue;
            colour=(c.colour_base+texel)&255;
            if(c.textured==2 && (c.scroll_x&256)!=0) colour=c.scroll_x&255;
        }
        if(!have_pixel || (sparse && owner>pixel_owner)) {
            packed=(packed&0x7fff0000U)|colour|(pixelTag<<8);have_pixel=true;pixel_owner=owner;
            if(pixelTag==0 && (materialPair&256)!=0)
                packed=(packed&0xffff00ffU)|((materialPair&255U)<<8)|0x80000000U;
            if(c.textured==0 && pixelTag==0 && c.dither!=0 && c.even!=c.odd)
                packed=(packed&0xffff00ffU)|(((colour==c.even?c.odd:c.even)&255U)<<8)|0x80000000U;
#if !defined(STARFOX_PIXEL_ONLY_RASTER)
            if(want_depth!=0) {
                depth=0;
                uint plane_id=c.has_surface>>1;
                // Screen-space warps do not describe a pinhole-projected plane.
                // Mark them unknown rather than inventing plausible geometry.
                if(plane_id>0 && plane_id<=plane_count && !wave_rows
                    && !(c.textured==0 && (c.scroll_y&4U)!=0)) {
                    float4 plane=geometry_planes[plane_id-1];
                    float3 ray=float3((float2(id.xy)+.5-depth_projection.zw)/depth_projection.xy,1);
                    float denominator=dot(plane.xyz,ray);
                    float candidate=plane.w/denominator;
                    if(abs(denominator)>1e-8 && isfinite(candidate) && candidate>0) depth=candidate;
                }
            }
#endif
        }
#if !defined(STARFOX_PIXEL_ONLY_RASTER)
        if(take_surface!=0 && (c.has_surface&1u)!=0 && (!have_surface || (sparse && owner>surface_owner))) {
            packed=(packed&0x8000ffffU)|(colour<<16)|(1U<<24);surface=c.surface;have_surface=true;surface_owner=owner;
        }
        if(!sparse && have_pixel && have_surface) break;
#else
        if(!sparse && have_pixel) break;
#endif
    }
    if(in_place && !have_pixel) return; // Texture holes preserve ALL ownership.
    if((reserved&0x40000000U)!=0 && have_pixel) packed|=0x04000000U;
    if((painterFlags&1u)!=0 && have_pixel && nativeTag(packed)==1u) packed|=0x10000000u;
    if(has_back!=0) {
        uint back=in_place?pixels[outputIndex]:back_pixels[id.y*width+id.x];
        if(!have_pixel) packed=(packed&0x01ff0000U)|(back&0x9c00ffffU);
#if !defined(STARFOX_PIXEL_ONLY_RASTER)
        if(want_depth!=0 && !have_pixel && has_back_depth!=0) depth=in_place?geometry_depth[outputIndex]:back_depth[id.y*width+id.x];
        if((packed&0x01000000U)==0 && has_back_surface!=0 && (back&0x01000000U)!=0) {
            packed=(packed&0x9c00ffffU)|(back&0x01ff0000U);
            surface=in_place?surfaces[outputIndex]:back_surfaces[id.y*width+id.x];
        }
#endif
    }
    // Emissive colour must erase hidden receiver metadata too. Transparent
    // texels retain the background unmodified, exactly like GpuScene.
#if !defined(STARFOX_PIXEL_ONLY_RASTER)
    if((painterFlags&2u)!=0 && have_pixel) {
        packed&=0x9c00ffffu;surface=float4(0,0,1,0);
    }
#endif
    pixels[outputIndex]=packed;
#if !defined(STARFOX_PIXEL_ONLY_RASTER)
    if(want_surface!=0) surfaces[outputIndex]=surface;
    if(want_depth!=0) geometry_depth[outputIndex]=depth;
#endif
}
