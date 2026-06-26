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

#include "fpphttp_types.h"

// Increment this when the plugin ABI changes (e.g. virtual method signatures)
#define FPP_PLUGIN_API_VERSION 3

// Plugins compiled with these headers will export their API version.
// weak linkage allows multiple TUs to define this; visibility ensures .so export.
#ifdef __cplusplus
extern "C" {
__attribute__((weak, visibility("default"))) int fpp_plugin_api_version() { return FPP_PLUGIN_API_VERSION; }
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
