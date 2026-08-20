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

#include "../CurlManager.h"
#include "../log.h"

#include "PlaylistEntryURL.h"

/*
 *
 */
PlaylistEntryURL::PlaylistEntryURL(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryURL::PlaylistEntryURL()\n");

    m_type = "url";
}

/*
 *
 */
PlaylistEntryURL::~PlaylistEntryURL() {
}

/*
 *
 */
int PlaylistEntryURL::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryURL::Init()\n");

    m_method = config["method"].asString();
    m_url = config["url"].asString();

    if (config.isMember("data"))
        m_data = config["data"].asString();

    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryURL::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryURL::StartPlaying()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    std::string repURL = ReplaceMatches(m_url);
    std::string repData;
    if (m_data.size())
        repData = ReplaceMatches(m_data);

    std::string method = m_method.empty() ? "GET" : m_method;

    // Fire-and-forget: the entry does not use the response and finishes the
    // moment the request is dispatched, so the request runs asynchronously
    // through CurlManager (pumped by the main loop) rather than blocking the
    // show for up to the request timeout on the main thread.  The callback
    // captures only the URL string by value, never `this`, so it is safe even
    // if this entry has been destroyed by the time the request completes.
    std::string urlForLog = repURL;
    CurlManager::INSTANCE.add(
        repURL, method, repData, {},
        [urlForLog](int rc, const std::string& resp) {
            if (rc < 200 || rc >= 300) {
                LogErr(VB_PLAYLIST, "Playlist URL request failed (%s): rc=%d\n", urlForLog.c_str(), rc);
                WarningHolder::AddWarningTimeout(60, 32, "Playlist URL request failed (" + urlForLog + ")");
            } else {
                LogDebug(VB_PLAYLIST, "Playlist URL response (%s): %s\n", urlForLog.c_str(), resp.c_str());
            }
        },
        "PlaylistEntryURL");

    PlaylistEntryBase::StartPlaying();

    FinishPlay();

    return 1;
}

/*
 *
 */
int PlaylistEntryURL::Stop(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryURL::Stop()\n");
    return PlaylistEntryBase::Stop();
}

/*
 *
 */
std::string PlaylistEntryURL::ReplaceMatches(std::string in) {
    std::string out = in;

    LogDebug(VB_PLAYLIST, "In: '%s'\n", in.c_str());

    LogDebug(VB_PLAYLIST, "Out: '%s'\n", out.c_str());

    return PlaylistEntryBase::ReplaceMatches(out);
}

/*
 *
 */
void PlaylistEntryURL::Dump(void) {
    LogDebug(VB_PLAYLIST, "URL     : %s\n", m_method.c_str());
    LogDebug(VB_PLAYLIST, "Method  : %s\n", m_url.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryURL::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();

    result["url"] = m_url;
    result["method"] = m_method;

    return result;
}
