/* sfvr_view.h - the L View button state machine (STANDARD.md section 1).
 *
 * Short press = the game's Select/Back. Hold 1 s = recentre. Hold 3 s =
 * recentre and recalibrate standing height. The caller feeds the raw button
 * level once per frame (or more often) with the absolute time in seconds
 * and gets back a bitmask of the events this call produced. Pure logic: the
 * port does the buzz (sfvr_haptics.h, SFVR_HAPTIC_SYSTEM) and the recentre.
 *
 * Rules:
 *  - RECENTRE fires once per press, on the first update where the press has
 *    been held >= recentre_hold_s.
 *  - RECALIBRATE_HEIGHT fires once per press, on the first update where the
 *    same press has been held >= height_hold_s. After one very late update
 *    both bits can be set in the same return value.
 *  - SHORT_PRESS fires on the update that sees the release, only when no hold
 *    event fired. The release update counts the time up to its own `now_s`
 *    first, so a release that arrives late (irregular updates) still yields a
 *    hold event, never a short press.
 *  - First call, or first call after sfvr_view_reset(), while already
 *    pressed: the press is ignored until the button is released and pressed
 *    again. A zero-initialised sfvr_view_button is ready to use.
 *  - sfvr_view_reset() (focus loss, session stop) cancels the current press
 *    without any event.
 *  - A NULL config means the defaults. Time going backwards counts as 0 s held.
 */
#ifndef SFVR_VIEW_H
#define SFVR_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif

#define SFVR_VIEW_SHORT_PRESS 0x1u
#define SFVR_VIEW_RECENTRE 0x2u
#define SFVR_VIEW_RECALIBRATE_HEIGHT 0x4u

#define SFVR_VIEW_DEFAULT_RECENTRE_HOLD_S 1.0f
#define SFVR_VIEW_DEFAULT_HEIGHT_HOLD_S 3.0f

typedef struct sfvr_view_config {
	float recentre_hold_s; /* default 1.0 */
	float height_hold_s;   /* default 3.0; never below recentre_hold_s */
} sfvr_view_config;

typedef struct sfvr_view_button {
	int state; /* internal; 0 = not yet seen */
	double press_s;
	int recentred;
	int recalibrated;
} sfvr_view_button;

void sfvr_view_config_default(sfvr_view_config *c);

/* Optional: same as zero-initialising. */
void sfvr_view_reset(sfvr_view_button *b);

/* Returns a bitmask of SFVR_VIEW_* events produced by this call (0 = none).
 * `pressed` is the raw button level (non-zero = down). */
unsigned sfvr_view_update(sfvr_view_button *b, const sfvr_view_config *c, int pressed, double now_s);

#ifdef __cplusplus
}
#endif

#endif
