/*
	ScaleFX - Pass 0
	by Sp00kyFox, 2017-03-01

Filter:	Nearest
Scale:	1x

ScaleFX is an edge interpolation algorithm specialized in pixel art. It was
originally intended as an improvement upon Scale3x but became a new filter in
its own right.
ScaleFX interpolates edges up to level 6 and makes smooth transitions between
different slopes. The filtered picture will only consist of colours present
in the original.

Pass 0 prepares metric data for the next pass.



Copyright (c) 2016 Sp00kyFox - ScaleFX@web.de

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/
#include "starfox/render/scalefx.hpp"
#include "scalefx_vectors.hpp"
#include <stdexcept>
namespace starfox::render {
namespace {
using namespace scalefx_detail;
struct Context {
    std::span<const std::uint32_t> original;
    ScaleFxScratch& scratch;
    unsigned width,height;
    V4 sample(int stage,int x,int y) const {
        x=std::clamp(x,0,int(width)-1);y=std::clamp(y,0,int(height)-1);
        const auto i=std::size_t(y)*width+x;
        if(stage>=0) return V4(scratch.passes[stage][i]);
        const auto p=original[i];
        return V4(float((p>>16)&255)/255.F,float((p>>8)&255)/255.F,
            float(p&255)/255.F,float(p>>24)/255.F);
    }
};
namespace pass0 {



// Reference: http://www.compuphase.com/cmetric.htm
float dist(V3 A, V3 B)
{
	float r = 0.5F * (A[0] + B[0]);
	V3 d = A - B;
	V3 c = V3(2 + r, 4, 3 - r);

	return sqrt(dot(c*d, d)) / 3;
}


V4 evaluate(const Context& c, unsigned x, unsigned y)
{
const int px=int(x/1),py=int(y/1);

	/*	grid		metric

		A B C		x y z
		  E F		  o w
	*/


#define TEX(x, y) sw<0,1,2>(c.sample(-1, px+(x), py+(y)))

	// read texels
	V3 A = TEX(-1,-1);
	V3 B = TEX( 0,-1);
	V3 C = TEX( 1,-1);
	V3 E = TEX( 0, 0);
	V3 F = TEX( 1, 0);

	// output
	return V4(dist(E,A), dist(E,B), dist(E,C), dist(E,F));
}


#undef TEX
#undef TEXm
#undef TEXs
#undef LE
#undef GE
#undef LEQ
#undef GEQ
#undef NOT
}

namespace pass1 {



// corner strength
float str(float d, V2 a, V2 b){
	float diff = a[0] - a[1];
	float wght1 = max(0.5F - d, 0) / 0.5F;
	float wght2 = clamp((1-d) + (min(a[0], b[0]) + a[0] > min(a[1], b[1]) + a[1] ? diff : -diff), 0.F, 1.F);
	return (1.0F == 1.F || 2.F*d < a[0] + a[1]) ? (wght1 * wght2) * (a[0] * a[1]) : 0.F;
}


V4 evaluate(const Context& c, unsigned x, unsigned y)
{
const int px=int(x/1),py=int(y/1);

	/*	grid		metric		pattern

		A B		x y z		x y
		D E F		  o w		w z
		G H I
	*/


#define TEX(x, y) c.sample(0, px+(x), py+(y))

	// metric data
	V4 A = TEX(-1,-1), B = TEX( 0,-1);
	V4 D = TEX(-1, 0), E = TEX( 0, 0), F = TEX( 1, 0);
	V4 G = TEX(-1, 1), H = TEX( 0, 1), I = TEX( 1, 1);

	// corner strength
	V4 res;
	res[0] = str(D[2], V2(D[3], E[1]), V2(A[3], D[1]));
	res[1] = str(F[0], V2(E[3], E[1]), V2(B[3], F[1]));
	res[2] = str(H[2], V2(E[3], H[1]), V2(H[3], I[1]));
	res[3] = str(H[0], V2(D[3], H[1]), V2(G[3], G[1]));
		
	return res;
}


#undef TEX
#undef TEXm
#undef TEXs
#undef LE
#undef GE
#undef LEQ
#undef GEQ
#undef NOT
}

