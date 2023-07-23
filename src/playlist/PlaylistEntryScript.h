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

#include "PlaylistEntryBase.h"

class PlaylistEntryScript : public PlaylistEntryBase {
public:
    PlaylistEntryScript(Playlist* playlist, PlaylistEntryBase* parent = NULL);
    ~PlaylistEntryScript();

    virtual int Init(Json::Value& config) override;

    virtual int StartPlaying(void) override;
    virtual int Process(void) override;
    virtual int Stop(void) override;

    bool isChildRunning();
    virtual void Dump(void) override;

    virtual Json::Value GetConfig(void) override;
    virtual Json::Value GetMqttStatus(void) override;
    std::string GetScriptName(void) { return m_scriptFilename; }

    virtual uint64_t GetElapsedMS() override;

private:
    std::string m_scriptFilename;
    std::string m_scriptArgs;
    bool m_blocking;
    int m_scriptProcess;
    long long m_startTime;
};
