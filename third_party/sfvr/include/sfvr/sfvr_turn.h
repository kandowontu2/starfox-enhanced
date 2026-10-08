/* sfvr_turn.h - snap / smooth / off turning from right-stick X
 * (STANDARD.md section 5).
 *
 * SIGN: the returned yaw delta is in DEGREES, positive = turn RIGHT
 * (clockwise seen from above). Stick X positive (pushed right) gives a
 * positive delta. Ports convert to their own yaw sign convention (Halo's
 * yaw is counter-clockwise positive, so it negates).
 *
 * Snap: when |x| >= arm_threshold (0.7) and the stick is armed, return one
 * snap_angle_deg step with the stick's sign and disarm. The stick re-arms
 * only when |x| < rearm_threshold (0.3). Between 0.3 and 0.7 nothing
 * changes (hysteresis). A zero-initialised sfvr_turn_state is armed.
 *
 * Smooth: |x| <= smooth_deadzone (0.2) gives 0; the rest of the range is
 * rescaled to 0..1, times smooth_speed_dps (120), times dt capped at
 * max_dt_s (0.1) so a hitch cannot spin the player.
 *
 * Off: always 0. Snap state is re-armed whenever the mode is not snap.
 *
 * Snap angle is normalised to 15, 30, 45, 60 or 90 (nearest, tie to the
 * smaller); smooth speed is clamped to 30..360. NULL config = defaults.
 * The mode values match the settings enum order (snap, smooth, off).
 */
#ifndef SFVR_TURN_H
#define SFVR_TURN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sfvr_turn_mode {
	SFVR_TURN_SNAP = 0,
	SFVR_TURN_SMOOTH = 1,
	SFVR_TURN_OFF = 2
} sfvr_turn_mode;

#define SFVR_TURN_DEFAULT_SNAP_ANGLE 30.0f
#define SFVR_TURN_DEFAULT_ARM 0.7f
#define SFVR_TURN_DEFAULT_REARM 0.3f
#define SFVR_TURN_DEFAULT_SMOOTH_SPEED 120.0f
#define SFVR_TURN_MIN_SMOOTH_SPEED 30.0f
#define SFVR_TURN_MAX_SMOOTH_SPEED 360.0f
#define SFVR_TURN_DEFAULT_DEADZONE 0.2f
#define SFVR_TURN_DEFAULT_MAX_DT 0.1f

typedef struct sfvr_turn_config {
	sfvr_turn_mode mode;
	float snap_angle_deg;
	float arm_threshold;
	float rearm_threshold;
	float smooth_speed_dps;
	float smooth_deadzone;
	float max_dt_s;
} sfvr_turn_config;

typedef struct sfvr_turn_state {
	int disarmed; /* 0 = armed (also the zero-initialised state) */
} sfvr_turn_state;

void sfvr_turn_config_default(sfvr_turn_config *c);
void sfvr_turn_reset(sfvr_turn_state *s);

/* Nearest of 15, 30, 45, 60, 90 (tie to the smaller; NaN gives 30). */
int sfvr_turn_normalise_snap_angle(float degrees);
/* Clamp to 30..360 (NaN gives 120). */
float sfvr_turn_normalise_smooth_speed(float dps);
/* Stick travel past the deadzone rescaled to 0..1, sign kept. */
float sfvr_turn_deadzoned(float axis, float deadzone);

/* Yaw delta in degrees for this frame, positive = right. */
float sfvr_turn_update(sfvr_turn_state *s, const sfvr_turn_config *c, float stick_x, float dt_s);

#ifdef __cplusplus
}
#endif

#endif
