# Deprecated / Planned Removals

Things intentionally kept for backward compatibility that should be removed once
the conditions below are met. Each entry lists **what**, **why it's still here**,
and the **removal condition**.

## BeagleBone pinmux: bone-pinmux-helper / cape-universal device tree

**Status:** migrating to the `am335xpm` kernel driver (`/dev/am335xpm`), which
performs the privileged register writes the AM335x hardware requires (userspace
`/dev/mem` writes to the control-module pinmux registers are silently dropped).

**Done:**
- `am335xpm` driver added to the custom kernel; the `pinctrl` tool writes through
  it (`capes/drivers/pinctrl/src/AM335x.cpp`, `Ball.cpp`).
- `CapeUtils`/`BBBUtils` prefer the `pinctrl` tool; `CapeUtils::ConfigurePin`
  migrated, and `findCapeEEPROM()` now muxes i2c2 itself (the helper used to do
  this at boot).
- `capes/drivers/bbb/fpp-base-overlay.dts` trimmed (~1723 → ~250 lines): the
  per-pin `BONE_PIN` definitions and `bone-pinmux-helper` nodes were removed.
  (Overlays are built at image time, so this only affects new images, which ship
  the `am335xpm` kernel.)

**Still here for old kernels — remove when no deployed device runs a
pre-`am335xpm` kernel image:**

1. `_pinmux/state` (bone-pinmux-helper sysfs) fallbacks in C++:
   - `src/non-gpl/CapeUtils/CapeUtils.cpp` — `ConfigurePin()` fallback branch.
   - `src/util/BBBUtils.cpp` — the `/sys/devices/platform/ocp/ocp:*_pinmux/state`
     fallback branches.
   - `capes/drivers/pinctrl/src/Ball.cpp` — `setMode()` pinmux-helper sysfs branch
     (used when `/dev/am335xpm` is absent).
2. Kernel `bone-pinmux-helper` driver and its config:
   - `../bb-kernel/patches/drivers/ti/gpio/0002-bone-pinmux-helper.patch`,
     `0004-*`, `0007-*`.
   - `CONFIG_BEAGLEBONE_PINMUX_HELPER` in `patches/defconfig` (set `=n`, then drop
     the patches). Harmless to leave enabled meanwhile — with the trimmed overlay
     the driver simply binds to nothing.

**Removal condition:** all supported/deployed BBB+PB images include the
`am335xpm` kernel (i.e., the last pre-`am335xpm` image is EOL). Until then the
fallbacks above keep older images working through normal master updates.

## BeagleBone gpio-of-helper

`gpio-of-helper` kernel patches (`0001-gpio-of-helper.patch`, `0005-*`) are carried
but **not enabled** (`CONFIG_GPIO_OF_HELPER` is unset in `patches/defconfig`, so the
driver isn't built). They can be dropped from the kernel patch set entirely at the
next convenient kernel rebase.

## HTTP API: routes that change state but answer GET

**Status:** planned for FPP 11 — these routes must require POST.

Any web page a user opens can make the browser send a GET to the player (a link,
an `<img>`, a no-cors `fetch`). The cross-site gate at the top of
`etc/apache2.site` refuses POST/PUT/DELETE that another site sends, by `Origin`.
A cross-site GET over plain HTTP carries neither `Sec-Fetch-Site` nor `Origin`,
so it cannot be told apart from an integration's request. Rule 2 of the gate
covers these routes only when the UI is reached over HTTPS.

**Routes:**
- `www/api/index.php`: `events/:id/trigger`, `file/move/:name`, `git/reset`,
  `network/interface/add/:if`,
  `playlists/{stop,pause,resume,stopgracefully,stopgracefullyafterloop}`,
  `playlist/:name/start[/...]`, `playlist/repeat/:r`, `plugin/:repo/upgrade`,
  `sequence/current/{stop,togglePause}`, `sequence/:name/start/:sec`,
  `scripts/:name/run`, `system/{reboot,shutdown}`,
  `system/fppd/{start,stop,restart}`.
- fppd: `GET /api/command/<name>/<args...>` (`CommandManager::render_GET`) runs
  any command. The `/commands` and `/commandPresets` GETs are reads and stay.
- Pages: `changebranch`, `copystorage`, `disableKiosk`, `flash-storage`,
  `formatstorage`, `gitCheckoutVersion`, `growsd`, `installKiosk`,
  `manualUpdate`, `newpartitionsd`, `packagesHelper`, `rebootRemoteFPP`,
  `rebuildfpp`, `resetConfig`, `restartRemoteFPPD`, `run_mp3gain`,
  `runEventScript`, `shutdownRemoteFPP`, `upgradeCapeFirmware`, `upgradefpp`,
  `upgradeOS` (`.php`).

**Still here because:** integrations call them by GET — xLights, Home Assistant,
Remote Falcon, Stream Deck plugins and many user scripts use
`GET /api/command/...` in particular.

**Plan:**
1. 10.x: accept POST on every route above alongside GET; move FPP's own UI and
   the MultiSync job runner to POST; mark GET deprecated (a `Deprecation`
   response header and a rate-limited log line); update `www/api/openapi.json`
   and the API help page; tell integrators.
2. 11: these routes answer GET with 405, and rule 2 of the gate is removed.

**Removal condition:** FPP 11. Until then, keep rule 2's route list in step with
any GET route that changes state.
