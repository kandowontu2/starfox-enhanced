// Per-eye drawn/neighbourhood styles. The caller supplies the actual composed
// colour (including rays) and independent raster ownership, never CPU pixels.
uint effect_layer(uint2 p) {return uint(round(receiver.Load(int3(p,0)).b*255.));}
uint2 effect_bounded(int2 p) {return uint2(clamp(p,int2(0,0),int2(width-1,height-1)));}
int3 effect_rgb(uint2 p) {return authored_rgb(finished_colour(p));}
int effect_light(uint2 p) {return dot(effect_rgb(p),int3(77,150,29))/256;}
#define FX_MIN min
#define FX_MAX max
#include "../../../include/starfox/render/special_fx.inc"
#undef FX_MIN
#undef FX_MAX
// Each tap must stay in its source group. Missing/out-of-group taps use the
// centre colour, exactly like the flat CPU reference; alpha is never sampled.
int3 warp_sample(int2 at,uint2 centre,uint layer) {
    uint2 p=effect_bounded(at);uint neighbour=effect_layer(p);
    return neighbour==0 || (neighbour==1)!=(layer==1)?effect_rgb(centre):effect_rgb(p);
}
int warp_wave(int phase) {int ramp=16-abs(phase%64-32);return ramp*(32-abs(ramp))/16;}
int3 native_warp_colour(uint effect,uint2 p,int3 rgb,uint layer) {
    int step=int(materialScale),x=int(p.x),y=int(p.y),w=int(width),h=int(height);
    if(effect>=84 && effect<=91) {
        uint2 right=min(p+uint2(materialScale,0),uint2(width-1,height-1));
        uint2 below=min(p+uint2(0,materialScale),uint2(width-1,height-1));
        float edge=effect_layer(right)!=layer || effect_layer(below)!=layer
            || abs(dot(rgb,int3(77,150,29))/256-effect_light(right))>28?1.:0.;
        SpecialFxSample fx=special_fx_sample(effect,float(x)/step,float(y)/step,float(w)/(2*step),float(h)/(2*step),effectSeconds,edge);
        float2 at=clamp(float2(p)+float2(fx.dx,fx.dy)*step,float2(0,0),float2(w-1,h-1));
        int2 low=int2(at);float2 weight=at-float2(low);float3 sample=0;
        for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx)
            sample+=float3(warp_sample(low+int2(dx,dy),p,layer))*(dx?weight.x:1-weight.x)*(dy?weight.y:1-weight.y);
        // CPU lround is half-away-from-zero; samples are nonnegative.
        return int3(floor(clamp(sample*fx.keep+255.*float3(fx.r,fx.g,fx.b)*fx.gain,0.,255.)+.5));
    }
    uint order[8]={0,1,2,3,4,5,6,7};int block=x/(step*8)*step*8;
    if(effect==45) {
        int lights[8];for(int n=0;n<8;++n) lights[n]=dot(warp_sample(int2(block+n*step,y),p,layer),int3(77,150,29))/256;
        for(int a=1;a<8;++a) {
            uint key=order[a];int b=a;
            while(b>0 && lights[order[b-1]]>lights[key]) {order[b]=order[b-1];--b;}
            order[b]=key;
        }
    }
    int3 result=rgb;
    for(int channel=0;channel<3;++channel) {
        int sx=x,sy=y,dx=x-w/2,dy=y-h/2;
        if(effect==43) {sx=abs(x*2-w+1);sy=abs(y*2-h+1);}
        else if(effect==44) sx+=(channel-1)*step*6;
        else if(effect==45) sx=block+int(order[(x/step)%8])*step+x%step;
        else if(effect==48) {
            int size=step*16,tx=x/size,ty=y/size,lx=x%size,ly=y%size;
            switch((tx*3+ty*5)%4) {
            case 0:sx=tx*size+ly;sy=ty*size+size-1-lx;break;
            case 1:sx=tx*size+size-1-lx;sy=ty*size+size-1-ly;break;
            case 2:sx=tx*size+size-1-ly;sy=ty*size+lx;break;
            }
        } else if(effect==49) {uint column=p.x/(materialScale*3);int drip=int(((column*13)^(column>>1))%32)*step;sy-=drip*y/max(1,h-1);}
        else if(effect==50) {sx+=warp_wave(y/step)*step;sy+=warp_wave(x/step)*step/4;}
        else if(effect==51) {int nx=dx*128/max(1,w),ny=dy*128/max(1,h),lens=192+(nx*nx+ny*ny)/64;sx=w/2+dx*lens/256;sy=h/2+dy*lens/256;}
        else if(effect==52) {int size=step*12;sy=y/size*size+(y%size)/2+size/4;sx+=(y/size%2?1:-1)*step*6;}
        else if(effect==53) {int size=step*24,tx=x/size,ty=y/size;if((tx+ty)%2) sx=tx*size+size-1-x%size;else sy=ty*size+size-1-y%size;}
        else if(effect==58) {int turn=clamp(96-(abs(dx)+abs(dy))/step,0,96);sx-=dy*turn/128;sy+=dx*turn/128;}
        else if(effect==59) {int radius=max(abs(dx),abs(dy))+min(abs(dx),abs(dy))*3/8,wave=16-abs((radius/step)%64-32);sx+=dx*wave*step/max(step,radius);sy+=dy*wave*step/max(step,radius);}
        else if(effect==60) {int size=step*32,side=(x%size+y%size<size)?1:-1;sx+=side*step*12;sy-=side*step*8;}
        result[channel]=warp_sample(int2(sx,sy),p,layer)[channel];
    }
    return result;
}
bool native_post_edge(uint effect,uint2 p,int3 rgb) {
    int light=dot(rgb,int3(77,150,29))/256;bool edge=false;
    if(effect==1 || effect==12) {
        const int2 offsets[4]={int2(-1,0),int2(1,0),int2(0,-1),int2(0,1)};
        for(uint i=0;i<4;++i) {
            uint2 n=effect_bounded(int2(p)+offsets[i]*int(materialScale));
            if(effect_layer(n)!=0) edge=edge || light-effect_light(n)>40;
        }
    } else if(effect==2 || effect==3 || effect==6 || effect==13 || effect==34 || effect==37
        || effect==39 || effect==41 || effect==66 || effect==67 || effect==68) {
        uint2 right=min(p+uint2(materialScale,0),uint2(width-1,height-1));
        uint2 below=min(p+uint2(0,materialScale),uint2(width-1,height-1));
        edge=abs(light-effect_light(right))>28 || abs(light-effect_light(below))>28;
    }
    return edge;
}
bool native_edge_style(uint effect) {
    return effect==1 || effect==2 || effect==3 || effect==6 || effect==12 || effect==13
        || effect==34 || effect==37 || effect==39 || effect==41 || effect==66 || effect==67 || effect==68;
}
int3 native_post_colour(uint effect,uint2 p,int3 rgb,uint layer,out uint witness) {
    witness=0;
    if((effect>=43 && effect<=45) || (effect>=48 && effect<=53) || (effect>=58 && effect<=60) || (effect>=84 && effect<=91))
        return native_warp_colour(effect,p,rgb,layer);
    int light=dot(rgb,int3(77,150,29))/256;
    bool edge=native_post_edge(effect,p,rgb);
    if(native_edge_style(effect)) {
        bool grid=(effect==6 && !edge && ((p.x/materialScale)%16==0 || (p.y/materialScale)%16==0))
            || (effect==39 && (p.y/materialScale)%4==3);
        witness=1U+(edge?2U:0U)+(grid?4U:0U);
    }
    int3 value=rgb;
    if(effect==1 || effect==12) {
        int peak=max(1,max(rgb.r,max(rgb.g,rgb.b)));
        int band=min(255,((peak+(effect==1?25:31))/(effect==1?51:64))*(effect==1?51:64));
        return edge?rgb/(effect==1?4:5):(rgb*band+peak/2)/peak;
    }
    if(effect==2) return edge?16:light<30?24:235;
    if(effect==3) return edge?int3(35,255,255):rgb/5;
    if(effect==5) {
        const int bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
        return clamp((rgb*4+bayer[((p.y/materialScale)%4)*4+(p.x/materialScale)%4]*16)/256,0,4)*255/4;
    }
    if(effect==6) return edge?int3(130,220,255):int3(8,24,58)+(((p.x/materialScale)%16==0 || (p.y/materialScale)%16==0)?12:0);
    if(effect==7 || effect==33 || effect==40 || effect==42) {
        int3 wash=0,glow=0;int weight=0;
        int counts[4]={0,0,0,0};int3 sums[4]={int3(0,0,0),int3(0,0,0),int3(0,0,0),int3(0,0,0)};
        int2 centre=effect==40?int2(p/(materialScale*3)*(materialScale*3)+materialScale):int2(p);
        for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
            uint2 n=effect_bounded(centre+int2(dx,dy)*int(materialScale));
            if(effect_layer(n)==0 || (effect_layer(n)==1)!=(layer==1)) continue;
            int3 sample=effect_rgb(n);int luma=dot(sample,int3(77,150,29))/256;
            if(effect==7) {
                int peak=max(sample.r,max(sample.g,sample.b));
                if(peak>160) glow+=sample*(peak-160)/95;
            } else if(effect==33) {
                if(abs(light-luma)>32) continue;
                int w=dx==0 && dy==0?4:1;weight+=w;wash+=sample*w;
            } else {
                int bin=effect==40?0:luma/64;++counts[bin];sums[bin]+=sample;
            }
        }
        if(effect==7) return min(255,rgb+glow/12);
        if(effect==33) return 24+min(255,((wash/max(1,weight)+15)/32)*32)*7/8;
        int best=0;for(int bin=1;bin<4;++bin) if(counts[bin]>counts[best]) best=bin;
        wash=counts[best]?sums[best]/counts[best]:rgb;
        return effect==40?((p.x/materialScale)%3==0 || (p.y/materialScale)%3==0?wash*2/3:min(255,wash+10))
            :clamp((wash-128)*6/5+128,0,255);
    }
    if(effect==10) {value=int3(light/7,min(255,24+light*6/5),light/4);return (p.y/materialScale)%2?value*4/5:value;}
    if(effect==13) return edge?int3(255,75,255):(int3(55,8,100)*(255-light)+int3(70,250,245)*light)/255;
    if(effect==25) return (p.y/materialScale)%2?rgb*3/5:rgb;
    if(effect==34) return edge?235:12+light/10;
    if(effect==35) {
        uint2 n=uint2(p.x>=materialScale?p.x-materialScale:0,p.y);
        int neighbour=effect_layer(n)==0?light:effect_light(n);
        return clamp(128+2*(light-neighbour),0,255);
    }
    if(effect==37) return edge?8:clamp(((rgb*5/4-24+31)/64)*64+16,0,255);
    if(effect==39) {int energy=edge?255:24+light*3/4;value=energy*int3(32,210,255)/255;return (p.y/materialScale)%4==3?value*2/5:value;}
    if(effect==41) return edge?32:clamp(246-(255-light)*(255-light)/510,0,255);
    if(effect==66) return edge || light<78?int3(25,30,29):light<166?int3(180,115,73):int3(248,224,170);
    if(effect==67) return clamp(int3(5,25,47)+(255-light)*int3(80,185,220)/255+(edge?70:0),0,255);
    if(effect==68) {
        int3 ink=rgb.b>rgb.r && rgb.b>rgb.g?int3(29,207,235):rgb.g>rgb.r?int3(243,48,135):int3(255,220,35);
        return edge?22:light<72?ink/3:ink;
    }
    if(effect==70) {
        uint stripe=(p.x/materialScale)%3;
        value=clamp(rgb*int3(stripe==0?115:43,stripe==1?115:43,stripe==2?115:43)/100,0,255);
        return (p.y/materialScale)%3==2?value*3/4:value;
    }
    return value;
}
float4 post_colour(uint2 p,out uint witness) {
    witness=0;
    float4 result=finished_colour(p);
    if((postEffects.x==0 || postEffects.z==0) && (postEffects.y==0 || postEffects.w==0)) return result;
    uint layer=effect_layer(p);
    uint effect=layer==1?postEffects.x:layer==2?postEffects.y:0;
    uint intensity=layer==1?postEffects.z:postEffects.w;
    if(effect!=0 && intensity!=0) {
        float4 palette;
        if(calibrated_palette_colour(result,uint4(effect,intensity,(flags&4U)!=0?1U:0U,0),palette)) return palette;
        int3 rgb=authored_rgb(result),value=native_post_colour(effect,p,rgb,layer,witness);
        float3 encoded=float3((rgb*int(100-intensity)+value*int(intensity)+50)/100)/255.;
        result.rgb=(flags&4U)!=0?calibrated_decode_srgb(encoded):encoded;
    }
    return result;
}
float4 post_colour(uint2 p) {uint unused;return post_colour(p,unused);}
