#include "sfvr/sfvr_turn.h"

#include <math.h>
#include <stddef.h>

void sfvr_turn_config_default(sfvr_turn_config *c)
{
	if (!c)
		return;
	c->mode = SFVR_TURN_SNAP;
	c->snap_angle_deg = SFVR_TURN_DEFAULT_SNAP_ANGLE;
	c->arm_threshold = SFVR_TURN_DEFAULT_ARM;
	c->rearm_threshold = SFVR_TURN_DEFAULT_REARM;
	c->smooth_speed_dps = SFVR_TURN_DEFAULT_SMOOTH_SPEED;
	c->smooth_deadzone = SFVR_TURN_DEFAULT_DEADZONE;
	c->max_dt_s = SFVR_TURN_DEFAULT_MAX_DT;
}

void sfvr_turn_reset(sfvr_turn_state *s)
{
	if (s)
		s->disarmed = 0;
}

int sfvr_turn_normalise_snap_angle(float degrees)
{
	static const int angles[5] = { 15, 30, 45, 60, 90 };
	int best, i;
	float best_distance;

	if (!(degrees == degrees)) /* NaN */
		return angles[1];
	best_distance = fabsf((float)angles[0] - degrees);
	best = 0;
	for (i = 1; i < 5; i++)
	{
		float distance = fabsf((float)angles[i] - degrees);

		if (distance < best_distance)
		{
			best = i;
			best_distance = distance;
		}
	}
	return angles[best];
}

float sfvr_turn_normalise_smooth_speed(float dps)
{
	if (!isfinite(dps))
		return SFVR_TURN_DEFAULT_SMOOTH_SPEED;
	if (dps < SFVR_TURN_MIN_SMOOTH_SPEED)
		return SFVR_TURN_MIN_SMOOTH_SPEED;
	if (dps > SFVR_TURN_MAX_SMOOTH_SPEED)
		return SFVR_TURN_MAX_SMOOTH_SPEED;
	return dps;
}

float sfvr_turn_deadzoned(float axis, float deadzone)
{
	float magnitude = fabsf(axis), scaled;

	if (!isfinite(axis))
		return 0.0f;
	if (!(deadzone >= 0.0f))
		deadzone = 0.0f;
	if (deadzone > 0.95f)
		deadzone = 0.95f;
	if (magnitude <= deadzone)
		return 0.0f;
	scaled = (magnitude - deadzone) / (1.0f - deadzone);
	if (scaled > 1.0f)
		scaled = 1.0f;
	return axis < 0.0f ? -scaled : scaled;
}

float sfvr_turn_update(sfvr_turn_state *s, const sfvr_turn_config *c, float stick_x, float dt_s)
{
	sfvr_turn_config def;
	float magnitude;

	if (!s)
		return 0.0f;
	if (!c)
	{
		sfvr_turn_config_default(&def);
		c = &def;
	}
	if (!isfinite(stick_x))
		stick_x = 0.0f;
	if (stick_x > 1.0f)
		stick_x = 1.0f;
	else if (stick_x < -1.0f)
		stick_x = -1.0f;
	magnitude = fabsf(stick_x);

	if (c->mode == SFVR_TURN_SMOOTH)
	{
		float dt = isfinite(dt_s) ? dt_s : 0.0f, cap = c->max_dt_s > 0.0f ? c->max_dt_s : SFVR_TURN_DEFAULT_MAX_DT;

		s->disarmed = 0;
		if (dt < 0.0f)
			dt = 0.0f;
		if (dt > cap)
			dt = cap;
		return sfvr_turn_deadzoned(stick_x, c->smooth_deadzone) * sfvr_turn_normalise_smooth_speed(c->smooth_speed_dps) * dt;
	}
	if (c->mode != SFVR_TURN_SNAP)
	{
		s->disarmed = 0;
		return 0.0f;
	}

	if (s->disarmed)
	{
		if (magnitude < c->rearm_threshold)
			s->disarmed = 0;
		return 0.0f;
	}
	if (magnitude >= c->arm_threshold)
	{
		float angle = (float)sfvr_turn_normalise_snap_angle(c->snap_angle_deg);

		s->disarmed = 1;
		return stick_x < 0.0f ? -angle : angle;
	}
	return 0.0f;
}
