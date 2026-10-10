# Steam Frame VR conventions

I've been porting a few old games to run natively on the Steam Frame, and I got tired of every one of them doing recentre, menus and comfort settings slightly differently. So these are the conventions they all share now, and sfvr is the bit of code that implements the fiddly parts. The section numbers are what the headers point at.

A lot of this comes from Valve's Steam Frame docs, Meta's and Microsoft's VR design guidance, and stuff that's worked well in other flat-to-VR ports (Team Beef's Quest ports, HaloCEVR, UEVR, DFUVR).

Games can add their own controls on top. They just shouldn't move the system buttons in section 1, and the buttons should always match what the game shows on screen.

## 0. What Valve asks for

At least 72 fps at 1728x1728 per eye. The default Frame bindings have to reach everything, and menus have to work with just the controllers. Use Frame (or Xbox) button names. Don't touch the System button, the runtime owns it. When the game loses focus, keep submitting frames, ignore input and pause.

## 1. System buttons

| Input | What it does |
| --- | --- |
| Right Menu | The game's pause/Start |
| Left View, tap | The game's Select or Back (back in menus) |
| Left View, hold 1 s | Recentre, with a little buzz on both hands |
| Left View, hold 3 s | Recentre and reset your standing height too, with a second buzz |
| Pause menu, "VR settings" | Opens the VR settings. If the game's pause menu can't be extended (say it's driven by the original ROM), hold Menu + View together for half a second instead |
| VR settings, "Quit to Steam" | Asks first, then exits cleanly with `xrRequestExitSession` |

Recentring moves your head's yaw into the game's heading and your horizontal position into the play-space origin, re-places any floating panels, and happens between stereo frames. A recentre from the runtime itself (`XrEventDataReferenceSpaceChangePending`) does the same thing. Stick clicks and button chords never do system stuff, they stay game controls.

`sfvr_view` handles the View button timing.

## 2. Default button roles

| Role | Button |
| --- | --- |
| Move or steer | Left stick |
| Turn | Right stick X (snap or smooth, see section 5), or whatever the game needs for vehicles and rail shooters |
| Main action | Trigger on your dominant hand |
| Second action | Other trigger |
| Grab, hold, gesture modifier | Grips |
| Extra actions | Bumpers |
| Jump or confirm | A |
| Cancel or back | B |
| Context actions | X, Y |
| Quick select | Left D-pad |
| Toggles | Stick clicks |

There's one `dominant_hand` setting that swaps everything tied to a hand (aim, triggers, grips, weapon, menu pointer). It doesn't move the physical buttons, since the Frame controllers aren't symmetrical. Index and Touch bindings should still reach everything, and the Frame profile only gets suggested when `XR_VALVE_frame_controller_interaction` is there.

## 3. Menus

Menus go on a flat panel about 2 m in front of you, roughly 50 degrees wide. It's anchored on yaw only and gets placed when it opens and when you recentre. Point at it with your dominant hand, the trigger clicks, A confirms, B or a tap of View goes back, and the right stick scrolls. Plain gamepad navigation always works too. Keep targets at least 2.5 to 3 degrees and text at least 0.6 degrees tall.

## 4. HUD

If the game can put its HUD somewhere physical (a cockpit dash, the gun, your wrist), that's the nicest option. Otherwise the HUD gets drawn in its own pass and shown on a panel that follows your head's yaw and pitch, but never roll. `hud_mode` picks where:

| Mode | Where |
| --- | --- |
| `near` | About 1.5 to 2 m away, about 40 degrees wide |
| `far` | About 15 m away, drawn from your head so markers line up with what they point at |
| `leash` | Follows your body and only turns once you've looked more than 20 to 30 degrees away |

Switching between flat, stereo and cutscene views fades through black.

## 5. Comfort

| Setting | Default | Range |
| --- | --- | --- |
| `turn_mode` | `snap` | `snap`, `smooth`, `off` |
| `snap_angle` | 30 | 15, 30, 45, 60, 90 |
| `smooth_turn_speed` | 120 deg/s | 30 to 360 |
| `vignette`, `vignette_strength` | off, 0.6 | only for artificial motion |
| `roomscale`, `lean_limit` | off, 0.35 m | 0.15 to 0.5 m |
| `horizon_lock`, `follow_vehicle_rotation` | off | vehicles and cockpits |
| `head_translation` | 1.0 | 0 to 2 |

`sfvr_turn` does snap turning (it fires at 70% stick and re-arms under 30%) and smooth turning (20% dead zone, frame time capped at 0.1 s). `sfvr_room` does roomscale walking: your character's collision body follows your real steps through the game's own collision code, a jump of more than 0.5 m in one step is ignored, and leaning is clamped.

## 6. Haptics

Everything is scaled by a `haptics` strength (0 to 1, default 0.6), and if two pulses land on the same hand in one frame, only the stronger one plays.

| Event | Strength / length |
| --- | --- |
| `ui_hover` | 0.2 / 10 ms |
| `ui_click` | 0.5 / 30 ms |
| `system` | 0.6 / 80 ms, both hands |
| `gesture` | about 0.5 / 45 ms |
| `weapon_fire` | depends on the weapon |
| `impact` | 0.35 to 1.0 / 30 to 80 ms |
| `rumble` | the game's own rumble, as max(low, high) in 40 ms pulses |

That's `sfvr_haptics`.

## 7. Settings

Settings use the names above and live in a versioned file in the game's save folder. Missing keys get their defaults, out-of-range values get clamped, unknown keys are left alone, and writes are atomic. Environment overrides look like `<PORT>_VR_<KEY>`. The in-headset settings page uses the same tab order everywhere: Comfort, Controls, HUD & menus, Display & performance, Accessibility & audio, Room, then Quit to Steam.

`sfvr_settings` has the list of keys and some helpers. Reading and writing the file is up to each game.

## 8. Debugging

| Setting | What it does |
| --- | --- |
| `force_render` | Renders stereo while the headset's asleep, with a fake head |
| `diag_*` | Fake head and hand poses for testing without wearing it |
| `dump_frame` | Saves the eye and HUD images for frame N |
| `timing`, `timing_gpu` | Logs timings per stage, and GPU time |

Every game logs the same line every 10 seconds, which `sfvr_perf` builds:

```
[vr-perf] fps=71.9 missed=3 cpu=7.31ms logic=1.20 render=5.02 eye=2.21/2.19ms gpu=8.40ms
```

## 9. Refresh rate

Ask for a refresh rate with `XR_FB_display_refresh_rate` (the highest one offered at or below the target, 90 Hz by default). If it can't hold 90% of that for two 10-second stretches in a row while you're playing, drop to 72 Hz. Render at the runtime's recommended size times `resolution_scale`.

## 10. Code

Keep the OpenXR calls behind a table of function pointers, so the session and bindings can be tested without a headset. sfvr itself is plain C99 with no allocations and no platform dependencies. Each game keeps its own copy, and that copy is under the game's licence.
