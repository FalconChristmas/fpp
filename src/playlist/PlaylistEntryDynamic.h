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

#include <mutex>
#include <string>
#include "fpp-json-fwd.h"
#include <vector>

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
    void FireLoadNextItem(void);      // chained from the prepNextItem callback
    bool EnsurePluginItemReady(void); // bounded fallback if prep hasn't finished

    int Started(void);
    int StartedPlugin(void);

    std::string PluginCommandURL(const char* command) const;
    void ClearPlaylistEntries(void);

    std::string m_subType;
    std::string m_data;

    int m_drainQueue;
    int m_currentEntry;
    std::vector<PlaylistEntryBase*> m_playlistEntries;

    std::string m_pluginHost;
    std::string m_url;
    std::string m_method;

    // Plugin subtype fetches the next item over HTTP through CurlManager instead
    // of blocking the main loop on curl_easy_perform. The prep -> load pair is
    // fired ahead of time during the previous entry's Prep() window and chained
    // (load starts from prep's completion callback); loadNextItem's response is
    // buffered here so StartPlaying consumes it without waiting. All of this
    // state is touched only on the main-loop thread (Prep, StartPlaying, the
    // dtor, and the CurlManager callbacks driven by processCurls() all run
    // there), so m_pluginMutex is defensive rather than load-bearing; see the
    // threading note in the .cpp.
    enum class PluginPrep { Idle,
                            Running,
                            Ready,
                            Failed };
    std::mutex m_pluginMutex;
    PluginPrep m_pluginPrep = PluginPrep::Idle;
    std::string m_pluginItem; // JSON from loadNextItem, awaiting StartPlaying

    // Unique per live instance: tags this entry's CurlManager requests (owner,
    // for cancelRequests() in the dtor) and names its cookie session. A
    // monotonic counter, never a reused pointer, so a freed+reallocated instance
    // can never collide with a predecessor's still-in-flight request.
    std::string m_curlToken;
};
