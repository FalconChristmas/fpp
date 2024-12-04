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

#include <fstream>

#include "../common.h"
#include "../log.h"

#include "Playlist.h"
#include "PlaylistEntryBoth.h"
#include "PlaylistEntryCommand.h"
#include "PlaylistEntryDynamic.h"
#include "PlaylistEntryEffect.h"
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
    m_curl(NULL),
    m_drainQueue(0),
    m_currentEntry(-1) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::PlaylistEntryDynamic()\n");

    m_type = "dynamic";
}

/*
 *
 */
PlaylistEntryDynamic::~PlaylistEntryDynamic() {
    ClearPlaylistEntries();

    if (m_curl)
        curl_easy_cleanup(m_curl);
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

    if ((m_subType == "plugin") || (m_subType == "url")) {
        CURLcode status;
        m_curl = curl_easy_init();
        if (!m_curl) {
            LogErr(VB_PLAYLIST, "Unable to create curl instance\n");
            return 0;
        }

        curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1);
        status = curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &PlaylistEntryDynamic::write_data);
        if (status != CURLE_OK) {
            LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting write callback function: %s\n", curl_easy_strerror(status));
            return 0;
        }

        status = curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
        if (status != CURLE_OK) {
            LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting class pointer: %s\n", curl_easy_strerror(status));
            return 0;
        }

        status = curl_easy_setopt(m_curl, CURLOPT_COOKIEFILE, "");
        if (status != CURLE_OK) {
            LogErr(VB_PLAYLIST, "curl_easy_setopt() Error initializing cookie jar: %s\n", curl_easy_strerror(status));
            return 0;
        }

        status = curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 5L);
        if (status != CURLE_OK) {
            LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting timeout: %s\n", curl_easy_strerror(status));
            return 0;
        }
    }

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
    if ((m_currentEntry >= 0) && (m_playlistEntries[m_currentEntry]) && (m_currentEntry < m_playlistEntries.size())) {
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

    std::string url;

    url = "http://";
    if (m_pluginHost != "")
        url += m_pluginHost;
    else
        url += "127.0.0.1";
    url += "/plugin.php?plugin=" + m_data + "&page=playlistCallback.php&nopage=1&command=loadNextItem";

    return ReadFromURL(url);
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromURL(std::string url) {
    LogDebug(VB_PLAYLIST, "ReadFromURL: %s\n", url.c_str());

    m_response = "";

    if (!m_curl) {
        LogErr(VB_PLAYLIST, "m_curl is null\n");
        return 0;
    }

    CURLcode status = curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    if (status != CURLE_OK) {
        LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting URL: %s\n", curl_easy_strerror(status));
        return 0;
    }

    status = curl_easy_perform(m_curl);
    if (status != CURLE_OK) {
        LogErr(VB_PLAYLIST, "curl_easy_perform() failed: %s\n", curl_easy_strerror(status));
        return 0;
    }

    return ReadFromString(m_response);
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

    if (!LoadJsonFromString(jsonStr, root)) {
        LogErr(VB_PLAYLIST, "Error parsing JSON: %s\n", jsonStr.c_str());
        return 0;
    }

    ClearPlaylistEntries();

    Json::Value entries;
    if (root.isMember("mainPlaylist"))
        entries = root["mainPlaylist"];
    else if (root.isMember("leadIn"))
        entries = root["leadIn"];
    else if (root.isMember("leadOut"))
        entries = root["leadOut"];
    else if (root.isMember("playlistEntries"))
        entries = root["playlistEntries"];
    else
        return 0;

    for (int i = 0; i < entries.size(); i++) {
        Json::Value pe = entries[i];
        playlistEntry = NULL;

        if (pe["type"].asString() == "both")
            playlistEntry = new PlaylistEntryBoth(m_parentPlaylist);
        else if (pe["type"].asString() == "command")
            playlistEntry = new PlaylistEntryCommand(m_parentPlaylist);
        else if (pe["type"].asString() == "effect")
            playlistEntry = new PlaylistEntryEffect(m_parentPlaylist);
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
            ClearPlaylistEntries();
            return 0;
        }

        if (!playlistEntry->Init(pe)) {
            LogErr(VB_PLAYLIST, "Error initializing %s Playlist Entry\n", pe["type"].asString().c_str());
            ClearPlaylistEntries();
            return 0;
        }

        m_playlistEntries.push_back(playlistEntry);
    }

    if (!m_playlistEntries.size()) {
        LogErr(VB_PLAYLIST, "Error, no valid playlistEntries in dynamic data!\n");
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
    std::string url;

    url = "http://";
    if (m_pluginHost != "")
        url += m_pluginHost;
    else
        url += "127.0.0.1";
    url += "/plugin.php?plugin=" + m_data + "&page=playlistCallback.php&nopage=1&command=startedNextItem";

    CURLcode status = curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    if (status != CURLE_OK) {
        LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting URL: %s\n", curl_easy_strerror(status));
        return 0;
    }

    status = curl_easy_perform(m_curl);
    if (status != CURLE_OK) {
        LogErr(VB_PLAYLIST, "curl_easy_perform returned an error: %d\n", status);
        return 0;
    }

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
    std::string url;

    url = "http://";
    if (m_pluginHost != "")
        url += m_pluginHost;
    else
        url += "127.0.0.1";
    url += "/plugin.php?plugin=" + m_data + "&page=playlistCallback.php&nopage=1&command=prepNextItem";

    LogDebug(VB_PLAYLIST, "URL: %s\n", url.c_str());

    CURLcode status = curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    if (status != CURLE_OK) {
        LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting URL: %s\n", curl_easy_strerror(status));
        return 0;
    }

    status = curl_easy_perform(m_curl);
    if (status != CURLE_OK) {
        LogErr(VB_PLAYLIST, "curl_easy_perform returned an error: %d\n", status);
        return 0;
    }

    return 1;
}

/*
 *
 */
int PlaylistEntryDynamic::ProcessData(void* buffer, size_t size, size_t nmemb) {
    LogDebug(VB_PLAYLIST, "ProcessData( %p, %d, %d)\n", buffer, size, nmemb);

    m_response.append(static_cast<const char*>(buffer), size * nmemb);

    LogDebug(VB_PLAYLIST, "m_response length: %d\n", m_response.size());

    return size * nmemb;
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

/*
 *
 */
size_t PlaylistEntryDynamic::write_data(void* buffer, size_t size, size_t nmemb, void* userp) {
    PlaylistEntryDynamic* peDynamic = (PlaylistEntryDynamic*)userp;

    return static_cast<PlaylistEntryDynamic*>(userp)->ProcessData(buffer, size, nmemb);
}
