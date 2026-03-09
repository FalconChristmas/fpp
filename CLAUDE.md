# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FPP (Falcon Player) is a lightweight, optimized sequence player for LED lighting control, designed for Raspberry Pi and BeagleBone SBCs. It speaks E1.31, DDP, DMX, ArtNet, KiNet, Pixelnet, and Renard protocols and can drive LED panels and WS2811 pixel strings via hardware capes. It also supports MQTT for remote control and integration.

## Build System

The project uses Make. The primary Makefile is `src/Makefile`, which includes fragments from `src/makefiles/`.

```bash
# Build everything (from src/ directory)
cd src && make

# Build targets
make              # default optimized build (-O3, -g1 on master)
make debug        # debug build (-g -DDEBUG)
make asan         # address sanitizer build
make tsan         # thread sanitizer build

# Clean
make clean        # remove all build artifacts
make cleanfpp     # remove just fpp artifacts (keeps PCH)
```

Platform is auto-detected: macOS uses clang/clang++, Linux uses g++. On macOS, Homebrew dependencies are expected at `/opt/homebrew` (ARM) or `/usr/local` (Intel). Linker preference: mold > gold > default ld. Precompiled headers used unless DISTCC_HOSTS is set.

### macOS Setup

Run `SD/FPP_Install_Mac.sh` from a directory that will serve as the media directory. It installs Homebrew and all required dependencies (php, httpd, ffmpeg, ccache, SDL2, zstd, taglib, mosquitto, jsoncpp, libhttpserver, graphicsmagick, libusb).

### Key Build Artifacts

- `libfpp.so` (`.dylib` on macOS) — core shared library with most functionality
- `fppd` — main daemon, links against libfpp
- `fpp` — CLI tool (connects to fppd via domain socket)
- `fppmm` — memory map utility
- `fppoled` — OLED display driver (Pi/BBB only)
- `fppcapedetect` — hardware cape auto-detection (Pi/BBB)
- `fpprtc` — real-time clock utility
- `fppinit` — FPP initialization
- `fsequtils` — FSEQ file utilities
- Channel output plugins: `libfpp-co-*.so` — loaded dynamically via `dlopen()`

### Platform Build Configuration (`src/makefiles/platform/`)

| Platform | File | Defines | Notes |
|----------|------|---------|-------|
| Raspberry Pi | `pi.mk` | `PLATFORM_PI` | libgpiod, builds all external submodules, fppoled/fppcapedetect/fpprtc |
| BeagleBone | `bb.mk` | `PLATFORM_BBB` or `PLATFORM_BB64` | PRU support, NEON SIMD (32-bit), fppoled/fppcapedetect |
| macOS | `osx.mk` | `PLATFORM_OSX` | clang++, CoreAudio framework, `.dylib` extension |
| Linux | `linux.mk` | `PLATFORM_DEBIAN`/`PLATFORM_UBUNTU`/etc. | Docker detection skips OLED/cape/RTC builds |

## Architecture

### Daemon Startup (`src/fppd.cpp`)

The daemon initializes these core modules in order: signal handlers, platform GPIO provider, CurlManager, EPollManager (Linux) or kqueue (macOS), Events, FileMonitor, FPPLocale, NetworkMonitor, OutputMonitor, Settings, Warnings, Commands, Plugins, Player, Scheduler, Sequence, MultiSync, ChannelOutputSetup, ChannelTester, MediaOutput, PixelOverlayManager, HTTPApi (port 32322 localhost).

### Main Loop and Event Architecture

The main loop calls `EPollManager::INSTANCE.waitForEvents(sleepms)` (50ms idle, 10ms playing) which blocks on epoll (Linux) or kqueue (macOS). EPollManager manages a single timerfd (Linux) or `EVFILT_TIMER` (macOS) that wakes the loop precisely when `Timers` deadlines are due, avoiding unnecessary latency. The timer callback fires `Timers::INSTANCE.fireTimers()`. After epoll returns, the loop also runs `Timers::fireTimers()`, `CurlManager::processCurls()`, and `GPIOManager::CheckGPIOInputs()`.

