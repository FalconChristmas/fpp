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
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "Plugin.h"
class MediaDetails;

namespace httpserver
{
    class webserver;
}

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

    void registerApis(httpserver::webserver* m_ws);
    void unregisterApis(httpserver::webserver* m_ws);

    void addControlCallbacks(std::map<int, std::function<bool(int)>>& callbacks);

    void modifySequenceData(int ms, uint8_t* seqData);
    void modifyChannelData(int ms, uint8_t* seqData);

    FPPPlugins::Plugin* findPlugin(const std::string& name, const std::string& shlibName = "");

    static PluginManager INSTANCE;

private:
    FPPPlugins::Plugin* loadSHLIBPlugin(const std::string& shlibName);
    FPPPlugins::Plugin* loadUserPlugin(const std::string& name);
    void addPlugin(FPPPlugins::Plugin* plugin);

    std::vector<FPPPlugins::Plugin*> mPlugins;
    std::vector<FPPPlugins::PlaylistEventPlugin*> mPlaylistPlugins;
    std::vector<FPPPlugins::ChannelOutputPlugin*> mChannelOutputPlugins;
    std::vector<FPPPlugins::ChannelDataPlugin*> mChannelDataPlugins;
    std::vector<FPPPlugins::APIProviderPlugin*> mAPIProviderPlugins;

    std::vector<void*> mShlibHandles;
    std::set<std::string> mLoadedUserPlugins;
};
