if((modes&3U)==0) return 1.f;
float depth=D_SURFACE(x,y,3);
if(depth<=0.f) return 1.f;
float ao=float(modes&3U)/3.f;
float focal=D_MAX(1.f,D_CAMERA(2));
#ifdef D_FOCAL_Y
float focal_y=D_MAX(1.f,D_FOCAL_Y);
#else
float focal_y=focal;
#endif
float radius=D_MIN(24.f*scale,D_MAX(2.f*scale,focal*160.f/depth));
float px=(x-D_CAMERA(0))*depth/focal,py=(y-D_CAMERA(1))*depth/focal_y;
float nx=D_SURFACE(x,y,0),ny=D_SURFACE(x,y,1),nz=D_SURFACE(x,y,2);
// Deterministic golden-angle disk; no changing noise after TAA resolution.
float dx=1.f,dy=0.f,occlusion=0.f;
int samples=8+8*int(modes&3U);
D_LOOP for(int j=0;j<samples;++j) {
    float r=radius*sqrt((float(j)+.5f)/float(samples));
    float sx=x+dx*r,sy=y+dy*r;
    float next_x=dx*-.73736888f-dy*.67549029f;
    dy=dx*.67549029f+dy*-.73736888f;dx=next_x;
    float z=D_SURFACE(sx,sy,3);if(z<=0.f) continue;
    float vx=(sx-D_CAMERA(0))*z/focal-px,vy=(sy-D_CAMERA(1))*z/focal_y-py,vz=z-depth;
    float distance=sqrt(vx*vx+vy*vy+vz*vz);
    float horizon=D_MAX(0.f,(vx*nx+vy*ny+vz*nz)/D_MAX(1.f,distance)-.08f);
    occlusion+=horizon*D_MAX(0.f,1.f-distance/240.f);
}
return 1.f-D_MIN(.7f,occlusion/float(samples)*ao*1.8f);
