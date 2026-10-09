// Independent scalar EASU/RCAS oracle, transcribed from AMD's MIT-licensed
// third_party/fsr1 equations. No production GPU result or adapter is reused.
// Includes the actual half-float intermediate quantization and clamp sampler.
namespace fsr_scalar {
using Rgb=std::array<float,3>;
float bits_float(unsigned bits){float f;std::memcpy(&f,&bits,4);return f;}
unsigned float_bits(float f){unsigned bits;std::memcpy(&bits,&f,4);return bits;}
float low_rcp(float f){return bits_float(0x7ef07ebbU-float_bits(f));}
float med_rcp(float f){float b=bits_float(0x7ef19fffU-float_bits(f));return b*(2-b*f);}
float low_rsq(float f){return bits_float(0x5f347d74U-(float_bits(f)>>1));}
float half(float value){
    value=std::clamp(value,0.F,1.F);int exponent{};std::frexp(value,&exponent);
    const float step=std::ldexp(1.F,std::max(-24,exponent-11));return std::nearbyint(value/step)*step;
}
std::vector<Rgb> upscale(const std::vector<unsigned char>& bytes,unsigned iw,unsigned ih,unsigned ow,unsigned oh,
    bool lower_x=false,bool lower_y=false,bool linear_output=false) {
    const auto load=[&](int x,int y){Rgb c{};unsigned at=(std::clamp(y,0,int(ih)-1)*iw+std::clamp(x,0,int(iw)-1))*4;
        for(unsigned k=0;k<3;++k)c[k]=bytes[at+k]/255.F;return c;};
    std::vector<Rgb> easu(ow*oh),result(ow*oh);
    constexpr int offsets[12][2]{{0,-1},{1,-1},{-1,1},{0,1},{0,0},{-1,0},{1,1},{2,1},{2,0},{1,0},{1,2},{0,2}};
    for(unsigned y=0;y<oh;++y)for(unsigned x=0;x<ow;++x){
        // Mirror the constants' reciprocal/multiply order (not integer/fp
        // division). One ULP at an integer centre changes the 12-tap stencil.
        float rx=float(iw)*(1.F/ow),ry=float(ih)*(1.F/oh);
        float px=std::fma(float(x),rx,.5F*float(iw)*(1.F/ow)-.5F);
        float py=std::fma(float(y),ry,.5F*float(ih)*(1.F/oh)-.5F);
        // Exact integer source centres are a floating-point stencil boundary.
        // D3D/Vulkan fused MAD may approach from either side. The fixture
        // independently checks both limiting filters, not a larger RGB tolerance.
        if(std::abs(px-std::round(px))<.00001F)px=std::round(px)-(lower_x?.00001F:0.F);
        if(std::abs(py-std::round(py))<.00001F)py=std::round(py)-(lower_y?.00001F:0.F);
        int fx=int(std::floor(px)),fy=int(std::floor(py));px-=fx;py-=fy;
        std::array<Rgb,12> c;std::array<float,12> l{};
        for(unsigned t=0;t<12;++t){c[t]=load(fx+offsets[t][0],fy+offsets[t][1]);l[t]=c[t][2]*.5F+c[t][0]*.5F+c[t][1];}
        float dx=0,dy=0,len=0;
        const auto set=[&](float w,float a,float b,float centre,float d,float e){
            float vx=d-b,vy=e-a;dx+=vx*w;dy+=vy*w;
            float lx=std::clamp(std::abs(vx)*low_rcp(std::max(std::abs(d-centre),std::abs(centre-b))),0.F,1.F);
            float ly=std::clamp(std::abs(vy)*low_rcp(std::max(std::abs(e-centre),std::abs(centre-a))),0.F,1.F);
            len+=(lx*lx+ly*ly)*w;};
        set((1-px)*(1-py),l[0],l[5],l[4],l[9],l[3]);
        set(px*(1-py),l[1],l[4],l[9],l[8],l[6]);
        set((1-px)*py,l[4],l[2],l[3],l[6],l[11]);
        set(px*py,l[9],l[3],l[6],l[7],l[10]);
        float d2=dx*dx+dy*dy;if(d2<1.F/32768){dx=1;dy=0;}else {float r=low_rsq(d2);dx*=r;dy*=r;}
        len*=.5F;len*=len;float stretch=(dx*dx+dy*dy)*low_rcp(std::max(std::abs(dx),std::abs(dy)));
        float ax=1+(stretch-1)*len,ay=1-.5F*len,lob=.5F+(.21F-.5F)*len,clp=low_rcp(lob);
        Rgb acc{};float sum=0;
        for(unsigned t=0;t<12;++t){float ox=offsets[t][0]-px,oy=offsets[t][1]-py;
            float vx=(ox*dx+oy*dy)*ax,vy=(-ox*dy+oy*dx)*ay,d=std::min(vx*vx+vy*vy,clp);
            float b=.4F*d-1,a=lob*d-1,w=(1.5625F*b*b-.5625F)*a*a;sum+=w;
            for(unsigned k=0;k<3;++k)acc[k]+=c[t][k]*w;}
        for(unsigned k=0;k<3;++k){float minimum=std::min({c[4][k],c[9][k],c[3][k],c[6][k]});
            float maximum=std::max({c[4][k],c[9][k],c[3][k],c[6][k]});easu[y*ow+x][k]=half(std::clamp(acc[k]/sum,minimum,maximum));}
    }
    const auto sample=[&](int x,int y){return easu[std::clamp(y,0,int(oh)-1)*ow+std::clamp(x,0,int(ow)-1)];};
    for(unsigned y=0;y<oh;++y)for(unsigned x=0;x<ow;++x){
        auto b=sample(x,int(y)-1),d=sample(int(x)-1,y),e=sample(x,y),f=sample(int(x)+1,y),h=sample(x,int(y)+1);float lobe=-1;
        for(unsigned k=0;k<3;++k){float mn=std::min({b[k],d[k],f[k],h[k]}),mx=std::max({b[k],d[k],f[k],h[k]});
            float hit_min=std::min(mn,e[k])/(4*mx),hit_max=(1-std::max(mx,e[k]))/(4*mn-4);
            // HLSL min/max ignore a NaN operand; handle constant zero/one.
            float lk=std::isnan(hit_min)?hit_max:std::isnan(hit_max)?-hit_min:std::max(-hit_min,hit_max);
            lobe=std::max(lobe,lk);}
        lobe=std::max(-.1875F,std::min(lobe,0.F))*std::exp2(-.2F);float r=med_rcp(4*lobe+1);
        for(unsigned k=0;k<3;++k){float value=std::clamp((lobe*b[k]+lobe*d[k]+lobe*h[k]+lobe*f[k]+e[k])*r,0.F,1.F);
            // sRGB RCAS decodes BEFORE storing its final half-float image;
            // quantizing encoded RGB first would introduce a second rounding.
            if(linear_output)value=value<=.04045F?value/12.92F:std::pow((value+.055F)/1.055F,2.4F);
            result[y*ow+x][k]=half(value);}
    }return result;
}
}
