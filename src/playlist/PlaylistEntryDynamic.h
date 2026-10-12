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
    int ReadFromString(std::string jsonStr);

    int PrepPlugin(void);
    void FireLoadNextItem(void); // chained from the prepNextItem callback
    int FetchURL(void);
    // Start the first loaded child; false if there is none to start.
    bool BeginEntries(int loaded);

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

    // Plugin and URL sources are fetched over HTTP through CurlManager instead
    // of blocking on curl_easy_perform.  For a plugin the prep -> load pair is
    // fired when the entry starts (or by an explicit Prep()) and chained (load
    // starts from prep's completion callback); the response (loadNextItem's, or
    // the URL's) is buffered here and consumed by Process().  The callbacks run
    // on the main loop (processCurls()), but StartPlaying() can run on an API
    // thread when a command starts the playlist, so m_pluginMutex guards the
    // state below.
    enum class PluginPrep { Idle,
                            Running,
                            Ready,
                            Failed };
    std::mutex m_pluginMutex;
    PluginPrep m_pluginPrep = PluginPrep::Idle;
    std::string m_pluginItem; // JSON from loadNextItem (or the URL), awaiting Process()

    // Plugin and URL sources are fetched without waiting: StartPlaying()
    // starts the request and the entry plays as "loading" until Process()
    // finds the response.  StartPlaying() used to wait for it -- up to 15 s,
    // with the playlist lock held, on whichever thread started the entry --
    // pumping CurlManager there, which also ran other subsystems' curl
    // callbacks off the main loop.
    bool m_loading = false;
    long long m_loadDeadline = 0;

    // Unique per live instance: tags this entry's CurlManager requests (owner,
    // for cancelRequests() in the dtor) and names its cookie session. A
    // monotonic counter, never a reused pointer, so a freed+reallocated instance
    // can never collide with a predecessor's still-in-flight request.
    std::string m_curlToken;
};
