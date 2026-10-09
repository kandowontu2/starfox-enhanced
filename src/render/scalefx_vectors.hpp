#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>

namespace starfox::render::scalefx_detail {
template<unsigned N,typename T=float> struct Vec {
    std::array<T,N> values{};
    Vec()=default;
    Vec(T value) {values.fill(value);}
    template<typename... A> requires(sizeof...(A)==N)
    Vec(A... a):values{static_cast<T>(a)...} {}
    template<typename U> Vec(const Vec<N,U>& other) {
        for(unsigned i=0;i<N;++i) values[i]=static_cast<T>(other[i]);
    }
    Vec(const std::array<T,N>& value):values(value) {}
    T& operator[](unsigned i) {return values[i];}
    T operator[](unsigned i) const {return values[i];}
};
using V2=Vec<2>;using V3=Vec<3>;using V4=Vec<4>;
using B2=Vec<2,bool>;using B4=Vec<4,bool>;
template<unsigned... I,unsigned N,typename T>
Vec<sizeof...(I),T> sw(const Vec<N,T>& v) {return {v[I]...};}
#define SFX_OP(OP) \
template<unsigned N> Vec<N> operator OP(const Vec<N>& a,const Vec<N>& b) { \
    Vec<N> r;for(unsigned i=0;i<N;++i) r[i]=a[i] OP b[i];return r;} \
template<unsigned N> Vec<N> operator OP(const Vec<N>& a,float b) {return a OP Vec<N>(b);} \
template<unsigned N> Vec<N> operator OP(float a,const Vec<N>& b) {return Vec<N>(a) OP b;}
SFX_OP(+) SFX_OP(-) SFX_OP(*) SFX_OP(/)
#undef SFX_OP
inline float min(float a,float b) {return std::min(a,b);}
inline float max(float a,float b) {return std::max(a,b);}
inline float clamp(float x,float a,float b) {return std::clamp(x,a,b);}
inline float step(float edge,float x) {return x<edge?0.F:1.F;}
#define SFX_FN(NAME) \
template<unsigned N> Vec<N> NAME(const Vec<N>& a,const Vec<N>& b) { \
    Vec<N> r;for(unsigned i=0;i<N;++i) r[i]=NAME(a[i],b[i]);return r;} \
template<unsigned N> Vec<N> NAME(const Vec<N>& a,float b) {return NAME(a,Vec<N>(b));} \
template<unsigned N> Vec<N> NAME(float a,const Vec<N>& b) {return NAME(Vec<N>(a),b);}
SFX_FN(min) SFX_FN(max) SFX_FN(step)
inline float mod(float a,float b) {return a-b*std::floor(a/b);}
SFX_FN(mod)
#undef SFX_FN
template<unsigned N> Vec<N> floor(const Vec<N>& a) {
    Vec<N> r;for(unsigned i=0;i<N;++i) r[i]=std::floor(a[i]);return r;
}
template<unsigned N> float dot(const Vec<N>& a,const Vec<N>& b) {
    float r=0;for(unsigned i=0;i<N;++i) r+=a[i]*b[i];return r;
}
}
