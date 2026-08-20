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

#include "fpp-pch.h"

#include "fpp-json.h"

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds

#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>

#include "../CurlManager.h"
#include "../common.h"
#include "../log.h"

#include "Playlist.h"
#include "PlaylistEntryBoth.h"
#include "PlaylistEntryCommand.h"
#include "PlaylistEntryDynamic.h"
#include "PlaylistEntryMedia.h"
#include "PlaylistEntryPause.h"
#include "PlaylistEntryRemap.h"
#include "PlaylistEntryScript.h"
#include "PlaylistEntrySequence.h"
#include "PlaylistEntryURL.h"

/*
 *
 */
PlaylistEntryDynamic::PlaylistEntryDynamic(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_drainQueue(0),
    m_currentEntry(-1) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::PlaylistEntryDynamic()\n");

    m_type = "dynamic";

    static std::atomic<uint64_t> s_dynamicCounter{ 0 };
    m_curlToken = "PlaylistEntryDynamic-" + std::to_string(s_dynamicCounter.fetch_add(1));
}

/*
 *
 */
PlaylistEntryDynamic::~PlaylistEntryDynamic() {
    // Ordering matters for UAF/lifecycle safety:
    //  1) cancelRequests() removes this entry's in-flight handles from
    //     CurlManager and destroys their callbacks (each captures `this`)
    //     WITHOUT invoking them, so no callback can run against a half-torn-down
    //     object. Safe from the dtor because processCurls() and this dtor both
    //     run on the main loop, so no callback is executing right now.
    //  2) only then release the cookie session: once the requests are gone, no
    //     easy handle still points CURLOPT_SHARE at the share, so
    //     curl_share_cleanup() is safe.
    CurlManager::INSTANCE.cancelRequests(m_curlToken);
    CurlManager::INSTANCE.releaseCookieSession(m_curlToken);

    ClearPlaylistEntries();
}

/*
 *
 */
int PlaylistEntryDynamic::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::Init()\n");

    m_subType = config["subType"].asString();

    m_drainQueue = config["drainQueue"].asInt();

    if (config.isMember("pluginHost"))
        m_pluginHost = config["pluginHost"].asString();

    if (m_subType == "file")
        m_data = config["dataFile"].asString();
    else if (m_subType == "plugin")
        m_data = config["pluginName"].asString();
    else if (m_subType == "url")
        m_data = config["url"].asString();

    // The plugin/url HTTP requests go through CurlManager now (see Prep()/
    // ReadFromURL()); there is no longer a per-instance curl handle to set up.
    // Cookie persistence across the plugin's prep/load/started calls is provided
    // by CurlManager's cookie session keyed on m_curlToken.

    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryDynamic::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::StartPlaying()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    if (!IsPrepped() && !Prep()) {
        FinishPlay();
        return 0;
    }

    int res = 0;
    if (m_subType == "command")
        res = ReadFromCommand();
    else if (m_subType == "file")
        res = ReadFromFile();
    else if (m_subType == "plugin")
        res = ReadFromPlugin();
    else if (m_subType == "url")
        res = ReadFromURL(m_data);

    if (!res) {
        FinishPlay();
        return 0;
    }

    m_isPrepped = 0;

    if (m_currentEntry >= 0) {
        m_playlistEntries[m_currentEntry]->StartPlaying();
        Started();
    } else {
        FinishPlay();
        return 0;
    }

    return PlaylistEntryBase::StartPlaying();
    ;
}

/*
 *
 */
int PlaylistEntryDynamic::Process(void) {
    if ((m_currentEntry >= 0) && (m_playlistEntries[m_currentEntry])) {
        m_playlistEntries[m_currentEntry]->Process();
        if (m_playlistEntries[m_currentEntry]->IsFinished()) {
            if ((m_parentPlaylist->getPlaylistStatus() == FPP_STATUS_STOPPING_GRACEFULLY) ||
                (m_parentPlaylist->getPlaylistStatus() == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP)) {
                FinishPlay();
            } else if (m_currentEntry < (m_playlistEntries.size() - 1)) {
                m_currentEntry++;
                m_playlistEntries[m_currentEntry]->StartPlaying();
            } else if (m_drainQueue) {
                // Check for more entries to play, if there are none,
                // then StartPlaying() will call FinishPlay().
                StartPlaying();
            } else {
                FinishPlay();
            }
        }

        return 1;
    }

    return 0;
}

/*
 *
 */
