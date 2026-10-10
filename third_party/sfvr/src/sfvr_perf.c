#include "sfvr/sfvr_perf.h"

#include <stdio.h>
#include <string.h>

void sfvr_perf_reset(sfvr_perf *p)
{
	int i;

	if (!p)
		return;
	p->started = 0;
	p->start_s = 0.0;
	p->frames = p->missed = p->gpu_frames = 0;
	for (i = 0; i < SFVR_PERF_MAX_STAGES; i++)
		p->stage_sum_ms[i] = 0.0;
	p->eye_sum_ms[0] = p->eye_sum_ms[1] = 0.0;
	p->gpu_sum_ms = 0.0;
}

void sfvr_perf_init(sfvr_perf *p, double window_s, const char *const *stage_names, int stage_count)
{
	int i;

	if (!p)
		return;
	memset(p, 0, sizeof(*p));
	p->window_s = window_s > 0.0 ? window_s : SFVR_PERF_DEFAULT_WINDOW_S;
	if (stage_count < 0 || !stage_names)
		stage_count = 0;
	if (stage_count > SFVR_PERF_MAX_STAGES)
		stage_count = SFVR_PERF_MAX_STAGES;
	p->stage_count = stage_count;
	for (i = 0; i < stage_count; i++)
	{
		if (stage_names[i])
		{
			strncpy(p->stage_name[i], stage_names[i], SFVR_PERF_STAGE_NAME_MAX - 1);
			p->stage_name[i][SFVR_PERF_STAGE_NAME_MAX - 1] = '\0';
		}
		else
			snprintf(p->stage_name[i], SFVR_PERF_STAGE_NAME_MAX, "stage%d", i);
	}
	sfvr_perf_reset(p);
}

static void start_window(sfvr_perf *p, double now_s)
{
	if (!p->started)
	{
		p->started = 1;
		p->start_s = now_s;
	}
}

void sfvr_perf_add_frame(sfvr_perf *p, double now_s, int missed, const float *stage_ms, const float eye_ms[2], float gpu_ms)
{
	int i;

	if (!p)
		return;
	start_window(p, now_s);
	p->frames++;
	if (missed)
		p->missed++;
	if (stage_ms)
		for (i = 0; i < p->stage_count; i++)
			p->stage_sum_ms[i] += (double)stage_ms[i];
	if (eye_ms)
	{
		p->eye_sum_ms[0] += (double)eye_ms[0];
		p->eye_sum_ms[1] += (double)eye_ms[1];
	}
	if (gpu_ms >= 0.0f)
	{
		p->gpu_sum_ms += (double)gpu_ms;
		p->gpu_frames++;
	}
}

/* append printf-style text, keeping buf terminated and never past n */
static void append(char *buf, size_t n, size_t *used, const char *format, double a, double b)
{
	int written;

	if (*used >= n - 1)
		return;
	written = snprintf(buf + *used, n - *used, format, a, b);
	if (written < 0)
		return;
	if ((size_t)written >= n - *used)
		*used = n - 1;
	else
		*used += (size_t)written;
}

static void append_text(char *buf, size_t n, size_t *used, const char *text)
{
	size_t length = strlen(text);

	if (*used >= n - 1)
		return;
	if (length > n - 1 - *used)
		length = n - 1 - *used;
	memcpy(buf + *used, text, length);
	*used += length;
	buf[*used] = '\0';
}

int sfvr_perf_poll(sfvr_perf *p, double now_s, char *buf, size_t n)
{
	double frames, elapsed, cpu = 0.0;
	size_t used = 0;
	int i;

	if (!p || !buf || n == 0)
		return 0;
	if (!p->started)
	{
		start_window(p, now_s);
		return 0;
	}
	elapsed = now_s - p->start_s;
	if (elapsed < p->window_s)
		return 0;

	frames = p->frames ? (double)p->frames : 1.0;
	for (i = 0; i < p->stage_count; i++)
		cpu += p->stage_sum_ms[i] / frames;

	buf[0] = '\0';
	append(buf, n, &used, "[vr-perf] fps=%.1f", elapsed > 0.0 ? (double)p->frames / elapsed : 0.0, 0.0);
	append(buf, n, &used, " missed=%.0f", (double)p->missed, 0.0);
	append(buf, n, &used, " cpu=%.2fms", cpu, 0.0);
	for (i = 0; i < p->stage_count; i++)
	{
		append_text(buf, n, &used, " ");
		append_text(buf, n, &used, p->stage_name[i]);
		append(buf, n, &used, "=%.2f", p->stage_sum_ms[i] / frames, 0.0);
	}
	append(buf, n, &used, " eye=%.2f/%.2fms", p->eye_sum_ms[0] / frames, p->eye_sum_ms[1] / frames);
	if (p->gpu_frames)
		append(buf, n, &used, " gpu=%.2fms", p->gpu_sum_ms / (double)p->gpu_frames, 0.0);
	else
		append_text(buf, n, &used, " gpu=n/a");

	/* windows are back to back: the next starts where this one ended */
	sfvr_perf_reset(p);
	p->started = 1;
	p->start_s = now_s;
	return 1;
}
