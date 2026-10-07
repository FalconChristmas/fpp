#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2026 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

// PLUGIN ABI: native plugins compile against this header and are loaded with
// dlopen(), so parts of it are baked into binaries FPP did not build.
//   - AudioSourceRegistry::AudioSource: plugins build it on their own stack
//     and pass it to registerSource(), so its members and their order are
//     fixed.
// Any such change must bump FPP_PLUGIN_API_VERSION in Plugin.h. Without the
// bump, a plugin built against the previous header still loads and silently
// uses the old layout - FPP 10.2 shipped exactly that and crash-looped fppd.
// See .claude/PLUGIN-ABI.md for which plugins use what.

#include <mutex>
#include <string>
#include <vector>

#include "fpp-json-fwd.h"

/**
 * AudioSourceRegistry — registry of PipeWire Audio/Source nodes published
 * by plugins (or core subsystems) so they can be offered as routable
 * inputs in the PipeWire Input Groups (mix bus) UI.
 *
 * A plugin that publishes its own Audio/Source node into the PipeWire
 * graph registers it here (and unregisters on shutdown/disable), the same
 * way plugins register with MultiSync::INSTANCE.  The registry is
 * metadata-only: creating the actual PipeWire node remains the caller's
 * responsibility.  Entries live in fppd memory only — routing configs
 * store the nodeName themselves, so nothing here needs to persist.
 *
 * Exposed via GET /pipewire/audio/plugin-sources on fppd's HTTP server;
 * the web UI merges the list into the "PipeWire Source" member picker.
 */
class AudioSourceRegistry {
public:
    struct AudioSource {
        std::string id;         // unique id, e.g. "fpp-smpte:ltc"
        std::string name;       // human readable label, e.g. "SMPTE LTC Timecode"
        std::string nodeName;   // PipeWire node.name, e.g. "fpp_smpte_ltc"
        std::string plugin;     // owning plugin name, e.g. "fpp-smpte"
        int channels = 1;
        int sampleRate = 48000;
    };

    static AudioSourceRegistry INSTANCE;

    /// Register (or update, matched by id) a source.
    void registerSource(const AudioSource& src);

    /// Remove a source by its unique id.  No-op if not present.
    void unregisterSource(const std::string& id);

    /// Remove every source owned by the given plugin.
    void unregisterPluginSources(const std::string& plugin);

    /// JSON snapshot: { "sources": [ {id, name, nodeName, plugin, channels, sampleRate}, ... ] }
    Json::Value toJson() const;

private:
    mutable std::mutex m_mutex;
    std::vector<AudioSource> m_sources;
};
