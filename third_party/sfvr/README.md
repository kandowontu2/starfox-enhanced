# sfvr

A tiny C99 library I use in my Steam Frame VR ports. It's the stuff every port
should do the same way (recentring, turning, haptics, settings names, the
`[vr-perf]` log line), so you don't have to relearn things when you switch games.
The conventions themselves are written up in [STANDARD.md](STANDARD.md).

It doesn't depend on OpenXR, GL or SDL, never allocates, and only uses
`<math.h>`, `<string.h>`, `<stdio.h>` and `<stddef.h>`. It builds clean with
`-std=c99 -Wall -Wextra -Werror -pedantic`, every header works from C++, and it
doesn't assume a pointer size, so it's fine in a 32-bit-pointer AArch64 build too.

Times are in seconds (`double` for absolute time, `float` for frame deltas),
distances in metres and angles in degrees unless the name ends in `_rad`.

## What's in it

| Section | Module |
| --- | --- |
| 1 System layer (L View short/hold) | `sfvr_view.h` |
| 5 Comfort turn and roomscale | `sfvr_turn.h`, `sfvr_room.h` |
| 6 Haptics vocabulary | `sfvr_haptics.h` |
| 7 Settings key registry | `sfvr_settings.h` |
| 8 `[vr-perf]` line | `sfvr_perf.h` |

The OpenXR session, settings file reading/writing and frame scheduling stay in
each game, since they're too tied to the engine.

## API summary

Include `sfvr/sfvr.h` for everything (it also has `SFVR_VERSION_MAJOR/MINOR/PATCH`,
currently 0.1.0), or just the header you need. Each header explains its rules at
the top.

```c
/* sfvr_view.h: bitmask of SFVR_VIEW_SHORT_PRESS | RECENTRE | RECALIBRATE_HEIGHT */
void     sfvr_view_config_default(sfvr_view_config *c);   /* 1.0 s, 3.0 s */
void     sfvr_view_reset(sfvr_view_button *b);            /* focus loss: cancel, no events */
unsigned sfvr_view_update(sfvr_view_button *b, const sfvr_view_config *c, int pressed, double now_s);

/* sfvr_turn.h: yaw delta in degrees, positive = turn right (clockwise from above) */
void  sfvr_turn_config_default(sfvr_turn_config *c);
void  sfvr_turn_reset(sfvr_turn_state *s);
float sfvr_turn_update(sfvr_turn_state *s, const sfvr_turn_config *c, float stick_x, float dt_s);
int   sfvr_turn_normalise_snap_angle(float degrees);      /* 15 30 45 60 90 */
float sfvr_turn_normalise_smooth_speed(float dps);        /* 30..360 */
float sfvr_turn_deadzoned(float axis, float deadzone);

/* sfvr_haptics.h: hand 0 = left, 1 = right, SFVR_HAND_BOTH = 2 */
sfvr_haptic_pulse sfvr_haptic_default(sfvr_haptic_event_id event);
void  sfvr_haptic_queue_clear(sfvr_haptic_queue *q);
void  sfvr_haptic_submit(sfvr_haptic_queue *q, int hand, float amplitude, float seconds);
void  sfvr_haptic_event(sfvr_haptic_queue *q, int hand, sfvr_haptic_event_id event);
int   sfvr_haptic_flush(sfvr_haptic_queue *q, float strength, sfvr_haptic_callback cb, void *user);
float sfvr_rumble_to_haptic(unsigned low, unsigned high); /* max/65535; pulse 0.040 s */
void  sfvr_haptic_rumble(sfvr_haptic_queue *q, int hand, unsigned low, unsigned high);

/* sfvr_room.h: tracking-space x,z metres */
void sfvr_room_init(sfvr_room_state *s);
void sfvr_room_hold(sfvr_room_state *s);
void sfvr_room_recentre(sfvr_room_state *s, const float head_xz[2]);
int  sfvr_room_step(sfvr_room_state *s, const sfvr_room_config *c, const float head_xz[2], int tracking_valid, float out_step_xz[2]);
void sfvr_room_moved(sfvr_room_state *s, const float head_xz[2]);
void sfvr_room_lean(const sfvr_room_state *s, const sfvr_room_config *c, const float head_xyz[3], float blend_t, float out_offset_xyz[3]);
void sfvr_room_lean_fixed(const float origin_xyz[3], const float head_xyz[3], float lean_limit, float out_offset_xyz[3]);

/* sfvr_perf.h: "[vr-perf] fps=71.9 missed=3 cpu=7.31ms logic=1.20 render=5.02 eye=2.21/2.19ms gpu=8.40ms" */
void sfvr_perf_init(sfvr_perf *p, double window_s, const char *const *stage_names, int stage_count);
void sfvr_perf_add_frame(sfvr_perf *p, double now_s, int missed, const float *stage_ms, const float eye_ms[2], float gpu_ms);
int  sfvr_perf_poll(sfvr_perf *p, double now_s, char *buf, size_t n);

/* sfvr_settings.h: registry only, no file IO */
size_t sfvr_settings_count(void);
const sfvr_setting_key *sfvr_settings_at(size_t i);
const sfvr_setting_key *sfvr_settings_find(const char *name);
float sfvr_settings_clamp_float(const sfvr_setting_key *k, float v);
int   sfvr_settings_clamp_int(const sfvr_setting_key *k, int v);
int   sfvr_settings_parse_enum(const sfvr_setting_key *k, const char *text);
int   sfvr_settings_parse_bool(const char *text, int fallback);
int   sfvr_settings_env_name(const char *prefix, const char *key, char *buf, size_t n); /* HALO, snap_angle -> HALO_VR_SNAP_ANGLE */
```

## Build and test

```
make test            # clang strict C99 + each header as a C++17 TU + all tests
make test CC=gcc     # the C parts with gcc
make sanitize        # tests under ASan + UBSan
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

`make test` also compiles each header on its own as C and as C++, and links a
small C++ program against the library to make sure that keeps working.

## Using it in a game

Just copy it in, no submodule. The copy becomes part of that game's source and
uses the game's licence.

1. Copy `include/` and `src/` to `<game>/third_party/sfvr/`.
2. Put the sfvr commit hash in `<game>/third_party/sfvr/VERSION`.
3. Add `third_party/sfvr/include` to the include path and the `src/*.c` files to
   the build. They only need `-lm`.
4. Try not to edit the copy. If something needs changing, change it here, add a
   test, and copy it over again so the games stay in sync.

```
sh -c 'dst=<port>/third_party/sfvr; rm -rf "$dst" && mkdir -p "$dst" &&
       cp -R include src "$dst"/ && git rev-parse HEAD > "$dst/VERSION"'
```
