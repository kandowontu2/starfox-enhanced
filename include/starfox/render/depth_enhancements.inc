float value=D_COLOUR(x,y);
float depth=D_SURFACE(x,y,3);
if(depth<=0.f) return value;
float dof=float((modes>>2)&3U)/3.f;
if(dof>0.f) {
    float focus=D_MAX(32.f,D_CAMERA(3));
    float coc=D_MIN(1.f,D_MAX(0.f,abs(depth-focus)-focus*.12f)/D_MAX(focus,depth));
    float radius=coc*dof*3.f*scale;
    if(radius>.25f) {
        float total=0.f,weight=0.f;
        // Dense Gaussian taps avoid the distinct duplicate silhouettes of a
        // sparse ring gather, especially on the original low-resolution art.
        for(int j=-2;j<=2;++j) for(int k=-2;k<=2;++k) {
            float sx=x+float(j)*radius*.5f,sy=y+float(k)*radius*.5f,z=D_SURFACE(sx,sy,3);
            if(z>0.f && abs(z-depth)<D_MAX(48.f,depth*.1f)) {
                float w=(j==0?6.f:abs(j)==1?4.f:1.f)*(k==0?6.f:abs(k)==1?4.f:1.f);
                total+=D_COLOUR(sx,sy)*w;weight+=w;
            }
        }
        value=total/D_MAX(1.f,weight);
    }
}
return D_MIN(1.f,D_MAX(0.f,value*ambient));
