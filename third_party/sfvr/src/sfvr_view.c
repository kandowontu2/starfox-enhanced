#include "sfvr/sfvr_view.h"

#include <stddef.h>

enum { VIEW_UNSEEN = 0, VIEW_IDLE, VIEW_WAIT_RELEASE, VIEW_DOWN };

void sfvr_view_config_default(sfvr_view_config *c)
{
	if (!c)
		return;
	c->recentre_hold_s = SFVR_VIEW_DEFAULT_RECENTRE_HOLD_S;
	c->height_hold_s = SFVR_VIEW_DEFAULT_HEIGHT_HOLD_S;
}

void sfvr_view_reset(sfvr_view_button *b)
{
	if (!b)
		return;
	b->state = VIEW_UNSEEN;
	b->press_s = 0.0;
	b->recentred = 0;
	b->recalibrated = 0;
}

unsigned sfvr_view_update(sfvr_view_button *b, const sfvr_view_config *c, int pressed, double now_s)
{
	sfvr_view_config def;
	unsigned events = 0;
	double held;

	if (!b)
		return 0;
	if (!c)
	{
		sfvr_view_config_default(&def);
		c = &def;
	}
	pressed = pressed != 0;

	switch (b->state)
	{
	case VIEW_IDLE:
		if (pressed)
		{
			b->state = VIEW_DOWN;
			b->press_s = now_s;
			b->recentred = 0;
			b->recalibrated = 0;
		}
		return 0;
	case VIEW_DOWN:
		break;
	case VIEW_WAIT_RELEASE:
		if (!pressed)
			b->state = VIEW_IDLE;
		return 0;
	default: /* VIEW_UNSEEN: a button already down is not a press we saw start */
		b->state = pressed ? VIEW_WAIT_RELEASE : VIEW_IDLE;
		return 0;
	}

	held = now_s - b->press_s;
	if (held < 0.0)
		held = 0.0;
	{
		float recentre = c->recentre_hold_s > 0.0f ? c->recentre_hold_s : SFVR_VIEW_DEFAULT_RECENTRE_HOLD_S;
		float height = c->height_hold_s > recentre ? c->height_hold_s : recentre;

		if (!b->recentred && held >= (double)recentre)
		{
			b->recentred = 1;
			events |= SFVR_VIEW_RECENTRE;
		}
		if (!b->recalibrated && held >= (double)height)
		{
			b->recalibrated = 1;
			events |= SFVR_VIEW_RECALIBRATE_HEIGHT;
		}
	}
	if (!pressed)
	{
		if (!b->recentred && !b->recalibrated)
			events |= SFVR_VIEW_SHORT_PRESS;
		b->state = VIEW_IDLE;
	}
	return events;
}
