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
make              # default optimized build (-O3; GCC builds add -g1)
make debug        # debug build (-g -DDEBUG)
make asan         # address sanitizer build (does NOT run on 4K-page arm64 kernels)
make tsan         # thread sanitizer build
make ubsan        # undefined behavior sanitizer build

# Clean
make clean        # remove all build artifacts
make cleanfpp     # remove just fpp artifacts (keeps PCH)
```

Platform is auto-detected: macOS uses clang/clang++, Linux uses g++. On macOS, Homebrew dependencies are expected at `/opt/homebrew` (ARM) or `/usr/local` (Intel). Linker preference: mold > gold > default ld. Precompiled headers are used unless a distributed compile is configured (`DISTCC_HOSTS`, `NOCC_SERVERS` or `NOCC_DISCOVER_MDNS`), which builds with `-DNOPCH` instead.

### macOS Setup

Run `SD/FPP_Install_Mac.sh` from a directory that will serve as the media directory. It installs Homebrew and all required dependencies; the `brew install` line in the script is the current list.

Media plays through the same GStreamer pipelines as on Linux (Homebrew's `gstreamer` formula, found via pkg-config). There is no PipeWire on macOS, so `isPipeWireBackend()` is always false and audio goes to `autoaudiosink` (the default output device). The HDMI output is an "FPP Video Output" window (`src/MacOSApp.mm`), whose position macOS saves per stream slot (`defaults read fppd`). Two traps when working on it:

- **Windows only appear when fppd runs in the GUI login session**, as the LaunchAgent does. An fppd started from an ssh or tool shell reports its window visible and nothing is on screen; test as a LaunchAgent (`launchctl bootstrap gui/$(id -u) <plist>`).
- **AppKit runs on fppd's main thread**, pumped from `EPollManager::waitForEvents()`. Never hop to the main thread from a GStreamer bus handler: GStreamer posts there holding its GL display lock, which the main thread needs to draw, and they deadlock. Create the window before the pipeline starts (`MacOSShowVideoWindow()`).

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
| macOS | `osx.mk` | `PLATFORM_OSX` | clang++, CoreAudio + Cocoa, GStreamer via Homebrew, `.dylib` extension |
| Linux | `linux.mk` | `PLATFORM_DEBIAN`/`PLATFORM_UBUNTU`/etc. | Docker detection skips OLED/cape/RTC builds |

## Plugin Compatibility

External plugins (`/media/plugins/`) are compiled separately and link against FPP headers. When modifying public headers (especially `fpp-pch.h`, `commands/Commands.h`, `Plugin.h`, `Plugins.h`, or any header included by channel output plugins), preserve backward compatibility:

- Do not remove or rename public macros, classes, or functions that plugins may depend on. If cleaning up internally, keep the old symbol as an alias/empty define with a comment.
- `HTTP_RESPONSE_CONST` in `fpp-pch.h` is an example: FPP's own code no longer uses it, but it's kept as an empty `#define` for plugin compatibility.
- Channel output plugins implement `ChannelOutput` or `ThreadedChannelOutput` and are loaded via `dlopen()`. Changes to these base class interfaces will break all plugins.
- Plugins are rebuilt by a git update, but **not** after an FPPOS reflash, and prebuilt plugins (no Makefile) never are. A layout or vtable change to any header a plugin compiles in must bump `FPP_PLUGIN_API_VERSION` in `src/Plugin.h`: inserting a virtual, adding or reordering a data member, or changing an inline function. Those headers carry a `PLUGIN ABI:` comment, and headers whose only dependency is call signatures carry `PLUGIN API:`. **Read `.claude/PLUGIN-ABI.md` before changing any of them.**

## Code Style

- **C++**: Configured via `.clang-format`. 4-space indent, no tabs, Allman-ish braces (custom), no column limit, C++20/23 standard.
- **JavaScript**: Configured via `.prettierrc`. Semicolons, tabs, experimental ternaries.
- **C++ standard**: GNU++23 with GCC, C++20 with Clang (set in `src/makefiles/common/setup.mk`).

## Frontend

When designing HTML, CSS, or working within `www/`, read `.claude/FRONTEND-GUIDELINES.md` before generating any markup.

### In-app help pages

