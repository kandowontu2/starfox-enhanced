// Integer scratch/copy protocol only. No optical inputs, roots or radiance.
RWByteAddressBuffer scratch : register(u0, space1);
cbuffer Params : register(b0, space2) { uint queries, phase, reserved0, reserved1; };
static const uint wordsPerQuery = 6144;
uint pattern(uint q, uint word) {
    return 0xa5c30001u ^ (q * 0x01010101u) ^ (word * 0x9e3779b9u);
}
[numthreads(64, 1, 1)]
void seed_main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 64 * wordsPerQuery) return;
    scratch.Store(id.x * 4, pattern(id.x / wordsPerQuery, id.x % wordsPerQuery));
}
[numthreads(64, 1, 1)]
void marker_main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= queries) return;
    // The production witness has the same 192-byte root prefix and 64-byte
    // destination. This deliberately does NOT evaluate an optical witness.
    [unroll] for (uint word = 48; word < 64; ++word)
        scratch.Store((id.x * wordsPerQuery + word) * 4,
            0x5afe0000u ^ (id.x * 0x01010101u) ^ ((word - 48) * 0x9e3779b9u) ^ phase);
}
