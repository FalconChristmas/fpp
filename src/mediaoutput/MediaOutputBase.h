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

#include <sys/select.h>
#include <pthread.h>
#include <unistd.h>

#include "MediaOutputStatus.h"

#define MEDIAOUTPUTPIPE_READ 0
#define MEDIAOUTPUTPIPE_WRITE 1

#define MEDIAOUTPUTSTATUS_IDLE 0
#define MEDIAOUTPUTSTATUS_PLAYING 1

#define MediaOutputSpeedDecrease -1
#define MediaOutputSpeedNormal 0
#define MediaOutputSpeedIncrease +1

class MediaOutputBase {
public:
    MediaOutputBase();
    virtual ~MediaOutputBase();

    virtual int Start(int msTime = 0);
    virtual int Stop(void);
    virtual int Process(void);
    virtual int AdjustSpeed(float masterPos);
    virtual void SetVolume(int volume);
    virtual int Close(void);

    virtual int IsPlaying(void);

    std::string m_mediaFilename;
    MediaOutputStatus* m_mediaOutputStatus;
    pid_t m_childPID;

protected:
    bool isChildRunning();

    unsigned int m_isPlaying;
    int m_childPipe[2];
    fd_set m_activeFDSet;
    fd_set m_readFDSet;

    pthread_mutex_t m_outputLock;
};