int PlaylistEntryDynamic::Stop(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::Stop()\n");

    if ((m_currentEntry >= 0) && (m_playlistEntries[m_currentEntry])) {
        m_playlistEntries[m_currentEntry]->Stop();
        return 1;
    }

    return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryDynamic::Dump(void) {
    LogDebug(VB_PLAYLIST, "SubType    : %s\n", m_subType.c_str());
    LogDebug(VB_PLAYLIST, "Data       : %s\n", m_data.c_str());
    LogDebug(VB_PLAYLIST, "Drain Queue: %d\n", m_drainQueue);
    LogDebug(VB_PLAYLIST, "Plugin Host: %s\n", m_pluginHost.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryDynamic::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();

    result["subType"] = m_subType;
    result["data"] = m_data;

    if (m_subType == "file") {
        result["dataFile"] = m_data;
    } else if (m_subType == "plugin") {
        result["pluginName"] = m_data;
        result["pluginHost"] = m_pluginHost;
    } else if (m_subType == "url") {
        result["url"] = m_data;
    }

    result["drainQueue"] = m_drainQueue;
    if ((m_currentEntry >= 0) && (m_playlistEntries[m_currentEntry]))
        result["dynamic"] = m_playlistEntries[m_currentEntry]->GetConfig();

    return result;
}
PlaylistEntryBase* PlaylistEntryDynamic::GetCurrentEntry() {
    if ((m_currentEntry >= 0) && (m_currentEntry < m_playlistEntries.size()) && (m_playlistEntries[m_currentEntry])) {
        return m_playlistEntries[m_currentEntry];
    }
    return nullptr;
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromCommand(void) {
    LogDebug(VB_PLAYLIST, "ReadFromCommand: %s\n", m_data.c_str());

    // FIXME, implement this and change return to 1
    return 0;
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromFile(void) {
    LogDebug(VB_PLAYLIST, "ReadFromFile: %s\n", m_data.c_str());

    if (!FileExists(m_data.c_str())) {
        LogErr(VB_PLAYLIST, "Filename %s does not exist\n", m_data.c_str());
        WarningHolder::AddWarningTimeout(60, 33, "Dynamic playlist source file does not exist: " + m_data);
        return 0;
    }

    std::ifstream t(m_data.c_str());
    std::stringstream buffer;

    buffer << t.rdbuf();

    return ReadFromString(buffer.str());
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromPlugin(void) {
    LogDebug(VB_PLAYLIST, "ReadFromPlugin: %s\n", m_data.c_str());

    // The loadNextItem response was fetched (or is being fetched) asynchronously
    // by the prep chain; consume the buffered JSON. EnsurePluginItemReady()
    // returns immediately in the common ahead-of-time case and only blocks
    // (bounded) for the rare cold start / inline-prep case.
    if (!EnsurePluginItemReady()) {
        return 0;
    }

    std::string item;
    {
        std::lock_guard<std::mutex> g(m_pluginMutex);
        item = std::move(m_pluginItem);
        m_pluginItem.clear();
        m_pluginPrep = PluginPrep::Idle; // consumed; the next transition re-preps
    }
    return ReadFromString(item);
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromURL(std::string url) {
    LogDebug(VB_PLAYLIST, "ReadFromURL: %s\n", url.c_str());

    // Only the "url" subtype reaches here now (the plugin subtype consumes its
    // buffered async response in ReadFromPlugin()). A single one-shot GET; it
    // stays synchronous, matching the prior behaviour for this subtype. rc==0
    // means the transfer never got an HTTP response (connect/timeout failure).
    int rc = 0;
    std::string resp = CurlManager::INSTANCE.doGet(url, rc);
    if (rc == 0) {
        LogErr(VB_PLAYLIST, "Dynamic playlist URL request failed (%s): %s\n", url.c_str(), resp.c_str());
        WarningHolder::AddWarningTimeout(60, 32, "Dynamic playlist URL request failed (" + url + "): " + resp);
        return 0;
    }

    return ReadFromString(resp);
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromString(std::string jsonStr) {
    Json::Value root;
    PlaylistEntryBase* playlistEntry = NULL;

    LogDebug(VB_PLAYLIST, "ReadFromString(): String:\n%s\n", jsonStr.c_str());

    if (jsonStr.empty()) {
        LogDebug(VB_PLAYLIST, "Empty string in ReadFromString()\n");
        return 0;
    }

    if (!LoadJsonFromString(jsonStr, root, JsonRoot::Object)) {
        LogErr(VB_PLAYLIST, "Error parsing JSON: %s\n", jsonStr.c_str());
        WarningHolder::AddWarningTimeout(60, 33, "Dynamic playlist data is not valid JSON");
        return 0;
    }

    ClearPlaylistEntries();

    Json::Value entries;
    if (JsonHas(root, "mainPlaylist"))
        entries = root["mainPlaylist"];
    else if (JsonHas(root, "leadIn"))
        entries = root["leadIn"];
    else if (JsonHas(root, "leadOut"))
        entries = root["leadOut"];
    else if (JsonHas(root, "playlistEntries"))
        entries = root["playlistEntries"];
    else
        return 0;

    if (!entries.isArray()) {
        LogErr(VB_PLAYLIST, "Expected a JSON array of dynamic playlist entries, got something else\n");
        WarningHolder::AddWarningTimeout(60, 33, "Dynamic playlist data is not valid JSON");
        return 0;
    }

    for (int i = 0; i < entries.size(); i++) {
        Json::Value pe = entries[i];
        if (!pe.isObject()) {
            LogErr(VB_PLAYLIST, "Invalid dynamic playlist entry: expected a JSON object\n");
            WarningHolder::AddWarningTimeout(60, 33, "Dynamic playlist data is not valid JSON");
            continue;
        }
        playlistEntry = NULL;

        if (pe["type"].asString() == "both")
            playlistEntry = new PlaylistEntryBoth(m_parentPlaylist);
        else if (pe["type"].asString() == "command")
            playlistEntry = new PlaylistEntryCommand(m_parentPlaylist);
        else if (pe["type"].asString() == "media")
            playlistEntry = new PlaylistEntryMedia(m_parentPlaylist);
        else if (pe["type"].asString() == "pause")
            playlistEntry = new PlaylistEntryPause(m_parentPlaylist);
        else if (pe["type"].asString() == "remap")
            playlistEntry = new PlaylistEntryRemap(m_parentPlaylist);
        else if (pe["type"].asString() == "script")
            playlistEntry = new PlaylistEntryScript(m_parentPlaylist);
        else if (pe["type"].asString() == "sequence")
            playlistEntry = new PlaylistEntrySequence(m_parentPlaylist);
        else if (pe["type"].asString() == "url")
            playlistEntry = new PlaylistEntryURL(m_parentPlaylist);
        else {
            LogErr(VB_PLAYLIST, "Invalid Playlist Entry Type: %s\n", pe["type"].asString().c_str());
            WarningHolder::AddWarningTimeout(60, 33, "Dynamic playlist contains an invalid entry type: " + pe["type"].asString());
            ClearPlaylistEntries();
            return 0;
        }

        if (!playlistEntry->Init(pe)) {
            LogErr(VB_PLAYLIST, "Error initializing %s Playlist Entry\n", pe["type"].asString().c_str());
            WarningHolder::AddWarningTimeout(60, 33, "Dynamic playlist entry failed to initialize: " + pe["type"].asString());
            // Not yet pushed into m_playlistEntries (that happens only after a
            // successful Init below), so ClearPlaylistEntries() won't free it.
            delete playlistEntry;
            ClearPlaylistEntries();
            return 0;
        }

        m_playlistEntries.push_back(playlistEntry);
    }

    if (!m_playlistEntries.size()) {
        LogErr(VB_PLAYLIST, "Error, no valid playlistEntries in dynamic data!\n");
        WarningHolder::AddWarningTimeout(60, 33, "Dynamic playlist data contained no valid entries");
        return 0;
    }

    m_currentEntry = 0;

    return 1;
}

/*
 *
 */
int PlaylistEntryDynamic::Started(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::Started()\n");

    int res = 1;
    if (m_subType == "plugin")
        res = StartedPlugin();

    if (!res) {
        return 0;
    }

    return res;
}

/*
 *
 */
int PlaylistEntryDynamic::StartedPlugin(void) {
    // Fire-and-forget notification. The response is discarded and the entry does
    // not wait on it, so it runs async through CurlManager. It uses the same
    // cookie session (m_curlToken) so the plugin sees one PHP session, and it is
    // ordered after loadNextItem because we only get here once the buffered item
    // has been consumed and started. The callback captures nothing (no `this`),
    // so it is safe even if this entry is destroyed before it completes; the
    // owner tag still lets the dtor's cancelRequests() reclaim it.
    std::string url = PluginCommandURL("startedNextItem");
    CurlManager::INSTANCE.addGet(
        url, [](int rc, const std::string& resp) {}, m_curlToken, m_curlToken);
    return 1;
}

/*
 *
 */
int PlaylistEntryDynamic::Prep(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::Prep()\n");

    int res = 1;
    if (m_subType == "plugin")
        res = PrepPlugin();

    if (!res) {
        return 0;
    }

    return PlaylistEntryBase::Prep();
    ;
}

/*
 *
 */
int PlaylistEntryDynamic::PrepPlugin(void) {
    // Idempotent: if a prep/load chain is already running or has already buffered
    // an item (an ahead-of-time Prep() primed it), leave it be. Only fire when
    // Idle or after a prior Failed attempt (an inline-prep retry).
    {
        std::lock_guard<std::mutex> g(m_pluginMutex);
        if (m_pluginPrep == PluginPrep::Running || m_pluginPrep == PluginPrep::Ready) {
            return 1;
        }
        m_pluginPrep = PluginPrep::Running;
        m_pluginItem.clear();
    }

    std::string url = PluginCommandURL("prepNextItem");
    LogDebug(VB_PLAYLIST, "PrepPlugin URL: %s\n", url.c_str());

    // prepNextItem's response is a notification (discarded). Its completion
    // callback chains loadNextItem, guaranteeing the stateful prep->load order
    // over the shared cookie session. rc==0 means the transfer never reached the
    // host (connect/timeout) -> abort the chain and mark Failed.
    CurlManager::INSTANCE.addGet(
        url,
        [this](int rc, const std::string& resp) {
            if (rc == 0) {
                std::lock_guard<std::mutex> g(m_pluginMutex);
                m_pluginPrep = PluginPrep::Failed;
                return;
            }
            FireLoadNextItem();
        },
        m_curlToken, m_curlToken);

    return 1;
}

/*
 *
 */
void PlaylistEntryDynamic::FireLoadNextItem(void) {
    std::string url = PluginCommandURL("loadNextItem");
    LogDebug(VB_PLAYLIST, "FireLoadNextItem URL: %s\n", url.c_str());

    // loadNextItem's response IS the next item's JSON; buffer it for
    // StartPlaying() to consume. Runs on the main-loop thread (processCurls()),
    // same as the consumer, so the lock only guards against the defensive case
    // of a callback landing on another processCurls() call site.
    CurlManager::INSTANCE.addGet(
        url,
        [this](int rc, const std::string& resp) {
            std::lock_guard<std::mutex> g(m_pluginMutex);
            if (rc == 0) {
                m_pluginPrep = PluginPrep::Failed;
            } else {
                m_pluginItem = resp;
                m_pluginPrep = PluginPrep::Ready;
            }
        },
        m_curlToken, m_curlToken);
}

/*
 *
 */
bool PlaylistEntryDynamic::EnsurePluginItemReady(void) {
    // Make sure a prep/load chain is in flight. PrepPlugin() is idempotent: a
    // no-op if one is already running or an item is already buffered, and it
    // (re)fires for the inline-prep / cold-start / post-failure retry case.
    PrepPlugin();

    // Pump CurlManager on this (main-loop) thread until the chain resolves or a
    // bounded deadline passes. processCurls() runs our own callbacks on this
    // thread, so pumping drives our requests to completion; the ahead-of-time
    // case usually already sees Ready and never spins. The deadline caps the
    // rare cold-start block - the old path was fully synchronous (up to ~15s of
    // stacked 5s timeouts), so a bounded wait here is no regression.
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (true) {
        {
            std::lock_guard<std::mutex> g(m_pluginMutex);
            if (m_pluginPrep == PluginPrep::Ready) {
                return true;
            }
            if (m_pluginPrep == PluginPrep::Failed) {
                return false;
            }
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            LogErr(VB_PLAYLIST, "Dynamic playlist plugin prep did not complete in time\n");
            WarningHolder::AddWarningTimeout(60, 32, "Dynamic playlist plugin request timed out");
            return false;
        }
        CurlManager::INSTANCE.processCurls();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

/*
 *
 */
std::string PlaylistEntryDynamic::PluginCommandURL(const char* command) const {
    std::string url = "http://";
    url += (m_pluginHost != "") ? m_pluginHost : "127.0.0.1";
    url += "/plugin.php?plugin=" + m_data + "&page=playlistCallback.php&nopage=1&command=";
    url += command;
    return url;
}

/*
 *
 */
void PlaylistEntryDynamic::ClearPlaylistEntries(void) {
    while (!m_playlistEntries.empty()) {
        delete m_playlistEntries.back();
        m_playlistEntries.pop_back();
    }

    m_currentEntry = -1;
}
