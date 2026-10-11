# Temporary Metal compositor source validation

Five complete graphics stages from the current game's resident SPIR-V:
fullscreen vertex, ray/material composition, MRT edge witness, retained liquid
guides, and the complete analytic water-panel guides. The existing calculations,
output locations, precision and source function boundaries remain unchanged.

Only entry resource annotations are remapped to SDL's uniforms-first Metal
buffer convention. Declared-but-unused vertex resources and samplers are not
invented. Original unused SSA initializers remain with unused-value attributes.

The manual ten-job matrix retains the existing O3, warnings-as-errors,
no-fast-math, contraction-off and resource/identity observer at Metal2.3,
macOS11/iOS15. This source-only snapshot contains no ROMs, user BINs,
credentials or release jobs. Compilation is not application/GPU acceptance.
