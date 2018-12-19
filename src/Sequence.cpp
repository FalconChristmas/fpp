/*
 *   Sequence Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

// This #define must be before any #include's
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>

#include "E131.h"
#include "channeloutputthread.h"
#include "common.h"
#include "events.h"
#include "effects.h"
#include "fpp.h" // for FPPstatus && #define-d status values
#include "fppd.h"
#include "log.h"
#include "MultiSync.h"
#include "PixelOverlay.h"
#include "Sequence.h"
#include "settings.h"
#include <chrono>
using namespace std::literals;
using namespace std::chrono_literals;
using namespace std::literals::chrono_literals;

#include "mediaoutput/SDLOut.h"

Sequence *sequence = NULL;

Sequence::Sequence()
  :
    m_seqDuration(0),
    m_seqSecondsElapsed(0),
    m_seqSecondsRemaining(0),
    m_seqMSRemaining(0),
    m_seqFile(nullptr),
    m_seqStarting(0),
    m_seqPaused(0),
    m_seqSingleStep(0),
    m_seqSingleStepBack(0),
    m_seqRefreshRate(20),
    m_seqLastControlMajor(0),
    m_seqLastControlMinor(0),
    m_remoteBlankCount(0),
    m_readThread(nullptr),
    m_lastFrameRead(-1),
    m_doneRead(false),
    m_shuttingDown(false),
    m_dataProcessed(false)
{
    m_seqFilename[0] = 0;
    memset(m_seqData, 0, sizeof(m_seqData));
}

Sequence::~Sequence()
{
    m_shuttingDown = true;
    frameLoadSignal.notify_all();
    if (m_readThread) {
        m_readThread->join();
        delete m_readThread;
    }
    clearCaches();
    if (m_seqFile) {
        delete m_seqFile;
    }
}
void Sequence::clearCaches() {
    while (!frameCache.empty()) {
        delete frameCache.front();
        frameCache.pop_front();
    }
    while (!pastFrameCache.empty()) {
        delete pastFrameCache.front();
        pastFrameCache.pop_front();
    }
}


/*
 *
 */

void ReadSequenceDataThread(Sequence *sequence) {
    sequence->ReadFramesLoop();
}
void Sequence::ReadFramesLoop() {
    std::unique_lock<std::mutex> lock(frameCacheLock);
    while (true) {
        if (m_shuttingDown) {
            return;
        }
        int cacheSize = frameCache.size();
        if (frameCache.size() < SEQUENCE_CACHE_FRAMECOUNT && m_seqStarting < 2 && m_seqFile && !m_doneRead) {
            uint64_t frame = m_lastFrameRead + 1;
            if (frame < m_seqFile->m_seqNumFrames) {
                lock.unlock();
                
                std::unique_lock<std::mutex> readlock(readFileLock);
                FSEQFile *file = m_seqFile;
                FrameData *fd = nullptr;
                if (m_doneRead || file == nullptr) {
                    //memset(fd->data, 0, maxChanToRead);
                } else {
                    fd = m_seqFile->getFrame(frame);
                }
                readlock.unlock();
                
                lock.lock();
                if (fd) {
                    if (m_lastFrameRead == (frame - 1)) {
                        m_lastFrameRead = frame;
                        frameCache.push_back(fd);

                        lock.unlock();
                        frameLoadedSignal.notify_all();
                        std::this_thread::sleep_for(5ms);
                        lock.lock();
                    } else {
                        //a skip is in progress, we don't need this frame anymore
                        delete fd;
                    }
                }
            } else {
                frameLoadedSignal.notify_all();
                m_doneRead = true;
            }
        } else {
            frameLoadSignal.wait_for(lock, 25ms);
        }
    }
}

