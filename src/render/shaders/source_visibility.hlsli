// Authored native-word and compensated continuous face tests. Both separate
// visibility dispatches and the single-model painter group use these equations.
int visibilityWord(int value) { return (value<<16)>>16; }
uint sourceWordVisibility(int4 a,int4 b,int4 c) {
    if(a.w<0 || b.w<0 || c.w<0) return 0;
    int bx=visibilityWord(b.x-a.x),by=visibilityWord(b.y-a.y);
    int cx=visibilityWord(c.x-a.x),cy=visibilityWord(c.y-a.y);
    int area=asint(asuint(bx*cy)-asuint(by*cx));
    bool oddBehind=(a.z<0)!=((b.z<0)!=(c.z<0));
    return (area<0)!=oddBehind?1:0;
}
float2 visibilitySum2(float a,float b) {
    precise float s=a+b;
    precise float v=s-a;
    precise float e=(a-(s-v))+(b-v);
    return float2(s,e);
}
float2 visibilityAdd2(float2 a,float2 b) {
    precise float2 s=visibilitySum2(a.x,b.x);
    precise float tail=(a.y+b.y)+s.y;
    return visibilitySum2(s.x,tail);
}
float2 visibilityProduct2(float a,float b) {
    precise float ca=4097.0*a,cb=4097.0*b;
    precise float ah=ca-(ca-a),bh=cb-(cb-b);
    precise float al=a-ah,bl=b-bh;
    precise float p=a*b;
    precise float e=((ah*bh-p)+ah*bl+al*bh)+al*bl;
    return float2(p,e);
}
float2 visibilityTriple(float a,float b,float c) {
    precise float2 p=visibilityProduct2(a,b);
    precise float2 q=visibilityProduct2(p.x,c);
    return visibilitySum2(q.x,q.y+p.y*c);
}
uint sourceContinuousVisibility(float4 av,float4 bv,float4 cv,uint kind) {
    if(av.w<0 || bv.w<0 || cv.w<0) return 0;
    // Exact source collinearity is established before camera rounding.
    if(kind==2U) return 1;
    float3 maximum=max(max(abs(av.xyz),abs(bv.xyz)),abs(cv.xyz));
    float scale=max(1.0,max(maximum.x,max(maximum.y,maximum.z)));
    float power=asfloat(asuint(scale)&0x7f800000U);
    precise float3 a=av.xyz/power,b=bv.xyz/power,c=cv.xyz/power;
    precise float2 determinant=visibilityAdd2(visibilityTriple(a.x,b.y,c.z),-visibilityTriple(a.x,b.z,c.y));
    determinant=visibilityAdd2(determinant,-visibilityTriple(a.y,b.x,c.z));
    determinant=visibilityAdd2(determinant,visibilityTriple(a.y,b.z,c.x));
    determinant=visibilityAdd2(determinant,visibilityTriple(a.z,b.x,c.y));
    determinant=visibilityAdd2(determinant,-visibilityTriple(a.z,b.y,c.x));
    precise float normalized=scale/power;
    precise float tolerance=normalized*normalized*normalized*1e-12;
    precise float2 difference=visibilityAdd2(determinant,float2(-tolerance,0));
    return (difference.x<0 || (difference.x==0 && difference.y<=0))?1:0;
}
