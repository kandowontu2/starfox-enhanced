// Native indexed transport only. CPU compositor flags use a separate format.
// A dithered, untextured 3D pixel retains its original selected palette index
// in byte 0 and the other material index in byte 1. Bit 31 distinguishes this
// from a layer tag. Decode before exporting pixels to post-processing.
uint nativeTag(uint pixel) {return (pixel&0x80000000u)!=0?0u:(pixel>>8)&255u;}
uint nativeIndexAndTag(uint pixel) {return (pixel&0x80000000u)!=0?pixel&255u:pixel&65535u;}
