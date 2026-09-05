# Developing a Plugin for FPP

FPP's Plugin Manager reads its list from
[`FalconChristmas/fpp-data/pluginList.json`](https://github.com/FalconChristmas/fpp-data/blob/master/pluginList.json).
Each entry is `[displayName, pluginInfoURL, category]`; `pluginInfoURL` points at a
`pluginInfo.json` in the plugin's own repository, which FPP fetches directly to list,
version-check, and install the plugin.

**Everything about building a plugin lives in the
[fpp-plugin-Template](https://github.com/FalconChristmas/fpp-plugin-Template)
repository, not here:**

- [`PLUGININFO_FORMAT.md`](https://github.com/FalconChristmas/fpp-plugin-Template/blob/master/PLUGININFO_FORMAT.md)
  — the full `pluginInfo.json` field reference (required/optional fields, the
  `versions[]` array, `platforms`, resource hints, the `dependencies` block).
- [`PLUGIN_GUIDELINES.md`](https://github.com/FalconChristmas/fpp-plugin-Template/blob/master/PLUGIN_GUIDELINES.md)
  — rules and conventions for a well-behaved plugin: logging, the install/uninstall
  lifecycle, talking to FPP through its API instead of its internals, dependency
  installation, and UI conventions.
- the template plugin itself — a working skeleton to fork.

For getting your finished plugin **listed** (or later de-listed/retired) in FPP's
Plugin Manager, see the
[fpp-data README](https://github.com/FalconChristmas/fpp-data/blob/master/README.md)
— it covers the submission process, the `pluginList.json` entry format, and how to
request removal.

Keep those as the source of truth; this page only covers the two things that are
about *FPP itself* rather than about writing a plugin.

## How FPP installs a plugin

FPP `git clone`s the `srcURL` from `pluginInfo.json` into the plugin's directory,
then runs `scripts/fpp_install.sh` to set it up. On uninstall it runs
`scripts/fpp_uninstall.sh`.

## Plugin types

- **Script plugin** — PHP or bash scripts invoked by FPP commands, wired up via
  `commands/descriptions.json`. See the
  [template plugin](https://github.com/FalconChristmas/fpp-plugin-Template/) for a
  working example.
- **C++ plugin** — a shared library (`.so`) linked into the `fppd` daemon at
  runtime, subclassing the `Plugin.h` base class. See
  [fpp-brightness](https://github.com/FalconChristmas/fpp-brightness) for a working
  example.

## When FPP rebuilds a C++ plugin for you

If your plugin has a root-level `Makefile`, you generally never need to build it
yourself outside of `scripts/fpp_install.sh` (which should build it once on
install). FPP already rebuilds it for you in the other two cases a stale binary
could otherwise happen:

- **Your plugin is updated on its own** (Plugin Manager "Update"): `upgrade_plugin`
  runs `scripts/fpp_upgrade.sh` if you have one, otherwise falls back to re-running
  `scripts/fpp_install.sh` — either way, your build step runs again.
- **FPP core itself is upgraded**: `compileBinaries()` (`scripts/functions`), called
  from the core upgrade path, loops every directory under `plugins/` that has a
  root `Makefile` and rebuilds it (`make -C <plugin> SRCDIR=$SRCDIR`) *before*
  restarting `fppd`.

Because of this, a `make`/`cmake`/`g++` step in `preStart.sh` or `postStart.sh` is
almost never necessary — those hooks run synchronously on **every** `fppd`
start/stop, so a build step there repeats work already done and just delays
startup every boot for no benefit. See `PLUGIN_GUIDELINES.md` §2.8 in the template
repo for the recommended pattern.

## Publishing a PipeWire audio source from a plugin

A C++ plugin can put its own audio into FPP's mix buses by publishing an
`Audio/Source` node into fppd's PipeWire graph and then telling FPP about it.
Creating the node is the plugin's job; FPP only needs the metadata so the node
can be offered in the **Input/Output Setup → Input Mixing** member picker
(member type "PipeWire Source"). The SMPTE plugin's LTC timecode node
(`fpp_smpte_ltc`) is the reference implementation.

Register with `AudioSourceRegistry::INSTANCE` the same way a plugin registers
with `MultiSync::INSTANCE`:

```cpp
#include "mediaoutput/AudioSourceRegistry.h"

AudioSourceRegistry::AudioSource src;
src.id = "fpp-smpte:ltc";        // unique, "<plugin>:<source>" by convention
src.name = "SMPTE LTC Timecode"; // label shown in the picker
src.nodeName = "fpp_smpte_ltc";  // PW_KEY_NODE_NAME of the node you created
src.plugin = "fpp-smpte";
src.channels = 1;
src.sampleRate = 48000;
AudioSourceRegistry::INSTANCE.registerSource(src);
```

and drop the registration in `shutdown()` (not just the destructor — the
registry outlives an unload):

```cpp
AudioSourceRegistry::INSTANCE.unregisterPluginSources("fpp-smpte");
```

The registry is metadata only and lives in fppd memory; routing configs store
the `nodeName` themselves, so nothing is persisted for you. The list is served
to the UI at `GET /api/pipewire/audio/plugin-sources`.

Things that bite:

- **Connect to fppd's PipeWire, not the session one.** fppd runs its own daemon
  with its own runtime dir — set `PIPEWIRE_RUNTIME_DIR` and `XDG_RUNTIME_DIR` to
  `/run/pipewire-fpp` (with `overwrite = 0`) before `pw_init()`.
- **Advertise the channel layout in the node properties**, not just in the
  format: FPP reads `audio.channels` off `pw-dump` when it builds the loopback
  that patches your node into a mix bus. A mono node that does not say
  `PW_KEY_AUDIO_CHANNELS`/`SPA_KEY_AUDIO_POSITION` gets mixed in as silence.
- **`node.autoconnect = false`, `node.always-process = true`** — FPP does the
  patching; the node must keep running while nothing is linked to it.
- **Input Mixing only exists on the advanced backend.** If `MediaBackend` is not
  `pipewire`, the settings group is hidden and there is nowhere to route your
  source — worth a `LogWarn` so the user knows why their node went nowhere.
- **Guard the include** so your plugin still builds against older cores:
  `#if __has_include("mediaoutput/AudioSourceRegistry.h")`.
