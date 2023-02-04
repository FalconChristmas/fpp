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
#include "mediaoutput/mediaoutput.h"

class PlaylistEntryMedia : public PlaylistEntryBase {
public:
    PlaylistEntryMedia(Playlist* playlist, PlaylistEntryBase* parent = NULL);
    virtual ~PlaylistEntryMedia();

    virtual int Init(Json::Value& config) override;

    virtual int PreparePlay();
    virtual int StartPlaying(void) override;
    virtual int Process(void) override;
    virtual int Stop(void) override;

    virtual void Pause() override;
    virtual bool IsPaused() override;
    virtual void Resume() override;

    virtual int HandleSigChild(pid_t pid) override;

    virtual void Dump(void) override;

    virtual Json::Value GetConfig(void) override;
    virtual Json::Value GetMqttStatus(void) override;

    std::string GetMediaName(void) { return m_mediaFilename; }

    virtual uint64_t GetLengthInMS() override;
    virtual uint64_t GetElapsedMS() override;

    int m_status;

    long long m_openTime;
    static int m_openStartDelay;

private:
    int OpenMediaOutput(void);
    int CloseMediaOutput(void);

    unsigned int m_fileSeed;
    int GetFileList(void);
    std::string GetNextRandomFile(void);

    std::string m_mediaFilename;
    std::string m_mediaPrefix;
    std::string m_videoOutput;
    MediaOutputBase* m_mediaOutput;
    pthread_mutex_t m_mediaOutputLock;

    uint64_t m_duration;

    std::string m_fileMode;
    std::vector<std::string> m_files;

    int m_pausedTime;
    MediaOutputStatus m_pausedStatus;
};