`www/help/` holds the F1 help screens. They are hand-written prose about the controls on
each page, so they go stale silently when a page changes — nothing breaks, the text just
becomes wrong. **Whenever you add, remove, rename, or change the meaning of a user-facing
control under `www/` (including a setting in `www/settings.json`), update the matching help
file in the same change**, and add a new `help/settings-<name>.php` whenever you add a
settings tab. Read `.claude/HELP-PAGES.md` for the page-to-help-file mapping and the
conventions these files follow.

## fppd Warnings

Warnings (`WarningHolder::AddWarning`) drive the banner shown across the top of every
web page. There is no expiry unless you ask for one and no dismiss button, so a warning
that is raised and never retracted stays on screen until fppd restarts — and
`RemoveWarning()` matches on the id **and the exact message text**, so a retyped message
at the remove site silently clears nothing. **Whenever you add a warning, decide in the
same change how it comes down**: a timeout, a matching removal on the recovery path, or a
documented decision that it is permanent. Read `.claude/WARNINGS.md` before adding or
changing one.

## Configuration Formats

- **Channel outputs**: `config/channeloutputs.json` — output type, startChannel, channelCount, per-output config
- **Overlay models**: `config/model-overlays.json` — pixel grid definitions
- **Command presets**: `config/commandPresets.json` — named command sequences with keyword replacement
- **Settings**: `/media/settings` — key=value text file
- **Web settings**: `www/settings.json` — declarative settings metadata with UI types and validation.
  Two flags there mark data that must not leave the device, and are read by every consumer rather
  than re-listed in each one:
  - `"type": "password"` — a credential. Also masks the field in the UI.
  - `"pii": true` — personal or household-identifying data that is not a credential (coordinates,
    contact addresses, account names, kiosk URL).
  - `"piiPurpose": "<purpose>"` — PII collected *for* a named purpose. A consumer serving that
    purpose uses it deliberately; every other consumer still redacts it. Today the only value is
    `"crash-report"`, on `emailAddress`: the field exists so developers can contact a user about
    a crash, so `scripts/generate_crash_report` lifts it into `contact.json` (at every level,
    including "stack traces only") while the statistics client still drops it.

  `scripts/generate_crash_report` redacts `password`/`pii` when bundling a crash report, and the
  statistics client omits them. **Mark new settings at the point of declaration** — a
  consumer-side list drifts, and has: one hand-written copy was already missing `gitHubPAT`.
- **Interface settings**: `www/interface-settings.json` — declarative metadata for
  `config/interface.<name>`, in the same shape and vocabulary as `settings.json` (`description`,
  `tip`, `type`, `options`, `default`, `gatherStats`, `pii`). Intended to drive the network
  configuration UI the way `settings.json` drives the rest; today two consumers read one key from
  it. A field is disclosed — to the statistics payload (`stats.php`) and to crash reports
  (`scripts/generate_crash_report`) — **only where it declares `"gatherStats": true`**. Both
  consumers fail closed if the file is unreadable. It is an allowlist on purpose: a field added
  without `gatherStats` is withheld automatically. `SSID`, `PSK`, `BACKUPSSID`, `BACKUPPSK` and
  the addresses stay off it.
- **Troubleshooting commands**: `www/troubleshoot-commands.json` — the commands on the
  Troubleshooting page, which also go into Diagnostic Reports (manual crash reports). Same rule
  as `settings.json`: a command whose output identifies the user or their network (SSIDs/BSSIDs,
  command lines, git identity, client IPs) is marked `"pii": true` where it is declared. A
  Diagnostic Report runs a command's `"manualCrashReportCmd"` where it has one, and leaves out a
  `pii` command that has none. The redactor only knows key=value secrets, so this flag is what
  keeps these out. **Decide `pii`
  for every command you add, and check it against real output, not the command name.**
  Commands run inside `sh -c '...'` (no single quotes), and `[[name]]` is replaced with a PHP
  variable (no `[[:space:]]`-style classes).
  A command that prints a web page (server-status, phpinfo) takes `"format": "html"`: the page
  renders it in a script-less sandboxed iframe instead of a `<pre>`, and the helper skips its
  `fold`. Diagnostic Reports still get the raw output.
- **Cape configs**: `capes/` directory — JSON files with GPIO pin mappings, output channel definitions
- **Audio**: `etc/asoundrc.*` — ALSA configurations (dmix, hdmi, pipewire, plain, softvol)
