#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <functional>
#include <map>
#include <string>

// For the FPPLogger layout fingerprints at the bottom of this file. They live
// here rather than in log.h because this is the header every plugin includes
// textually: log.h usually arrives through the PCH, and a weak definition that
// nothing in the translation unit references is not emitted from there.
#include "log.h"

#include "fpphttp_types.h"

// Increment this when the plugin ABI changes. "ABI" is not just virtual method
// signatures - it is anything that changes the layout of a type a plugin can
// construct, destroy, or subclass. Adding a data member to Command::CommandArg
// counts, because plugins push_back into Command::args with the old sizeof while
// FPP destroys those nodes with the new one; adding a virtual to Command counts,
// because it shifts every later vtable slot.
//
// Version 4 (FPP 10.0): Command::CommandArg gained advanced/help/children/
// toggleStyle/toggleLabel, and Command briefly gained a mid-vtable
// disallowMultisync(). Plugins built against version 3 headers corrupt the heap
// in Command::~Command and misdispatch getDescription() into the plugin's run().
//
// Version 5 (FPP 10.0): Command::CommandArg gained a std::string section,
// inserted between help and children - so it both grew sizeof(CommandArg) and
// shifted children/toggleStyle/toggleLabel. Plugins built against version 4
// headers hit the same heap corruption version 4 was introduced to stop.
//
// Version 5 is intended to be the LAST bump this file needs for Command or
// CommandArg growth: both now carry a unique_ptr to an out-of-line struct
// (Command::Data / CommandArg::Ext, defined in Commands.cpp) and new state goes
// there, where plugins only ever see a pointer. Their sizes are additionally
// published as fingerprint symbols the loader compares, so a slip is refused at
// dlopen rather than corrupting the heap. Bump this only for a change the
// pimpls cannot absorb - a new virtual on Command, or a change to one of the
// frozen members.
//
// "ABI" is also not only about Command. FPPLoggerInstance (log.h) grew a
// crashRingCapture bool, taking it from 28 to 32 bytes on 32-bit ARM, and the
// VB_* macros expand to a member of FPPLogger::INSTANCE - so plugins built
// against the older header resolved VB_PLUGIN 52 bytes short of where libfpp put
// it and handed _LogWrite() a reference into the middle of another facility;
// formatting that facility's name ran strlen() over raw character bytes and
// aborted fppd on the plugin's first log line. That layout is fingerprinted
// below rather than version-bumped, deliberately: a bump would also refuse the
// plugin binaries that have since been rebuilt correctly. The consequence is
// that a plugin built before those fingerprints existed exports nothing to
// compare and is still loaded - the gate covers recurrence, not the binaries
// already in the field.
#define FPP_PLUGIN_API_VERSION 5

// Plugins compiled with these headers will export their API version.
// weak linkage allows multiple TUs to define this; visibility ensures .so export.
//
// Alongside it, fingerprints for the FPPLogger layout described by the LAYOUT
// RULE in log.h - the same second-gate idea as the Command/CommandArg sizes in
// commands/Commands.h, and for the same reason: relying on a human to remember
// the version bump has visibly not worked. Each module computes these from its
// own copy of log.h, so a plugin that disagrees about where VB_* lives is
// refused at dlopen instead of handing _LogWrite() a reference into the middle
// of another facility. The span is measured rather than sizeof()'d because the
// two ways this breaks differ: growing FPPLoggerInstance and inserting a
// facility both move the later VB_* macros, and only the first shows up in a
// sizeof.
#ifdef __cplusplus
extern "C" {
__attribute__((weak, visibility("default"))) int fpp_plugin_api_version() { return FPP_PLUGIN_API_VERSION; }
__attribute__((weak, visibility("default"))) unsigned int fpp_logger_instance_abi_size() {
    return (unsigned int)sizeof(FPPLoggerInstance);
}
__attribute__((weak, visibility("default"))) unsigned int fpp_logger_abi_span() {
    return (unsigned int)((const char*)&FPPLogger::INSTANCE.HTTP - (const char*)&FPPLogger::INSTANCE.General);
}
}
#endif

