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

#include <string>
#include <vector>

#include <curl/curl.h>

#include "PlaylistEntryBase.h"

class PlaylistEntryDynamic : public PlaylistEntryBase {
public:
    PlaylistEntryDynamic(Playlist* playlist, PlaylistEntryBase* parent = NULL);
    virtual ~PlaylistEntryDynamic();

    virtual int Init(Json::Value& config) override;

    virtual int StartPlaying(void) override;
    virtual int Prep(void) override;
    virtual int Process(void) override;
    virtual int Stop(void) override;

    virtual void Dump(void) override;

    virtual Json::Value GetConfig(void) override;

    PlaylistEntryBase* GetCurrentEntry();
private:
    int ReadFromCommand(void);
    int ReadFromFile(void);
    int ReadFromPlugin(void);
    int ReadFromURL(std::string url);
    int ReadFromString(std::string jsonStr);

    int PrepPlugin(void);

    int Started(void);
    int StartedPlugin(void);

    int ProcessData(void* buffer, size_t size, size_t nmemb);
    void ClearPlaylistEntries(void);

    static size_t write_data(void* ptr, size_t size, size_t nmemb,
                             void* ourpointer);

    std::string m_subType;
    std::string m_data;

    CURL* m_curl;

    int m_drainQueue;
    int m_currentEntry;
    std::vector<PlaylistEntryBase*> m_playlistEntries;

    std::string m_pluginHost;
    std::string m_url;
    std::string m_method;
    std::string m_response;
};
