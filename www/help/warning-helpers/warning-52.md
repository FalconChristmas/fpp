# WLED Sound Reactive Input Failed
FPP could not start capturing audio for WLED sound-reactive effects, so those effects will not react to sound. The message names the device and the underlying error.

- **Could not open / start capture for `<device>`** — the selected audio input device could not be opened (it may be missing, in use, or the name may be wrong).
- **GStreamer pipeline error / could not find appsink** — the audio capture pipeline failed to build.
- **Audio capture requires GStreamer** — this build/device does not have GStreamer available for audio capture.

1. Open the [Audio/Video settings](../../settings.php#settings-av) and check the **WLED Sound Reactive Source** — make sure it points at an audio input device that exists on this system. This control is only shown when **User Interface Level** is **Advanced** or higher.
2. Confirm an audio input (microphone/line-in/capture device) is actually connected and not in use by another program.
3. Check `fppd.log` for the exact capture error.