### Core Shared Library (`libfpp.so` / `fpp_so.mk`)

Contains 125+ object files covering all core functionality. Key linked libraries: zstd, jsoncpp, curl, mosquitto, SDL2, FFmpeg (avformat/avcodec/avutil/swresample/swscale), GraphicsMagick, libhttpserver, libgpiod, tag (audio metadata), vlc (optional), kms++ (optional).

### C++ Backend (`src/`)

- **fppd** (`fppd.cpp`): Main daemon entry point
- **fpp** (`fpp.cpp`): CLI tool connecting via domain socket, framebuffer device queries
- **libfpp** (`fpp_so.mk`): Core shared library containing all subsystems below

---

## Channel Output System (`src/channeloutput/`)

### Class Hierarchy

- **`ChannelOutput`** — Base class. Pure virtuals: `SendData()`, `GetRequiredChannelRanges()`. Virtuals: `Init()`, `Close()`, `PrepData()`, `StartingOutput()`, `StoppingOutput()`.
- **`ThreadedChannelOutput`** — Extends ChannelOutput with double-buffered async sending via pthread. Subclasses implement `RawSendData()`.
- **`SerialChannelOutput`** — Mixin for serial port management.
- **`UDPOutput`** — Singleton managing all UDP-based protocols (E1.31, DDP, ArtNet, KiNet, Twinkly). Socket pooling, `sendmmsg()` batching, background worker threads.

### Channel Output Plugins (41 `.so` plugins)

Plugins are loaded at runtime via `dlopen()` in `Plugins.cpp`. Each plugin exports a `createPlugin()` factory function. Configured via JSON in `config/channeloutputs.json`.

#### Network Protocol Plugins

| Plugin | Protocol | Notes |
|--------|----------|-------|
| `libfpp-co-UDPOutput` | E1.31/DDP/ArtNet/KiNet/Twinkly | Unified UDP output, multicast, frame dedup |
| `libfpp-co-GenericUDP` | Custom UDP | Configurable packet format |
| `libfpp-co-MQTTOutput` | MQTT | RGB/RGBW publish, threaded |
| `libfpp-co-HTTPVirtualDisplay` | HTTP | Remote rendering |
| `libfpp-co-HTTPVirtualDisplay3D` | HTTP | 3D remote rendering |

#### Pixel String Drivers

| Plugin | Hardware | Platform |
|--------|----------|----------|
| `libfpp-co-RPIWS281X` | WS2811/WS2812 via SPI+PWM | RPi |
| `libfpp-co-spixels` | SPiWare SPI pixels | RPi |
| `libfpp-co-SPIws2801` | WS2801 SPI | RPi/Linux |
| `libfpp-co-SPInRF24L01` | nRF24L01 wireless SPI | RPi/Linux |
| `libfpp-co-BBB48String` | 48-channel string cape | BBB |
| `libfpp-co-BBShiftString` | Shift register strings | BBB |
| `libfpp-co-DPIPixels` | DPI parallel RGB pixels | BBB |
| `libfpp-co-ModelPixelStrings` | Virtual pixel string models | All |

#### Matrix/Panel Drivers

| Plugin | Hardware | Platform |
|--------|----------|----------|
| `libfpp-co-RGBMatrix` | rpi-rgb-led-matrix GPIO panels | RPi |
| `libfpp-co-BBBMatrix` | PRU-driven RGB matrix | BBB |
| `libfpp-co-FBMatrix` | Framebuffer (X11/KMS) | Linux/Mac |
| `libfpp-co-MAX7219Matrix` | MAX7219 SPI LED driver | SPI |
| `libfpp-co-ColorLight5a75` | ColorLight 5A-75 receiver | Multi |
| `libfpp-co-ILI9488` | ILI9488 TFT LCD (SPI) | RPi |
| `libfpp-co-X11Matrix` | X11 window (dev/demo) | Linux/Mac |
| `libfpp-co-X11PanelMatrix` | X11 panel layout viz | Linux/Mac |
| `libfpp-co-VirtualDisplay` | Abstract virtual display | All |

