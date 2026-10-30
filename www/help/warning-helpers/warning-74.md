# Display Refused Video
A video started but the display refused to show it, so the video may stop. Audio and pixel output are unaffected. The message names the display, and the reason:

- **No space left on device**, on a Raspberry Pi 0–3 — the display hardware can't keep up with this video on this display, often because the video has to be scaled down to a smaller screen or shares the port with a Virtual Matrix output. Encode the video at the display's resolution, or turn a Virtual Matrix on that port off while videos play.
- **Anything else in brackets** — the reason the display gave; check `fppd.log` for more.

**Help → [Troubleshooting Commands](../../troubleshooting.php)** shows what the display offers: **Video → DRM Connectors** / **kmsprint** for its modes and the one in use, **Display EDID** for what it reports about itself, and **Display Engine Load** (Pi 0–3) for the display load limit.
