// Mirrors GpuMsaaTriangle. Affine UVs are in source texels, before scrolling.
struct Triangle {
    float2 a,b,c;uint color0,color1;
    float2 uvA,uvB,uvC;
    uint textureOffset,uMask,vMask,colorBase;
    uint scrollX,scrollY,textured;
    uint3 padding;
};