#### Serial/DMX Protocols

| Plugin | Protocol |
|--------|----------|
| `libfpp-co-GenericSerial` | Generic serial (configurable baud, headers/footers) |
| `libfpp-co-Renard` | Renard serial |
| `libfpp-co-LOR` | Light-O-Rama serial |
| `libfpp-co-LOREnhanced` | Enhanced LOR |
| `libfpp-co-BBBSerial` | BeagleBone serial |
| `libfpp-co-USBPixelnet` | Pixelnet via USB |
| `libfpp-co-USBDMX` | USB DMX |
| `libfpp-co-UDMX` | uDMX (FTDI-based) |

#### GPIO/I2C Expander Plugins

| Plugin | Hardware |
|--------|----------|
| `libfpp-co-GPIO` | Direct GPIO (binary on/off, PWM) |
| `libfpp-co-GPIO-595` | 74HC595 shift register |
| `libfpp-co-MCP23017` | MCP23017 I2C 16-bit GPIO |
| `libfpp-co-PCF8574` | PCF8574 I2C 8-bit GPIO |
| `libfpp-co-PCA9685` | PCA9685 I2C 16-ch PWM servo |
| `libfpp-co-USBRelay` | USB relay module |

#### Special

| Plugin | Purpose |
|--------|---------|
| `libfpp-co-FalconV5Support` | Falcon V5 controller hardware |
| `libfpp-co-ControlChannel` | Control channel preset triggers |
| `libfpp-co-Debug` | Debug/test output (logs channel data) |

### Output Processor Pipeline (`src/channeloutput/processors/`)

Chain-of-responsibility pattern applied sequentially to channel data each frame. Configured per-output in JSON. Thread-safe via mutex.

| Processor | Purpose |
|-----------|---------|
| `RemapOutputProcessor` | Sparse channel remapping |
| `BrightnessOutputProcessor` | Global/per-model brightness + gamma curve |
| `ColorOrderOutputProcessor` | RGB byte reordering (RGB→BGR, etc.) |
| `ScaleValueOutputProcessor` | Linear brightness scaling |
| `ClampValueOutputProcessor` | Min/max value clamping |
| `SetValueOutputProcessor` | Force channels to fixed values |
| `HoldValueOutputProcessor` | Hold last value (interpolation) |
| `ThreeToFourOutputProcessor` | RGB→RGBW expansion |
| `OverrideZeroOutputProcessor` | Force zero channels to non-zero |
| `FoldOutputProcessor` | Bit depth reduction |

### String Testers (`src/channeloutput/stringtesters/`)

Test pattern generators for pixel string verification: `PixelCountStringTester`, `PixelFadeStringTester`, `PortNumberStringTester`.

---

## Pixel Overlay System (`src/overlays/`)

### Model Hierarchy

- **`PixelOverlayModel`** — Base class. Represents a 2D pixel grid mapped to DMX channels. Manages shared memory buffers (`shm_open`) for external process access. Supports horizontal/vertical orientation, start corner configuration, and custom node mapping.
- **`PixelOverlayModelFB`** — Framebuffer-based model for hardware displays. Copies data to framebuffer on overlay.
- **`PixelOverlayModelSub`** — Sub-model (child) with X/Y offset within a parent model. Delegates rendering to parent.

### Overlay States

| State | Value | Behavior |
|-------|-------|----------|
| Disabled | 0 | No overlay rendering |
| Enabled | 1 | Opaque — direct channel copy |
| Transparent | 2 | Only non-zero channels overlay |
| TransparentRGB | 3 | Only if all RGB channels non-zero |

### PixelOverlayManager (Singleton)

Manages all models (loaded from `config/model-overlays.json`), state transitions, HTTP API endpoints, font discovery, and periodic effect update thread. Called each frame via `doOverlays()` to blend overlay data into the channel buffer.

### Effect System

