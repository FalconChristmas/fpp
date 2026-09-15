<h3>Audio / Video</h3>
<p>Choose how FPP handles sound and video. The <b>Media Backend</b> toggle at the top selects Simple (recommended) or Advanced Full PipeWire.</p>

<h4>A/V Mode</h4>
<ul>
    <li><b>Media Backend</b> (dropdown) — <b>Simple</b> = one audio card + one display, FPP configures it automatically. <b>Advanced (Full PipeWire)</b> = unlocks Output Groups, Input Mixing, Routing Matrix, Video Groups, AES67 and Opus RTP. Requires reboot when changed. All PipeWire options below appear only in Advanced; Simple uses the <i>Simple Audio / Simple Video</i> sections instead.</li>
</ul>

<h4>General Audio</h4>
<ul>
    <li><b>WLED Sound Reactive Source</b> (dropdown) — Source for WLED Sound-Reactive effects. Can be the currently playing media or a live audio input.</li>
    <li><b>WLED Audio Sync</b> (dropdown) — How to sync WLED audio data over the network via UDP 11988. <b>Off</b> = no sync. <b>Send</b> = broadcast FPP’s FFT/volume to external WLED nodes. <b>Receive</b> = use a remote microphone instead of local audio.</li>
    <li><b>WLED Audio Sync Address</b> (text) — Multicast/unicast/broadcast address for WLED sync. Default <code>239.0.0.1</code> (WLED multicast group). Use a specific IP or broadcast on networks that filter multicast.</li>
    <li><b>WLED Audio Sync Port</b> (number) — UDP port for WLED sync. Default 11988. Change only if you changed it on your other WLED devices. <i>Advanced level.</i></li>
    <li><b>Disable IP announcement</b> (checkbox) — When unchecked, FPP speaks the current IP addresses over the audio output at boot. Check to silence this for shows that use a transmitter or speakers.</li>
    <li><b>Disable Volume Slider</b> (checkbox) — Hides the volume slider on the Status page to prevent accidental changes.</li>
    <li><b>Global Audio/Sequence Offset</b> (number, ms, -9999 to 9999) — Shifts audio vs. sequence on every device via MultiSync. Positive moves audio ahead. Affects all files; for per-file fixes, edit the audio/sequence instead. <i>Requires FPPD restart.</i></li>
    <li><b>Configure Sound Card Aliases</b> (button / modal) — Give cards friendly names like “Transmitter” or “Amplifier” shown in all audio dropdowns. Works for both ALSA and PipeWire.</li>
</ul>

<h4>Simple Audio (shown when Media Backend = Simple)</h4>
<ul>
    <li><b>Audio Output Device</b> (dropdown) — Which sound card plays audio. Stored by the card’s stable ALSA ID so adding/removing a USB device does not lose the selection. If empty, FPP picks the first available card. <i>Requires reboot to apply.</i></li>
    <li><b>Audio Sample Rate</b> (dropdown) — Rate the PipeWire graph runs at. <b>Default</b> = 44100 unless the card clocks elsewhere. Options: Default, 44100, 48000, 96000. In Advanced, per-card rates are set in Audio Output Groups.</li>
    <li><b>Audio Period Size</b> (dropdown) — Buffer size handed to the card (graph quantum). Smaller = lower latency; larger = more stable on a busy player. Options: 1024,1536,2048,2560,3072,3584,4096,5120,6144,7168,8192. In Advanced, per-card in Output Groups.</li>
    <li><b>Force Audio Card ID</b> (text, Advanced+) — Override the probed ALSA card ID if the system reports a wrong one. Leave blank unless you see “Could not open audio device: Invalid argument”.</li>
</ul>

<h4>General PipeWire (Advanced only)</h4>
<ul>
    <li><b>Suspend Audio Device When Idle</b> (checkbox) — Lets PipeWire suspend the card when nothing plays. Saves ~4–5% CPU on single-core boards when idle. Turn off only if a card fails to wake correctly.</li>
    <li><b>Configure Routing Matrix</b> (button) — Grid of input-group → output-group connections with per-path volume, mute, effects and presets.</li>
    <li><b>Visualise Current Pipeline</b> (link) — Opens <code>pipewire-graph.php</code>, a live graph of sources, sinks and processing nodes for troubleshooting.</li>
</ul>

<h4>PipeWire Audio (Advanced only)</h4>
<ul>
    <li><b>Configure Input Mixing (Mix Buses)</b> (button) — Mix multiple sources (FPP streams, ALSA line-in, AES67 receives) into input groups before routing.</li>
    <li><b>Configure Output Audio Groups</b> (button) — Combine multiple sound cards into virtual sinks with per-card volume/EQ/channel mapping.</li>
</ul>

<h4>PipeWire Audio Network Streams (Advanced only)</h4>
<ul>
    <li><b>Configure AES67 Instances</b> (button) — Pro-audio over IP; each instance becomes a virtual sound card on the network.</li>
    <li><b>Configure Opus RTP Instances</b> (button) — Compressed audio over WiFi/wired using Opus codec — good for wireless or where AES67 is costly.</li>
</ul>

<h4>PipeWire Video (Advanced only)</h4>
<ul>
    <li><b>Configure Video Input Sources</b> (button) — Persistent sources (test patterns, cameras) visible in the graph.</li>
    <li><b>Configure Video Output Groups</b> (button) — Each group fans one video signal to many destinations (HDMI, overlays, network).</li>
    <li><b>Configure RTSP Outputs</b> (button) — Serves video+audio as an RTSP URL on every interface, no SDP copy needed.</li>
</ul>

<h4>Simple Video (shown when Media Backend = Simple)</h4>
<ul>
    <li><b>Default Video Output Device</b> (dropdown) — Where playlist videos play: a Pixel Overlay model on any platform, or HDMI/composite on Raspberry Pi.</li>
    <li><b>Force HDMI Display</b> (checkbox, Raspberry Pi) — Force HDMI as default display. Needed if HDMI is not detected at boot or was dark when the Pi started. When on, composite output is unavailable until disabled. <i>Reboot required.</i></li>
    <li><b>Enable HDMI Display</b> (checkbox, BeagleBone Black variants) — Enables the BeagleBone HDMI port. <b>Warning:</b> when enabled, many GPIO pins used by capes are disabled.</li>
    <li><b>Force HDMI Resolution</b> (dropdown) — Lock HDMI to a specific resolution from <code>hdmi_table.json.php</code>. Default = monitor-reported. Shows only when the related HDMI toggle above is active. <i>Reboot required.</i></li>
    <li><b>Force Port 2 HDMI Resolution</b> (dropdown, Pi 4 / Pi 5) — Same for the Pi 4/5’s second HDMI port.</li>
    <li><b>Hardware Decoding</b> (checkbox) — Use the hardware decoder for video. On is faster; off uses more CPU but can give finer control.</li>
    <li><b>Local Media/Sequence Offset</b> (number, ms) — Shifts media vs. sequence on this device only. Positive moves media ahead. For per-file fixes, edit the file instead. <i>Requires restart.</i></li>
    <li><b>Ignore Media Sync Packets</b> (checkbox, Remote only) — Remote starts/stops video with the Player but does not chase sync during playback. Smoother but may drift.</li>
</ul>
