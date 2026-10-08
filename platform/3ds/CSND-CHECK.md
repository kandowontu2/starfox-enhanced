# Original 3DS audio compatibility check

This is an asset-free diagnostic, not the game or a streaming CSND backend.
The experimental player still uses NDSP and both accurate cartridge SPC stems.
The CSND test does not require a ROM, asset BIN or bundled DSP firmware.

Use the `StarFoxEnhanced-3ds-audio-compatibility-check` artifact from the native
test branch's current passing CI run. Copy `starfox_3ds_frontend_check.3dsx` to
your SD card's `/3ds/` directory and launch it through your existing Homebrew
Launcher. No CIA installation or firmware changes are involved.

- Press **Y first** to test CSND: a quiet low tone on the left for half a second,
  then a higher tone on the right for half a second. The lower screen reports
  completion only after both hardware channels report inactive.
- Press Y again to repeat. Switching tests retires the previous output service
  before touching its memory. X runs the separate NDSP check, which needs your
  console's existing DSP setup; avoid X if that initialization crashes on yours.
- Select + Start exits. Service/allocation/command errors remain visible on the
  upper screen. Please report the code/message, console model and which test
  you pressed, plus left/right sound, completion and repeated-test behavior.

CSND service access may be unavailable in a particular launch environment.
This does not silently change the game to no audio or prove NDSP is broken.
The reference port documents a retail NDSP startup abort, but that is not proof
that this project's NDSP adapter has the same fault.

This test owns two immutable mono planes (128,000 bytes). It uses granted
channels rather than assuming 8/9, one shared stereo-start command batch,
complete cache flushing and actual channel status. No elapsed-time DMA cursor,
looping ring, source drops or one-frame insertion is borrowed from the reference.
PCM is never rewritten while either channel is active. Output ownership is
shared with NDSP; service exit precedes freeing DMA memory.

Command-list acknowledgment yields in 1 ms steps and fails after 100 ms rather
than using libctru's unbounded `csndExecCmds(true)` spin. Failed/unacknowledged
lists poison the owner and retire its service before another list is allowed.
This bounds the acknowledgment poll, not every kernel/service IPC operation.
The legacy timer is quantized (2094 at requested 32 kHz); this one-shot test does
not claim exact long-running output-clock matching or seamless streaming.
Gameplay integration, timing, sleep/Home and physical-console audio acceptance
remain pending. Host fake-service tests and ARM compilation are not those tests.

References: [libctru CSND API](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/csnd.h),
[command execution/lifetime implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/csnd.c),
and [authorized Starwing reference](https://github.com/EstebanPdN/starwing-3ds/blob/753952be5b170059d95068350d37759a966a3bd5/platform/3ds/source/audio_3ds.cpp).
