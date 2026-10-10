// Same exact checked optical writer and ABI, with whole-frame query-parallel
// scheduling. The owner must first zero the unused local-region scratch.
#define STARFOX_SOURCE_STREAM_ROOTS 1
// Keep every query and its exact writer; spread a 64-query batch over eight
// independent groups rather than pinning the whole batch to one group.
#define STARFOX_SOURCE_ROOT_LANES 8
#define feature_jets_main feature_jets_stream_main
#include "reflection_source_jets.hlsl"