- **`RunningEffect`** — Abstract base for active effect instances. `update()` returns ms until next update (0 = done). One effect per model.
- **`PixelOverlayEffect`** — Abstract base extending `Command`. Registry via `GetPixelOverlayEffect(name)`. Static add/remove/list methods.

#### Built-in Effects (`PixelOverlayEffects.cpp`)

| Effect | Description |
|--------|-------------|
| Color Fade | Fade in/out solid color with configurable timing |
| Bars | Animated color bars (Up/Down/Left/Right) |
| Text | Text rendering with scrolling, stationary, or centered modes |
| Stop Effects | Stop running effect, optionally auto-disable model |

### WLED Effects Port (`src/overlays/wled/`)

A port of the WLED firmware's effect engine providing 217 effect modes. Integrated via `WLEDEffect` / `WLEDRunningEffect` / `WS2812FXExt` wrapper classes.

**Effect Parameters**: Buffer mapping, Brightness, Speed, Intensity, Custom1/2/3, Check1/2/3 (bool), Palette, Color1/2/3, Text.

**Effect Categories** (217 total modes defined in `FX.h`):

- **Classic 1D** (~80 effects): Blink, Breath, Chase variants, Comet, Dissolve, Drip, Fade, Fire variants, Fireworks, Glitter, Larson Scanner, Lightning, Meteor, Popcorn, Rainbow, Ripple, Scan, Sparkle, Strobe, Twinkle variants, Wipe variants
- **Noise/Math** (~20 effects): BPM, Colorwaves, Juggle, Noise 16 variants, Oscillate, Palette, Plasma, Pride 2015, Sinelon variants
- **2D Effects** (~30 effects): Akemi, Black Hole, Colored Bursts, DNA, Drift, Fire Noise, Frizzles, Game of Life, GEQ, Julia, Lissajous, Matrix, Metaballs, Noise, Octopus, Plasma Rotozoom, Polar Lights, Pulser, Scrolling Text, Squared Swirl, Sun Radiation, Swirl, Tartan, Waverly
- **Audio-Reactive** (marked with ♪, ~20 effects): DJ Light, Freq Map/Matrix/Pixels/Wave, Grav Center/Centric/Freq, Gravimeter, Mid Noise, Noise Meter, Waterfall
- **Particle System** (~30 effects): Attractor, Blobs, Box, Center GEQ, Dancing Shadows, Drip, Fire, Fireworks, GEQ, Ghost Rider, Hourglass, Impact, Perlin, Pinball, Pit, Sonic Boom/Stream, Sparkler, Spray, Starburst, Volcano, Vortex, Waterfall

**WLED Source Files** (60+ files in `wled/`): `FX.h` (effect definitions), `FX.cpp` (1D implementations), `FX_2Dfcn.cpp` (2D implementations), `FXparticleSystem.h/cpp` (particle effects), plus color utilities, math, FFT, palettes, fonts.

---

## Playlist System (`src/playlist/`)

### Playlist Engine (`Playlist.h/cpp`)

Manages three sections: **leadIn**, **mainPlaylist**, **leadOut**. Each contains `PlaylistEntryBase`-derived entries. Supports looping, randomization, pause/resume, position seeking, time-based queries, and nested sub-playlists (max 5 levels deep). Status tracked via mutex-protected state machine.

**Playlist States**: `IDLE`, `PLAYING`, `STOPPING_GRACEFULLY`, `STOPPING_GRACEFULLY_AFTER_LOOP`, `STOPPING_NOW`, `PAUSED`.

### Playlist Entry Types (14 types)