namespace pass2 {



auto LE(auto x, auto y) {return 1-step(y,x);}
auto GE(auto x, auto y) {return 1-step(x,y);}
auto LEQ(auto x, auto y) {return step(x,y);}
auto GEQ(auto x, auto y) {return step(y,x);}
auto NOT(auto x) {return 1-x;}

// corner dominance at junctions
V4 dom(V3 x, V3 y, V3 z, V3 w){
	return 2 * V4(x[1], y[1], z[1], w[1]) - (V4(x[0], y[0], z[0], w[0]) + V4(x[2], y[2], z[2], w[2]));
}

// necessary but not sufficient junction condition for orthogonal edges
float clear(V2 crn, V2 a, V2 b){
	return (crn[0] >= max(min(a[0], a[1]), min(b[0], b[1]))) && (crn[1] >= max(min(a[0], b[1]), min(b[0], a[1]))) ? 1.F : 0.F;
}


V4 evaluate(const Context& c, unsigned x, unsigned y)
{
const int px=int(x/1),py=int(y/1);

	/*	grid		metric		pattern

		A B C		x y z		x y
		D E F		  o w		w z
		G H I
	*/


#define TEXm(x, y) c.sample(0, px+(x), py+(y))
#define TEXs(x, y) c.sample(1, px+(x), py+(y))


	// metric data
	V4 A = TEXm(-1,-1), B = TEXm( 0,-1);
	V4 D = TEXm(-1, 0), E = TEXm( 0, 0), F = TEXm( 1, 0);
	V4 G = TEXm(-1, 1), H = TEXm( 0, 1), I = TEXm( 1, 1);	

	// strength data
	V4 As = TEXs(-1,-1), Bs = TEXs( 0,-1), Cs = TEXs( 1,-1);
	V4 Ds = TEXs(-1, 0), Es = TEXs( 0, 0), Fs = TEXs( 1, 0);
	V4 Gs = TEXs(-1, 1), Hs = TEXs( 0, 1), Is = TEXs( 1, 1);

	// strength & dominance junctions
	V4 jSx = V4(As[2], Bs[3], Es[0], Ds[1]), jDx = dom(sw<1,2,3>(As), sw<2,3,0>(Bs), sw<3,0,1>(Es), sw<0,1,2>(Ds));
	V4 jSy = V4(Bs[2], Cs[3], Fs[0], Es[1]), jDy = dom(sw<1,2,3>(Bs), sw<2,3,0>(Cs), sw<3,0,1>(Fs), sw<0,1,2>(Es));
	V4 jSz = V4(Es[2], Fs[3], Is[0], Hs[1]), jDz = dom(sw<1,2,3>(Es), sw<2,3,0>(Fs), sw<3,0,1>(Is), sw<0,1,2>(Hs));
	V4 jSw = V4(Ds[2], Es[3], Hs[0], Gs[1]), jDw = dom(sw<1,2,3>(Ds), sw<2,3,0>(Es), sw<3,0,1>(Hs), sw<0,1,2>(Gs));


	// majority vote for ambiguous dominance junctions
	V4 zero4 = V4(0);
	V4 jx = min(GE(jDx, zero4) * (LEQ(sw<1,2,3,0>(jDx), zero4) * LEQ(sw<3,0,1,2>(jDx), zero4) + GE(jDx + sw<2,3,0,1>(jDx), sw<1,2,3,0>(jDx) + sw<3,0,1,2>(jDx))), 1);
	V4 jy = min(GE(jDy, zero4) * (LEQ(sw<1,2,3,0>(jDy), zero4) * LEQ(sw<3,0,1,2>(jDy), zero4) + GE(jDy + sw<2,3,0,1>(jDy), sw<1,2,3,0>(jDy) + sw<3,0,1,2>(jDy))), 1);
	V4 jz = min(GE(jDz, zero4) * (LEQ(sw<1,2,3,0>(jDz), zero4) * LEQ(sw<3,0,1,2>(jDz), zero4) + GE(jDz + sw<2,3,0,1>(jDz), sw<1,2,3,0>(jDz) + sw<3,0,1,2>(jDz))), 1);
	V4 jw = min(GE(jDw, zero4) * (LEQ(sw<1,2,3,0>(jDw), zero4) * LEQ(sw<3,0,1,2>(jDw), zero4) + GE(jDw + sw<2,3,0,1>(jDw), sw<1,2,3,0>(jDw) + sw<3,0,1,2>(jDw))), 1);


	// inject strength without creating new contradictions
	V4 res;
	res[0] = min(jx[2] + NOT(jx[1]) * NOT(jx[3]) * GE(jSx[2], 0) * (jx[0] + GE(jSx[0] + jSx[2], jSx[1] + jSx[3])), 1);
	res[1] = min(jy[3] + NOT(jy[2]) * NOT(jy[0]) * GE(jSy[3], 0) * (jy[1] + GE(jSy[1] + jSy[3], jSy[0] + jSy[2])), 1);
	res[2] = min(jz[0] + NOT(jz[3]) * NOT(jz[1]) * GE(jSz[0], 0) * (jz[2] + GE(jSz[0] + jSz[2], jSz[1] + jSz[3])), 1);
	res[3] = min(jw[1] + NOT(jw[0]) * NOT(jw[2]) * GE(jSw[1], 0) * (jw[3] + GE(jSw[1] + jSw[3], jSw[0] + jSw[2])), 1);	


	// single pixel & end of line detection
	res = min(res * (V4(jx[2], jy[3], jz[0], jw[1]) + NOT(sw<3,0,1,2>(res) * sw<1,2,3,0>(res))), 1);


	// output

	V4 clr;
	clr[0] = clear(V2(D[2], E[0]), V2(D[3], E[1]), V2(A[3], D[1]));
	clr[1] = clear(V2(F[0], E[2]), V2(E[3], E[1]), V2(B[3], F[1]));
	clr[2] = clear(V2(H[2], I[0]), V2(E[3], H[1]), V2(H[3], I[1]));
	clr[3] = clear(V2(H[0], G[2]), V2(D[3], H[1]), V2(G[3], G[1]));

	V4 h = V4(min(D[3], A[3]), min(E[3], B[3]), min(E[3], H[3]), min(D[3], G[3]));
	V4 v = V4(min(E[1], D[1]), min(E[1], F[1]), min(H[1], I[1]), min(H[1], G[1]));

	V4 orien = GE(h + V4(D[3], E[3], E[3], D[3]), v + V4(E[1], E[1], H[1], H[1]));	// orientation
	V4 hori  = LE(h, v) * clr;	// horizontal edges
	V4 vert  = GE(h, v) * clr;	// vertical edges

	return (res + 2 * hori + 4 * vert + 8 * orien) / 15;
}


#undef TEX
#undef TEXm
#undef TEXs
#undef LE
#undef GE
#undef LEQ
#undef GEQ
#undef NOT
}