#include "fpp-json-fwd.h"

class MediaDetails;
class ChannelOutput;

namespace FPPPlugins
{
    class Plugin {
    public:
        Plugin(const std::string& n);
        Plugin(const std::string& n, bool monitorSettings);
        virtual ~Plugin();

        const std::string& getName() const { return name; }

        // A plugin can call PluginManager::INSTANCE.multiSyncData(pluginName, data, len);
        // with data and that data is multisynced out to all the remotes.  If the plugin
        // is installed and running on the remote, it will get that data in via this method
        virtual void multiSyncData(const uint8_t* data, int len) {}

    protected:
        void reloadSettings();
        virtual void settingChanged(const std::string& key, const std::string& value) {}

        std::string name;
        std::map<std::string, std::string> settings;
    };

    class ChannelOutputPlugin {
    public:
        ChannelOutputPlugin() {}
        virtual ~ChannelOutputPlugin() {}

        // A plugin can provide an implementation of a channel output
        virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) { return nullptr; }
    };

    class PlaylistEventPlugin {
    public:
        PlaylistEventPlugin() {}
        virtual ~PlaylistEventPlugin() {}

        // Touch points for various triggers within FPP
        virtual void eventCallback(const char* id, const char* impetus) {}
        virtual void mediaCallback(const Json::Value& playlist, const MediaDetails& mediaDetails) {}
        virtual void playlistCallback(const Json::Value& playlist, const std::string& action, const std::string& section, int item) {}
        virtual void playlistInserted(const std::string& playlist, const int position, int endPosition, bool immediate) {}
    };

    class ChannelDataPlugin {
    public:
        ChannelDataPlugin() {}
        virtual ~ChannelDataPlugin() {}

        // direct access to channel data.
        // modifySequenceData is immedately after data is loaded/recieved (before overlays)
        virtual void modifySequenceData(int ms, uint8_t* seqData) {}
        // modifyChannelData is immediately before sending to outputs (after overlays)
        virtual void modifyChannelData(int ms, uint8_t* seqData) {}
    };

    class APIProviderPlugin {
    public:
        APIProviderPlugin() {}
        virtual ~APIProviderPlugin() {}

        // New API (called by FPP host). Default implementation calls the
        // deprecated webserver* overload below, so old plugin source code
        // that overrides registerApis(webserver*) still works when recompiled.
        virtual void registerApis() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            httpserver::webserver shimWs;
            if (auto* p = dynamic_cast<FPPPlugins::Plugin*>(this))
                shimWs.setPluginName(p->getName());
            registerApis(&shimWs);
#pragma GCC diagnostic pop
        }
        virtual void unregisterApis() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            httpserver::webserver shimWs;
            if (auto* p = dynamic_cast<FPPPlugins::Plugin*>(this))
                shimWs.setPluginName(p->getName());
            unregisterApis(&shimWs);
#pragma GCC diagnostic pop
        }

        // Deprecated: old libhttpserver-based API, kept for source compatibility.
        // Recompile against the new fpphttp.h drogon-based API and override
        // registerApis() (no parameters) instead.
        [[deprecated("Override registerApis() (no parameters) and use drogon::app() or fpphttp.h helpers directly")]]
        virtual void registerApis(httpserver::webserver*) {}
        [[deprecated("Override unregisterApis() (no parameters) instead")]]
        virtual void unregisterApis(httpserver::webserver*) {}

        virtual void addControlCallbacks(std::map<int, std::function<bool(int)>>& callbacks) {}
    };
}

class FPPPlugin : public FPPPlugins::Plugin, public FPPPlugins::PlaylistEventPlugin, public FPPPlugins::ChannelOutputPlugin, public FPPPlugins::ChannelDataPlugin, public FPPPlugins::APIProviderPlugin {
public:
    FPPPlugin(const std::string& n) :
        FPPPlugins::Plugin(n) {}
    FPPPlugin(const std::string& n, bool monitorSettings) :
        FPPPlugins::Plugin(n, monitorSettings) {}
    virtual ~FPPPlugin() {}
};
