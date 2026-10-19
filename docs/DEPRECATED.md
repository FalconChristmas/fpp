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
  `system/fppd/{start,stop,restart}`, and `remoteAction` with
  `action=reboot`, `restartFppd` or `upgradeOS` (its other actions are reads).
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

**What a GET to one of them can do today:** over plain HTTP, a page on any site
can, with nothing more than an `<img>` tag:
- flash an unsigned OS image from a URL it chooses (`upgradeOS.php?os=`);
- check out and build any public pull request as root (`changebranch.php`);
- reboot the player, run its scripts, or run any FPP command.

The 10.3 review weighed refusing browser GETs that lack a same-host `Referer`. It
deferred that to FPP 11, because user scripts, plugins and third-party pages may
still call these routes by GET from a browser.

**Done in 10.3:** every GET to one of these routes is marked:
- **Deprecation header:** the response carries `Deprecation: true`.
- **Log line:** fppd.log gets a warning naming the route, the client and its
  User-Agent, at most once an hour per route and client.
- **Where it's done:** `www/common.php`
  (`NoteDeprecatedStateChangingGet()`, which holds the list for the PHP routes
  and pages) and fppd's `CommandManager::render_GET` (for `/api/command/...`).
  Keep the list in common.php in step with rule 2 of the gate.

**Also done in 10.3:**
- **POST accepted:** every route above takes POST as well as GET (the pages
  read their parameters from the query string either way). fppd's command
  route takes `POST /api/command/<name>` with the arguments as a JSON array,
  or `POST /api/command` with `{"command": ..., "args": [...]}`.
- **FPP's own callers send POST:** the web UI (`PostCommand()` and
  `PostNavigate()` in `www/js/fpp.js`, and `StreamURL(..., 'POST')`), the PHP
  controllers' calls to fppd (`FppdCommand()` in `www/common.php`), the MultiSync
  job runner, the remote reboot/restart/shutdown pages and `remoteAction`,
  fppd's own reboot/shutdown, and fppoled's menu. Calls to another player
  (`RemotePlayerAction()`, `RemoteJobAction()`) fall back to GET when it is
  older than 10.3 and answers the POST 404 or 405; a 10.2-or-older player
  answers `POST /api/file/move/...` as an upload to a directory named "move",
  so the job runner retries that one by GET too.
- **Docs:** `www/api/openapi.json` lists the POST of each route and marks its
  GET `deprecated` (`@route ... deprecated` in the docblocks); the API help
  pages and `fpp -h` show the POST forms.

**Plan:**
1. 10.x: tell integrators, pointing them at the log lines above to find their
   own GET callers.
2. 11: these routes answer GET with 405, and rule 2 of the gate is removed.
   Keep the fallbacks to GET: an FPP 11 player can still drive older ones.

**Removal condition:** FPP 11. Until then, keep rule 2's route list in step with
any GET route that changes state.
