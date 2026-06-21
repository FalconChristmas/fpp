# Network Audio Stream Failed
An AES67 or Opus RTP network audio stream could not start, so audio will not be sent to (or received from) the network for that stream.

1. Open the [AES67](../../aes67-config.php) or [Opus RTP](../../opus-rtp-config.php) configuration page and review the stream's settings (interface, address/port, sample rate, channels).
2. Confirm the audio backend is running and the configured source/sink exists — network audio is built on PipeWire/GStreamer. The [PipeWire Graph](../../pipewire-graph.php) page shows the current audio nodes and links.
3. Make sure the selected network interface is up and on the correct network.
4. For AES67, if the **AES67 PTP Clock Sync Failed** warning is also present, fix that first — a stream can fail to start when PTP clock sync is not available.
5. Check `fppd.log` for the underlying GStreamer error.