int Sequence::OpenSequenceFile(const char *filename, int startFrame, int startSecond) {
    LogDebug(VB_SEQUENCE, "OpenSequenceFile(%s, %d, %d)\n", filename, startFrame, startSecond);

    if (!filename || !filename[0]) {
        LogErr(VB_SEQUENCE, "Empty Sequence Filename!\n", filename);
        return 0;
    }

    
    size_t bytesRead = 0;
    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);

    if (IsSequenceRunning())
        CloseSequenceFile();

    m_seqStarting = 2;
    m_doneRead = false;
    m_lastFrameRead = -1;
    if (startFrame) {
        m_lastFrameRead = startFrame - 1;
    }

    std::unique_lock<std::mutex> lock(frameCacheLock);
    clearCaches();
    lock.unlock();
    
    m_seqPaused   = 0;
    m_seqDuration = 0;
    m_seqSecondsElapsed = 0;
    m_seqSecondsRemaining = 0;
    SetChannelOutputFrameNumber(m_lastFrameRead + 1);
    if (m_readThread == nullptr) {
        m_readThread = new std::thread(ReadSequenceDataThread, this);
    }

    strcpy(m_seqFilename, filename);

    char tmpFilename[2048];
    unsigned char tmpData[2048];
    strcpy(tmpFilename,(const char *)getSequenceDirectory());
    strcat(tmpFilename,"/");
    strcat(tmpFilename, filename);

    if (getFPPmode() == REMOTE_MODE)
        CheckForHostSpecificFile(getSetting("HostName"), tmpFilename);

    if (!FileExists(tmpFilename)) {
        if (getFPPmode() == REMOTE_MODE)
            LogDebug(VB_SEQUENCE, "Sequence file %s does not exist\n", tmpFilename);
        else
            LogErr(VB_SEQUENCE, "Sequence file %s does not exist\n", tmpFilename);

        m_seqStarting = 0;
        return 0;
    }
    
    m_seqFile = nullptr;
    FSEQFile *seqFile = FSEQFile::openFSEQFile(tmpFilename);
    if (seqFile == NULL) {
        LogErr(VB_SEQUENCE, "Error opening sequence file: %s. FSEQFile::openFSEQFile returned NULL\n",
            tmpFilename);
        m_seqStarting = 0;
        return 0;
    }

    if (getFPPmode() == MASTER_MODE) {
        seqLock.unlock();
        multiSync->SendSeqSyncStartPacket(filename);

        // Give the remotes a head start spining up so they are ready
        std::this_thread::sleep_for(10ms);
        seqLock.lock();
    }

    m_seqStepTime = seqFile->m_seqStepTime;
    m_seqRefreshRate = 1000 / m_seqStepTime;
    
    if (startSecond >= 0) {
        int frame = startSecond * 1000;
        frame /= seqFile->m_seqStepTime;
        m_lastFrameRead = frame - 1;
        if (m_lastFrameRead < -1) m_lastFrameRead = -1;
    }

    seqFile->prepareRead(GetOutputRanges());
    // Calculate duration
    m_seqMSRemaining = seqFile->m_seqNumFrames * seqFile->m_seqStepTime;
    m_seqDuration = m_seqMSRemaining;
    m_seqDuration /= 1000;
    m_seqSecondsRemaining = m_seqDuration;
    SetChannelOutputRefreshRate(m_seqRefreshRate);
    
    
    //start reading frames
    m_seqFile = seqFile;
    m_seqStarting = 1;  //beyond header, read loop can start reading frames
    frameLoadSignal.notify_all();
    m_seqStarting = 0;
    m_seqPaused = 0;
    m_seqSingleStep = 0;
    m_seqSingleStepBack = 0;
    m_dataProcessed = false;
    ReadSequenceData(true);
    seqLock.unlock();
    StartChannelOutputThread();


    LogDebug(VB_SEQUENCE, "seqRefreshRate        : %d\n", m_seqRefreshRate);
    LogDebug(VB_SEQUENCE, "seqFileSize           : %lld\n", seqFile->m_seqFileSize);
    LogDebug(VB_SEQUENCE, "seqDuration           : %d\n", m_seqDuration);
    LogDebug(VB_SEQUENCE, "seqMSRemaining        : %d\n", m_seqMSRemaining);
    return 1;
}

