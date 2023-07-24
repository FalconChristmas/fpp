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

#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>

#include "../log.h"

#include "MediaOutputBase.h"

MediaOutputBase::MediaOutputBase(void) :
    m_isPlaying(0),
    m_childPID(0) {
    LogDebug(VB_MEDIAOUT, "MediaOutputBase::MediaOutputBase()\n");

    pthread_mutex_init(&m_outputLock, NULL);

    m_childPipe[0] = 0;
    m_childPipe[1] = 0;
}

MediaOutputBase::~MediaOutputBase() {
    LogDebug(VB_MEDIAOUT, "MediaOutputBase::~MediaOutputBase()\n");

    Close();

    pthread_mutex_destroy(&m_outputLock);
}

/*
 *
 */
int MediaOutputBase::Start(int msTime) {
    return 0;
}

/*
 *
 */
int MediaOutputBase::Stop(void) {
    return 0;
}

/*
 *
 */
int MediaOutputBase::Process(void) {
    return 0;
}

/*
 *
 */
int MediaOutputBase::Close(void) {
    LogDebug(VB_MEDIAOUT, "MediaOutputBase::Close\n");

    pthread_mutex_lock(&m_outputLock);

    for (int i = 0; i < 2; i++) {
        if (m_childPipe[i]) {
            close(m_childPipe[i]);
            m_childPipe[i] = 0;
        }
    }

    pthread_mutex_unlock(&m_outputLock);

    return 1;
}

/*
 *
 */
int MediaOutputBase::AdjustSpeed(float masterPos) {
    return 1;
}

/*
 *
 */
void MediaOutputBase::SetVolume(int volume) {
}

/*
 *
 */
int MediaOutputBase::IsPlaying(void) {
    int result = 0;

    pthread_mutex_lock(&m_outputLock);

    if (m_childPID > 0) {
        result = 1;
    }

    pthread_mutex_unlock(&m_outputLock);

    return result;
}

bool MediaOutputBase::isChildRunning() {
    if (m_childPID > 0) {
        int status = 0;
        if (waitpid(m_childPID, &status, WNOHANG)) {
            return false;
        } else {
            return true;
        }
    }
    return false;
}
