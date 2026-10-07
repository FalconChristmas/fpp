# Plugin ABI (`FPP_PLUGIN_API_VERSION`)

Native (C++) plugins live in `/home/fpp/media/plugins/<name>`. Each one is built on the
device against whatever FPP headers were installed at that moment, and is then
`dlopen()`ed by every later fppd. Plugins are rebuilt on some upgrade paths and not on
others:

- **Git updates** (`git_pull`, `upgrade_FPP`, `git_branch`) rebuild them. `compileBinaries()`
  in `scripts/functions` cleans and rebuilds every plugin that has a Makefile.
- **FPPOS reflashes** do not. The plugin clones survive on `/home/fpp/media`, built against
  the old headers.
- **Prebuilt plugins** (no Makefile, e.g. FPPMon) are never rebuilt on any path.

For those last two, a header change that alters something a plugin compiled in leaves a
binary that disagrees with libfpp about where things are, and nothing fails until it
runs.

`FPP_PLUGIN_API_VERSION` (src/Plugin.h) is the gate: the loader refuses a plugin whose
compiled-in version differs from FPP's. **It only works if the version is bumped.** FPP
10.2 shipped two layout changes without a bump. `PinCapabilities` gained a virtual in
mid-vtable, and `PixelOverlayModel` gained members ahead of the ones its inline getters
read. Plugins built under 10.1 loaded and crash-looped fppd.

## What counts as an ABI change

Plugins depend on a header in two different ways, and the rule is different for each.

**Layout ABI. Any change needs a version bump.** A plugin bakes the layout into its own
binary when it:

- **subclasses** an FPP class. FPP then calls virtuals through the plugin's vtable, so
  inserting, removing, reordering *or appending* a virtual breaks it. Appending reads
  past the end of an older plugin's vtable.
- **calls a virtual** through an FPP pointer. The vtable slot index is compiled in, so
  inserting, removing or reordering breaks it. Appending after the last virtual is safe
  only if no plugin subclasses the class.
- **reads or writes a data member**, directly or through an **inline** getter or setter.
  The field offset is compiled in.
- **constructs, `new`s, copies, or embeds** an FPP type, or builds a container of it.
  `sizeof` is compiled in.
- calls an **inline function** whose body touches any of the above.
- uses a **macro** that expands to a member access. `LogDebug`/`VB_*` resolve to
  `FPPLogger::INSTANCE` member offsets.

None of these leaves a symbol in the plugin's `.so`. The loader cannot detect them unless
the version is bumped or a fingerprint is checked.

**Symbol API. Keep the old signature.** A plugin that only calls exported, non-virtual
functions (`Player::INSTANCE.IsPlaying()`, `FileMonitor::INSTANCE.AddFile(...)`) depends
on the mangled names alone. Data layout there is free to change. Changing or removing a
signature makes `dlopen(RTLD_NOW)` refuse the plugin with "undefined symbol": loud, but
it still breaks the plugin. Add the new signature as an overload and keep the old one, as
3d8aab31c did for `Timers::addPeriodicTimer`, or bump.

Prefer a pimpl for anything that will keep growing (`Command`/`CommandArg` did this in
v5), so new state stays out of the plugin-visible layout. A loader fingerprint
(`fpp_*_abi_size`, see Plugin.h/Commands.h) catches a missed bump on future builds, but
not binaries already in the field, which export no fingerprint.

## After an FPPOS upgrade

An `.fppos` reflash replaces `/opt/fpp` and keeps the plugin clones. FPPINIT sets
`pluginReinstallNeededAfterOS`, and while a plugin is on that list fppd **does not load
its native library** (`PluginManager::awaitingReinstallAfterOS`). The Plugin Manager's
reinstall rebuilds the plugin and then loads it. Source plugins therefore recover with
one click. Prebuilt plugins (FPPMon) depend on publishing a binary for the new FPP.

The git upgrade path rebuilds every plugin that has a Makefile. Prebuilt plugins are the
case the version bump (and the loader fingerprints) still has to protect on every path.

