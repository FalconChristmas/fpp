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
//
// Version 6 (FPP 10.0): FPPPlugins::Plugin gained a virtual shutdown(), which
// shifts every later vtable slot - a plugin built against version 5 headers
// would have FPP call shutdown() and land in whatever its multiSyncData() slot
// held. This is the teardown point plugin unloading needs: a plugin's
// destructor is too late to stop threads, because another thread calling in
// during destruction reads a half-destroyed object.
//
// Version 6 also removed the deprecated registerApis(httpserver::webserver*) /
// unregisterApis(httpserver::webserver*) virtuals and the httpserver:: source
// shims behind them, which shifts vtable slots again. That shim existed to let
// libhttpserver-era plugin source recompile unchanged against FPP 10; no listed
// plugin still needed it, and the version bump forces a rebuild regardless, so
// there was no longer a plugin it could carry across.
//
// Landed alongside it (no ABI impact, but this is the version to build against
// for it): FPPPlugins::registerPluginApi()/unregisterPluginApi() in fpphttp.h.
// Plugins must use those instead of drogon::app().registerHandler(), which
// cannot be undone - drogon has no route removal - and so pins a plugin's .so
// in memory for the life of the process. Registering through FPP keeps the
// route in libfpp and lets a replacement build of the plugin take it over.
#define FPP_PLUGIN_API_VERSION 6

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

// Opt in to being dlclose()d when unloaded. Put FPP_PLUGIN_SUPPORTS_UNLOAD() at
// file scope in exactly one of the plugin's translation units.
//
// Without it FPP still unloads the plugin - routes disarmed, shutdown() called,
// registrations withdrawn, the object destroyed - but keeps the library mapped,
// because it cannot prove nothing still points into it. The macro is a plugin
// asserting that it does not leave anything behind that would outlive the .so:
//
//   - shutdown() stops and JOINS every thread it started, and stops its timers.
//   - Every CurlManager request it issues passes its plugin name as the owner
//     argument, so FPP can cancel the ones still in flight.
//   - Any epoll descriptors it registered are gone (FPP withdraws the ones it
//     added through addControlCallbacks(); anything registered directly with
//     EPollManager is the plugin's own to remove).
//   - It registered HTTP routes through FPPPlugins::registerPluginApi() only.
//   - Anything it handed to a drogon/trantor event loop has finished, or it
//     returns a settling time from shutdown() long enough that it will have.
//     This is the case to think hardest about: runInLoop()/queueInLoop() are
//     obvious, but so is any drogon client object - a WebSocketClient queues ITS
//     handlers, which are the plugin's code, onto a loop whenever a network
//     event arrives, and stopping the client does not close that window because
//     the socket-closed callback is queued right afterwards. Trantor offers no
//     way to enumerate or cancel what is already queued, so there is nothing to
//     wait on: draining every loop in shutdown() was measured to still take a
//     SIGSEGV inside doRunInLoopFuncs() moments after dlclose(). A settling time
//     is what makes that survivable.
//
// Get one of those wrong and the failure is a jump into unmapped memory, so the
// default is to keep the mapping.
#define FPP_PLUGIN_SUPPORTS_UNLOAD()                                        \
    extern "C" __attribute__((visibility("default"))) int fpp_plugin_supports_unload() { \
        return 1;                                                           \
    }

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

        // Called by FPP once the plugin's HTTP routes have been disarmed and
        // before the plugin object is destroyed. Quiesce here: stop threads and
        // join them, stop timers, remove event handlers and commands, close
        // connections.
        //
        // Destructors run too late for this: another thread calling in while the
        // object is being destroyed reads a half-destroyed plugin. Doing it here
        // gives a point where FPP guarantees no further calls in.
        //
        // Return an empty function when teardown is complete on return.
        // Otherwise return a predicate FPP polls on the main loop about once a
        // second, and which returns true when the plugin is finished. FPP
        // detaches the plugin immediately either way - nothing calls into it
        // again and its routes are gone - but waits for the predicate before
        // DESTROYING it and (if the plugin opted into unmapping, see
        // FPP_PLUGIN_SUPPORTS_UNLOAD) closing the library.
        //
        // That split lets shutdown() be "ask everything to stop" with the real
        // teardown in the destructor once it actually has: a media pipeline that
        // unwinds asynchronously reports true when it is done, rather than the
        // caller guessing a duration. Anything arriving in the meantime finds a
        // live, quiesced object instead of freed memory.
        //
        // A plugin that cannot observe when it is finished can return a deadline
        // instead:
        //
        //   long long until = GetTimeMS() + 5000;
        //   return [until]() { return GetTimeMS() >= until; };
        //
        // which is what the drogon/trantor case needs - a stopped WebSocket
        // client can still have its handlers queued onto a loop by a socket
        // event arriving right afterwards, and trantor offers no way to see or
        // cancel what is queued, so there is nothing to poll and only elapsed
        // time helps. Waiting turns a near-certain race into one needing an
        // event seconds late; it narrows the window rather than closing it.
        //
        // The predicate itself lives in the plugin, so FPP destroys it before
        // closing the library. FPP gives up waiting after 60s and tears down
        // anyway, so a predicate that never returns true delays teardown, it
        // does not leak the plugin forever.
        virtual std::function<bool()> shutdown() { return nullptr; }

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

        // Register HTTP routes with FPPPlugins::registerPluginApi() (fpphttp.h),
        // and give them back in unregisterApis() with unregisterPluginApi().
        virtual void registerApis() {}
        virtual void unregisterApis() {}

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
