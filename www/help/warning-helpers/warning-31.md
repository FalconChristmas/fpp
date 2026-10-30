# Video Output Pipeline Failed
FPP could not build or start the GStreamer pipeline used for video output, so video will not display (audio and pixel output are unaffected).

Common causes named in the message:

- **kmssink element missing** — the GStreamer plugin needed for HDMI video is not installed.
- **Could not open DRM device** — FPP could not access the HDMI/display hardware.
- **Failed to create / pipeline creation failed** — the pipeline could not be built (missing plugin, bad video output configuration, or display not available).

1. Confirm a display/HDMI is connected and enabled, and that the correct **Video Output** device is selected on the [Audio/Video](../../settings.php#settings-av) settings page.
2. If you do not use video output, you can ignore this — or set the video output to a Pixel Overlay Model / disabled so the HDMI pipeline is not attempted.
3. For more detail check `fppd.log`; or open **Help → [Troubleshooting Commands](../../troubleshooting.php)** and run **Video → DRM Connectors** / **kmsprint** to see whether the HDMI/display is detected, and **Media Backend → GStreamer …** to check the GStreamer plugins.
