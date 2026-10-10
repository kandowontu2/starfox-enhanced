/* sfvr_settings.h - the VR settings key registry (STANDARD.md section 7, with the
 * keys of sections 4, 5, 6, 8 and 9).
 *
 * This is a registry and a set of value helpers only: key names, types,
 * defaults, ranges and enum strings. No file IO and no storage; ports own their
 * file formats (TOML `vr.` tables, Unity camelCase JSON) and persistence.
 *
 * Value model: every key's default/min/max is a double. BOOL: 0 or 1. INT:
 * integral. FLOAT: as is. ENUM: the default is an index into enum_values and
 * min..max is 0..(count-1). Units follow the standard (degrees, metres, seconds,
 * hud_scale in percent, haptics 0..1).
 *
 * Notes on per-port defaults the standard allows to differ: hud_mode (Halo: far;
 * default here is near), snap_angle (Daggerfall 45; default 30), hud_distance
 * (default 2.0 m is the near panel; far mode is ~15 m). snap_angle is clamped to
 * 15..90 here; also pass it through sfvr_turn_normalise_snap_angle().
 * dump_frame 0 means off.
 */
#ifndef SFVR_SETTINGS_H
#define SFVR_SETTINGS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The version of this key set; ports store it as the file's schema `version`. */
#define SFVR_SETTINGS_SCHEMA 1

typedef enum sfvr_setting_type {
	SFVR_SETTING_BOOL = 0,
	SFVR_SETTING_INT,
	SFVR_SETTING_FLOAT,
	SFVR_SETTING_ENUM
} sfvr_setting_type;

typedef struct sfvr_setting_key {
	const char *name;
	sfvr_setting_type type;
	double def;
	double min;
	double max;
	const char *const *enum_values; /* NULL-terminated; ENUM only, else NULL */
} sfvr_setting_key;

/* The table of standard keys, in the order of the standard (comfort, hud,
 * controls, display, diagnostics). */
size_t sfvr_settings_count(void);
const sfvr_setting_key *sfvr_settings_at(size_t index);

/* Exact (case-sensitive) name lookup; NULL when unknown. */
const sfvr_setting_key *sfvr_settings_find(const char *name);

/* Clamp to the descriptor's range. NaN gives the default. A NULL key returns the value. */
float sfvr_settings_clamp_float(const sfvr_setting_key *key, float value);
int sfvr_settings_clamp_int(const sfvr_setting_key *key, int value);

/* Number of strings in enum_values (0 for a non-enum key). */
int sfvr_settings_enum_count(const sfvr_setting_key *key);

/* Index of `text` among enum_values, ASCII case-insensitive, surrounding
 * whitespace ignored. NULL/unknown text gives the key's default index. */
int sfvr_settings_parse_enum(const sfvr_setting_key *key, const char *text);

/* "1"/"true"/"yes"/"on" -> 1, "0"/"false"/"no"/"off" -> 0 (case-insensitive,
 * surrounding whitespace ignored); anything else, or NULL, gives `fallback`. */
int sfvr_settings_parse_bool(const char *text, int fallback);

/* "<PREFIX>_VR_<KEY>" upper-cased, e.g. ("HALO", "snap_angle") ->
 * "HALO_VR_SNAP_ANGLE". Characters of key that are not letters or digits become
 * '_'. Returns 1 on success; 0 (buf = "" when n > 0) when the result does not
 * fit in n bytes including the NUL, or an argument is NULL. */
int sfvr_settings_env_name(const char *prefix, const char *key, char *buf, size_t n);

#ifdef __cplusplus
}
#endif

#endif
