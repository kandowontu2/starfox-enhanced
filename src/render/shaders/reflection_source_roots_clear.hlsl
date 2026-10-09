// Integer-only initialization of private root scratch, once per complete
// same-command stream. No current, accepted, indexed or published image access.
RWByteAddressBuffer results:register(u0,space1);
cbuffer ClearSettings:register(b0,space2) {uint clearBytes,clearReserved0,clearReserved1,clearReserved2;};
[numthreads(64,1,1)]
void feature_roots_clear_main(uint3 id:SV_DispatchThreadID) {
    const uint at=id.x*16;
    if(clearReserved0 || clearReserved1 || clearReserved2 || clearBytes!=1572864 || at>=clearBytes)return;
    results.Store4(at,0);
}
