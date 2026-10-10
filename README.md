# Star Fox Enhanced — native material coverage SDK candidate

This temporary, source-only tree compiles one new material-opacity decoder for
the macOS and iOS Metal SDKs. It is not a release or a replacement for the
existing complete 25-program reflection pipeline and its validation suite.

The decoder handles the native 64-byte kind-2 solid/dither and kind-3
RGBA/indexed-atlas material records. It returns only fully opaque texels and
does not apply material styles, lighting or sRGB conversions. Palette index
zero can be opaque black; partially transparent texels are ray cutouts.

The same scalar decoder passed 4,217,088 independent CPU opacity comparisons
across all 24 existing opacity variants before this SDK check. That result is
not Metal execution, ray traversal, reflection-colour, game integration or
performance acceptance. The Metal probe adds a query-count bounds check.

`tools/compile_coverage.py` verifies every source byte against `inputs.json`,
then uses the unchanged native compiler observer from the complete-pipeline
SDK recipe. Only its source-qualification callback changes to this explicitly
separate component's complete input inventory. Compiler flags, SDK minimums,
process ownership, memory floors and resident cap are unchanged.

The manual workflow compiles and links this one kernel per SDK, preserves real
diagnostics and native-operation receipts, and never executes it on a GPU. It
does not publish a release, package assets, change main or validate production
adoption. GPU dispatch and integration remain separate work.

Sources are licensed under GPLv3, like Star Fox Enhanced. The project uses
AI/Codex assistance for programming, testing and documentation.
