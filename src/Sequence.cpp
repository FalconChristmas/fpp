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

#include "fpp-pch.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include "MultiSync.h"
#include "Player.h"
#include "Plugins.h"
#include "effects.h"
#include "fppd.h"
#include "channeloutput/E131.h"
#include "channeloutput/channeloutput.h"
#include "channeloutput/channeloutputthread.h"
#include "overlays/PixelOverlay.h"

using namespace std::literals;
using namespace std::chrono_literals;
using namespace std::literals::chrono_literals;

#include "mediaoutput/SDLOut.h"

#define SEQUENCE_CACHE_FRAMECOUNT 40

Sequence* sequence = NULL;
Sequence::Sequence() :
    m_seqMSDuration(0),
    m_seqMSElapsed(0),
    m_seqMSRemaining(0),
    m_seqFile(nullptr),
    m_seqStarting(0),
    m_seqPaused(0),
    m_seqSingleStep(0),
    m_seqSingleStepBack(0),
    m_seqRefreshRate(20),
    m_seqLastControlValue(0),
    m_remoteBlankCount(0),
    m_readThread(nullptr),
    m_lastFrameRead(-1),
    m_doneRead(false),
    m_shuttingDown(false),
    m_lastFrameData(nullptr),
    m_dataProcessed(false),
    m_seqFilename(""),
    m_bridgeData(nullptr) {
    memset(m_seqData, 0, sizeof(m_seqData));
    for (int x = 0; x < 4; x++) {
        m_seqData[FPPD_OFF_CHANNEL + x] = 0;
        m_seqData[FPPD_WHITE_CHANNEL] = 0xFF;
    }

    m_blankBetweenSequences = getSettingInt("blankBetweenSequences");
    m_prioritize_sequence_over_bridge = false;
    std::string bridgeDataPriority = getSetting("bridgeDataPriority", "Prioritize Bridge");
    if (bridgeDataPriority == "Prioritize Sequence") {
        m_prioritize_sequence_over_bridge = true;
    }
}

Sequence::~Sequence() {
    m_shuttingDown = true;
    frameLoadSignal.notify_all();
    if (m_readThread) {
        m_readThread->join();
        delete m_readThread;
    }
    SetLastFrameData(nullptr);
    clearCaches();
    if (m_seqFile) {
        delete m_seqFile;
    }
    if (m_bridgeData) {
        free(m_bridgeData);
    }
}
void Sequence::clearCaches() {
    while (!frameCache.empty()) {
        if (frameCache.front() != m_lastFrameData)
            delete frameCache.front();
        frameCache.pop_front();
    }
    while (!pastFrameCache.empty()) {
        if (pastFrameCache.front() != m_lastFrameData)
            delete pastFrameCache.front();
        pastFrameCache.pop_front();
    }
}

void Sequence::SetLastFrameData(FSEQFile::FrameData* data) {
    if (m_lastFrameData == data)
        return;

    if (m_lastFrameData) {
        bool found = false;
        for (auto const& i : pastFrameCache) {
            if (i == m_lastFrameData)
                found = true;
        }

        if (!found) {
            for (auto const& i : frameCache) {
                if (i == m_lastFrameData)
                    found = true;
            }
        }

        if (!found)
            delete m_lastFrameData;
    }

    m_lastFrameData = data;
}

/*
 *
 */

void ReadSequenceDataThread(Sequence* sequence) {
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
            uint32_t frame = (m_lastFrameRead + 1);
            if (frame < m_seqFile->getNumFrames()) {
                lock.unlock();

                long long start = GetTimeMS();
                std::unique_lock<std::mutex> readlock(readFileLock);
                long long lockt = GetTimeMS();
                FSEQFile* file = m_seqFile;
                FSEQFile::FrameData* fd = nullptr;
                if (m_doneRead || file == nullptr) {
                    //memset(fd->data, 0, maxChanToRead);
                } else {
                    fd = m_seqFile->getFrame(frame);
                }
                long long unlock = GetTimeMS();
                readlock.unlock();
                long long end = GetTimeMS();
                long long total = end - start;
                if (total > 20 || fd == nullptr) {
                    uint32_t lfr = m_lastFrameRead;
                    int lt = lockt - start;
                    int ul = end - unlock;
                    int gf = unlock - lockt;

                    LogDebug(VB_SEQUENCE, "Problem reading frame %d:   %X    Time: %d ms     Last: %d     Lock: %d   GetFrame: %d   Unlock: %d\n",
                             frame, fd, ((int)total), lfr, lt, gf, ul);
                }

                lock.lock();
                if (fd) {
                    if (m_lastFrameRead == (frame - 1)) {
                        m_lastFrameRead = frame;
                        frameCache.push_back(fd);

                        lock.unlock();
                        frameLoadedSignal.notify_all();
                        std::this_thread::sleep_for(1ms);
                        lock.lock();
                    } else {
                        //a skip is in progress, we don't need this frame anymore
                        delete fd;
                    }
                }
            } else {
                m_doneRead = true;
                lock.unlock();
                frameLoadedSignal.notify_all();
                std::this_thread::sleep_for(1ms);
                lock.lock();
            }
        } else {
            frameLoadSignal.wait_for(lock, 25ms);
        }
    }
}

