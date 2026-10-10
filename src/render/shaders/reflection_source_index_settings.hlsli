// Resident source-candidate ABI. No colour or inverse inputs.
#ifndef STARFOX_REFLECTED_VISIBILITY_SETTINGS
#define STARFOX_REFLECTED_VISIBILITY_SETTINGS
cbuffer Settings:register(b0,space2) {
    uint width,height,lobes,primaryPrefix;
    uint recordPrefix,pathStride,totalNodes,levelCount;
    uint workLevel,queryCount,leafBudget,nodeBudget;
    uint4 levels[12]; // offset, width, height, unused; leaf first
};
#include "reflection_source_feature.hlsli"
#endif
