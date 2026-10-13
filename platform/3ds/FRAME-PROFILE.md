# Optional native phase timings

`STARFOX_3DS_PROFILE_FRAMES=ON` records bounded owner-thread timings from the
actual guest clock. The player captures at most 512 one-second windows in a
preallocated buffer smaller than 512 KiB. No profile CSV formatting, SD writes
or flushes run during gameplay. On normal exit, after retiring the source,
audio and GPU owners, it writes the separate CSV once. The CSV header, phase
indices and timing values remain unchanged; stream errors do not stop gameplay.
Copy the frame profile **after exiting normally**. A hard shutdown can leave
that file empty; the separately flushed, bounded startup trace remains available
for locating a freeze. Neither diagnostic contains game assets or save data.

The October 12 hardware file has seven window gaps of approximately 5.6--6.2
seconds outside measured frame work. The old loop synchronously serialized and
flushed the profile after its frame timer ended, so its own SD stalls were not
included in the reported frame duration and fed into the next source catch-up.
Removing this gameplay I/O addresses that identified stall path, not the
remaining layer/geometry costs or a claim of stable console 60 FPS.

With frame-audio overlap enabled, the final complete SPC block runs on the
joined core-2 task while the owner prepares/presents the immutable source
snapshot. Its timing counters are captured in a separate profile and merged
only after the join; there are no concurrent writes to the owner's counters.
PCM submission and source-port acknowledgement stay on the owner thread.
The `audio` phase can overlap `layers`, `composite` and `present`, so those
durations are not additive either. Source catch-up blocks before the final
phase remain synchronous. A host parity test establishes unchanged samples
and source state, not physical-console frame rate.

On supported New models, a distinct optional effects worker requests only an
APT-granted core-1 time budget. During frame overlap, music runs on core 2,
effects on that worker and presentation on the owner. The inner worker joins
before the outer worker publishes the complete block. Denied budget/allocation
keeps both stems serial on core 2. Existing granted budgets are respected;
newly borrowed budgets are restored after worker retirement. State restores
inherit both executor owners. No samples, voices, commands or VM acknowledgements
are omitted. Worker timings can overlap one another as well as rendering, so
their sum is not the frame duration or proof of sustained console 60 FPS.

Background cache dependencies now include the complete configured tilemap and
every character it references, rather than all 1024 characters in its bank.
All map pages, tile priorities and offscreen entries are conservative
dependencies; 16x16 tiles also retain the +1/+16/+17 subcharacters with 10-bit
wrap. HDMA tables, palette/subtraction, source scroll, ownership and renderer
policy checks are unchanged. An unchanged bank keeps its fast full-range byte
comparison; a changed bank uses bounded, allocation-free dependency scratch.
Changes to unrelated in-bank model/HUD artwork do not force a background
traversal, recolour or texture replacement. Independent pixel/ownership tests
exercise 2/4/8-bpp banks, all map sizes, address wrapping and uniform-merging
invalidation. This does not establish how often a real stage benefits or its
console frame rate.

The original twenty-five phase names and enum indices stay unchanged. The
source/audio/raster detail phases and five appended GPU phases distinguish
the following work:

| Phase | Work included |
| --- | --- |
| `cpu` | Bounded 65C816 subroutine/task execution, including transfers |
| `strategies` | The complete active/no-objects strategy scheduler pass |
| `view` | Source camera/world transform calculation |
| `cull` | Source visibility/removal pass |
| `audio_irq` | Source sound queue and port acknowledgement handling |
| `music`, `effects` | Each unchanged, independent SPC stem render |
| `spc_emulate` | Timestamped port writes and frame completion in the SPC core |
| `spc_filter` | The original post-SPC sample filter |
| `bg_decode` | Fallback background/OBJ decode into indexed pixels and ownership |
| `bg_colour` | Cached indexed pixels to RGBA, coverage and strip bounds |
| `gpu_sync` | Citro3D LCD/VBlank synchronization |
| `gpu_wait` | FrameBegin queue retirement before changing borrowed uploads |
| `gpu_upload` | Residency comparisons, changed texture packing/uploads and VBO flush |
| `gpu_record` | CPU-side shader/state setup and eye/lower-LCD command encoding |
| `gpu_submit` | CPU-side FrameEnd command flush/submission |
| `terrain_build` | Landscape-atlas clipping/unprojection/publication, including declined attempts |
| `terrain_reuse` | Reuse of the identical validated landscape mesh with current borrowed artwork |
| `mono_background` | Reference indexed background preparation/composition before models |
| `mono_prepare` | Reference interpolated poses and world-cache validation |
| `mono_raster` | Reference indexed model/particle/dot rasterization |
| `mono_finish` | Reference world composition, native draw and final LCD canvas |
| `lcd_copy` | Exact lower RGB comparison, changed-buffer transpose and SDK cache flush |

