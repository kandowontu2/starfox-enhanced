/* sfvr_room.h - roomscale walking in tracking space (STANDARD.md section 5),
 * generalised from Halo CE VR's vr_room_step / vr_room_moved / vr_room_hold /
 * head_offset.
 *
 * Idea: each game tick the player is moved by how far the head walked since the
 * last tick (the "standing point" is where the game's body has followed the head
 * to). Everything is in TRACKING SPACE: horizontal x, z in metres, the OpenXR
 * LOCAL/STAGE axes (+x right, +y up, -z forward), as the runtime reports the
 * head. Ports rotate the returned step by the game heading and convert units.
 *
 * Per game tick:
 *     float step[2];
 *     if (sfvr_room_step(&room, &cfg, head_xz, tracking_valid, step))
 *         move_player_by(rotate(step));      // may be blocked by a wall
 *     sfvr_room_moved(&room, head_xz);        // the whole step counts as taken
 * Hold (sfvr_room_hold) on anything that makes tracking untrustworthy: not
 * stereo/active, pause, cutscene, view lost. Recentre (sfvr_room_recentre) on
 * a system-layer recentre or XrEventDataReferenceSpaceChangePending.
 *
 * Per rendered frame, the head's lean offset from the standing point (so the
 * head moves the view only by what the game's own camera does not already carry):
 *     sfvr_room_lean(&room, &cfg, head_xyz, interpolation_fraction, off);
 * With roomscale disabled call sfvr_room_lean_fixed(recentre_origin, head, limit, off).
 *
 * Use sfvr_room_init() (or a state with held = 1): a zero state is "walking" from
 * the origin, and the step limit then swallows the first jump anyway.
 */
#ifndef SFVR_ROOM_H
#define SFVR_ROOM_H

#ifdef __cplusplus
extern "C" {
#endif

#define SFVR_ROOM_DEFAULT_STEP_LIMIT 0.5f
#define SFVR_ROOM_DEFAULT_LEAN_LIMIT 0.35f

typedef struct sfvr_room_config {
	float step_limit; /* metres per tick above which a step is no walk; default 0.5 */
	float lean_limit; /* metres, max head offset from the standing point; default 0.35 */
} sfvr_room_config;

typedef struct sfvr_room_state {
	int held;          /* 1 = next valid step only seeds the standing point */
	float previous[2]; /* standing point (x, z) one tick ago */
	float now[2];      /* standing point (x, z) after the last tick */
} sfvr_room_state;

void sfvr_room_config_default(sfvr_room_config *c);

/* held = 1, standing point at the origin. */
void sfvr_room_init(sfvr_room_state *s);

/* The head leans from where the player last stood until walking resumes:
 * previous = now, held = 1. */
void sfvr_room_hold(sfvr_room_state *s);

/* Standing point = head_xz (previous = now = head), held = 1 so the next step seeds
 * again (a recentre moves the tracking origin under the head). */
void sfvr_room_recentre(sfvr_room_state *s, const float head_xz[2]);

/* The step to apply this tick, in tracking space metres (out_step_xz is always
 * written, zero when not applying). Returns 1 when the step should be applied
 * (it may be zero length), 0 when not:
 *  - !tracking_valid or a non-finite head: hold, return 0.
 *  - first call after a hold/recentre/init: seeds the standing point at the
 *    head, return 0.
 *  - a step longer than step_limit (tracking lost and found, a recentre): the
 *    standing point is taken up to the head without moving, return 0. */
int sfvr_room_step(sfvr_room_state *s, const sfvr_room_config *c, const float head_xz[2], int tracking_valid, float out_step_xz[2]);

/* The whole step is taken, even if the game blocked it (the view stays with
 * the player, not in the wall): previous = now, now = head. */
void sfvr_room_moved(sfvr_room_state *s, const float head_xz[2]);

/* Head minus standing point (x, z blended previous -> now by blend_t, 0..1,
 * y taken as 0), clamped to lean_limit in 3D (direction kept). */
void sfvr_room_lean(const sfvr_room_state *s, const sfvr_room_config *c, const float head_xyz[3], float blend_t, float out_offset_xyz[3]);

/* Head minus a fixed origin, clamped to lean_limit in 3D. */
void sfvr_room_lean_fixed(const float origin_xyz[3], const float head_xyz[3], float lean_limit, float out_offset_xyz[3]);

#ifdef __cplusplus
}
#endif

#endif
