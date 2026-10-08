// Independent byte-domain spatial reference. It consumes separately rendered
// geometric labels, never the native renderer's private AA ownership target.
std::vector<unsigned char> native_spatial_aa_oracle(const std::vector<unsigned char>& input,
    const starfox::render::Framebuffer& labels,unsigned type,unsigned quality) {
    auto output=input;const unsigned w=labels.width(),h=labels.height();
    const auto eligible=[&](unsigned pixel) {return labels.layer_tags()[pixel]==unsigned(starfox::render::PixelLayer::three_d);};
    for(unsigned y=1;y+1<h;++y) for(unsigned x=1;x+1<w;++x) {
        const unsigned pixel=y*w+x,at=pixel*4;if(!eligible(pixel)) continue;
        std::array<unsigned,5> taps{pixel,pixel-1,pixel+1,pixel-w,pixel+w};std::array<int,5> light{};
        for(unsigned tap=0;tap<5;++tap) {
            auto& sample=taps[tap];if(!eligible(sample)) sample=pixel;
            light[tap]=(int(input[sample*4])*77+int(input[sample*4+1])*150+int(input[sample*4+2])*29)/256;
        }
        const auto [low,high]=std::minmax_element(light.begin(),light.end());
        const int floor=quality==1?20:quality==2?12:6,divisor=quality==1?6:quality==2?8:12;
        if(*high-*low<std::max(floor,*high/divisor)) continue;
        const bool vertical=type==1?std::abs(light[3]+light[4]-2*light[0])<std::abs(light[1]+light[2]-2*light[0])
            :std::abs(light[1]-light[2])>=std::abs(light[3]-light[4]);
        const unsigned weight=quality==1?6:quality==2?2:1;
        for(unsigned c=0;c<3;++c) {
            unsigned sum=input[at+c]*weight;
            if(type==2) for(unsigned tap=1;tap<5;++tap) sum+=input[taps[tap]*4+c];
            else sum+=input[taps[vertical?3:1]*4+c]+input[taps[vertical?4:2]*4+c];
            output[at+c]=static_cast<unsigned char>(sum/(weight+(type==2?4:2)));
        }
    }
    return output;
}
