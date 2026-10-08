// Stored-pixel rounding above 4x. Spans round each fractional vertex to
// round-half-away(x*scale), as SoftwareRenderer does in binary64. Scales up
// to 4x are dyadic, so a float product is already exact; 5x-10x are not, and
// a rounded float product can land on (k+0.5) and pick the next pixel.
#ifndef STARFOX_SCALED_ROUND_HLSLI
#define STARFOX_SCALED_ROUND_HLSLI
// Dekker product: hi+lo equals a*b exactly for these magnitudes.
precise float2 sr_product(float a,float b) {
    precise float ca=4097*a,cb=4097*b,ah=ca-(ca-a),bh=cb-(cb-b),al=a-ah,bl=b-bh;
    precise float p=a*b,e=((ah*bh-p)+ah*bl+al*bh)+al*bl;return float2(p,e);
}
// Round hi+lo half away from zero, where |lo| <= ulp(hi)/2. Only an exact
// half in hi needs lo to break the tie.
int sr_round_away_pair(float hi,float lo) {
    precise float whole=trunc(hi),fraction=abs(hi-whole);
    bool away=fraction>0.5 || (fraction==0.5 && (lo==0 || (lo>0)==(hi>0)));
    return int(whole)+(away?(hi<0?-1:1):0);
}
int sr_scaled_round(float x,float scale) {
    float2 product=sr_product(x,scale);return sr_round_away_pair(product.x,product.y);
}
#endif