int Sequence::OpenSequenceFile(const std::string& filename, int startFrame, int startSecond) {
    LogDebug(VB_SEQUENCE, "OpenSequenceFile(%s, %d, %d)\n", filename.c_str(), startFrame, startSecond);

    if (filename == "") {
        LogErr(VB_SEQUENCE, "Empty Sequence Filename!\n", filename.c_str());
        return 0;
    }

    size_t bytesRead = 0;
    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);

    if (m_seqFile) {
        if (m_seqFilename == filename && m_seqStarting) {
            //same filename AND we haven't started yet, we can continue
            return 1;
        }
    }

    if (IsSequenceRunning())
        CloseSequenceFile();

    if (m_seqFile) {
        delete m_seqFile;
        m_seqFile = nullptr;
    }

    m_seqStarting = 2;
    m_doneRead = false;
    m_lastFrameRead = -1;
    if (startFrame) {
        m_lastFrameRead = startFrame - 1;
    }

    std::unique_lock<std::mutex> lock(frameCacheLock);
    clearCaches();
    lock.unlock();

    m_seqPaused = 0;
    m_seqMSDuration = 0;
    m_seqMSElapsed = 0;
    m_seqMSRemaining = 0;
    SetChannelOutputFrameNumber(m_lastFrameRead + 1);
    if (m_readThread == nullptr) {
        m_readThread = new std::thread(ReadSequenceDataThread, this);
    }

    m_seqFilename = filename;

    char tmpFilename[2048];
    unsigned char tmpData[2048];
    strcpy(tmpFilename, FPP_DIR_SEQUENCE);
    strcat(tmpFilename, "/");
    strcat(tmpFilename, filename.c_str());

    if (getFPPmode() == REMOTE_MODE)
        CheckForHostSpecificFile(getSetting("HostName").c_str(), tmpFilename);

    if (!FileExists(tmpFilename)) {
        if (getFPPmode() == REMOTE_MODE)
            LogDebug(VB_SEQUENCE, "Sequence file %s does not exist\n", tmpFilename);
        else
            LogErr(VB_SEQUENCE, "Sequence file %s does not exist\n", tmpFilename);

        m_seqStarting = 0;
        return 0;
    }
    if (multiSync->isMultiSyncEnabled()) {
        seqLock.unlock();
        multiSync->SendSeqOpenPacket(filename);
        seqLock.lock();
    }

    m_seqFile = nullptr;
    FSEQFile* seqFile = FSEQFile::openFSEQFile(tmpFilename);
    if (seqFile == NULL) {
        LogErr(VB_SEQUENCE, "Error opening sequence file: %s. FSEQFile::openFSEQFile returned NULL\n",
               tmpFilename);
        m_seqStarting = 0;
        return 0;
    }

    m_seqStepTime = seqFile->getStepTime();
    m_seqRefreshRate = 1000.0f / m_seqStepTime;

    if (startSecond >= 0) {
        int frame = startSecond * 1000;
        frame /= seqFile->getStepTime();
        m_lastFrameRead = frame - 1;
        if (m_lastFrameRead < -1)
            m_lastFrameRead = -1;
    }

    seqFile->prepareRead(GetOutputRanges(), startFrame < 0 ? 0 : startFrame);
    // Calculate duration
    m_seqMSRemaining = seqFile->getNumFrames() * seqFile->getStepTime();
    m_seqMSDuration = m_seqMSRemaining;
    m_seqMSElapsed = 0;
    SetChannelOutputRefreshRate(m_seqRefreshRate);

    //start reading frames
    m_seqFile = seqFile;
    m_seqStarting = 1; //beyond header, read loop can start reading frames
    frameLoadSignal.notify_all();
    m_seqPaused = 0;
    m_seqSingleStep = 0;
    m_seqSingleStepBack = 0;
    m_dataProcessed = false;
    ReadSequenceData(true);
    seqLock.unlock();
    StartChannelOutputThread();

    m_numSeek = 0;
    LogDebug(VB_SEQUENCE, "seqRefreshRate        : %f\n", m_seqRefreshRate);
    LogDebug(VB_SEQUENCE, "seqMSDuration         : %d\n", m_seqMSDuration);
    LogDebug(VB_SEQUENCE, "seqMSRemaining        : %d\n", m_seqMSRemaining);
    return 1;
}