int Sequence::SeekSequenceFile(int frameNumber) {
    LogDebug(VB_SEQUENCE, "SeekSequenceFile(%d)\n", frameNumber);

    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if (!IsSequenceRunning()) {
        LogErr(VB_SEQUENCE, "No sequence is running\n");
        return 0;
    }
    seqLock.unlock();
    
    std::unique_lock<std::mutex> lock(frameCacheLock);
    while (!pastFrameCache.empty()
        && frameNumber >= pastFrameCache.back()->frame) {
        //Going backwords but frame is cached, we'll push the old frames
        frameCache.push_front(pastFrameCache.back());
        pastFrameCache.pop_back();
    }
    while (!frameCache.empty() && frameCache.front()->frame < frameNumber) {
        delete frameCache.front();
        frameCache.pop_front();
    }
    if (!frameCache.empty() && frameNumber < frameCache.front()->frame) {
        clearCaches();
    }
    LogDebug(VB_SEQUENCE, "Seeking to %d.   Last read is %d\n", frameNumber, (int)m_lastFrameRead);
    if (frameCache.empty()) {
        m_lastFrameRead = frameNumber - 1;
        frameLoadSignal.notify_all();
    }
    lock.unlock();
    frameLoadSignal.notify_all();
}


char *Sequence::CurrentSequenceFilename(void) {
    return m_seqFilename;
}

int Sequence::IsSequenceRunning(void) {
    if (m_seqFile && !m_seqStarting)
        return 1;

    return 0;
}

int Sequence::IsSequenceRunning(char *filename) {
    int result = 0;

    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if ((!strcmp(sequence->m_seqFilename, filename)) && m_seqFile)
        result = 1;

    return result;
}

void Sequence::BlankSequenceData(void) {
    memset(m_seqData, 0, sizeof(m_seqData));
}

int Sequence::SequenceIsPaused(void) {
    return m_seqPaused;
}

void Sequence::ToggleSequencePause(void) {
    if (m_seqPaused)
        m_seqPaused = 0;
    else
        m_seqPaused = 1;
}

void Sequence::SingleStepSequence(void) {
    m_seqSingleStep = 1;
}

void Sequence::SingleStepSequenceBack(void) {
    m_seqSingleStepBack = 1;
}

void Sequence::ReadSequenceData(bool forceFirstFrame) {
    LogExcess(VB_SEQUENCE, "ReadSequenceData()\n");
    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if (!forceFirstFrame && m_seqStarting) {
        return;
    }

    if ((IsSequenceRunning()) && (m_seqPaused)) {
        if (m_seqSingleStep) {
            m_seqSingleStep = 0;
        } else if (m_seqSingleStepBack) {
            m_seqSingleStepBack = 0;
            std::unique_lock<std::mutex> lock(frameCacheLock);
            if (!pastFrameCache.empty()) {
                frameCache.push_front(pastFrameCache.back());
                pastFrameCache.pop_back();
            } else if (frameCache.empty()) {
                m_lastFrameRead = -1;
                frameLoadSignal.notify_all();
            } else {
                int f = frameCache.front()->frame - 1;
                m_lastFrameRead = f - 1;
                if (m_lastFrameRead < -1) {
                    m_lastFrameRead = -1;
                }
                clearCaches();
                frameLoadSignal.notify_all();
            }
        } else {
            return;
        }
    }

    if (forceFirstFrame || IsSequenceRunning()) {
        m_remoteBlankCount = 0;

        std::unique_lock<std::mutex> lock(frameCacheLock);
        if (frameCache.empty() && !m_doneRead) {
            //wait up to the step time, if we don't have the frame, bail
            frameLoadedSignal.wait_for(lock, std::chrono::milliseconds(m_seqStepTime - 1));
        }
        if (!frameCache.empty()) {
            FrameData *data = frameCache.front();
            frameCache.pop_front();
            if (pastFrameCache.size() > 5) {
                delete pastFrameCache.front();
                pastFrameCache.pop_front();
            }
            pastFrameCache.push_back(data);
            lock.unlock();
            frameLoadSignal.notify_all();
            
            data->readFrame((uint8_t*)m_seqData);
            SetChannelOutputFrameNumber(data->frame);
            m_seqSecondsElapsed = data->frame * m_seqStepTime;
            m_seqSecondsElapsed /= 1000;
            m_seqSecondsRemaining = m_seqDuration - m_seqSecondsElapsed;
            m_dataProcessed = false;
        } else if (m_doneRead) {
            lock.unlock();
            m_seqSecondsElapsed = m_seqDuration;
            m_seqSecondsRemaining = m_seqDuration - m_seqSecondsElapsed;
            CloseSequenceFile();
        } else {
            if (m_lastFrameRead > 0) {
                //we'll have the read thread discard the frame
                m_lastFrameRead++;
                if (!pastFrameCache.empty()) {
                    //and copy the last frame data
                    pastFrameCache.back()->readFrame((uint8_t*)m_seqData);
                    m_dataProcessed = false;
                }
            }
            lock.unlock();
            frameLoadSignal.notify_all();
        }
    } else {
        if (getFPPmode() != REMOTE_MODE || getSettingInt("blankBetweenSequences")) {
            BlankSequenceData();
        } else {
            //on a remote, we will get a "stop" and then a "start" a short time later
            //we don't want to blank immediately (unless the master tells us to)
            //so we don't get black blinks on the remote.  We'll wait the equivalent
            //of 5 frames to then blank
            if (m_remoteBlankCount > 5) {
                BlankSequenceData();
            } else {
                m_remoteBlankCount++;
            }
        }
    }
}