| Entry Type | Class | Purpose |
|-----------|-------|---------|
| Sequence | `PlaylistEntrySequence` | Play FSEQ files with frame-based timing |
| Media | `PlaylistEntryMedia` | Audio/video playback (random file selection, backend abstraction) |
| Both | `PlaylistEntryBoth` | Synchronized sequence + media playback |
| Command | `PlaylistEntryCommand` | Execute FPP command, check for completion |
| Script | `PlaylistEntryScript` | Run external shell script (blocking/non-blocking) |
| Effect | `PlaylistEntryEffect` | Trigger overlay effect (blocking/non-blocking) |
| Image | `PlaylistEntryImage` | Display image on overlay model (async load, ImageMagick, caching) |
| Pause | `PlaylistEntryPause` | Timed delay between entries |
| Playlist | `PlaylistEntryPlaylist` | Nested sub-playlist |
| Branch | `PlaylistEntryBranch` | Conditional branching (time/loop/MQTT-based, true/false targets) |
| Remap | `PlaylistEntryRemap` | Channel remapping operations |
| URL | `PlaylistEntryURL` | HTTP GET/POST requests (libcurl, token replacement) |
| Dynamic | `PlaylistEntryDynamic` | Load entries at runtime from command/file/plugin/URL/JSON |
| Plugin | `PlaylistEntryPlugin` | Plugin-defined entries |

---

## Command System (`src/commands/`)

### CommandManager (Singleton)

Registration-based dispatch. Commands registered with `addCommand()`, executed via `run(name, args)`. Supports JSON args, HTTP endpoints, MQTT topics. File-watched preset system (`config/commandPresets.json`) with keyword replacement.

**HTTP Endpoints**: `GET/POST /command/{name}`, `GET /commands`, `GET /commandPresets`.
**MQTT Topic**: `/set/command/{command}/{arg1}/{arg2}`.

### Built-in Commands

#### Playlist Commands (`PlaylistCommands.h/cpp`)
Stop Now, Stop Gracefully, Restart/Next/Prev Playlist Item, Start Playlist, Toggle Playlist, Start At Position/Random, Insert Playlist (next/immediate/random), Pause/Resume Playlist.

#### Media Commands (`MediaCommands.h/cpp`)
Set/Adjust/Increase/Decrease Volume, Play Media, Stop Media, Stop All Media, URL Command.

#### Event Commands (`EventCommands.h/cpp`)
Trigger Preset (immediate/future/slot/multiple), Run Script Event, Start/Stop Effect, Start FSEQ As Effect, Stop All Effects, All Lights Off, Switch to Player/Remote Mode.

---

## Media Output System (`src/mediaoutput/`)

### Backends

- **`VLCOutput`** — libvlc backend. Supports MP4/AVI/MOV/MKV/MPG (video) and MP3/OGG/M4A/WAV/FLAC/AAC (audio). Speed adjustment, volume fine-tuning, event hooks.
- **`SDLOutput`** — SDL2 backend. Video display with overlay integration, audio sample extraction for audio-reactive effects.

### Format Support
- **Audio**: mp3, ogg, m4a, m4p, wav, au, wma, flac, aac
- **Video**: mp4, avi, mov, mkv, mpg, mpeg

Volume control via platform-specific mixer (amixer on Linux, CoreAudio on macOS).

---

## FSEQ File Format (`src/fseq/`)

Frame-based sequence format for LED channel data.

### Versions

| Version | Compression | Features |
|---------|-------------|----------|
| V1 | None | Fixed header, raw sequential frames |
| V2 | zstd/zlib/none | Variable headers, sparse channel reading, frame offset indexing, extended blocks |
| V2E (ESEQ) | None | Special 'E' header, 50ms fixed step time |

### Variable Headers
2-byte codes for metadata. Common: `"mf"` = media filename (associated audio/video).

### Key API
- `FSEQFile::openFSEQFile(filename)` — factory for reading
- `FSEQFile::createFSEQFile(filename, version, compression)` — factory for writing
- `prepareRead(ranges, startFrame)` — optimize sparse reads
- `getFrame(frame)` → `FrameData::readFrame()` — read frame data
- `addFrame()` / `finalize()` — write frames

---

## Sensor System (`src/sensors/`)

### Sensor Source Backends

| Backend | Class | Hardware |
|---------|-------|----------|
| IIO | `IIOSensorSource` | Linux IIO ADC (buffer or direct reads, voltage scaling) |
| I2C ADC | `ADS7828Sensor` | TI ADS7828 8-channel 12-bit I2C ADC |
| Multiplexer | `MuxSensorSource` | GPIO-based MUX expanding sensor count |
| Multi | `MultiSensorSource` | Aggregates multiple backends |