void Sequence::StartSequence() {
    if (!IsSequenceRunning() && m_seqFile) {
        if (multiSync->isMultiSyncEnabled()) {
            ResetChannelOutputFrameNumber();
            multiSync->SendSeqSyncStartPacket(m_seqFilename);
        }
        m_seqStarting = 0;
        StartChannelOutputThread();
    }

    std::map<std::string, std::string> keywords;
    keywords["SEQUENCE_NAME"] = m_seqFilename;
    CommandManager::INSTANCE.TriggerPreset("SEQUENCE_STARTED", keywords);
}

void Sequence::StartSequence(const std::string& filename, int frameNumber) {
    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if (sequence->m_seqFilename != filename) {
        CloseSequenceFile();
        OpenSequenceFile(filename, frameNumber);
    }
    if (sequence->m_seqFilename == filename) {
        StartSequence();
        if (frameNumber != 0) {
            SeekSequenceFile(frameNumber);
        }
    }
}

void Sequence::SeekSequenceFile(int frameNumber) {
    LogDebug(VB_SEQUENCE, "SeekSequenceFile(%d)\n", frameNumber);

    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if (!IsSequenceRunning()) {
        LogDebug(VB_SEQUENCE, "No sequence is running\n");
        return;
    }
    seqLock.unlock();

    std::unique_lock<std::mutex> lock(frameCacheLock);
    while (!pastFrameCache.empty() && frameNumber >= pastFrameCache.back()->frame) {
        //Going backwords but frame is cached, we'll push the old frames
        frameCache.push_front(pastFrameCache.back());
        pastFrameCache.pop_back();
    }
    while (!frameCache.empty() && frameCache.front()->frame < frameNumber) {
        if (frameCache.front() != m_lastFrameData)
            delete frameCache.front();
        frameCache.pop_front();
    }
    if (!frameCache.empty() && frameNumber < frameCache.front()->frame) {
        clearCaches();
    }
    if (frameCache.empty()) {
        LogDebug(VB_SEQUENCE, "Seeking to %d.   Last read is %d\n", frameNumber, (int)m_lastFrameRead);
        m_lastFrameRead = frameNumber - 1;
        frameLoadSignal.notify_all();

        if ((frameNumber < 100) && (getFPPmode() == REMOTE_MODE)) {
            m_numSeek++;
            if (m_numSeek > 6) {
                WarningHolder::AddWarning("Multiple Frame Skips During Playback - Likely Slow Network");
            }
        }
    }
    lock.unlock();
    frameLoadSignal.notify_all();
}

int Sequence::IsSequenceRunning(void) {
    if (m_seqFile && !m_seqStarting)
        return 1;

    return 0;
}

int Sequence::IsSequenceRunning(const std::string& filename) {
    int result = 0;

    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if (sequence->m_seqFilename == filename)
        result = sequence->IsSequenceRunning();

    return result;
}