Parents include children. In particular, `cpu` also occurs inside strategies,
view/cull/audio IRQ and video work; the two stem rows contain their SPC rows.
Do not sum all rows or turn a single component duration into an FPS claim.
Window-end flow/background labels do not guarantee identical source poses.
The working R376 New 3DS XL upload records 5,397 completed frames in the
longest gameplay/background-27 stretch, averaging 78.67 ms (12.71 FPS) over
424.56 seconds of timed frame work. Its `layers`/`composite`/`present` costs
average 18.84/18.30/22.35 ms respectively. This older file has no GPU subphase
rows, so it cannot separate upload/validation from actual queue wait inside
`present`. The appended counters address that limitation in the follow-up.
CPU markers are per bounded call, not per emulated instruction. SPC markers
retain every timestamped command, sample, voice and filter operation.

The raster library must also receive the profiling definition, otherwise its
detail rows compile out even when the cartridge owner is profiling. Without
the option all markers are no-ops, with no clock read or counter update.
`gpu_record` and `gpu_submit` are CPU durations, not asynchronous GPU execution
times. GPU queue pressure appears in `gpu_wait`; do not infer a GPU bottleneck
from the parent `present` duration alone. Markers do not change submission,
queue ownership, source cadence or stereo policy.

Changed RGBA/RGB uploads now compare logical pixels against successful native
residency and repack only dirty 8x8 Morton tiles. One enclosing, tile-aligned
cache-flush range includes every write. Initial/layout-change uploads, released
allocations and retries after a failed flush fully initialize the texture;
broad/dense changes retain the complete packer. Row padding, clamped/repeated
edge texels and alpha are preserved. A8 source ownership keeps its existing
validation/upload path. Sparse packing uses bounded reusable scratch, not a
new whole-image staging allocation. Host byte/flush-range tests and packing
benchmarks do not establish Citro3D execution cost or New-console FPS.

R382 classifies the existing vertex envelope using exact IEEE binary32 bit
ranges: finite position, [0,1] colour, absolute UV <= 65536, and [0,1] UV for
non-repeating textures. Signed zero stays valid; all infinity/NaN encodings
remain rejected. Draw ownership, opacity, fixed-colour equality, matrix,
submission-range and budget checks remain. No vertex or sample is modified.
An independent floating-point oracle exercises every exponent/sign, boundary
neighbors and randomized encodings in all nine lanes, with textured/repeating/
untextured draw integration checks. The full PICA suite passes 106,491,655 checks.
Alternating host benchmarks include an unchanged R381 full composition baseline,
24,576 vertices, 32 draws, two eyes and ordinary changing geometry. The revised
candidate's composition throughput is 1.64--1.79x the baseline in that fixture;
the earlier helper-only-fast candidate actually regressed full composition and
was corrected before publication. These host measurements are not ARM timings,
whole-game speed gains or New-console FPS. Other project compilers keep running.

The same CSV header is retained; consumers must allow all thirty-seven phase
names and reject unknown names rather than silently ignoring malformed data.
The terrain rows are children of `layers`; a cache-hit count is not a console
FPS claim. The first thirty enum indices remain unchanged. Finite vertices
are retained only when the plane, focal/near/far limits and exact source atlas
vertices match. Eye/slider changes still apply through their current matrices;
guard coverage, resident byte budgets and transactional failure behavior remain.

`starfox_3ds_cpu_timing_check` prints every observed phase but still requires
the original logic/video/capture/audio timers. Its default complete-state/PCM
hashing and source-cadence assertions remain intact. This diagnostic is not a
substitute for physical New-3DS-family performance/audio/slider acceptance.
