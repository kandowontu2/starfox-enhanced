#include "sfvr/sfvr_room.h"

#include <math.h>
#include <stddef.h>

void sfvr_room_config_default(sfvr_room_config *c)
{
	if (!c)
		return;
	c->step_limit = SFVR_ROOM_DEFAULT_STEP_LIMIT;
	c->lean_limit = SFVR_ROOM_DEFAULT_LEAN_LIMIT;
}

void sfvr_room_init(sfvr_room_state *s)
{
	if (!s)
		return;
	s->held = 1;
	s->previous[0] = s->previous[1] = 0.0f;
	s->now[0] = s->now[1] = 0.0f;
}

void sfvr_room_hold(sfvr_room_state *s)
{
	if (!s)
		return;
	s->previous[0] = s->now[0];
	s->previous[1] = s->now[1];
	s->held = 1;
}

void sfvr_room_recentre(sfvr_room_state *s, const float head_xz[2])
{
	if (!s || !head_xz)
		return;
	s->previous[0] = s->now[0] = head_xz[0];
	s->previous[1] = s->now[1] = head_xz[1];
	s->held = 1;
}

int sfvr_room_step(sfvr_room_state *s, const sfvr_room_config *c, const float head_xz[2], int tracking_valid, float out_step_xz[2])
{
	sfvr_room_config def;
	float dx = 0.0f, dz = 0.0f;

	if (out_step_xz)
		out_step_xz[0] = out_step_xz[1] = 0.0f;
	if (!s)
		return 0;
	if (!c)
	{
		sfvr_room_config_default(&def);
		c = &def;
	}
	if (!tracking_valid || !head_xz || !isfinite(head_xz[0]) || !isfinite(head_xz[1]))
	{
		sfvr_room_hold(s);
		return 0;
	}
	if (!s->held)
	{
		dx = head_xz[0] - s->now[0];
		dz = head_xz[1] - s->now[1];
		if (dx * dx + dz * dz > c->step_limit * c->step_limit)
			s->held = 1; /* no walk: take it up below, without moving */
	}
	if (s->held)
	{
		s->previous[0] = s->now[0] = head_xz[0];
		s->previous[1] = s->now[1] = head_xz[1];
		s->held = 0;
		return 0;
	}
	if (out_step_xz)
	{
		out_step_xz[0] = dx;
		out_step_xz[1] = dz;
	}
	return 1;
}

void sfvr_room_moved(sfvr_room_state *s, const float head_xz[2])
{
	if (!s || !head_xz)
		return;
	s->previous[0] = s->now[0];
	s->previous[1] = s->now[1];
	s->now[0] = head_xz[0];
	s->now[1] = head_xz[1];
}

void sfvr_room_lean_fixed(const float origin_xyz[3], const float head_xyz[3], float lean_limit, float out_offset_xyz[3])
{
	float v[3], length, scale = 1.0f;
	int i;

	if (!out_offset_xyz)
		return;
	for (i = 0; i < 3; i++)
		v[i] = (head_xyz ? head_xyz[i] : 0.0f) - (origin_xyz ? origin_xyz[i] : 0.0f);
	length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	if (!(lean_limit > 0.0f))
		scale = 0.0f;
	else if (length > lean_limit)
		scale = lean_limit / length;
	for (i = 0; i < 3; i++)
		out_offset_xyz[i] = v[i] * scale;
}

void sfvr_room_lean(const sfvr_room_state *s, const sfvr_room_config *c, const float head_xyz[3], float blend_t, float out_offset_xyz[3])
{
	sfvr_room_config def;
	float origin[3];

	if (!c)
	{
		sfvr_room_config_default(&def);
		c = &def;
	}
	if (!(blend_t > 0.0f))
		blend_t = 0.0f;
	if (blend_t > 1.0f)
		blend_t = 1.0f;
	if (s)
	{
		origin[0] = s->previous[0] + (s->now[0] - s->previous[0]) * blend_t;
		origin[2] = s->previous[1] + (s->now[1] - s->previous[1]) * blend_t;
	}
	else
		origin[0] = origin[2] = 0.0f;
	origin[1] = 0.0f;
	sfvr_room_lean_fixed(origin, head_xyz, c->lean_limit, out_offset_xyz);
}
