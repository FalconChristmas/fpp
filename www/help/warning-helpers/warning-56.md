# Video Input Source Failed
A configured video input source could not start, so its video will not be available. The message names the source and the problem:

- **No device path / no URI** — the source is missing its capture device (e.g. `/dev/video0`) or its stream URL.
- **Unknown source type** — the source type is not one FPP supports.
- **Failed to resolve video URL** — the remote stream URL could not be resolved.
- **Failed to start** — the capture pipeline could not be started (device busy, unreachable stream, or unsupported format).
- **GStreamer not available** — this device does not have GStreamer, which video input requires.

1. Open the [Video Inputs](../../pipewire-video-inputs.php) configuration and review the affected source (type, device path, and URI/URL).
2. For a camera/capture device, confirm it is connected and not in use by another program.
3. For a network/stream source, confirm the URL is correct and reachable from this device.
4. Check `fppd.log` for the exact source error.
