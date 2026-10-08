// Native surface finishes use the existing flat material equations, including
// their authored-colour luminance and actual neighbouring composed samples.
// This is a surface-finish pass, not physical glass refraction/transmission.
int3 calibrated_material_colour(uint material,int3 rgb,bool edge) {
    int light=(rgb.r*77+rgb.g*150+rgb.b*29)/256;
    int shine=max(0,light-128),specular=shine*shine/64;
    int3 value=rgb;
    if(material>=54 && material<=57) {
        int3 phase=(light*2+int3(0,128,256))%384;
        int3 spectrum=clamp(255-abs(phase-192)*4,0,255);
        if(material==54) value=min(255,24+spectrum*3/4+specular/2+(edge?48:0));
        else if(material==55) value=min(255,rgb/8+light/4+int3(30,65,85)+specular/2+(edge?75:0));
        else if(material==56) value=min(255,int3(9,7,18)+light/16+specular*3/4+(edge?36:0));
        else value=min(255,125+light/4+spectrum/5+specular/4+(edge?18:0));
    } else if(material==61) value=min(255,int3(40,3,12)+light*int3(180,12,42)/255
        +specular*int3(255,115,145)/255+(edge?int3(50,15,22):0));
    else if(material==62) value=min(255,int3(6,30,20)+light*int3(65,145,90)/255
        +specular*int3(120,230,170)/255+(edge?int3(20,35,25):0));
    else if(material==63) value=min(255,25+light*int3(200,190,166)/255+specular/3+(edge?18:0));
    else if(material>=74 && material<=83) {
        shine=max(0,light-105);specular=shine*shine/90;
        int3 base=0,diffuse=0,highlight=0;int edgeLight=0;
        if(material==74) {base=int3(31,36,46);diffuse=int3(175,190,208);highlight=int3(235,245,255);edgeLight=20;}
        else if(material==75) {base=int3(55,37,8);diffuse=int3(165,127,42);highlight=int3(255,224,120);edgeLight=14;}
        else if(material==76) {base=int3(53,24,25);diffuse=int3(177,112,99);highlight=int3(255,205,191);edgeLight=16;}
        else if(material==77) {base=int3(20,30,42);diffuse=int3(117,145,171);highlight=int3(185,220,255);edgeLight=22;}
        else if(material==78) {base=int3(24,8,40);diffuse=int3(108,43,156);highlight=int3(212,129,255);edgeLight=28;}
        else if(material==79) {base=int3(6,18,54);diffuse=int3(26,85,167);highlight=int3(110,190,255);edgeLight=26;}
        else if(material==80) {
            int3 spectrum=clamp(180-abs((light*3+int3(0,107,214))%360-180),0,180);
            return clamp(80+light/3+spectrum/3+specular/2+(edge?20:0),0,255);
        } else if(material==81) {
            int vein=abs(rgb.r-rgb.g)+abs(rgb.g-rgb.b);
            return clamp(178+light/5-vein/5+int3(8,5,0)+specular/4+(edge?10:0),0,255);
        } else if(material==82) {base=int3(11,14,18);diffuse=int3(56,67,78);highlight=int3(148,165,181);edgeLight=12;}
        else {
            int ember=max(0,light-65)*2;
            return clamp(int3(24,8,5)+ember*int3(220,68,12)/255
                +specular*int3(255,171,85)/255+(edge?18:0),0,255);
        }
        value=clamp(base+light*diffuse/255+specular*highlight/255+(edge?edgeLight:0),0,255);
    }
    return value;
}
