# Video Output Pipeline Failed
FPP could not build or start the GStreamer pipeline used for video output, so video will not display (audio and pixel output are unaffected).

Common causes named in the message:

- **kmssink element missing** — the GStreamer plugin needed for HDMI video is not installed.
- **Could not open DRM device** — FPP could not access the HDMI/display hardware.
- **Failed to create / pipeline creation failed** — the pipeline could not be built (missing plugin, bad video output configuration, or display not available).
- **Video output to … failed** — a video started but the display refused to show it; the text in brackets is the reason, and the media may stop with it. On a Raspberry Pi 0–3, "No space left on device" means the display hardware can't keep up with this video on this display, often because the video has to be scaled down to a smaller screen or shares the port with a Virtual Matrix output. Encode the video at the display's resolution (**Video → DRM Connectors** below shows the mode), or turn a Virtual Matrix on that port off while videos play.

1. Confirm a display/HDMI is connected and enabled, and that the correct **Video Output** device is selected on the [Audio/Video](../../settings.php#settings-av) settings page.
2. If you do not use video output, you can ignore this — or set the video output to a Pixel Overlay Model / disabled so the HDMI pipeline is not attempted.
3. For more detail check `fppd.log`; or open **Help → [Troubleshooting Commands](../../troubleshooting.php)** and run **Video → DRM Connectors** / **kmsprint** to see whether the HDMI/display is detected, **Display EDID** to see what the display reports about itself, **Display Engine Load** (Pi 0–3) for the display load limit, and **Media Backend → GStreamer …** to check the GStreamer plugins.
