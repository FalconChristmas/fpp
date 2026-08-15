# System Architecture

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

## Sensor System (`src/sensors/`)

| Backend | Class | Hardware |
| --- | --- | --- |
| IIO | `IIOSensorSource` | Linux IIO ADC (buffer or direct reads, voltage scaling) |
| I2C ADC | `ADS7828Sensor` | TI ADS7828 8-channel 12-bit I2C ADC |
| Multiplexer | `MuxSensorSource` | GPIO-based MUX expanding sensor count |
| Multi | `MultiSensorSource` | Aggregates multiple backends |

Configured via JSON. Callback-based update notifications. `Sensors::INSTANCE` singleton manages all sources.

---

## Plugin System (`src/Plugin.h`, `src/Plugins.h/cpp`)

### Plugin Types (namespace `FPPPlugins`)

| Type | Interface |
| --- | --- | --- |
| `Plugin` | Base — name, settings, multiSync |
| `ChannelOutputPlugin` | Create custom channel output implementations |
| `PlaylistEventPlugin` | Hooks: eventCallback, mediaCallback, playlistCallback |
| `ChannelDataPlugin` | Direct channel data modification (modifySequenceData) |
| `APIProviderPlugin` | Register HTTP endpoints and control callbacks |

Plugins loaded from `/media/plugins/` via `dlopen()`. Settings from `<plugin>/plugin.cfg`. Supports both compiled (`.so`) and interpreted (Lua) plugins.

## Channel Testing (`src/channeltester/`)

Singleton HTTP resource with mutex-protected test patterns. Patterns: `RGBChase`, `RGBCycle`, `RGBFill`, `SingleChase`. Overlay test data into channel output via `OverlayTestData()`.

## OLED Display (`src/oled/`)

Small monochrome display support for SBCs. Drivers: SSD1306 (128x32/64 I2C), I2C 16x2/20x4 LCD. Page framework: `OLEDPage` -> `TitledOLEDPage` / `ListOLEDPage` / `MenuOLEDPage` / `PromptOLEDPage`. Pages: `FPPStatusOLEDPage` (CPU/RAM/IP/playlist), `NetworkOLEDPage`, `FPPMainMenu`. Runs as standalone `fppoled` daemon.

---

## Web API (`www/api/controllers/`)

PHP-based REST API using Limonade micro-framework. 24 controllers, 150+ endpoints. Full docs in `www/api/endpoints.json`.

| Controller | Key Endpoints |
| --- | --- |
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
- **Flashing**: `flash_storage.sh` (shared engine, all platforms), with `Pi-FlashUSB.sh`,
  `BBB-AutoFlash.sh` and `BB64-AutoFlash.sh` as its callers
- **OS upgrade**: `upgradeOS-part1.sh`, `upgradeOS-part2.sh`

## External Dependencies (`external/`)

Git submodules (auto-fetched during build): RF24 (2.4GHz wireless), rpi-rgb-led-matrix (GPIO LED panels), spixels (SPI pixel strings).
