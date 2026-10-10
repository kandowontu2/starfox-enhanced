#include "sfvr/sfvr_haptics.h"

#include <stddef.h>

static const sfvr_haptic_pulse event_table[SFVR_HAPTIC_EVENT_COUNT] = {
	{ 0.20f, 0.010f }, /* UI_HOVER */
	{ 0.50f, 0.030f }, /* UI_CLICK */
	{ 0.60f, 0.080f }, /* SYSTEM */
	{ 0.50f, 0.045f }, /* GESTURE */
	{ 0.60f, 0.060f }, /* WEAPON_FIRE (pistol) */
	{ 0.60f, 0.050f }, /* IMPACT */
	{ 0.50f, SFVR_RUMBLE_PULSE_SECONDS } /* RUMBLE */
};

static const char *const event_names[SFVR_HAPTIC_EVENT_COUNT] = {
	"ui_hover", "ui_click", "system", "gesture", "weapon_fire", "impact", "rumble"
};

sfvr_haptic_pulse sfvr_haptic_default(sfvr_haptic_event_id event)
{
	sfvr_haptic_pulse none = { 0.0f, 0.0f };

	if ((int)event < 0 || (int)event >= SFVR_HAPTIC_EVENT_COUNT)
		return none;
	return event_table[event];
}

const char *sfvr_haptic_event_name(sfvr_haptic_event_id event)
{
	if ((int)event < 0 || (int)event >= SFVR_HAPTIC_EVENT_COUNT)
		return "unknown";
	return event_names[event];
}

void sfvr_haptic_queue_clear(sfvr_haptic_queue *q)
{
	if (!q)
		return;
	q->amplitude[0] = q->amplitude[1] = 0.0f;
	q->seconds[0] = q->seconds[1] = 0.0f;
}

static void submit_one(sfvr_haptic_queue *q, int hand, float amplitude, float seconds)
{
	if (amplitude > q->amplitude[hand])
		q->amplitude[hand] = amplitude;
	if (seconds > q->seconds[hand])
		q->seconds[hand] = seconds;
}

void sfvr_haptic_submit(sfvr_haptic_queue *q, int hand, float amplitude, float seconds)
{
	if (!q || hand < 0 || hand > SFVR_HAND_BOTH)
		return;
	/* NaN and negatives never beat the zero a cleared queue holds */
	if (!(amplitude > 0.0f))
		amplitude = 0.0f;
	if (!(seconds > 0.0f))
		seconds = 0.0f;
	if (hand == SFVR_HAND_BOTH)
	{
		submit_one(q, SFVR_HAND_LEFT, amplitude, seconds);
		submit_one(q, SFVR_HAND_RIGHT, amplitude, seconds);
	}
	else
		submit_one(q, hand, amplitude, seconds);
}

void sfvr_haptic_event(sfvr_haptic_queue *q, int hand, sfvr_haptic_event_id event)
{
	sfvr_haptic_pulse pulse;

	if ((int)event < 0 || (int)event >= SFVR_HAPTIC_EVENT_COUNT)
		return;
	pulse = event_table[event];
	if (event == SFVR_HAPTIC_SYSTEM)
		hand = SFVR_HAND_BOTH;
	sfvr_haptic_submit(q, hand, pulse.amplitude, pulse.seconds);
}

int sfvr_haptic_flush(sfvr_haptic_queue *q, float strength, sfvr_haptic_callback callback, void *user)
{
	int hand, count = 0;

	if (!q)
		return 0;
	if (!(strength > 0.0f))
		strength = 0.0f;
	if (strength > 1.0f)
		strength = 1.0f;
	for (hand = 0; hand < 2; hand++)
	{
		float amplitude = q->amplitude[hand] * strength, seconds = q->seconds[hand];

		if (amplitude > 1.0f)
			amplitude = 1.0f;
		if (amplitude > 0.0f)
		{
			if (callback)
				callback(hand, amplitude, seconds, user);
			count++;
		}
	}
	sfvr_haptic_queue_clear(q);
	return count;
}

float sfvr_rumble_to_haptic(unsigned low, unsigned high)
{
	unsigned loudest = low > high ? low : high;

	if (loudest > 65535u)
		loudest = 65535u;
	return (float)loudest / 65535.0f;
}

void sfvr_haptic_rumble(sfvr_haptic_queue *q, int hand, unsigned low, unsigned high)
{
	sfvr_haptic_submit(q, hand, sfvr_rumble_to_haptic(low, high), SFVR_RUMBLE_PULSE_SECONDS);
}
