# FPP Audio Architecture — GStreamer, PipeWire, WirePlumber & AES67

**Version:** 2026-02-18  
**Branch:** `gstreamer-media-experiment`  
**Platform:** Raspberry Pi 5 / Bookworm (armhf), PipeWire 1.4.2, WirePlumber 0.5.8, GStreamer 1.26.2

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Layer Stack](#layer-stack)
3. [Service Architecture](#service-architecture)
4. [PipeWire Graph Topology](#pipewire-graph-topology)
5. [Signal Flow Examples](#signal-flow-examples)
6. [GStreamer Pipelines](#gstreamer-pipelines)
7. [WirePlumber Linking & Lua Hooks](#wireplumber-linking--lua-hooks)
8. [Volume Control](#volume-control)
9. [MultiSync Rate Adjustment](#multisync-rate-adjustment)
10. [AES67 Audio-over-IP](#aes67-audio-over-ip)
11. [Configuration Files](#configuration-files)
12. [Debugging & Diagnostics](#debugging--diagnostics)

---

## System Overview

FPP's audio system routes decoded audio from GStreamer through PipeWire's graph to multiple simultaneous outputs — USB sound cards, AES67 network streams, and HDMI — with per-output delay compensation and equalization.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              fppd process                              │
│                                                                        │
│  ┌──────────────────────┐    ┌──────────────────────────────────────┐  │
│  │   GStreamer Pipeline  │    │        AES67Manager                  │  │
│  │                       │    │                                      │  │
│  │  filesrc → decodebin  │    │  ┌─────────────────────────────┐    │  │
│  │    → audioconvert     │    │  │ GStreamer Send Pipeline      │    │  │
│  │    → audioresample    │    │  │ pipewiresrc → audioconvert   │    │  │
│  │    → tee ─┬─ queue    │    │  │   → rtpL24pay → udpsink     │    │  │
│  │           │  → volume │    │  │   (multicast 239.69.0.x)    │    │  │
│  │           │  → pwsink─┼────┼──┤                              │    │  │
│  │           │           │    │  └─────────────────────────────┘    │  │
│  │           └─ queue    │    │  SAP announcer thread               │  │
│  │              → appsink│    │  PTP clock (gst_ptp_init)           │  │
│  │              (WLED)   │    └──────────────────────────────────────┘  │
│  └──────────────────────┘                                              │
└──────────┬─────────────────────────────────────────────────────────────┘
           │ PipeWire protocol (unix socket)
           ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                    PipeWire (fpp-pipewire.service)                       │
│                                                                          │
│  Graph nodes:                                                            │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                 │
│  │ combine-sink │   │ filter-chain │   │ ALSA sink    │                 │
│  │ (Audio Group)│──►│ (Delay + EQ) │──►│ (Sound Card) │                 │
│  └──────┬───────┘   └──────────────┘   └──────────────┘                 │
│         │           ┌──────────────┐   ┌──────────────┐                 │
│         ├──────────►│ filter-chain │──►│ GStr pwsrc   │ (AES67 send)   │
│         │           │ (Delay + EQ) │   │ → RTP → UDP  │                 │
│         │           └──────────────┘   └──────────────┘                 │
│         │           ┌──────────────┐   ┌──────────────┐                 │
│         └──────────►│ filter-chain │──►│ ALSA sink    │                 │
│                     │ (Delay + EQ) │   │ (Sound Card) │                 │
│                     └──────────────┘   └──────────────┘                 │
└──────────────────────────────────────────────────────────────────────────┘
           │
           │ Managed by:
           ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                  WirePlumber (fpp-wireplumber.service)                    │
│                                                                          │
│  - Creates ALSA sink/source nodes from hardware                          │
│  - Links nodes together (select-target event chain)                      │
│  - Lua hooks: fpp-block-combine-fallback.lua                             │
│  - Runs linking scripts: find-defined-target → find-filter-target         │
│                         → [FPP hook blocks fallback] → find-default-target│
└──────────────────────────────────────────────────────────────────────────┘
           │
           ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                           ALSA / Kernel                                  │
│  USB sound cards, HDMI audio, onboard audio                              │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Layer Stack

From bottom to top:

| Layer               | Component                                     | Role                                                                                                                                |
| ------------------- | --------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| **Kernel**          | ALSA drivers                                  | USB audio class, HDMI audio, I2S HATs                                                                                               |
| **Audio Server**    | PipeWire 1.4.2 (`fpp-pipewire.service`)       | Graph-based audio routing, mixing, resampling. Runs as root system-wide (no user session). Socket at `/run/pipewire-fpp/pipewire-0` |
| **Session Manager** | WirePlumber 0.5.8 (`fpp-wireplumber.service`) | Creates ALSA nodes from hardware, manages linking policy, handles node appearance/disappearance. Extends behavior via Lua scripts.  |
| **Media Framework** | GStreamer 1.26.2 (in fppd)                    | Decodes audio/video files, connects to PipeWire via `pipewiresink`/`pipewiresrc`. Also handles AES67 RTP encoding/decoding.         |
| **Application**     | fppd                                          | Playlist/sequence engine, HTTP API, volume control, AES67 management                                                                |
| **UI**              | PHP web interface                             | Audio group configuration, EQ, delay, AES67 instance management                                                                     |

### Why Each Layer Exists

- **PipeWire** replaces PulseAudio/JACK — provides the audio graph that splits one input to multiple outputs with mixing, format conversion, and sample-rate conversion. Its combine-stream and filter-chain modules enable audio groups with per-output processing.

- **WirePlumber** replaces PipeWire's built-in session manager — handles the policy of *which* nodes get linked to *which* targets. Without it, nodes would exist but have no connections. WirePlumber's Lua scripting allows FPP to customize linking behavior.

- **GStreamer** replaces VLC and SDL — provides a single media decode pipeline that connects into PipeWire as a graph node. Also provides RTP payloading/depayloading for AES67 that the PipeWire RTP modules couldn't do correctly (no PTP media clock, non-compliant SAP/SDP).

---

## Service Architecture

### Services (systemd)

```
fpp-pipewire.service          PipeWire audio server (system-wide, root)
  └── fpp-wireplumber.service   WirePlumber session manager (After=fpp-pipewire)
        └── fppd.service          FPP daemon (After=fpp-wireplumber)
```

### Startup Order

```
1. fpp-pipewire.service starts
   - Loads: /etc/pipewire/pipewire.conf.d/95-fpp-alsa-sink.conf (ALSA sinks)
   - Loads: /etc/pipewire/pipewire.conf.d/96-fpp-audio-groups.conf (filter-chains + combine-stream)
   - Socket: /run/pipewire-fpp/pipewire-0

2. fpp-wireplumber.service starts (3s after PipeWire)
   - Loads: /etc/wireplumber/wireplumber.conf.d/*.conf
   - Loads: /usr/share/wireplumber/scripts/linking/fpp-block-combine-fallback.lua
   - Scans ALSA hardware, creates sink/source nodes (e.g., alsa_output.usb-...analog-stereo)
   - Links combine-stream outputs → filter-chains → ALSA sinks

3. fppd starts (3s after WirePlumber)
   - Calls gst_init() with PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp
   - Initializes AES67Manager (creates send/receive GStreamer pipelines)
   - Calls setVolume() which sets PipeWire sink volume via wpctl
```

### Restart Procedure

```bash
# Full restart (order matters):
systemctl stop fppd
systemctl restart fpp-pipewire.service
sleep 3
systemctl restart fpp-wireplumber.service
sleep 3
systemctl start fppd
sleep 8   # Allow pipelines to stabilize
```

### Environment Variables

All PipeWire/WirePlumber commands need the correct socket path:

```bash
# For wpctl, pw-cli, pw-top, pw-dump, pw-link:
export PIPEWIRE_REMOTE=/run/pipewire-fpp/pipewire-0

# For GStreamer (pipewiresink/pipewiresrc):
export PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp
export XDG_RUNTIME_DIR=/run/pipewire-fpp
```

---

## PipeWire Graph Topology

### Node Types in the Graph

When an audio group with 3 members is configured, PipeWire creates these nodes:

| Node Type                  | Example Name                                                                              | media.class         | Purpose                                                                              |
| -------------------------- | ----------------------------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------ |
| **ALSA Sink**              | `alsa_output.usb-Creative_Technology_Ltd_Sound_Blaster_Play__3_00307683-00.analog-stereo` | Audio/Sink          | Physical output to USB sound card                                                    |
| **Filter-Chain Sink**      | `fpp_fx_g1_s3`                                                                            | Audio/Sink          | Virtual sink — input side of delay+EQ filter-chain for Sound Blaster                 |
| **Filter-Chain Output**    | `fpp_fx_g1_s3_out`                                                                        | Stream/Output/Audio | Output side of filter-chain — streams processed audio to ALSA sink                   |
| **Combine-Stream Sink**    | `fpp_group_stuart_headphones_and_hdmi`                                                    | Audio/Sink          | Virtual sink that splits audio to all group members. This is the default audio sink. |
| **Combine-Stream Output**  | `output.fpp_group_stuart_headphones_and_hdmi_fpp_fx_g1_s3`                                | Stream/Output/Audio | One output leg per member — streams to that member's filter-chain                    |
| **GStreamer pipewiresink** | `fppd`                                                                                    | Stream/Output/Audio | GStreamer playback node — connects to combine-stream sink                            |
| **GStreamer pipewiresrc**  | `aes67_aes67_stream_1_send`                                                               | Stream/Input/Audio  | AES67 send pipeline — captures from filter-chain output                              |
| **Plugin Audio Source**    | `fpp_smpte_ltc`                                                                           | Audio/Source        | Virtual source published by a plugin, routable as an Input Mixing member (see [Plugin_Development.md](Plugin_Development.md#publishing-a-pipewire-audio-source-from-a-plugin)) |

### Live Topology Example

With a group "Stuart Headphones and HDMI" containing Sound Blaster (2ch), AES67 Stream 1 (2ch), and ICUSBAUDIO7D (8ch):

```
                     fppd (GStreamer pipewiresink)
                         │
                         ▼
              ┌──────────────────────┐
              │  fpp_group_stuart_   │  ← Combine-Stream Sink (default sink)
              │  headphones_and_hdmi │     Volume controlled via wpctl
              └───┬────────┬────────┬┘
                  │        │        │
        output_s3│  output_│  output_│
                  │  aes67_1│  icusb  │
                  ▼        ▼        ▼
          ┌──────────┐ ┌──────────┐ ┌──────────────┐
          │fpp_fx_g1_│ │fpp_fx_g1_│ │fpp_fx_g1_    │
          │s3 (Delay)│ │aes67_1   │ │icusbaudio7d  │  ← Filter-Chain Sinks
          │          │ │(Delay)   │ │(Delay)       │     (delay + EQ processing)
          └────┬─────┘ └────┬─────┘ └──────┬───────┘
               │            │              │
          fpp_fx_g1_   fpp_fx_g1_    fpp_fx_g1_
          s3_out       aes67_1_out   icusbaudio7d_out  ← Filter-Chain Outputs
               │            │              │
               ▼            ▼              ▼
     ┌─────────────┐ ┌────────────┐ ┌────────────────┐
     │ Sound       │ │ aes67_     │ │ ICUSBAUDIO7D   │
     │ Blaster     │ │ stream_1_  │ │ (ALSA sink,    │  ← Final Destinations
     │ (ALSA sink, │ │ send       │ │  8ch)          │
     │  2ch stereo)│ │ (GStr      │ └────────────────┘
     └─────────────┘ │  pwsrc)    │
                     └─────┬──────┘
                           │ GStreamer pipeline
                           ▼
                     ┌────────────┐
                     │ rtpL24pay  │
                     │ → udpsink  │
                     │ 239.69.0.1 │  ← AES67 multicast RTP
                     │ :5004      │
                     └────────────┘
```

### Link Count

A healthy topology with 3 members has exactly **18 PipeWire links**:
- 3 members × 2 links (combine-output → filter-chain sink) = 6 (stereo) or more for multi-channel
- 3 members × 2 links (filter-chain output → ALSA/AES67 sink) = 6
- 6+ links for the 8-channel ICUSBAUDIO7D (8 channels × 1 link each for combine→filter)

Verify: `PIPEWIRE_REMOTE=/run/pipewire-fpp/pipewire-0 pw-link -l 2>/dev/null | grep '|' | wc -l`

---

## Signal Flow Examples

### Example 1: Playing an MP3 Through Sound Blaster

```
Step 1: fppd starts playing audio
  GStreamer pipeline:
    filesrc location="/home/fpp/media/music/song.mp3"
      ! decodebin            (→ MP3 decoder selected automatically)
      ! audioconvert         (→ format conversion)
      ! audioresample        (→ 48kHz resampling)
      ! audio/x-raw,rate=48000
      ! tee name=t
        t. ! queue ! volume name=vol ! pipewiresink target-object=fpp_group_...
        t. ! queue ! audioconvert ! audio/x-raw,format=F32LE,channels=1 ! appsink (WLED)

Step 2: pipewiresink node "fppd" appears in PipeWire graph
  WirePlumber links it to combine-stream sink (fpp_group_...)

Step 3: Combine-stream splits audio to 3 outputs
  output.fpp_group_..._fpp_fx_g1_s3          → fpp_fx_g1_s3
  output.fpp_group_..._fpp_fx_g1_aes67_1     → fpp_fx_g1_aes67_1
  output.fpp_group_..._fpp_fx_g1_icusbaudio7d → fpp_fx_g1_icusbaudio7d

Step 4: Each filter-chain applies delay + EQ
  Filter-chain internal graph (per channel):
    input → delay_l (builtin delay, configurable seconds) → eq_l_0 (bq_peaking) → output
    input → delay_r (builtin delay, configurable seconds) → eq_r_0 (bq_peaking) → output

Step 5: Filter-chain outputs link to final sinks
  fpp_fx_g1_s3_out → alsa_output.usb-Creative_...analog-stereo  (Sound Blaster DAC)
  fpp_fx_g1_aes67_1_out → aes67_aes67_stream_1_send             (GStreamer pipewiresrc)
  fpp_fx_g1_icusbaudio7d_out → alsa_output.usb-0d8c_...stereo   (ICUSBAUDIO7D DAC)

Step 6: For AES67, GStreamer encodes and sends as RTP
  pipewiresrc (captures from filter-chain output)
    → audioconvert → audio/x-raw,format=S24BE,rate=48000,channels=2
    → rtpL24pay pt=96 min-ptime=4000000 max-ptime=4000000
    → udpsink host=239.69.0.1 port=5004 ttl-mc=4 sync=false
```

### Example 2: Volume Change (User Adjusts Slider to 43%)

```
Step 1: HTTP API receives volume change
  PUT http://localhost:32322/fppd/volume?set=43

Step 2: setVolume(43) in mediaoutput.cpp
  Detects MediaBackend = "pipewire"

Step 3: wpctl sets volume on PipeWire default sink
  Command: PIPEWIRE_REMOTE=/run/pipewire-fpp/pipewire-0 wpctl set-volume @DEFAULT_AUDIO_SINK@ 0.4300
  Target: node 48 (fpp_group_stuart_headphones_and_hdmi)

Step 4: PipeWire applies volume scaling to combine-stream sink
  All audio passing through the combine-stream is attenuated
  → All downstream outputs (SB, AES67, ICUSBAUDIO7D) hear the change

Note: GStreamer's volume element is NOT used for user volume (prevents double attenuation).
      It IS used for per-track volumeAdjust (dB offset for individual media files).
```

### Example 3: Track Change (Song Ends, Next Starts)

```
Step 1: GStreamer EOS received
  GStreamerOutput::Close() called

Step 2: Flush AES67 send pipelines
  AES67Manager::FlushSendPipelines()
    → Sets dropCounter = 50 on each send pipeline
    → Pad probe on pipewiresrc src pad drops next 50 buffers (~200ms)
    → Prevents stale audio garbage from old track appearing in RTP stream

Step 3: Flush PipeWire delay buffers
  FlushPipeWireDelayBuffers() — detached thread
    → Sets each filter-chain delay to 0.0, waits, restores original value
    → Clears stale audio sitting in delay buffers

Step 4: GStreamer pipeline torn down
  Pipeline set to GST_STATE_NULL, elements cleaned up

Step 5: Next track starts
  New GStreamer pipeline created, pipewiresink reconnects to combine-stream
  AES67 pad probe counter reaches 0, RTP packets flow with fresh audio
```

### Example 4: Audio Group Apply (UI Save & Apply)

```
Step 1: PHP API receives apply request
  POST /api/pipewire/audio/groups/apply

Step 2: Generate PipeWire config
  GeneratePipeWireGroupsConfig() in pipewire.php
    → Queries pw-dump for existing ALSA sinks
    → Resolves card IDs to PipeWire node names
    → Generates filter-chain modules (delay + EQ per member)
    → Generates combine-stream module with match rules
    → Writes to /etc/pipewire/pipewire.conf.d/96-fpp-audio-groups.conf

Step 3: Install WirePlumber Lua hook
  InstallWirePlumberFppLinkingHook()
    → Writes fpp-block-combine-fallback.lua to /usr/share/wireplumber/scripts/linking/
    → Writes 60-fpp-block-combine-fallback.conf to /etc/wireplumber/wireplumber.conf.d/

Step 4: Restart services
  systemctl restart fpp-pipewire.service   (picks up new config)
  sleep 500ms
  systemctl restart fpp-wireplumber.service (re-links with new topology)
  sleep 3s

Step 5: WirePlumber linking sequence
  For each node that appears:
    a) find-defined-target.lua — checks node.target property
    b) find-filter-target.lua — checks if node belongs to a filter-chain
    c) [FPP hook] fpp-block-combine-fallback.lua — blocks fallback for FPP nodes
    d) find-default-target.lua — falls back to default sink (blocked for FPP nodes)
    e) find-best-target.lua — last resort

Step 6: Apply AES67 instances
  Signals fppd via command API to rebuild AES67 GStreamer pipelines
```

---

## GStreamer Pipelines

### Audio-Only Playback Pipeline

```
filesrc location="song.mp3"
  ! decodebin
  ! audioconvert
  ! audioresample
  ! audio/x-raw,rate=48000
  ! tee name=t
    t. ! queue ! volume name=vol ! pipewiresink target-object=<group_sink_name>
    t. ! queue max-size-buffers=3 leaky=downstream
       ! audioconvert ! audio/x-raw,format=F32LE,channels=1
       ! appsink name=sampletap emit-signals=true sync=false max-buffers=3 drop=true
```

- `pipewiresink` connects to the combine-stream sink
- `appsink` provides raw audio samples for WLED audio-reactive effects
- `volume` element used only for per-track dB adjustment, not user volume

### Video Overlay Pipeline (LED Matrix)

```
filesrc location="video.mp4"
  ! decodebin name=decoder
    decoder.audio → audioconvert ! audioresample ! tee (same as above)
    decoder.video → queue ! videoconvert ! videoscale
                    ! video/x-raw,format=RGB,width=W,height=H
                    ! appsink name=videosink emit-signals=true sync=true
```

- Video `appsink` delivers RGB frames to `PixelOverlayModel::setData()`
- `sync=true` on video appsink ensures frames are paced to media clock
- Stride padding handled: GStreamer pads RGB rows to 4-byte alignment

### HDMI Video Pipeline

```
filesrc location="video.mp4"
  ! decodebin name=decoder
    decoder.audio → (same audio chain as above)
    decoder.video → queue ! videoconvert ! videoscale
                    ! video/x-raw,width=W,height=H
                    ! kmssink driver-name=vc4 connector-id=N skip-vsync=TRUE restore-crtc=TRUE
```

- `kmssink` outputs directly to DRM/KMS (no X/Wayland needed)
- `ResolveDrmConnector()` scans `/sys/class/drm/` for active HDMI connector

### AES67 Send Pipeline

```
pipewiresrc min-buffers=2
  stream-properties="props,node.autoconnect=false,media.class=Stream/Input/Audio,node.name=aes67_<name>_send"
  ! audioconvert
  ! audio/x-raw,format=S24BE,rate=48000,channels=2
  ! rtpL24pay pt=96 min-ptime=4000000 max-ptime=4000000
  ! udpsink host=239.69.0.x port=5004 multicast-iface=eth0 auto-multicast=true ttl-mc=4 sync=false
```

- `pipewiresrc` with `node.autoconnect=false` — WirePlumber links it to the filter-chain output
- S24BE: 24-bit big-endian signed integer (AES67 L24 format)
- `min-ptime=4000000` / `max-ptime=4000000` = 4ms packet time (192 samples at 48kHz)
- `sync=false` on udpsink: don't pace packets to clock, send as fast as they arrive
- `ttl-mc=4` for local network multicast

---

## WirePlumber Linking & Lua Hooks

### The Linking Problem

WirePlumber uses an event-driven linking pipeline to decide which nodes connect to which targets. The default behavior causes problems with FPP's combine-stream + filter-chain topology:

```
Problem scenario (without FPP Lua hook):

1. PipeWire creates combine-stream outputs and filter-chain nodes
2. Nodes appear at slightly different times during startup
3. Filter-chain output fpp_fx_g1_s3_out has node.target = "alsa_output.usb-...-SB"
   BUT the SB ALSA sink hasn't appeared yet
4. WirePlumber runs find-defined-target → target not found
5. WirePlumber runs find-default-target → links to default sink (combine-stream!)
6. This creates a loop: filter-chain output → combine-stream → filter-chain
7. canLink() in linking-utils.lua detects the link-group loop
8. Now combine-stream output for SB can't link to SB's filter-chain (loop detected)
9. Combine-stream output falls back to default → links directly to SB ALSA sink
10. Result: DOUBLED AUDIO on Sound Blaster (one copy through filter-chain, one direct)
```

### The FPP Lua Hook Solution

File: `/usr/share/wireplumber/scripts/linking/fpp-block-combine-fallback.lua`

```lua
-- Intercepts select-target events for FPP nodes
-- Runs AFTER find-defined-target and find-filter-target
-- Runs BEFORE find-default-target and find-best-target

SimpleEventHook {
  name = "linking/fpp-block-combine-fallback",
  after = { "linking/find-defined-target", "linking/find-filter-target" },
  before = { "linking/find-default-target", "linking/find-best-target" },
  execute = function(event)
    local si = event:get_subject()
    local si_props = si.properties
    local node_name = si_props["node.name"] or ""

    -- Only intercept FPP audio group and filter-chain output nodes
    local is_combine_output = node_name:match("^output%.fpp_group_")
    local is_fx_output = node_name:match("^fpp_fx_g%d+_.*_out$")

    if not is_combine_output and not is_fx_output then
      return    -- Not an FPP node, let normal linking proceed
    end

    -- Check if a target was already found by earlier hooks
    local has_target = si_props["node.target"] ~= nil or
                       si_props["target.object"] ~= nil
    local target = event:get_data("target")

    if target then
      return    -- Target found, let linking proceed normally
    end

    -- No target found: BLOCK the default fallback
    -- Don't set was_handled — WirePlumber will rescan when target appears
    event:stop_processing()
  end
}
```

**Key design decisions:**
- Does NOT set `was_handled = true` — if it did, WirePlumber would stop monitoring for the real target
- `event:stop_processing()` prevents `find-default-target` from running, which would create a rogue link
- When the real target node appears later, WirePlumber rescans and `find-defined-target` succeeds

### WirePlumber Linking Script Chain

```
Event: select-target fires for a node needing a link

1. find-defined-target.lua (priority: interest after=prepare-link)
   Checks: node.target, target.object properties
   If found: sets event data "target", returns

2. find-filter-target.lua
   Checks: if node is part of a filter-chain, links to its companion node
   If found: sets event data "target", returns

3. fpp-block-combine-fallback.lua  ← FPP CUSTOM HOOK
   Checks: if node matches FPP patterns AND no target was found
   Action: event:stop_processing() — prevents fallback

4. find-default-target.lua (BLOCKED by step 3 for FPP nodes)
   Would link to @DEFAULT_AUDIO_SINK@ — causes rogue links

5. find-best-target.lua (BLOCKED by step 3)
   Would try any compatible sink — also causes rogue links
```

### Registration Config

File: `/etc/wireplumber/wireplumber.conf.d/60-fpp-block-combine-fallback.conf`

```
wireplumber.components = [
  {
    name = linking/fpp-block-combine-fallback, type = script/lua
    provides = linking.fpp-block-combine-fallback
  }
]
wireplumber.profiles = {
  main = {
    linking.fpp-block-combine-fallback = required
  }
}
```

---

## Volume Control

### Architecture

```
User adjusts volume slider (0-100)
  │
  ▼
HTTP API: /fppd/volume?set=43
  │
  ▼
setVolume(43) in mediaoutput.cpp
  │
  ├── MediaBackend = "pipewire" ?
  │     YES: wpctl set-volume @DEFAULT_AUDIO_SINK@ 0.43
  │           → Sets volume on combine-stream sink node
  │           → All downstream outputs affected equally
  │           → Mute/unmute via wpctl set-mute
  │
  │     NO (ALSA fallback):
  │           amixer set -c N 'Speaker' -- 43%
  │
  └── Per-track dB adjustment (volumeAdjust):
        GStreamer volume element in pipeline
        g_object_set(m_volume, "volume", pow(10.0, volAdj/2000.0))
```

### Why wpctl Instead of pactl

| Feature      | pactl                                      | wpctl                               |
| ------------ | ------------------------------------------ | ----------------------------------- |
| Requires     | pipewire-pulse module                      | Only WirePlumber                    |
| Protocol     | PulseAudio D-Bus compat                    | Direct PipeWire protocol            |
| Env var      | `PIPEWIRE_RUNTIME_DIR` + `XDG_RUNTIME_DIR` | `PIPEWIRE_REMOTE`                   |
| Volume scale | Percentage (0-100%)                        | Linear (0.0-1.0) with cubic mapping |

FPP uses `PIPEWIRE_REMOTE=/run/pipewire-fpp/pipewire-0 wpctl set-volume @DEFAULT_AUDIO_SINK@ <float>`.

### Double Attenuation Prevention

When PipeWire backend is active, `mediaOutput->SetVolume(vol)` is **skipped** to prevent:
- PipeWire sink at 43% AND GStreamer volume element at 43% = effective 18.5% (0.43 × 0.43)

The GStreamer `volume` element is only used for per-track `volumeAdjust` (dB offset configured per media file).

---

## MultiSync Rate Adjustment

### Purpose

When an FPP unit runs as a **remote** (synced to a master), its local playback position drifts relative to the master over time. The `AdjustSpeed()` method adjusts the GStreamer playback rate so the remote converges on the master's position.

### Algorithm (ported from VLC)

1. **Diff calculation:** `rawdiff = local_ms - master_ms`
2. **Sign-flip detection:** If diff crosses zero and rate ≠ 1.0, reset to normal speed (prevents oscillation)
3. **Dead zone:** |diff| < 30ms → close enough, return to rate 1.0
4. **Jump threshold:** |diff| > 10s → seek to master position (handles fppd restarts)
5. **Transient filter:** 30-100ms diff with no prior diff → wait one cycle before adjusting
6. **Proportional rate steps:** `rateDiff = diff / 100` (or `/50` in first 10s for faster lock-on), then:
   - Speed up: `rate *= 1.02` per step (when local is behind master)
   - Slow down: `rate *= 0.98` per step (when local is ahead of master)
7. **Rate averaging:** Running average of last 20 applied rates for smoothing
8. **Cross-unity check:** If rate crosses 1.0, reset to 1.0 before further adjustment
9. **Clamping:** Rate clamped to [0.5, 2.0]

### GStreamer Implementation

Rate changes use `GST_SEEK_FLAG_INSTANT_RATE_CHANGE` (GStreamer ≥ 1.18) for glitch-free adjustment without flushing the pipeline. Falls back to a flush seek at the current position if instant-rate-change is unsupported by the pipeline elements.

Position jumps (>10s drift) use `gst_element_seek()` with `GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE`.

### Configuration

- **`remoteIgnoreSync`** setting (int): When set to 1, disables speed adjustment (`m_allowSpeedAdjust = false`). Default is 0 (adjustment enabled).

### Key Members (`GStreamerOut.h`)

| Member               | Type                         | Purpose                              |
| -------------------- | ---------------------------- | ------------------------------------ |
| `m_currentRate`      | `float`                      | Current playback rate (1.0 = normal) |
| `m_diffs[]`          | `array<pair<int,float>, 10>` | Circular buffer of diff/rate pairs   |
| `m_lastDiff`         | `int`                        | Previous diff value (-1 initially)   |
| `m_rateDiff`         | `int`                        | Current rate-diff step count         |
| `m_lastRates`        | `list<float>`                | Running rate history (max 20)        |
| `m_allowSpeedAdjust` | `bool`                       | Enabled unless `remoteIgnoreSync=1`  |

---

## AES67 Audio-over-IP

### Architecture

```
┌──────────────────────────────────────────────────┐
│              AES67Manager (singleton in fppd)     │
│                                                    │
│  Configuration: pipewire-aes67-instances.json      │
│                                                    │
│  ┌─────────────────────────────────────────────┐  │
│  │ Per-instance Send Pipeline:                  │  │
│  │   pipewiresrc → audioconvert → S24BE,48kHz  │  │
│  │     → rtpL24pay pt=96 (4ms ptime)           │  │
│  │     → udpsink 239.69.0.x:5004               │  │
│  │                                              │  │
│  │   Pad probe: DropBufferProbe on pipewiresrc  │  │
│  │     → Drops 50 buffers on track change       │  │
│  └─────────────────────────────────────────────┘  │
│                                                    │
│  ┌─────────────────────────────────────────────┐  │
│  │ SAP Announcer Thread:                        │  │
│  │   RFC 2974 SAP to 239.255.255.255:9875      │  │
│  │   SDP with ts-refclk, mediaclk, rtpmap      │  │
│  │   Deletion packets on shutdown               │  │
│  └─────────────────────────────────────────────┘  │
│                                                    │
│  ┌─────────────────────────────────────────────┐  │
│  │ PTP Clock:                                   │  │
│  │   gst_ptp_init() at startup                  │  │
│  │   GstPtpClock for domain 0                   │  │
│  │   PTP-derived RTP timestamps                 │  │
│  └─────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────┘
```

### AES67 in Audio Groups

AES67 instances appear as virtual sinks in the audio group UI. They participate in the same topology as physical sound cards:

```
Combine-stream → filter-chain (delay+EQ) → pipewiresrc (GStreamer) → RTP multicast
```

**Delay and EQ fully apply to AES67 outputs** — the filter-chain processes audio before it reaches the GStreamer pipewiresrc that captures it for RTP encoding. Delay is especially useful to compensate for network/receiver latency.

### Track Transition Handling

When a song ends and a new one begins:

1. `GStreamerOutput::Close()` calls `AES67Manager::FlushSendPipelines()`
2. Each send pipeline's `dropCounter` is set to 50
3. The permanent `DropBufferProbe` on `pipewiresrc`'s src pad drops the next 50 buffers (~200ms)
4. This prevents stale audio from the old track from being sent as RTP garbage
5. When the new track starts playing, fresh audio flows through PipeWire to pipewiresrc
6. After 50 clean buffers, the probe passes all buffers through normally

---

## Configuration Files

### PipeWire Configs

| File                                                     | Purpose                                         | Generated By       |
| -------------------------------------------------------- | ----------------------------------------------- | ------------------ |
| `/etc/pipewire/pipewire.conf.d/95-fpp-alsa-sink.conf`    | ALSA sink nodes using stable card IDs (hw:S3)   | FPPINIT.cpp        |
| `/etc/pipewire/pipewire.conf.d/96-fpp-audio-groups.conf` | Filter-chains + combine-stream for audio groups | pipewire.php Apply |

### WirePlumber Configs

| File                                                                     | Purpose                              | Generated By       |
| ------------------------------------------------------------------------ | ------------------------------------ | ------------------ |
| `/etc/wireplumber/wireplumber.conf.d/60-fpp-block-combine-fallback.conf` | Registers FPP Lua hook               | pipewire.php Apply |
| `/usr/share/wireplumber/scripts/linking/fpp-block-combine-fallback.lua`  | Blocks rogue default-target fallback | pipewire.php Apply |

### FPP Audio Configs

| File                                           | Purpose                                                  |
| ---------------------------------------------- | -------------------------------------------------------- |
| `<media>/config/pipewire-audio-groups.json`    | Audio group definitions (groups, members, delay, EQ)     |
| `<media>/config/pipewire-aes67-instances.json` | AES67 instance definitions (multicast IPs, ports, modes) |

---

## Debugging & Diagnostics

### Essential Commands

```bash
# Set environment for all PipeWire commands
export PIPEWIRE_REMOTE=/run/pipewire-fpp/pipewire-0

# View all PipeWire links
pw-link -l

# Monitor nodes in real-time (WAIT/BUSY times, formats, errors)
pw-top

# Dump full graph as JSON (for scripting)
pw-dump | python3 -c "import json,sys; ..."

# List all nodes
pw-cli ls Node

# Check volume
wpctl get-volume @DEFAULT_AUDIO_SINK@

# Set volume (0.0 to 1.0)
wpctl set-volume @DEFAULT_AUDIO_SINK@ 0.43

# View WirePlumber status (shows Sinks, Sources, Filters, Streams)
wpctl status

# Check AES67 RTP packets
tcpdump -i eth0 -c 20 dst 239.69.0.1 and udp port 5004

# Check SAP announcements
tcpdump -i eth0 dst 239.255.255.255 and udp port 9875

# Watch fppd logs
journalctl -u fppd -f

# Check PipeWire service logs
journalctl -u fpp-pipewire -f

# Check WirePlumber service logs
journalctl -u fpp-wireplumber -f

# GStreamer debug (set before starting fppd)
export GST_DEBUG=2               # Warnings + errors
export GST_DEBUG=pipewiresink:4  # Verbose for PipeWire sink only

# Count links (verify topology)
pw-link -l | grep '|' | wc -l
```

### Common Issues & Solutions

| Symptom                        | Cause                                                          | Fix                                                  |
| ------------------------------ | -------------------------------------------------------------- | ---------------------------------------------------- |
| No audio from any output       | PipeWire not running or fppd not connected                     | `systemctl status fpp-pipewire fpp-wireplumber fppd` |
| Volume slider has no effect    | pactl used instead of wpctl (pipewire-pulse not running)       | Fixed in commit 8c48b0d3 — uses wpctl now            |
| Doubled audio on one output    | WirePlumber rogue default-target fallback links                | FPP Lua hook should be installed — re-run Apply      |
| AES67 garbage between tracks   | Stale buffers in GStreamer elements                            | FlushSendPipelines pad probe (automatic)             |
| AES67 silence (no RTP packets) | pipewiresrc stalled (e.g., after testing with gst-launch)      | `systemctl restart fppd`                             |
| 0 links in pw-link output      | WirePlumber hasn't finished linking yet                        | Wait 5-8 seconds after restart                       |
| Filter-chain not running       | PipeWire config not applied                                    | POST /api/pipewire/audio/groups/apply                |
| Wrong volume scale (too quiet) | Double attenuation (PipeWire + GStreamer both applying volume) | Ensure PipeWire backend skips GStreamer SetVolume    |

### Verifying a Healthy System

```bash
export PIPEWIRE_REMOTE=/run/pipewire-fpp/pipewire-0

# 1. Check services
systemctl is-active fpp-pipewire fpp-wireplumber fppd

# 2. Check node count (should be ~15-20 for 3-member group)
pw-cli ls Node 2>/dev/null | grep "^    " | wc -l

# 3. Check link count (should be ~18 for 3 members)
pw-link -l | grep '|' | wc -l

# 4. Check volume is set correctly
wpctl get-volume @DEFAULT_AUDIO_SINK@

# 5. Check AES67 RTP flow (should see packets every ~4ms)
timeout 1 tcpdump -i eth0 -c 10 dst 239.69.0.1 and udp port 5004 2>&1 | tail -1

# 6. Check for errors in all logs
journalctl -u fppd -u fpp-pipewire -u fpp-wireplumber --since "5 min ago" -p err
```
