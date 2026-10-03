#include "sfvr/sfvr_settings.h"

#include <string.h>

static const char *const turn_mode_values[] = { "snap", "smooth", "off", NULL };
static const char *const move_relative_values[] = { "head", "left", "right", NULL };
static const char *const hud_mode_values[] = { "near", "far", "leash", NULL };
static const char *const hand_values[] = { "right", "left", NULL };

#define BOOL_KEY(name, def) { name, SFVR_SETTING_BOOL, def, 0.0, 1.0, NULL }
#define INT_KEY(name, def, min, max) { name, SFVR_SETTING_INT, def, min, max, NULL }
#define FLOAT_KEY(name, def, min, max) { name, SFVR_SETTING_FLOAT, def, min, max, NULL }
#define ENUM_KEY(name, def, values, last) { name, SFVR_SETTING_ENUM, def, 0.0, last, values }

static const sfvr_setting_key keys[] = {
	/* section 5: comfort */
	ENUM_KEY("turn_mode", 0.0, turn_mode_values, 2.0),
	INT_KEY("snap_angle", 30.0, 15.0, 90.0),
	FLOAT_KEY("smooth_turn_speed", 120.0, 30.0, 360.0),
	BOOL_KEY("vignette", 0.0),
	FLOAT_KEY("vignette_strength", 0.6, 0.0, 1.0),
	ENUM_KEY("move_relative", 0.0, move_relative_values, 2.0),
	BOOL_KEY("roomscale", 0.0),
	FLOAT_KEY("lean_limit", 0.35, 0.15, 0.5),
	BOOL_KEY("duck_crouch", 1.0),
	FLOAT_KEY("height_offset", 0.0, -1.0, 1.0),
	BOOL_KEY("seated", 0.0),
	BOOL_KEY("horizon_lock", 0.0),
	BOOL_KEY("follow_vehicle_rotation", 0.0),
	FLOAT_KEY("head_translation", 1.0, 0.0, 2.0),
	BOOL_KEY("camera_shake", 0.0),
	/* section 4: HUD */
	ENUM_KEY("hud_mode", 0.0, hud_mode_values, 2.0),
	FLOAT_KEY("hud_distance", 2.0, 0.8, 20.0),
	FLOAT_KEY("hud_scale", 100.0, 50.0, 200.0),
	BOOL_KEY("hud_visible", 1.0),
	/* sections 2 and 6: controls */
	FLOAT_KEY("haptics", 0.6, 0.0, 1.0),
	ENUM_KEY("dominant_hand", 0.0, hand_values, 1.0),
	/* section 9: display */
	INT_KEY("refresh_rate", 90.0, 72.0, 144.0),
	FLOAT_KEY("resolution_scale", 1.0, 0.75, 1.5),
	/* section 8: diagnostics (config/env only, never in the menu) */
	BOOL_KEY("force_render", 0.0),
	FLOAT_KEY("diag_yaw", 0.0, -360.0, 360.0),
	FLOAT_KEY("diag_hand_yaw", 0.0, -360.0, 360.0),
	BOOL_KEY("diag_two_handed", 0.0),
	FLOAT_KEY("diag_walk_speed", 0.0, 0.0, 5.0),
	INT_KEY("dump_frame", 0.0, 0.0, 1000000.0),
	FLOAT_KEY("probe_seconds", 0.0, 0.0, 60.0),
	BOOL_KEY("timing", 0.0),
	BOOL_KEY("timing_gpu", 0.0)
};

#define KEY_COUNT (sizeof(keys) / sizeof(keys[0]))

size_t sfvr_settings_count(void)
{
	return KEY_COUNT;
}

const sfvr_setting_key *sfvr_settings_at(size_t index)
{
	return index < KEY_COUNT ? &keys[index] : NULL;
}

const sfvr_setting_key *sfvr_settings_find(const char *name)
{
	size_t i;

	if (!name)
		return NULL;
	for (i = 0; i < KEY_COUNT; i++)
		if (strcmp(keys[i].name, name) == 0)
			return &keys[i];
	return NULL;
}

float sfvr_settings_clamp_float(const sfvr_setting_key *key, float value)
{
	if (!key)
		return value;
	if (!(value == value))
		return (float)key->def;
	if ((double)value < key->min)
		return (float)key->min;
	if ((double)value > key->max)
		return (float)key->max;
	return value;
}

int sfvr_settings_clamp_int(const sfvr_setting_key *key, int value)
{
	if (!key)
		return value;
	if ((double)value < key->min)
		return (int)key->min;
	if ((double)value > key->max)
		return (int)key->max;
	return value;
}

int sfvr_settings_enum_count(const sfvr_setting_key *key)
{
	int count = 0;

	if (!key || !key->enum_values)
		return 0;
	while (key->enum_values[count])
		count++;
	return count;
}

static char lower(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static char upper(char c)
{
	return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static int is_space(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* does text, trimmed, equal word (word lower-case), ignoring ASCII case */
static int equals_word(const char *text, const char *word)
{
	size_t length = strlen(text), i;

	while (length && is_space(text[length - 1]))
		length--;
	while (length && is_space(*text))
	{
		text++;
		length--;
	}
	if (length != strlen(word))
		return 0;
	for (i = 0; i < length; i++)
		if (lower(text[i]) != lower(word[i]))
			return 0;
	return 1;
}

int sfvr_settings_parse_enum(const sfvr_setting_key *key, const char *text)
{
	int i;

	if (!key)
		return 0;
	if (text && key->enum_values)
		for (i = 0; key->enum_values[i]; i++)
			if (equals_word(text, key->enum_values[i]))
				return i;
	return (int)key->def;
}

int sfvr_settings_parse_bool(const char *text, int fallback)
{
	static const char *const on[] = { "1", "true", "yes", "on", NULL };
	static const char *const off[] = { "0", "false", "no", "off", NULL };
	int i;

	if (!text)
		return fallback;
	for (i = 0; on[i]; i++)
		if (equals_word(text, on[i]))
			return 1;
	for (i = 0; off[i]; i++)
		if (equals_word(text, off[i]))
			return 0;
	return fallback;
}

static int put(char *buf, size_t n, size_t *used, char c)
{
	if (*used + 1 >= n)
		return 0;
	buf[(*used)++] = c;
	return 1;
}

int sfvr_settings_env_name(const char *prefix, const char *key, char *buf, size_t n)
{
	static const char infix[] = "_VR_";
	size_t used = 0, i;

	if (!buf || n == 0)
		return 0;
	buf[0] = '\0';
	if (!prefix || !key)
		return 0;
	for (i = 0; prefix[i]; i++)
		if (!put(buf, n, &used, upper(prefix[i])))
			goto overflow;
	for (i = 0; infix[i]; i++)
		if (!put(buf, n, &used, infix[i]))
			goto overflow;
	for (i = 0; key[i]; i++)
	{
		char c = upper(key[i]);

		if (!put(buf, n, &used, ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) ? c : '_'))
			goto overflow;
	}
	buf[used] = '\0';
	return 1;
overflow:
	buf[0] = '\0';
	return 0;
}
