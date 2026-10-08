/* sfvr_haptics.h - haptic event vocabulary and per-frame coalescing queue
 * (STANDARD.md section 6).
 *
 * Usage per frame: submit pulses from anywhere in the frame with
 * sfvr_haptic_submit()/sfvr_haptic_event(); once per frame call
 * sfvr_haptic_flush() with the user's `haptics` setting (0..1) and a callback
 * that does the actual xrApplyHapticFeedback.
 *
 * Coalescing: per hand the queue keeps the strongest amplitude and, separately,
 * the longest duration submitted since the last flush. Overlapping pulses
 * therefore become one pulse.
 *
 * Hands: 0 = left, 1 = right, SFVR_HAND_BOTH (2) = both. Other values are ignored.
 * Amplitudes are 0..1, durations in seconds.
 *
 * Default table (amplitude / seconds), from the standard:
 *   UI_HOVER     0.20 / 0.010   pointing hand
 *   UI_CLICK     0.50 / 0.030
 *   SYSTEM       0.60 / 0.080   both hands (sfvr_haptic_event always sends both)
 *   GESTURE      0.50 / 0.045   standard range 0.4-0.6 / 30-60 ms: midpoint
 *   WEAPON_FIRE  0.60 / 0.060   pistol; ports submit heavier weapons by hand
 *                               (shotgun 1.0/0.120, rocket 1.0/0.200)
 *   IMPACT       0.60 / 0.050   standard range 0.35-1.0 / 30-80 ms; scale by
 *                               speed or damage with sfvr_haptic_submit
 *   RUMBLE       0.50 / 0.040   authored rumble: use sfvr_rumble_to_haptic
 */
#ifndef SFVR_HAPTICS_H
#define SFVR_HAPTICS_H

#ifdef __cplusplus
extern "C" {
#endif

#define SFVR_HAND_LEFT 0
#define SFVR_HAND_RIGHT 1
#define SFVR_HAND_BOTH 2

#define SFVR_RUMBLE_PULSE_SECONDS 0.040f

typedef enum sfvr_haptic_event_id {
	SFVR_HAPTIC_UI_HOVER = 0,
	SFVR_HAPTIC_UI_CLICK,
	SFVR_HAPTIC_SYSTEM,
	SFVR_HAPTIC_GESTURE,
	SFVR_HAPTIC_WEAPON_FIRE,
	SFVR_HAPTIC_IMPACT,
	SFVR_HAPTIC_RUMBLE,
	SFVR_HAPTIC_EVENT_COUNT
} sfvr_haptic_event_id;

typedef struct sfvr_haptic_pulse {
	float amplitude; /* 0..1 */
	float seconds;
} sfvr_haptic_pulse;

typedef struct sfvr_haptic_queue {
	float amplitude[2];
	float seconds[2];
} sfvr_haptic_queue;

/* Called by flush once per hand with something to play. */
typedef void (*sfvr_haptic_callback)(int hand, float amplitude, float seconds, void *user);

/* The default table entry for an event ({0,0} for an unknown id). */
sfvr_haptic_pulse sfvr_haptic_default(sfvr_haptic_event_id event);
const char *sfvr_haptic_event_name(sfvr_haptic_event_id event);

/* A zero-initialised queue is empty. */
void sfvr_haptic_queue_clear(sfvr_haptic_queue *q);

void sfvr_haptic_submit(sfvr_haptic_queue *q, int hand, float amplitude, float seconds);
/* Submit the table pulse for an event. SYSTEM goes to both hands whatever `hand` is. */
void sfvr_haptic_event(sfvr_haptic_queue *q, int hand, sfvr_haptic_event_id event);

/* Scale each hand's pulse by strength (clamped 0..1), clamp the amplitude to
 * 0..1, call `callback` for hands with a non-zero amplitude (left first), then
 * clear the queue. Returns the number of callbacks made. callback may be NULL. */
int sfvr_haptic_flush(sfvr_haptic_queue *q, float strength, sfvr_haptic_callback callback, void *user);

/* Authored dual-band rumble (0..65535 each) to one amplitude:
 * max(low, high) / 65535. Pulse length is SFVR_RUMBLE_PULSE_SECONDS. */
float sfvr_rumble_to_haptic(unsigned low, unsigned high);
/* Convenience: submit sfvr_rumble_to_haptic() for 0.040 s. */
void sfvr_haptic_rumble(sfvr_haptic_queue *q, int hand, unsigned low, unsigned high);

#ifdef __cplusplus
}
#endif

#endif
