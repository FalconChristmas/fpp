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
| -------- | ---- | ------- | ----- |
| Raspberry Pi | `pi.mk` | `PLATFORM_PI` | libgpiod, builds all external submodules, fppoled/fppcapedetect/fpprtc |
| BeagleBone | `bb.mk` | `PLATFORM_BBB` or `PLATFORM_BB64` | PRU support, NEON SIMD (32-bit), fppoled/fppcapedetect |
| macOS | `osx.mk` | `PLATFORM_OSX` | clang++, CoreAudio framework, `.dylib` extension |
| Linux | `linux.mk` | `PLATFORM_DEBIAN`/`PLATFORM_UBUNTU`/etc. | Docker detection skips OLED/cape/RTC builds |

## Plugin Compatibility

External plugins (`/media/plugins/`) are compiled separately and link against FPP headers. When modifying public headers (especially `fpp-pch.h`, `commands/Commands.h`, `Plugin.h`, `Plugins.h`, or any header included by channel output plugins), preserve backward compatibility:

- Do not remove or rename public macros, classes, or functions that plugins may depend on. If cleaning up internally, keep the old symbol as an alias/empty define with a comment.
- `HTTP_RESPONSE_CONST` in `fpp-pch.h` is an example: FPP's own code no longer uses it, but it's kept as an empty `#define` for plugin compatibility.
- Channel output plugins implement `ChannelOutput` or `ThreadedChannelOutput` and are loaded via `dlopen()`. Changes to these base class interfaces will break all plugins.

## Code Style

- **C++**: Configured via `.clang-format`. 4-space indent, no tabs, Allman-ish braces (custom), no column limit, C++20/23 standard.
- **JavaScript**: Configured via `.prettierrc`. Semicolons, tabs, experimental ternaries.
- **C++ standard**: GNU++23 with GCC 12+, GNU++2a with older GCC, C++20 with Clang.

## Frontend

When designing HTML, CSS, or working within `www/`, read `.claude/FRONTEND-GUIDELINES.md` before generating any markup.

## Web API

When adding, modifying, or reviewing routes in `www/api/`, read `.claude/WWWAPI-GUIDELINES.md` before making changes.

## Configuration Formats

- **Channel outputs**: `config/channeloutputs.json` — output type, startChannel, channelCount, per-output config
- **Overlay models**: `config/model-overlays.json` — pixel grid definitions
- **Command presets**: `config/commandPresets.json` — named command sequences with keyword replacement
- **Settings**: `/media/settings` — key=value text file
- **Web settings**: `www/settings.json` — declarative settings metadata with UI types and validation
- **Cape configs**: `capes/` directory — JSON files with GPIO pin mappings, output channel definitions
- **Audio**: `etc/asoundrc.*` — ALSA configurations (dmix, hdmi, plain, softvol)
