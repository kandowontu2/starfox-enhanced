float value=S_SAMPLE(x,y);
for(int n=0;n<int(S_CAMERA(3));++n) {
    int at=n*3;
    float dx=x-S_DATA(at,0),dy=y-S_DATA(at,1),r=S_DATA(at,2),strength=S_DATA(at,3);
    float type=S_DATA(at+1,0),z=S_DATA(at+1,1);
    float stretch=type==4?5.f:1.f;
    if(abs(dx)>r+4*scale || abs(dy)>(r+4*scale)*stretch) continue;
    float distance=sqrt(dx*dx+dy*dy/(stretch*stretch));
    if(type==1) {
        if(depth<=0) continue;
        float px=(x-S_CAMERA(0))*depth/S_CAMERA(2),py=(y-S_CAMERA(1))*depth/S_CAMERA(2);
        float lx=S_DATA(at+2,0)-px,ly=S_DATA(at+2,1)-py,lz=z-depth;
        float d=sqrt(lx*lx+ly*ly+lz*lz),range=S_DATA(at+2,3);
        float fall=S_MAX(0.f,1.f-d/range);
        float diffuse=S_MAX(0.f,(normal_x*lx+normal_y*ly+normal_z*lz)/S_MAX(1.f,d));
        float tint=S_DATA(at+1,2)>0?(channel==0?1.f:channel==1?.45f:.12f):(channel==0?.25f:channel==1?.6f:1.f);
        value+=strength*fall*fall*(.12f+.88f*diffuse)*tint*.8f;
    } else {
        if(depth>0 && depth+8<z) continue;
        if(type==0) {
            float ring=S_MAX(0.f,1.f-abs(distance-r)/S_MAX(2.f*scale,r*.08f));
            float k=ring*strength*3.f*scale/S_MAX(distance,1.f);
            value=value*(1-ring*.5f)+S_SAMPLE(x+dx*k,y+dy*k)*ring*.5f;
            value+=ring*strength*.1f;
        } else if(type==8) {
            float focus=S_DATA(at+2,0),range=S_DATA(at+2,1);
            if(depth>0 && abs(depth-focus)<range) continue;
            float fall=S_MAX(0.f,1.f-distance/S_MAX(r,.5f));
            float age=S_DATA(at+1,2),a=fall*fall*strength;
            float u=dx/S_MAX(r,1.f),v=dy/S_MAX(r,1.f);
            float sx=sin(u*5.f+v*3.f-age*19.f),sy=cos(v*5.f-u*2.f-age*13.f);
            value=value*(1-a)+S_HEAT_SAMPLE(x+sx*3.f*scale,y+sy*2.f*scale,focus,range,z)*a;
        } else if(type==7) {
            // Opaque, spinning debris flecks, not additive glowing dots.
            float angle=S_DATA(at+1,2)*11.f;
            float u=abs(dx*cos(angle)-dy*sin(angle)),v=abs(dx*sin(angle)+dy*cos(angle));
            float coverage=S_MIN(1.f,S_MAX(0.f,(r-S_MAX(u,v))/S_MAX(.5f,scale*.5f)))*strength;
            value=value*(1-coverage)+S_DATA(at+2,channel)*coverage;
        } else {
            float alpha=S_MAX(0.f,1.f-distance/S_MAX(r,.5f));alpha*=alpha*strength;
            value+=alpha*S_DATA(at+2,channel);
        }
    }
}
return S_MIN(1.f,S_MAX(0.f,value));