Configured via JSON. Callback-based update notifications. `Sensors::INSTANCE` singleton manages all sources.

---

## Plugin System (`src/Plugin.h`, `src/Plugins.h/cpp`)

### Plugin Types (namespace `FPPPlugins`)

| Type | Interface |
|------|-----------|
| `Plugin` | Base — name, settings, multiSync |
| `ChannelOutputPlugin` | Create custom channel output implementations |
| `PlaylistEventPlugin` | Hooks: eventCallback, mediaCallback, playlistCallback |
| `ChannelDataPlugin` | Direct channel data modification (modifySequenceData) |
| `APIProviderPlugin` | Register HTTP endpoints and control callbacks |

Plugins loaded from `/media/plugins/` via `dlopen()`. Settings from `<plugin>/plugin.cfg`. Supports both compiled (`.so`) and interpreted (Lua) plugins.

---

## Core Infrastructure

### EPollManager (`src/EPollManager.h/cpp`)
Singleton wrapping epoll (Linux) or kqueue (macOS) for the main event loop. Manages file descriptor callbacks via `addFileDescriptor()`/`removeFileDescriptor()` and a single wakeup timer (timerfd on Linux, `EVFILT_TIMER` on macOS). The timer is armed via `armTimer(deadlineMS)` with an absolute `GetTimeMS()` deadline, which causes `waitForEvents()` to return precisely when the deadline is reached. `setTimerCallback()` registers the function invoked on timer expiry. Used by `Timers` to ensure timer deadlines wake the main loop without reducing `sleepms`.

### Timers (`src/Timers.h/cpp`)
Singleton timer system supporting one-shot (`addTimer`) and periodic (`addPeriodicTimer`) timers. Fires callbacks or command presets. `fireTimers()` is called each main loop iteration and also via the EPollManager timer callback for precise wakeup. When the next timer deadline changes in `updateTimers()`, it calls `EPollManager::INSTANCE.armTimer(nextTimer)` so epoll/kqueue wakes at exactly the right time. Used by GPIO debounce, eFuse retry, Twinkly token refresh, MQTT disconnect, FileMonitor, and command presets.

### MultiSync (`src/MultiSync.h/cpp`)
Master/slave synchronization across multiple FPP instances. UDP port 32320. Packet types: SYNC (playback sync with frame number), PING, FPPCOMMAND, BLANK, PLUGIN. Knows about FPP variants (Pi models, BBB variants) and Falcon controllers (F16v2-v5, F48v4-v5, etc.).

### Scheduler (`src/Scheduler.h/cpp`)
Schedule-based playback with day patterns (EVERYDAY, WEEKDAYS, WEEKEND, M_W_F, T_TH, custom bitmask). Priority-based scheduling with start/end times.

### Events (`src/Events.h/cpp`)
Publish/subscribe event system. `EventHandler` base with Publish/RegisterCallback. `EventNotifier` for frequency-based notifications.

### Player (`src/Player.h/cpp`)
Singleton managing playlist playback lifecycle. `StartPlaylist()`, `StopNow()`, `StopGracefully()`, `Pause()`, `Resume()`, `Process()`. HTTP resource for status queries.

### Sequence (`src/Sequence.h/cpp`)
FSEQ playback engine. `FPPD_MAX_CHANNELS = 8MB`. `OpenSequenceFile()`, `ProcessSequenceData()`, `SendSequenceData()`, `SeekSequenceFile()`. Bridge data support for E1.31/ArtNet input with expiry.

### MQTT (`src/mqtt.h/cpp`)
`MosquittoClient` wrapping mosquitto library. SSL/TLS support, message caching, QoS 0/1, retain flags, topic prefix hierarchy. MQTT command for playlist/schedule integration.

### Network Monitor (`src/NetworkMonitor.h/cpp`)
Linux netlink (NETLINK_ROUTE) for interface events (NEW_LINK, DEL_LINK, NEW_ADDR, DEL_ADDR). Callback registration. macOS: graceful no-op.

