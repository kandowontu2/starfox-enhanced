# Optional native phase timings

`STARFOX_3DS_PROFILE_FRAMES=ON` records bounded owner-thread timings from the
actual guest clock. The player writes at most 512 one-second windows to its
separate diagnostic CSV; stream errors disable logging instead of stopping
gameplay. It does not change source cadence, sound commands or presentation.

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

The same CSV header is retained; consumers must allow all thirty-two phase
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
