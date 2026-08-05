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

#include <atomic>
#include "fpp-json-fwd.h"
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "Plugin.h"
class MediaDetails;
class Command;

class PluginManager {
public:
    PluginManager();
    ~PluginManager();
    void init(void);
    void loadUserPlugins();
    void Cleanup();

    bool hasPlugins();

    void mediaCallback(const Json::Value& playlist, const MediaDetails& mediaDetails);
    void playlistCallback(const Json::Value& playlist, const std::string& action, const std::string& section, int item);
    void playlistInserted(const std::string& filename, const int position, int endPosition, bool immediate);
    void multiSyncData(const std::string& pn, uint8_t* data, int len);

    void registerApis();
    void unregisterApis();

    void addControlCallbacks(std::map<int, std::function<bool(int)>>& callbacks);

    void modifySequenceData(int ms, uint8_t* seqData);
    void modifyChannelData(int ms, uint8_t* seqData);

    FPPPlugins::Plugin* findPlugin(const std::string& name, const std::string& shlibName = "");

    // ---- Runtime load/unload -------------------------------------------------
    // Main loop only. Lets an install take effect and an uninstall stop having
    // effect without restarting fppd, which would interrupt a running show.
    //
    // unloadPlugin() detaches the plugin at once, then waits for the readiness
    // predicate shutdown() handed back (see Plugin.h) before destroying it.
    // It dlclose()s the library only for a plugin that declared
    // FPP_PLUGIN_SUPPORTS_UNLOAD (see Plugin.h for what that asserts). Otherwise
    // the mapping is kept until fppd restarts - a few hundred KB, against a
    // crash class that would land in the middle of a show. Either way the plugin
    // object is destroyed, its routes are disarmed and its registrations are
    // withdrawn, so it stops doing anything.
    bool loadPlugin(const std::string& name, std::string& error);
    bool unloadPlugin(const std::string& name, std::string& error);
    bool isPluginLoaded(const std::string& name);

    // Called when a plugin actually produced a ChannelOutput. That object is
    // owned by the output system and may be mid-show, so its plugin cannot be
    // unloaded. Note this is NOT the same as implementing ChannelOutputPlugin:
    // everything deriving from the FPPPlugin convenience class inherits that
    // interface whether or not it ever returns an output.
    void noteChannelOutputCreated(FPPPlugins::ChannelOutputPlugin* plugin);

    static PluginManager INSTANCE;

private:
    // Raise or clear the "FPPOS upgraded, plugins must be reinstalled" warning
    // based on the pluginReinstallNeededAfterOS setting (set at boot after an
    // FPPOS reflash, cleared by the Plugin Manager once a reinstall succeeds).
    void checkPluginReinstallWarning();

    FPPPlugins::Plugin* loadSHLIBPlugin(const std::string& shlibName);
    FPPPlugins::Plugin* loadUserPlugin(const std::string& name);
    void addPlugin(FPPPlugins::Plugin* plugin);

    std::vector<FPPPlugins::Plugin*> mPlugins;
    std::vector<FPPPlugins::PlaylistEventPlugin*> mPlaylistPlugins;
    std::vector<FPPPlugins::ChannelOutputPlugin*> mChannelOutputPlugins;
    std::vector<FPPPlugins::ChannelDataPlugin*> mChannelDataPlugins;
    std::vector<FPPPlugins::APIProviderPlugin*> mAPIProviderPlugins;

    // Brings a newly loaded plugin up to the state the boot sequence would have
    // left it in: HTTP routes registered, control fds handed to the epoll loop.
    void startPlugin(FPPPlugins::Plugin* plugin);
    // Drops a plugin from every registry that holds it, so nothing calls into it.
    void detachPlugin(FPPPlugins::Plugin* plugin);

    // A plugin that has been detached but is still finishing. Held until its
    // readiness predicate says so (or the cap expires), then destroyed and, if
    // it opted in, unmapped.
    struct PendingUnload {
        FPPPlugins::Plugin* plugin = nullptr;
        void* handle = nullptr;
        bool unmap = false;
        bool hadLibrary = false;
        std::string name;
        std::function<bool()> ready;
        long long deadlineMS = 0;
    };
    std::vector<PendingUnload> mPendingUnloads;
    void pollPendingUnload(const std::string& name);
    void finishUnload(PendingUnload& p);

    std::vector<void*> mShlibHandles;
    std::set<std::string> mLoadedUserPlugins;

    // The library each shlib-backed plugin came from, and whether it declared
    // itself safe to unmap (FPP_PLUGIN_SUPPORTS_UNLOAD). mShlibHandles alone
    // cannot say which handle belongs to which plugin.
    struct LoadedLibrary {
        void* handle = nullptr;
        bool supportsUnload = false;
    };
    std::map<std::string, LoadedLibrary> mPluginLibraries;
    // Which file each plugin library path was loaded FROM, so a reload can tell
    // that the file was replaced underneath it. See loadSHLIBPlugin().
    std::map<std::string, ino_t> mLoadedShlibInodes;

    // Which epoll file descriptors each plugin registered, so unloadPlugin() can
    // withdraw them. Attributed by diffing the callback map around each plugin's
    // addControlCallbacks() call rather than by asking plugins to report them -
    // the API predates unloading and cannot be changed without an ABI break.
    // (A plugin may also remove its own fds via EPollManager in shutdown();
    // removing an already-removed fd is harmless.)
    std::map<std::string, std::vector<int>> mPluginControlFds;

    // Plugins that handed a live ChannelOutput to the output system.
    std::set<std::string> mPluginsWithOutputs;

    // Commands FPP registered on a script plugin's behalf from its
    // commands/descriptions.json. CommandManager::removeCommand() only
    // unregisters, so unloading has to delete these too.
    std::map<std::string, std::vector<Command*>> mPluginCommands;
};
