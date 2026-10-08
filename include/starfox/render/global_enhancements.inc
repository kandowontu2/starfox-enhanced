// Included inside CPU/HLSL functions. Values are linear combinations of the
// current frame, not temporal history; pause and stereo never share feedback.
float nx=(x+.5f)/w*2.f-1.f,ny=(y+.5f)/h*2.f-1.f;
float sx=x,sy=y;
float q=float((packed>>22)&3U)/3.f;
if(q>0.f) {float r=nx*nx+ny*ny;sx+=nx*r*q*w*.035f;sy+=ny*r*q*h*.035f;}
float original=G_SAMPLE(sx,sy);
q=float((packed>>10)&3U)/3.f;
if(q>0.f) {
    float hot=G_MAX(0.f,original-.45f);
    sx+=sin(y/(step*11.f)-seconds*2.1f)*hot*q*step*3.f;
    sy+=sin(x/(step*19.f)+seconds*1.4f)*hot*q*step;
}
float value=G_SAMPLE(sx,sy);
q=float((packed>>6)&3U)/3.f;
float sharp=float((packed>>12)&3U)/3.f;
float halo=float((packed>>4)&3U)/3.f;
if(q+sharp+halo>0.f) {
    float radius=step*(1.f+q*2.f+halo*2.f);
    float blur=(G_SAMPLE(sx-radius,sy)+G_SAMPLE(sx+radius,sy)
        +G_SAMPLE(sx,sy-radius)+G_SAMPLE(sx,sy+radius))*.25f;
    value=value*(1.f-q*.55f)+blur*q*.55f+(value-blur)*sharp*.65f;
    value+=G_MAX(0.f,blur-.55f)*halo*(channel==0?.65f:channel==1?.24f:.1f);
}
q=float((packed>>8)&3U)/3.f;
if(q>0.f) {
    float total=0.f;
    for(int j=1;j<=4;++j) {float k=float(j)*q*.012f;total+=G_SAMPLE(sx-(sx-w*.5f)*k,sy-(sy-h*.5f)*k);}
    value=value*(1.f-q*.65f)+total*.25f*q*.65f;
}
q=float((packed>>2)&3U)/3.f;
if(q>0.f) {
    float light=0.f;
    for(int j=1;j<=6;++j) {
        float offset=float(j*j)*step*1.5f;
        light+=(G_MAX(0.f,G_SAMPLE(sx-offset,sy)-.78f)+G_MAX(0.f,G_SAMPLE(sx+offset,sy)-.78f))/float(j+1);
    }
    value+=light*q*(channel==0?.24f:channel==1?.55f:1.f);
}
q=float(packed&3U)/3.f;
if(q>0.f) {
    float light=0.f;
    for(int j=1;j<=8;++j) {
        float k=float(j)*.055f;
        light+=G_MAX(0.f,G_SAMPLE(sx+(w*.65f-sx)*k,sy+(h*.12f-sy)*k)-.72f)*(1.f-k);
    }
    value+=light*q*.14f;
}
q=float((packed>>24)&3U)/3.f;
if(q>0.f) {
    float light=0.f;
    for(int j=1;j<=3;++j) {
        float k=float(j)*.35f;
        light+=G_MAX(0.f,G_SAMPLE(w*.5f-(sx-w*.5f)*k,h*.5f-(sy-h*.5f)*k)-.8f)/float(j);
    }
    value+=light*q*(channel==0?.65f:channel==1?.4f:.8f);
}
q=float((packed>>14)&3U)/3.f;
value*=1.f-q*G_MIN(1.f,(nx*nx+ny*ny)*.36f);
q=float((packed>>16)&3U)/3.f;
if(q>0.f) {
    uint seed=uint(x/step)*1973U+uint(y/step)*9277U+uint(floor(seconds*24.f))*26699U;
    seed=(seed^(seed>>13))*1274126177U;seed^=seed>>16;
    value+=(float(seed&1023U)/1023.f-.5f)*q*.065f;
}
q=float((packed>>18)&3U)/3.f;
value*=1.f-q*(.1f+.1f*sin((y+.5f)/step*3.14159265f));
q=float((packed>>20)&3U)/3.f;
if(q>0.f && uint(x/step)%3U!=channel) value*=1.f-q*.22f;
return G_MIN(1.f,G_MAX(0.f,value));