void Sequence::ProcessSequenceData(int ms, int checkControlChannels) {
    if (IsEffectRunning())
        OverlayEffects(m_seqData);

    if (SDLOutput::IsOverlayingVideo()) {
        SDLOutput::ProcessVideoOverlay(ms);
    }
    if (UsingMemoryMapInput())
        OverlayMemoryMap(m_seqData);

    if (checkControlChannels && getControlMajor() && getControlMinor())
    {
        char thisMajor = NormalizeControlValue(m_seqData[getControlMajor()-1]);
        char thisMinor = NormalizeControlValue(m_seqData[getControlMinor()-1]);

        if ((m_seqLastControlMajor != thisMajor) ||
            (m_seqLastControlMinor != thisMinor))
        {
            m_seqLastControlMajor = thisMajor;
            m_seqLastControlMinor = thisMinor;

            if (m_seqLastControlMajor && m_seqLastControlMinor)
                TriggerEvent(m_seqLastControlMajor, m_seqLastControlMinor);
        }
    }

    if (channelTester->Testing())
        channelTester->OverlayTestData(m_seqData);
    
    PrepareChannelData(m_seqData);
    m_dataProcessed = true;
}

void Sequence::SendSequenceData(void) {
    SendChannelData(m_seqData);
}

void Sequence::SendBlankingData(void) {
    LogDebug(VB_SEQUENCE, "SendBlankingData()\n");
    std::this_thread::sleep_for(5ms);

    if (getFPPmode() == MASTER_MODE)
        multiSync->SendBlankingDataPacket();

    BlankSequenceData();
    ProcessSequenceData(0, 0);
    SendSequenceData();
}

void Sequence::CloseIfOpen(char *filename) {
    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if (!strcmp(m_seqFilename, filename))
        CloseSequenceFile();
}

void Sequence::CloseSequenceFile(void) {
    LogDebug(VB_SEQUENCE, "CloseSequenceFile() %s\n", m_seqFilename);

    if (getFPPmode() == MASTER_MODE)
        multiSync->SendSeqSyncStopPacket(m_seqFilename);

    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);

    std::unique_lock<std::mutex> readLock(readFileLock);
    if (m_seqFile) {
        delete m_seqFile;
        m_seqFile = nullptr;
    }
    readLock.unlock();
    
    std::unique_lock<std::mutex> lock(frameCacheLock);
    clearCaches();
    m_doneRead = true;
    m_lastFrameRead = -1;
    lock.unlock();
    frameLoadedSignal.notify_all();
    
    m_seqFilename[0] = '\0';
    m_seqPaused = 0;

    if ((!IsEffectRunning()) &&
        ((getFPPmode() != REMOTE_MODE) &&
         (FPPstatus != FPP_STATUS_PLAYLIST_PLAYING)) ||
        (getSettingInt("blankBetweenSequences")))
        SendBlankingData();
}

/*
 * Normalize control channel values into buckets
 */
char Sequence::NormalizeControlValue(char in) {
    char result = (char)(((unsigned char)in + 5) / 10);

    if (result == 26)
        return 25;

    return result;
}