namespace pass3 {



// extract first bool4 from float4 - corners
B4 loadCorn(V4 x){
	return B4(floor(mod(x*15 + 0.5F, 2)));
}

// extract second bool4 from float4 - horizontal edges
B4 loadHori(V4 x){
	return B4(floor(mod(x*7.5F + 0.25F, 2)));
}

// extract third bool4 from float4 - vertical edges
B4 loadVert(V4 x){
	return B4(floor(mod(x*3.75F + 0.125F, 2)));
}

// extract fourth bool4 from float4 - orientation
B4 loadOr(V4 x){
	return B4(floor(mod(x*1.875F + 0.0625F, 2)));
}



V4 evaluate(const Context& c, unsigned x, unsigned y)
{
const int px=int(x/1),py=int(y/1);

	/*	grid		corners		mids		

		  B		x   y	  	  x
		D E F				w   y
		  H		w   z	  	  z
	*/

#define TEX(x, y) c.sample(2, px+(x), py+(y))

	// read data
	V4 E = TEX( 0, 0);
	V4 D = TEX(-1, 0), D0 = TEX(-2, 0), D1 = TEX(-3, 0);
	V4 F = TEX( 1, 0), F0 = TEX( 2, 0), F1 = TEX( 3, 0);
	V4 B = TEX( 0,-1), B0 = TEX( 0,-2), B1 = TEX( 0,-3);
	V4 H = TEX( 0, 1), H0 = TEX( 0, 2), H1 = TEX( 0, 3);

	// extract data
	B4 Ec = loadCorn(E), Eh = loadHori(E), Ev = loadVert(E), Eo = loadOr(E);
	B4 Dc = loadCorn(D),	Dh = loadHori(D), Do = loadOr(D), D0c = loadCorn(D0), D0h = loadHori(D0), D1h = loadHori(D1);
	B4 Fc = loadCorn(F),	Fh = loadHori(F), Fo = loadOr(F), F0c = loadCorn(F0), F0h = loadHori(F0), F1h = loadHori(F1);
	B4 Bc = loadCorn(B),	Bv = loadVert(B), Bo = loadOr(B), B0c = loadCorn(B0), B0v = loadVert(B0), B1v = loadVert(B1);
	B4 Hc = loadCorn(H),	Hv = loadVert(H), Ho = loadOr(H), H0c = loadCorn(H0), H0v = loadVert(H0), H1v = loadVert(H1);

	
	// lvl1 corners (hori, vert)
	bool lvl1x = Ec[0] && (Dc[2] || Bc[2] || 1.0F == 1);
	bool lvl1y = Ec[1] && (Fc[3] || Bc[3] || 1.0F == 1);
	bool lvl1z = Ec[2] && (Fc[0] || Hc[0] || 1.0F == 1);
	bool lvl1w = Ec[3] && (Dc[1] || Hc[1] || 1.0F == 1);

	// lvl2 mid (left, right / up, down)
	B2 lvl2x = B2((Ec[0] && Eh[1]) && Dc[2], (Ec[1] && Eh[0]) && Fc[3]);
	B2 lvl2y = B2((Ec[1] && Ev[2]) && Bc[3], (Ec[2] && Ev[1]) && Hc[0]);
	B2 lvl2z = B2((Ec[3] && Eh[2]) && Dc[1], (Ec[2] && Eh[3]) && Fc[0]);
	B2 lvl2w = B2((Ec[0] && Ev[3]) && Bc[2], (Ec[3] && Ev[0]) && Hc[1]);

	// lvl3 corners (hori, vert)
	B2 lvl3x = B2(lvl2x[1] && (Dh[1] && Dh[0]) && Fh[2], lvl2w[1] && (Bv[3] && Bv[0]) && Hv[2]);
	B2 lvl3y = B2(lvl2x[0] && (Fh[0] && Fh[1]) && Dh[3], lvl2y[1] && (Bv[2] && Bv[1]) && Hv[3]);
	B2 lvl3z = B2(lvl2z[0] && (Fh[3] && Fh[2]) && Dh[0], lvl2y[0] && (Hv[1] && Hv[2]) && Bv[0]);
	B2 lvl3w = B2(lvl2z[1] && (Dh[2] && Dh[3]) && Fh[1], lvl2w[0] && (Hv[0] && Hv[3]) && Bv[1]);

	// lvl4 corners (hori, vert)
	B2 lvl4x = B2((Dc[0] && Dh[1] && Eh[0] && Eh[1] && Fh[0] && Fh[1]) && (D0c[2] && D0h[3]), (Bc[0] && Bv[3] && Ev[0] && Ev[3] && Hv[0] && Hv[3]) && (B0c[2] && B0v[1]));
	B2 lvl4y = B2((Fc[1] && Fh[0] && Eh[1] && Eh[0] && Dh[1] && Dh[0]) && (F0c[3] && F0h[2]), (Bc[1] && Bv[2] && Ev[1] && Ev[2] && Hv[1] && Hv[2]) && (B0c[3] && B0v[0]));
	B2 lvl4z = B2((Fc[2] && Fh[3] && Eh[2] && Eh[3] && Dh[2] && Dh[3]) && (F0c[0] && F0h[1]), (Hc[2] && Hv[1] && Ev[2] && Ev[1] && Bv[2] && Bv[1]) && (H0c[0] && H0v[3]));
	B2 lvl4w = B2((Dc[3] && Dh[2] && Eh[3] && Eh[2] && Fh[3] && Fh[2]) && (D0c[1] && D0h[0]), (Hc[3] && Hv[0] && Ev[3] && Ev[0] && Bv[3] && Bv[0]) && (H0c[1] && H0v[2]));

	// lvl5 mid (left, right / up, down)
	B2 lvl5x = B2(lvl4x[0] && (F0h[0] && F0h[1]) && (D1h[2] && D1h[3]), lvl4y[0] && (D0h[1] && D0h[0]) && (F1h[3] && F1h[2]));
	B2 lvl5y = B2(lvl4y[1] && (H0v[1] && H0v[2]) && (B1v[3] && B1v[0]), lvl4z[1] && (B0v[2] && B0v[1]) && (H1v[0] && H1v[3]));
	B2 lvl5z = B2(lvl4w[0] && (F0h[3] && F0h[2]) && (D1h[1] && D1h[0]), lvl4z[0] && (D0h[2] && D0h[3]) && (F1h[0] && F1h[1]));
	B2 lvl5w = B2(lvl4x[1] && (H0v[0] && H0v[3]) && (B1v[2] && B1v[1]), lvl4w[1] && (B0v[3] && B0v[0]) && (H1v[1] && H1v[2]));

	// lvl6 corners (hori, vert)
	B2 lvl6x = B2(lvl5x[1] && (D1h[1] && D1h[0]), lvl5w[1] && (B1v[3] && B1v[0]));
	B2 lvl6y = B2(lvl5x[0] && (F1h[0] && F1h[1]), lvl5y[1] && (B1v[2] && B1v[1]));
	B2 lvl6z = B2(lvl5z[0] && (F1h[3] && F1h[2]), lvl5y[0] && (H1v[1] && H1v[2]));
	B2 lvl6w = B2(lvl5z[1] && (D1h[2] && D1h[3]), lvl5w[0] && (H1v[0] && H1v[3]));

	
	// subpixels - 0 = E, 1 = D, 2 = D0, 3 = F, 4 = F0, 5 = B, 6 = B0, 7 = H, 8 = H0

	V4 crn;
	crn[0] = ((lvl1x && Eo[0]) || (lvl3x[0] && Eo[1]) || (lvl4x[0] && Do[0]) || (lvl6x[0] && Fo[1])) ? 5 : (lvl1x || (lvl3x[1] && !Eo[3]) || (lvl4x[1] && !Bo[0]) || (lvl6x[1] && !Ho[3])) ? 1 : lvl3x[0] ? 3 : lvl3x[1] ? 7 : lvl4x[0] ? 2 : lvl4x[1] ? 6 : lvl6x[0] ? 4 : lvl6x[1] ? 8 : 0;
	crn[1] = ((lvl1y && Eo[1]) || (lvl3y[0] && Eo[0]) || (lvl4y[0] && Fo[1]) || (lvl6y[0] && Do[0])) ? 5 : (lvl1y || (lvl3y[1] && !Eo[2]) || (lvl4y[1] && !Bo[1]) || (lvl6y[1] && !Ho[2])) ? 3 : lvl3y[0] ? 1 : lvl3y[1] ? 7 : lvl4y[0] ? 4 : lvl4y[1] ? 6 : lvl6y[0] ? 2 : lvl6y[1] ? 8 : 0;
	crn[2] = ((lvl1z && Eo[2]) || (lvl3z[0] && Eo[3]) || (lvl4z[0] && Fo[2]) || (lvl6z[0] && Do[3])) ? 7 : (lvl1z || (lvl3z[1] && !Eo[1]) || (lvl4z[1] && !Ho[2]) || (lvl6z[1] && !Bo[1])) ? 3 : lvl3z[0] ? 1 : lvl3z[1] ? 5 : lvl4z[0] ? 4 : lvl4z[1] ? 8 : lvl6z[0] ? 2 : lvl6z[1] ? 6 : 0;
	crn[3] = ((lvl1w && Eo[3]) || (lvl3w[0] && Eo[2]) || (lvl4w[0] && Do[3]) || (lvl6w[0] && Fo[2])) ? 7 : (lvl1w || (lvl3w[1] && !Eo[0]) || (lvl4w[1] && !Ho[3]) || (lvl6w[1] && !Bo[0])) ? 1 : lvl3w[0] ? 3 : lvl3w[1] ? 5 : lvl4w[0] ? 2 : lvl4w[1] ? 8 : lvl6w[0] ? 4 : lvl6w[1] ? 6 : 0;

	V4 mid;
	mid[0] = ((lvl2x[0] && Eo[0]) || (lvl2x[1] && Eo[1]) || (lvl5x[0] && Do[0]) || (lvl5x[1] && Fo[1])) ? 5 : lvl2x[0] ? 1 : lvl2x[1] ? 3 : lvl5x[0] ? 2 : lvl5x[1] ? 4 : ((Ec[0] && Dc[2]) && (Ec[1] && Fc[3])) ? ( Eo[0] ?  Eo[1] ? 5 : 3 : 1) : 0;
	mid[1] = ((lvl2y[0] && !Eo[1]) || (lvl2y[1] && !Eo[2]) || (lvl5y[0] && !Bo[1]) || (lvl5y[1] && !Ho[2])) ? 3 : lvl2y[0] ? 5 : lvl2y[1] ? 7 : lvl5y[0] ? 6 : lvl5y[1] ? 8 : ((Ec[1] && Bc[3]) && (Ec[2] && Hc[0])) ? (!Eo[1] ? !Eo[2] ? 3 : 7 : 5) : 0;
	mid[2] = ((lvl2z[0] && Eo[3]) || (lvl2z[1] && Eo[2]) || (lvl5z[0] && Do[3]) || (lvl5z[1] && Fo[2])) ? 7 : lvl2z[0] ? 1 : lvl2z[1] ? 3 : lvl5z[0] ? 2 : lvl5z[1] ? 4 : ((Ec[2] && Fc[0]) && (Ec[3] && Dc[1])) ? ( Eo[2] ?  Eo[3] ? 7 : 1 : 3) : 0;
	mid[3] = ((lvl2w[0] && !Eo[0]) || (lvl2w[1] && !Eo[3]) || (lvl5w[0] && !Bo[0]) || (lvl5w[1] && !Ho[3])) ? 1 : lvl2w[0] ? 5 : lvl2w[1] ? 7 : lvl5w[0] ? 6 : lvl5w[1] ? 8 : ((Ec[3] && Hc[1]) && (Ec[0] && Bc[2])) ? (!Eo[3] ? !Eo[0] ? 1 : 5 : 7) : 0;


	// ouput
	return (crn + 9 * mid) / 80;

}


#undef TEX
#undef TEXm
#undef TEXs
#undef LE
#undef GE
#undef LEQ
#undef GEQ
#undef NOT
}

