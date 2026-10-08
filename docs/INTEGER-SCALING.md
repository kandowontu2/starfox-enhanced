# Integer scaling

The main page has **INTEGER SCALING: OFF / ON**, immediately after DISPLAY.
It is saved as `INTEGER_SCALING 0` or `INTEGER_SCALING 1` in the existing
pre-game settings file, and is also retained in save states. Default is OFF.

ON scales the presented canvas by whole-number factors, with black margins
where necessary. This takes precedence over Fit to Screen's fractional stretch;
turn it OFF to fill the panel again. It does not change the internal render
upscale setting or add rendering work. Touch controls remain in screen space.