### Network Controller (`src/NetworkController.h/cpp`)
Detects and classifies network LED controllers: FPP, Falcon, SanDevices, ESPixelStick, Baldrick, AlphaPixel, HinksPixel, DIYLE, WLED.

### Output Monitor (`src/OutputMonitor.h/cpp`)
Physical port/pin management with eFuse protection, current monitoring, pixel count tracking, smart receiver callbacks, port grouping.

### Warnings (`src/Warnings.h/cpp`)
Warning system with timeouts, listener callbacks, persistence to file. Used for power supply warnings, hardware issues, etc.

### HTTP API (`src/httpAPI.h/cpp`)
RESTful API on port 32322 (localhost only). Player status, playlist control, effects, log levels, GPIO, output config, schedule, MultiSync stats, E1.31 byte counters.

### Logging (`src/log.h/cpp`)
Facility-based logging with per-module levels. Facilities: General, ChannelOut, ChannelData, Command, E131Bridge, Effect, MediaOut, Playlist, Schedule, Sequence, Settings, Control, Sync, Plugin, GPIO, HTTP. Levels: ERR, WARN, INFO, DEBUG, EXCESSIVE. Complex level strings: `"debug:schedule,player;excess:mqtt"`.

---

## Hardware Abstraction (`src/util/`)

### GPIO (`GPIOUtils.h/cpp`)
- **`PinCapabilities`** — Abstract base for pin features (GPIO I/O, PWM, I2C, UART)
- **`GPIODCapabilities`** — libgpiod implementation
- **`BBBPinCapabilities`** (`BBBUtils.h`) — BeagleBone with PRU pin support, variants: Black, Green, PocketBeagle, PocketBeagle2, BeaglePlay
- **`PiGPIOPinProvider`** (`PiGPIOUtils.h`) — Raspberry Pi pin mappings
- **`TmpFileGPIO`** — File-based fallback (macOS dev)
- **GPIO Manager** (`src/gpio.h/cpp`) — Singleton HTTP resource, poll/event-based input. Debounce uses settle-then-fire logic: edge detection (via polling or gpiod interrupt) records a pending value, then `scheduleDebounceCheck()` registers a one-shot `Timers` callback at the debounce deadline. When the timer fires, `checkDebounceTimers()` re-reads the pin to confirm it held. Per-pin timer names (`gpio_db_<pin>`) prevent interference. Debounce time configurable via `debounceTime` (ms) in `gpio.json`, default 100ms. Edge selection (`debounceEdge`: both/rising/falling) controls which transitions are debounced.

### I2C (`I2CUtils.h`)
Read/write byte/word/block data via `/dev/i2c*`. Device detection and validity checking.

### SPI (`SPIUtils.h`)
Channel-based duplex SPI transfers with configurable baud rate.

### Expression Processor (`ExpressionProcessor.h`)
Runtime expression evaluation with variable binding. Uses tinyexpr internally. Includes `RegExCache` for compiled regex caching.

### PRU Support (`BBBPruUtils.h`, `src/pru/`)
BeagleBone PRU (Programmable Realtime Unit) assembly programs for microsecond-precision LED timing:
- `FalconSerial.asm` — DMX/Pixelnet serial output
- `FalconMatrix.asm` — LED matrix panel driving (plus ByRow, ByDepth, PRUCpy variants)
- `FalconUtils.asm` — Shared PRU macros
- Hardware defs for AM33XX (BBB, 5ns/cycle, 12KB shared mem) and AM62X (BB64, 4ns/cycle, 32KB shared mem)

---

## Channel Testing (`src/channeltester/`)

Singleton HTTP resource with mutex-protected test patterns. Patterns: `RGBChase`, `RGBCycle`, `RGBFill`, `SingleChase`. Overlay test data into channel output via `OverlayTestData()`.

## OLED Display (`src/oled/`)