void Sequence::BlankSequenceData(bool clearBridge) {
    LogExcess(VB_SEQUENCE, "BlankSequenceData()\n");
    for (auto& a : GetOutputRanges()) {
        memset(&m_seqData[a.first], 0, a.second);
    }
    if (m_bridgeData && clearBridge) {
        for (auto& a : GetOutputRanges()) {
            memset(&m_bridgeData[a.first], 0, a.second);
        }
        std::unique_lock<std::mutex> lock(m_bridgeRangesLock);
        m_bridgeRanges.clear();
    }

    std::unique_lock<std::mutex> lock(frameCacheLock);
    SetLastFrameData(nullptr);
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
            frameLoadSignal.notify_all();
            frameLoadedSignal.wait_for(lock, std::chrono::milliseconds(m_seqStepTime - 1));
        }
        if (!frameCache.empty()) {
            FSEQFile::FrameData* data = frameCache.front();
            frameCache.pop_front();
            if (pastFrameCache.size() > 20) {
                if (pastFrameCache.front() != m_lastFrameData)
                    delete pastFrameCache.front();
                pastFrameCache.pop_front();
            }
            pastFrameCache.push_back(data);
            SetLastFrameData(data);
            lock.unlock();
            frameLoadSignal.notify_all();

            data->readFrame((uint8_t*)m_seqData, FPPD_MAX_CHANNELS);
            SetChannelOutputFrameNumber(data->frame);
            m_seqMSElapsed = data->frame * m_seqStepTime;
            m_seqMSRemaining = m_seqMSDuration - m_seqMSElapsed;
            m_dataProcessed = false;
        } else if (m_doneRead) {
            lock.unlock();
            m_seqMSElapsed = m_seqMSDuration;
            m_seqMSRemaining = 0;
            CloseSequenceFile();
        } else {
            if (m_lastFrameRead > 0) {
                //we'll have the read thread discard the frame
                m_lastFrameRead++;
                if (!pastFrameCache.empty()) {
                    //and copy the last frame data
                    SetLastFrameData(pastFrameCache.back());
                    pastFrameCache.back()->readFrame((uint8_t*)m_seqData, FPPD_MAX_CHANNELS);
                    m_dataProcessed = false;
                }
            }
            lock.unlock();
            frameLoadSignal.notify_all();
        }
    } else {
        if (m_blankBetweenSequences) {
            BlankSequenceData();
        } else if (getFPPmode() == REMOTE_MODE) {
            //on a remote, we will get a "stop" and then a "start" a short time later
            //we don't want to blank immediately (unless the master tells us to)
            //so we don't get black blinks on the remote.  We'll wait the equivalent
            //of 5 frames to then blank
            if (m_remoteBlankCount > 5) {
                BlankSequenceData();
            } else {
                m_remoteBlankCount++;
            }
        } else if (!Player::INSTANCE.IsPlaying() && (getFPPmode() & PLAYER_MODE)) {
            //Player, but not playlist running (so not between sequences)
            //yet we are likely outputting something (overlay, etc...)  Need to blank
            BlankSequenceData();
        }
    }
}

void Sequence::ProcessSequenceData(int ms, int checkControlChannels) {
    static unsigned int controlChannel = (unsigned int)getSettingInt("PresetControlChannel");

    if (m_dataProcessed) {
        // we shouldn't normally be reprocessing the same data, so
        // if we are then see if we can start with a pristine copy
        std::unique_lock<std::mutex> lock(frameCacheLock);
        if (m_lastFrameData)
            m_lastFrameData->readFrame((uint8_t*)m_seqData, FPPD_MAX_CHANNELS);
    }

    std::unique_lock<std::mutex> bridgesLock(m_bridgeRangesLock);
    if (m_bridgeData && !m_bridgeRanges.empty()) {
        // copy the latest bridge data to the sequence data
        uint64_t nt = GetTimeMS();
        std::map<uint32_t, uint32_t> rngs;
        for (auto& a : m_bridgeRanges) {
            BridgeRangeData& rd = a.second;
            auto it = rd.expires.begin();
            uint32_t len = 0;
            while (it != rd.expires.end()) {
                if (it->second < nt) {
                    rd.expires.erase(it);
                    it = rd.expires.begin();
                } else {
                    len = std::max(len, it->first);
                    ++it;
                }
            }
            if (len > 0) {
                rngs[rd.startChannel] = len;
            }
        }
        auto it = m_bridgeRanges.begin();
        while (it != m_bridgeRanges.end()) {
            if (it->second.expires.empty()) {
                m_bridgeRanges.erase(it);
                it = m_bridgeRanges.begin();
            } else {
                ++it;
            }
        }

        uint32_t curStart = 0xFFFFFFFF;
        uint32_t nextStart = 0xFFFFFFFF;
        for (auto& a : rngs) {
            if (curStart == 0xFFFFFFFF) {
                curStart = a.first;
                nextStart = a.first + a.second;
            } else if (a.first > nextStart) {
                memcpy(&m_seqData[curStart], &m_bridgeData[curStart], nextStart - curStart);
                curStart = a.first;
                nextStart = a.first + a.second;
            } else if (a.first == nextStart) {
                nextStart += a.second;
            } else {
                nextStart += a.second - (nextStart - a.first);
            }
        }
        if (curStart != 0xFFFFFFFF) {
            memcpy(&m_seqData[curStart], &m_bridgeData[curStart], nextStart - curStart);
        }
    }
    bridgesLock.unlock();
    PluginManager::INSTANCE.modifySequenceData(ms, (uint8_t*)m_seqData);

    if (IsEffectRunning())
        OverlayEffects(m_seqData);

    if (SDLOutput::IsOverlayingVideo()) {
        SDLOutput::ProcessVideoOverlay(ms);
    }
    if (PixelOverlayManager::INSTANCE.hasActiveOverlays()) {
        PixelOverlayManager::INSTANCE.doOverlays((uint8_t*)m_seqData);
    }

    if (checkControlChannels && !m_dataProcessed && controlChannel) {
        unsigned char thisValue = (unsigned char)m_seqData[controlChannel - 1];

        if (m_seqLastControlValue != thisValue) {
            m_seqLastControlValue = thisValue;

            if (m_seqLastControlValue) {
                CommandManager::INSTANCE.TriggerPreset(m_seqLastControlValue);
            }
        }
    }

    if (ChannelTester::INSTANCE.Testing())
        ChannelTester::INSTANCE.OverlayTestData(m_seqData);

    PluginManager::INSTANCE.modifyChannelData(ms, (uint8_t*)m_seqData);

    PrepareChannelData(m_seqData);
    m_dataProcessed = true;
}

