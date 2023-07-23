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

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

namespace httpserver
{
    class webserver;
}
class MediaDetails;
class ChannelOutput;

namespace FPPPlugins
{
    class Plugin {
    public:
        Plugin(const std::string& n);
        virtual ~Plugin() {}

        const std::string& getName() const { return name; }

        // A plugin can call PluginManager::INSTANCE.multiSyncData(pluginName, data, len);
        // with data and that data is multisynced out to all the remotes.  If the plugin
        // is installed and running on the remote, it will get that data in via this method
        virtual void multiSyncData(const uint8_t* data, int len) {}

    protected:
        void reloadSettings();

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

        // Register/Handle API routines
        virtual void registerApis(httpserver::webserver* m_ws) {}
        virtual void unregisterApis(httpserver::webserver* m_ws) {}
        virtual void addControlCallbacks(std::map<int, std::function<bool(int)>>& callbacks) {}
    };
}

class FPPPlugin : public FPPPlugins::Plugin, public FPPPlugins::PlaylistEventPlugin, public FPPPlugins::ChannelOutputPlugin, public FPPPlugins::ChannelDataPlugin, public FPPPlugins::APIProviderPlugin {
public:
    FPPPlugin(const std::string& n) :
        FPPPlugins::Plugin(n) {}
    virtual ~FPPPlugin() {}
};
