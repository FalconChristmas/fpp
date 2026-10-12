# No Display on a Video Output
A Virtual Matrix (or another pixel overlay model) is set to draw on an HDMI, DVI or DisplayPort
output, but nothing is answering on that connector. FPP keeps the output running in a standard
1920x1080 @ 60Hz mode so nothing has to be restarted, but there is nothing there to show it.

Things to check:

1. **The projector or monitor is on**, and set to the input this player is plugged into.
2. **The cable** is seated at both ends, and in the port named in the warning (on a Pi 4 or
   Pi 5, HDMI-A-1 is the port nearest the power connector).
3. **Anything in between**, such as an HDMI extender, splitter or switch, is powered.

There is nothing to restart once the display is back. FPP notices it within a few seconds
(about 10 at most), switches to the display's own resolution, and this warning clears by
itself.
