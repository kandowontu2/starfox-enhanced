#pragma once
#include "starfox/render/environment_effects.hpp"
#include <stdexcept>
#include <iostream>

// Test-only screen-space optical reference. Scenery is finished with reflection
// disabled, then independently bilinearly sampled: never feed a reflected pixel
// back into the same presentation or accept a pre-enhancement sky as its source.
template<class Render>
void check_environment_screen_reflection_cases(Render&& render) {
    using namespace starfox::render;
    const auto check=[](bool ok,const char* message) {
        if(!ok) throw std::runtime_error(message);
    };
    BackdropImage sky;sky.width=16;sky.height=8;
    for(unsigned y=0;y<8;++y) for(unsigned x=0;x<16;++x)
        sky.pixels.push_back(0xff000000u|(24+x*3)|((120+y*11)<<8)|((210+x*2)<<16));
    unsigned cases=0;std::uint64_t reflected_pixels=0,protected_pixels=0;
    for(unsigned material:{1U,6U,7U,8U}) for(unsigned scale:{1U,3U,6U})
    for(float bank:{-.25F,0.F,.25F}) for(bool photographic:{false,true}) {
        Framebuffer frame(40,32,scale);frame.enable_layer_tags(true);
        const unsigned width=frame.stored_width(),height=frame.stored_height();
        std::vector<std::uint8_t> source(width*height*4);
        EnvironmentEffects e;e.modes={material,0,1,1};
        e.motion={16,31,-47,1.25F};e.plane={bank,5,0,1};
        e.classes[1]=5;e.classes[2]=6;e.classes[3]=7;
        e.scroll_fraction={.23F,-.17F,.375F,0};
        if(photographic) e.backdrop=&sky;
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const unsigned i=y*width+x;
            const float logical_x=(float(x)+.5F)/scale-20.F,logical_y=(float(y)+.5F)/scale;
            const bool ground=logical_y>=16+bank*logical_x;
            frame.pixels()[i]=ground?1:2;
            frame.layer_tags()[i]=unsigned(PixelLayer::background);
            source[i*4]=ground?75:170;source[i*4+1]=ground?42:18;source[i*4+2]=ground?20:9;
            // Foreground model, protected UI, terrain geometry and a retained
            // celestial ink all intersect the reflected source/destination.
            if(x/scale>=7 && x/scale<10) frame.layer_tags()[i]=unsigned(PixelLayer::three_d);
            if(x/scale>=12 && x/scale<15) frame.layer_tags()[i]=unsigned(PixelLayer::two_d);
            if(x/scale>=18 && x/scale<20) frame.layer_tags()[i]=unsigned(PixelLayer::terrain_geometry);
            if(x/scale>=26 && x/scale<28) frame.pixels()[i]=3;
            if(frame.layer_tags()[i]!=unsigned(PixelLayer::background) || frame.pixels()[i]==3) {
                source[i*4]=(x*19+y*7)&255;source[i*4+1]=(x*11+y*17)&255;source[i*4+2]=(x*3+y*23)&255;
            }
            source[i*4+3]=(i*7+31)&255;
        }
        auto finished=source;apply_environment(e,frame,finished);
        unsigned changed_sky=0;
        for(unsigned i=0;i<width*height;++i)
            if(frame.layer_tags()[i]==unsigned(PixelLayer::background) && frame.pixels()[i]==2
                && (finished[i*4]!=source[i*4] || finished[i*4+1]!=source[i*4+1]
                    || finished[i*4+2]!=source[i*4+2])) ++changed_sky;
        check(changed_sky>10,"Screen reflection fixture did not change its sky");
        // Qualify shading separately, then use this renderer's exact completed
        // UNORM source bytes for the optical reference. Otherwise a permitted
        // one-quantum shader shade difference can be counted again after the
        // second pass, obscuring whether its tap coordinates are actually wrong.
        const auto completed=render(frame,source,e);
        check(completed.size()==finished.size(),"Environment shade changed image extent");
        for(unsigned i=0;i<completed.size();++i)
            check(std::abs(int(completed[i])-int(finished[i]))<=1,
                "Screen reflection source shade differs from the CPU reference");
        auto expected=completed;
        for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x) {
            const unsigned i=y*width+x,kind=e.classes[frame.pixels()[i]];
            const float px=(float(x)+.5F)/scale-20.F,py=(float(y)+.5F)/scale;
            const float distance=py-e.motion[0]-bank*px;
            if(frame.layer_tags()[i]!=unsigned(PixelLayer::background)
                || kind<1 || kind>5 || distance<=0) {++protected_pixels;continue;}
            const float offset=(material>=7?2.F:1.55F)*distance/(1+bank*bank);
            const float sx=std::clamp(float(x)+offset*bank*scale
                +(material>=7?0:std::sin(py*.12F+e.motion[3])*(2*scale)),0.F,float(width-1));
            const float sy=std::clamp((py-offset)*scale-.5F,0.F,float(height-1));
            const unsigned ax=unsigned(sx),ay=unsigned(sy);
            std::array<float,3> sum{};float weight=0;
            for(unsigned by=0;by<2;++by) for(unsigned bx=0;bx<2;++bx) {
                const unsigned tap=std::min(ay+by,height-1)*width+std::min(ax+bx,width-1);
                if(frame.layer_tags()[tap]==unsigned(PixelLayer::two_d)) continue;
                const float w=(bx?sx-ax:1-(sx-ax))*(by?sy-ay:1-(sy-ay));
                weight+=w;
                for(unsigned c=0;c<3;++c) sum[c]+=completed[tap*4+c]*w;
            }
            if(weight<=0) continue;
            const float amount=material>=7?.8F:.15F+.20F*std::clamp(1-distance/200.F,0.F,1.F);
            constexpr float gold[]{1,.875F,.58F};
            for(unsigned c=0;c<3;++c) expected[i*4+c]=std::uint8_t(float(completed[i*4+c])*(1-amount*weight)
                +sum[c]*(material==8?gold[c]:1)*amount+.5F);
            ++reflected_pixels;
        }
        e.water_reflections=true;
        const auto actual=render(frame,source,e);
        check(actual.size()==expected.size(),"Screen reflection changed image extent");
        for(unsigned i=0;i<actual.size();++i) {
            if(i%4==3) check(actual[i]==source[i],"Screen reflection changed source alpha");
            else if(std::abs(int(actual[i])-int(expected[i]))>1)
                throw std::runtime_error("Screen reflection sampled unfinished scenery: material="
                    +std::to_string(material)+" scale="+std::to_string(scale)+" bank="+std::to_string(bank)
                    +" photo="+std::to_string(photographic)+" byte="+std::to_string(i)
                    +" actual="+std::to_string(actual[i])+" expected="+std::to_string(expected[i]));
        }
        e.ray_water=true;
        auto ray_expected=source;apply_environment(e,frame,ray_expected);
        e.water_reflections=false;
        auto off=source;apply_environment(e,frame,off);
        check(ray_expected==off,"Physical ray finish also acquired a screen-space reflection");
        ++cases;
    }
    check(reflected_pixels>10000 && protected_pixels>10000,"Incomplete screen reflection fixture");
    std::cout<<"Finished-scenery screen reflections: "<<cases<<" cases, "<<reflected_pixels
        <<" independently mixed pixels and "<<protected_pixels<<" excluded/protected destinations; "
        <<"Auto water/water/mirror/gold, bank, scale, scroll, sky, foreground, HUD and alpha pass\n";
}
