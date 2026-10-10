/* sfvr_perf.h - window accumulator for the `[vr-perf]` log line
 * (STANDARD.md section 8).
 *
 * Setup: sfvr_perf_init(&p, 10.0, names, count) with up to 8 caller-named CPU
 * stages (names are copied, at most 15 characters each, so the strings need not
 * outlive the call). Per frame: sfvr_perf_add_frame(). Once per frame (or
 * whenever convenient): sfvr_perf_poll(); when the window has elapsed it writes
 * one line and starts the next window.
 *
 * LINE FORMAT (one line, no trailing newline):
 *
 *   [vr-perf] fps=71.9 missed=3 cpu=7.31ms logic=1.20 render=5.02 eye=2.21/2.19ms gpu=8.40ms
 *
 *   fps      frames in the window / window length, 1 decimal ("%.1f")
 *   missed   number of frames added with the missed flag, integer
 *   cpu      mean over frames of the sum of all stages, ms, 2 decimals, then "ms"
 *   <stage>  one `name=value` per stage in init order, window mean in ms, 2
 *            decimals, no unit (the cpu= value carries it)
 *   eye      window mean per-eye ms, left/right, 2 decimals, then "ms"
 *   gpu      window mean GPU ms, 2 decimals, then "ms"; `gpu=n/a` when no frame in
 *            the window had a GPU time (gpu_ms < 0). Means use only the frames that
 *            had one.
 *
 * With no stages the line is `... cpu=0.00ms eye=...` (no stage fields). The
 * window length is measured from the first sfvr_perf_add_frame()/poll after
 * init (or sfvr_perf_reset) to the `now_s` of the poll that emits; the next
 * window then starts at that same time, so windows are back to back.
 * A window with no frames still emits (fps=0.0, zero means): a stall is data.
 * Numbers use the C locale's decimal point as long as the host never calls
 * setlocale (games that do should not use this).
 */
#ifndef SFVR_PERF_H
#define SFVR_PERF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SFVR_PERF_MAX_STAGES 8
#define SFVR_PERF_STAGE_NAME_MAX 16 /* including the terminating NUL */
#define SFVR_PERF_DEFAULT_WINDOW_S 10.0

typedef struct sfvr_perf {
	double window_s;
	double start_s;
	int started;
	int stage_count;
	char stage_name[SFVR_PERF_MAX_STAGES][SFVR_PERF_STAGE_NAME_MAX];
	/* the window so far */
	unsigned long frames;
	unsigned long missed;
	unsigned long gpu_frames;
	double stage_sum_ms[SFVR_PERF_MAX_STAGES];
	double eye_sum_ms[2];
	double gpu_sum_ms;
} sfvr_perf;

/* window_s <= 0 gives 10. stage_names may be NULL when stage_count is 0;
 * stage_count is clamped to 0..SFVR_PERF_MAX_STAGES; a NULL name becomes "stageN". */
void sfvr_perf_init(sfvr_perf *p, double window_s, const char *const *stage_names, int stage_count);

/* Drop the current window's data (the next frame or poll starts a new window). */
void sfvr_perf_reset(sfvr_perf *p);

/* stage_ms: stage_count values (NULL = zeros); eye_ms: 2 values (NULL = zeros);
 * gpu_ms < 0 means unknown. `missed` non-zero counts a missed frame. */
void sfvr_perf_add_frame(sfvr_perf *p, double now_s, int missed, const float *stage_ms, const float eye_ms[2], float gpu_ms);

/* When now_s - window start >= window_s: format the line into buf (always
 * NUL-terminated, truncated if n is too small), reset, return 1. Otherwise 0.
 * Returns 0 and changes nothing when buf is NULL or n is 0. */
int sfvr_perf_poll(sfvr_perf *p, double now_s, char *buf, size_t n);

#ifdef __cplusplus
}
#endif

#endif