Small monochrome display support for SBCs. Drivers: SSD1306 (128x32/64 I2C), I2C 16x2/20x4 LCD. Page framework: `OLEDPage` → `TitledOLEDPage` / `ListOLEDPage` / `MenuOLEDPage` / `PromptOLEDPage`. Pages: `FPPStatusOLEDPage` (CPU/RAM/IP/playlist), `NetworkOLEDPage`, `FPPMainMenu`. Runs as standalone `fppoled` daemon.

---

## Web API (`www/api/controllers/`)

PHP-based REST API using Limonade micro-framework. 24 controllers, 150+ endpoints. Full docs in `www/api/endpoints.json`.

| Controller | Key Endpoints |
|-----------|---------------|
| `playlist.php` | CRUD playlists, start/pause/resume/stop |
| `sequence.php` | FSEQ file management, metadata |
| `files.php` | File ops across media dirs (copy/rename/delete/upload) |
| `system.php` | Reboot, shutdown, start/stop/restart FPPD, status/info |
| `settings.php` | Get/put settings, JSON settings |
| `channel.php` | Channel I/O stats, output processors, output config |
| `network.php` | Interface list, WiFi scan, DNS, interface config |
| `cape.php` | Cape info/options, EEPROM signing |
| `backups.php` | JSON backup/restore |
| `plugin.php` | Install/uninstall/update plugins |
| `schedule.php` | Get/save/reload schedule |
| `effects.php` | Effect file listing |
| `media.php` | Media metadata, duration |
| `testmode.php` | Channel test control |
| `proxies.php` | Remote system proxy management |
| `git.php` | Git status, branches, OS releases |

---

## Scripts (`scripts/`)

- **Daemon control**: `fppd_start`, `fppd_stop`, `fppd_restart`
- **Common/Functions**: `common` (env vars), `functions` (26KB utility library)
- **Health**: `healthCheck` (FPPD, memory, network, audio checks)
- **Git wrappers**: `git_branch`, `git_status`, `git_reset`, `git_fetch`, `git_pull`, `git_origin_log`, `git_checkout_version`
- **Hardware**: `detect_cape`, `upgradeCapeFirmware`, `generateEEPROM`
- **Utilities**: `format_storage.sh`, `start_kiosk.sh`, `wifi_scan.sh`, `generate_crash_report`

## Installation (`SD/`)

- **`FPP_Install.sh`** (62KB) — Master installer for Pi/BBB (dependency install, repo clone, build, systemd services)
- **`FPP_Install_Mac.sh`** — macOS dev setup (Homebrew, Apache/PHP-FPM, LaunchAgent)
- **Platform flashers**: `BBB-FlashMMC.sh`, `BB64-AutoFlash.sh`, `Pi-FlashUSB.sh`
- **OS upgrade**: `upgradeOS-part1.sh`, `upgradeOS-part2.sh`

## External Dependencies (`external/`)

Git submodules (auto-fetched during build): RF24 (2.4GHz wireless), rpi-rgb-led-matrix (GPIO LED panels), rpi_ws281x (WS2811/2812 driver), spixels (SPI pixel strings).

## Configuration Formats

- **Channel outputs**: `config/channeloutputs.json` — output type, startChannel, channelCount, per-output config
- **Overlay models**: `config/model-overlays.json` — pixel grid definitions
- **Command presets**: `config/commandPresets.json` — named command sequences with keyword replacement
- **Settings**: `/media/settings` — key=value text file
- **Web settings**: `www/settings.json` — declarative settings metadata with UI types and validation
- **Cape configs**: `capes/` directory — JSON files with GPIO pin mappings, output channel definitions
- **Audio**: `etc/asoundrc.*` — ALSA configurations (dmix, hdmi, plain, softvol)

## Code Style

- **C++**: Configured via `.clang-format`. 4-space indent, no tabs, Allman-ish braces (custom), no column limit, C++20/23 standard.
- **JavaScript**: Configured via `.prettierrc`. Semicolons, tabs, experimental ternaries.
- **C++ standard**: GNU++23 with GCC 12+, GNU++2a with older GCC, C++20 with Clang.