## Where plugins actually depend on FPP

This is from an audit of every C++ plugin in `fpp-data/pluginList.json` plus FPPMon,
21 plugins in all (2026-10). Each header listed carries a `PLUGIN ABI:` (layout) or
`PLUGIN API:` (symbol) comment naming what plugins use.

### Layout ABI

| Header | What plugins compile in | Plugins |
|---|---|---|
| `Plugin.h` | `FPPPlugins::Plugin` and its interface classes (subclassed; `name`/`settings` read directly); `FPPPlugin` is entirely inline | all 21 |
| `log.h` | `LogDebug`/`VB_*` → `FPPLogger::INSTANCE` offsets (fingerprinted) | nearly all |
| `commands/Commands.h` | `Command` subclassed; `CommandArg` built and inline setters (fingerprinted); `Command::Result`/`ErrorResult` entirely inline (**not** fingerprinted) | ArtNetAdv, Capture, FPPMon, HomeAssistant, PixelRadio, VideoCapture, arcade, brightness, edmrds, kfmt, osc, buttonqueue, tplink, vastfmt, midi |
| `MultiSync.h` | `MultiSyncPlugin` subclassed; inline `isMultiSyncEnabled()` reads `MultiSync` layout; `MultiSyncSystemType` values | ArtNetAdv, LoRa, ShowPilot, pulsemesh, smpte, brightness, FPPMon |
| `util/GPIOUtils.h` | `PinCapabilities` virtual calls (`ptr`, `configPin`, `getValue`, `setValue`); `name` | vastfmt, edmrds |
| `util/I2CUtils.h` | `new I2CUtils` (sizeof); inline `isOk()` | vastfmt, kfmt |
| `mediadetails.h` | `MediaDetails` fields read directly | vastfmt, edmrds, kfmt, PixelRadio |
| `overlays/PixelOverlayModel.h` | inline getters (width/height/size/name/type/runningEffect/effect mutex); `setState()` virtual; `PixelOverlayState` by value | arcade, VideoCapture, HomeAssistant |
| `overlays/PixelOverlayEffects.h` | `RunningEffect`/`PixelOverlayEffect` subclassed; `RunningEffect::model` | arcade, VideoCapture |
| `overlays/PixelOverlay.h` | inline `getModelNames()` | VideoCapture |
| `playlist/Playlist.h` | `PlaylistHandle` by value, inline `operator->` | ArtNetAdv, smpte |
| `mediaoutput/AudioSourceRegistry.h` | `AudioSource` built on the plugin's stack | smpte |
| `mqtt.h`, `Events.h` | inline `MosquittoClient::GetBaseTopic()`; `Publish()` through `EventHandler`'s vtable | HomeAssistant |
| `fseq/FSEQFile.h` | `FSEQFile`/`V2FSEQFile`/`FrameData` virtual calls; inline setters; `m_sparseRanges`; `VariableHeader` by value | Capture |
| `util/ExpressionProcessor.h` | `new ExpressionVariable` (no pimpl) | osc, midi |
| `CurlManager.h` | `CurlPrivateData::req`/`resp` read; method signatures | FPPMon |

### Symbol API only

`Player.h`, `EPollManager.h`, `FileMonitor.h`, `Timers.h`, `Sequence.h` (and
`FPPD_MAX_CHANNELS`), `sensors/Sensors.h`, `Warnings.h`, `settings.h`/`common.h` free
functions and `FPP_DIR_*` macros. `fpphttp.h`'s inline helpers are compiled into
plugins but touch no FPP layout.

## Re-running the audit

Clone every repo in `https://raw.githubusercontent.com/FalconChristmas/fpp-data/master/pluginList.json`.
The ones with `.cpp` files are the native plugins. For each one, classify every FPP
entity it uses by the kinds above, checking each against the declaration. Usage that
leaves no symbol has to be found from source: virtual calls, inline code and field
access do not appear in `nm` output.
