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

#include <curl/curl.h>

#include "PlaylistEntryBase.h"

class PlaylistEntryURL : public PlaylistEntryBase {
public:
    PlaylistEntryURL(Playlist* playlist, PlaylistEntryBase* parent = NULL);
    virtual ~PlaylistEntryURL();

    virtual int Init(Json::Value& config) override;

    virtual int StartPlaying(void) override;
    virtual int Stop(void) override;

    virtual std::string ReplaceMatches(std::string in) override;

    virtual void Dump(void) override;

    Json::Value GetConfig(void) override;

private:
    int ProcessData(void* buffer, size_t size, size_t nmemb);

    static size_t write_data(void* ptr, size_t size, size_t nmemb,
                             void* ourpointer);

    std::string m_url;
    std::string m_method;
    std::string m_data;
    std::string m_response;

    CURL* m_curl;
};
