# Display Output Failed To Start
A framebuffer-backed display output (FB matrix / virtual matrix) could not start, so it will not show anything. The message names the specific problem:

- **Could not open / map framebuffer device** — the configured framebuffer (e.g. `/dev/fb0`) is wrong, in use, or not present.
- **Unsupported color depth** — the framebuffer is not 16- or 24-bit.
- **X11 virtual display: could not connect to the X server** — no X server is running/reachable.

1. Open [Channel Outputs](../../channeloutputs.php) and check the display output's settings (correct framebuffer device, dimensions, color depth).
2. Confirm the target framebuffer/display actually exists on this hardware.
3. For more detail check `fppd.log`; or open **Help → [Troubleshooting Commands](../../troubleshooting.php)** and run **Video → Video** to confirm the framebuffer (e.g. `/dev/fb0`) exists, and **Video → DRM Connectors** to see whether the display is detected.

Note: on a device that has a cape or hat installed, some outputs are hidden while **User Interface Level** is set to **Basic**. Set it to **Advanced** on the [System](../../settings.php#settings-system) settings page to see and edit them.
