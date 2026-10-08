# Fit to Screen

Select **DISPLAY → FIT TO SCREEN** in the pre-game options. This shared runtime
option fills the current window (or the display in fullscreen) on desktop and
mobile builds. Resizing a desktop window updates the horizontal game canvas;
the option does not resize the window itself. The selection is saved.

Fit to Screen has separate Original and EX HUD layouts under **Options →
Customize Screen**, independent of all fixed-aspect profiles. Existing saved
layouts are preserved; the new Fit layouts start at their default positions.

Desktop launches default to fullscreen. **Options → Fullscreen** changes and
saves this preference; Alt+Enter also toggles it. `--fullscreen` overrides a
saved windowed preference for that launch.

The fixed 4:3, 16:9, 16:10, 21:9 and 32:9 presets remain available. Existing iOS
FIT DEVICE settings retain their previous behavior. Fit to Screen is also
available there as the new explicit choice.

The canvas retains the existing 256–800 source-pixel width limits. Outside that
aspect range, presentation stretches to fill rather than introducing bars.
Game-authored black regions and transitions are not cropped away. This is a
flat-display option, not a change to headset projection or field of view.
