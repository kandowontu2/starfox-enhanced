# Optional native phase timings

`STARFOX_3DS_PROFILE_FRAMES=ON` records bounded owner-thread timings from the
actual guest clock. The player writes at most 512 one-second windows to its
separate diagnostic CSV; stream errors disable logging instead of stopping
gameplay. It does not change source cadence, sound commands or presentation.

The original fourteen phase names and enum indices stay unchanged. Eleven
appended phases distinguish the following work:

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

Parents include children. In particular, `cpu` also occurs inside strategies,
view/cull/audio IRQ and video work; the two stem rows contain their SPC rows.
Do not sum all rows or turn a single component duration into an FPS claim.
Window-end flow/background labels do not guarantee identical source poses.
CPU markers are per bounded call, not per emulated instruction. SPC markers
retain every timestamped command, sample, voice and filter operation.

The raster library must also receive the profiling definition, otherwise its
detail rows compile out even when the cartridge owner is profiling. Without
the option all markers are no-ops, with no clock read or counter update.
The same CSV header is retained; consumers must allow all twenty-five phase
names and reject unknown names rather than silently ignoring malformed data.

`starfox_3ds_cpu_timing_check` prints every observed phase but still requires
the original logic/video/capture/audio timers. Its default complete-state/PCM
hashing and source-cadence assertions remain intact. This diagnostic is not a
substitute for physical Original-3DS performance/audio/slider acceptance.