namespace pass4 {



// extract corners
V4 loadCrn(V4 x){
	return floor(mod(x*80 + 0.5F, 9));
}

// extract mids
V4 loadMid(V4 x){
	return floor(mod(x*8.888888F + 0.055555F, 9));
}


V4 evaluate(const Context& c, unsigned x, unsigned y)
{
const int px=int(x/3),py=int(y/3);

	/*	grid		corners		mids

		  B		x   y	  	  x
		D E F				w   y
		  H		w   z	  	  z
	*/


	// read data
	V4 E = c.sample(3, px, py);

	// extract data
	V4 crn = loadCrn(E);
	V4 mid = loadMid(E);

	// determine subpixel
	V2 fp = V2(x % 3, y % 3);
	float sp = fp[1] == 0.F ? (fp[0] == 0.F ? crn[0] : fp[0] == 1.F ? mid[0] : crn[1]) : (fp[1] == 1.F ? (fp[0] == 0.F ? mid[3] : fp[0] == 1.F ? 0.F : mid[1]) : (fp[0] == 0.F ? crn[3] : fp[0] == 1.F ? mid[2] : crn[2]));

	// output coordinate - 0 = E, 1 = D, 2 = D0, 3 = F, 4 = F0, 5 = B, 6 = B0, 7 = H, 8 = H0
	V2 res = sp == 0.F ? V2(0,0) : sp == 1.F ? V2(-1,0) : sp == 2.F ? V2(-2,0) : sp == 3.F ? V2(1,0) : sp == 4.F ? V2(2,0) : sp == 5.F ? V2(0,-1) : sp == 6.F ? V2(0,-2) : sp == 7.F ? V2(0,1) : V2(0,2);

	// ouput
	return c.sample(-1, px+int(res[0]), py+int(res[1]));
}


#undef TEX
#undef TEXm
#undef TEXs
#undef LE
#undef GE
#undef LEQ
#undef GEQ
#undef NOT
}

}
void scale_scalefx(std::span<const std::uint32_t> source,
    std::span<std::uint32_t> output,std::uint32_t width,std::uint32_t height,
    ScaleFxScratch& scratch,RowWorkers& workers) {
    const std::uint64_t count=std::uint64_t(width)*height;
    if(!width || !height || count>UINT32_MAX/9 || source.size()!=count || output.size()!=count*9)
        throw std::invalid_argument("Invalid ScaleFX image dimensions");
    for(auto& pass:scratch.passes) pass.resize(static_cast<std::size_t>(count));
    Context c{source,scratch,width,height};
    using Eval=V4(*)(const Context&,unsigned,unsigned);
    constexpr Eval passes[]{pass0::evaluate,pass1::evaluate,pass2::evaluate,pass3::evaluate};
    for(unsigned stage=0;stage<4;++stage)
        workers.parallel_rows(height,[&](unsigned first,unsigned last) {
            for(unsigned y=first;y<last;++y) for(unsigned x=0;x<width;++x)
                scratch.passes[stage][std::size_t(y)*width+x]=passes[stage](c,x,y).values;
        });
    workers.parallel_rows(height*3,[&](unsigned first,unsigned last) {
        for(unsigned y=first;y<last;++y) for(unsigned x=0;x<width*3;++x) {
            const auto colour=pass4::evaluate(c,x,y);
            const auto byte=[&](unsigned i){return std::uint32_t(std::clamp(colour[i]*255.F+0.5F,0.F,255.F));};
            output[std::size_t(y)*width*3+x]=(byte(3)<<24)|(byte(0)<<16)|(byte(1)<<8)|byte(2);
        }
    });
}
}