void Sequence::SendSequenceData(void) {
    SendChannelData(m_seqData);
}

void Sequence::SendBlankingData(void) {
    LogDebug(VB_SEQUENCE, "SendBlankingData()\n");
    std::this_thread::sleep_for(5ms);

    if (multiSync->isMultiSyncEnabled())
        multiSync->SendBlankingDataPacket();

    BlankSequenceData(true);

    if (ChannelOutputThreadIsRunning() && ChannelOutputThreadIsEnabled()) {
        m_dataProcessed = false;
        ForceChannelOutputNow();
    } else {
        ProcessSequenceData(0, 0);
        SendSequenceData();
    }
}

void Sequence::CloseIfOpen(const std::string& filename) {
    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if (m_seqFilename == filename)
        CloseSequenceFile();
}

void Sequence::CloseSequenceFile(void) {
    LogDebug(VB_SEQUENCE, "CloseSequenceFile() %s\n", m_seqFilename.c_str());

    if (multiSync->isMultiSyncEnabled())
        multiSync->SendSeqSyncStopPacket(m_seqFilename);

    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);

    std::unique_lock<std::mutex> readLock(readFileLock);
    if (m_seqFile) {
        delete m_seqFile;
        m_seqFile = nullptr;

        std::map<std::string, std::string> keywords;
        keywords["SEQUENCE_NAME"] = m_seqFilename;
        CommandManager::INSTANCE.TriggerPreset("SEQUENCE_STOPPED", keywords);
    }
    readLock.unlock();

    std::unique_lock<std::mutex> lock(frameCacheLock);
    clearCaches();
    m_doneRead = true;
    m_lastFrameRead = -1;
    lock.unlock();
    frameLoadedSignal.notify_all();

    m_seqFilename = "";
    m_seqPaused = 0;

    if ((!IsEffectRunning()) &&
            ((getFPPmode() != REMOTE_MODE) &&
             (Player::INSTANCE.GetStatus() != FPP_STATUS_PLAYLIST_PLAYING)) ||
        (m_blankBetweenSequences)) {
        SendBlankingData();
    }
}

void Sequence::SetBridgeData(uint8_t* data, int startChannel, int len, uint64_t expireMS) {
    if (m_prioritize_sequence_over_bridge && this->IsSequenceRunning() ) {
        return;
    }

    if (!m_bridgeData) {
        m_bridgeData = (uint8_t*)calloc(1, FPPD_MAX_CHANNEL_NUM);
    }
    memcpy(&m_bridgeData[startChannel], data, len);

    std::unique_lock<std::mutex> lock(m_bridgeRangesLock);
    auto& a = m_bridgeRanges[startChannel];
    a.startChannel = startChannel;
    a.expires[len] = expireMS;
    lock.unlock();

    setDataNotProcessed();
}

bool Sequence::hasBridgeData() {
    std::unique_lock<std::mutex> lock(m_bridgeRangesLock);
    return !m_bridgeRanges.empty();
}
