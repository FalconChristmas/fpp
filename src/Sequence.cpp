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

#ifndef PLATFORM_OSX
#include <asm-generic/hugetlb_encode.h>
#endif
#include <sys/mman.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <list>
#include <map>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "MultiSync.h"
#include "Player.h"
#include "Plugins.h"
#include "Warnings.h"
#include "common.h"
#include "effects.h"
#include "log.h"
#include "settings.h"
#include "channeloutput/ChannelOutputSetup.h"
#include "channeloutput/channeloutputthread.h"
#include "channeltester/ChannelTester.h"
#include "commands/Commands.h"
#include "mediaoutput/SDLOut.h"
#include "overlays/PixelOverlay.h"
#include "playlist/Playlist.h"

#include "Sequence.h"

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
    m_remoteBlankCount(0),
    m_readThread(nullptr),
    m_lastFrameRead(-1),
    m_doneRead(false),
    m_shuttingDown(false),
    m_lastFrameData(nullptr),
    m_dataProcessed(false),
    m_seqFilename(""),
    m_bridgeData(nullptr),
    m_seqData(nullptr) {
#ifndef PLATFORM_OSX
    m_seqData = (char*)mmap(NULL, FPPD_MAX_CHANNEL_NUM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | HUGETLB_FLAG_ENCODE_2MB, -1, 0);
#endif
    if (m_seqData == nullptr || m_seqData == MAP_FAILED) {
        m_seqData = (char*)mmap(NULL, FPPD_MAX_CHANNEL_NUM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        LogDebug(VB_SEQUENCE, "Not using Huge Pages for Sequence Data   %p\n", m_seqData);
    } else {
        LogDebug(VB_SEQUENCE, "Using Huge Pages for Sequence Data   %p\n", m_seqData);
    }
    memset(m_seqData, 0, FPPD_MAX_CHANNEL_NUM);
    for (int x = 0; x < 4; x++) {
        m_seqData[FPPD_OFF_CHANNEL + x] = 0;
        m_seqData[FPPD_WHITE_CHANNEL] = 0xFF;
    }

    m_blankBetweenSequences = getSettingInt("blankBetweenSequences");
    setBridgePrioritySetting(getSetting("bridgeDataPriority", "Warn If Sequence Running"));

    registerSettingsListener("sequence", "blankBetweenSequences",
                             [this](const std::string& value) {
                                 m_blankBetweenSequences = getSettingInt("blankBetweenSequences");
                             });
    registerSettingsListener("sequence", "bridgeDataPriority",
                             [this](const std::string& value) {
                                 setBridgePrioritySetting(value);
                             });
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
    munmap(m_seqData, FPPD_MAX_CHANNELS);
}

void Sequence::setBridgePrioritySetting(const std::string& value) {
    m_prioritize_sequence_over_bridge = false;
    m_warn_if_bridging = false;
    if (value == "Prioritize Sequence") {
        m_prioritize_sequence_over_bridge = true;
    } else if (value == "Warn If Sequence Running") {
        m_prioritize_sequence_over_bridge = true;
        m_warn_if_bridging = true;
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
    SetThreadName("FPP-ReadFrames");
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
                    // memset(fd->data, 0, maxChanToRead);
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
                        // a skip is in progress, we don't need this frame anymore
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
            // same filename AND we haven't started yet, we can continue
            return 1;
        }
    }

    if (IsSequenceRunning())
        CloseSequenceFile();

    std::unique_lock<std::mutex> lock(frameCacheLock);
    if (m_seqFile) {
        delete m_seqFile;
        m_seqFile = nullptr;
        commandPresets.clear();
        effectsOn.clear();
        effectsOff.clear();
    }

    m_seqStarting = 2;
    m_doneRead = false;
    m_lastFrameRead = -1;
    if (startFrame) {
        m_lastFrameRead = startFrame - 1;
    }
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
    strcpy(tmpFilename, FPP_DIR_SEQUENCE("/" + filename).c_str());

    if (getFPPmode() == REMOTE_MODE)
        CheckForHostSpecificFile(getSetting("HostName").c_str(), tmpFilename);

    if (!FileExists(tmpFilename)) {
        std::string warning = "Sequence file ";
        warning += tmpFilename;
        warning += " does not exist\n";

        if (getFPPmode() == REMOTE_MODE) {
            LogDebug(VB_SEQUENCE, "Sequence file %s does not exist\n", tmpFilename);

            // Look for fallback.fseq instead
            strcpy(tmpFilename, FPP_DIR_SEQUENCE("/fallback.fseq").c_str());

            if (!FileExists(tmpFilename)) {
                LogDebug(VB_SEQUENCE, "Fallback Sequence file %s does not exist\n", tmpFilename);
                WarningHolder::AddWarningTimeout(warning, 60);
                m_seqStarting = 0;
                return 0;

            } else {
                m_seqFilename = "fallback.fseq";
                LogDebug(VB_SEQUENCE, "Playing Fallback Sequence file %s\n", tmpFilename);
            }

        } else {
            LogErr(VB_SEQUENCE, "Sequence file %s does not exist\n", tmpFilename);
            WarningHolder::AddWarningTimeout(warning, 60);
            m_seqStarting = 0;
            return 0;
        }
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

    // start reading frames
    lock.lock();
    m_seqFile = seqFile;
    lock.unlock();
    m_seqStarting = 1; // beyond header, read loop can start reading frames
    frameLoadSignal.notify_all();
    m_seqPaused = 0;
    m_seqSingleStep = 0;
    m_seqSingleStepBack = 0;
    m_dataProcessed = false;
    ProcessVariableHeaders();
    ReadSequenceData(true);
    seqLock.unlock();
    StartChannelOutputThread();

    m_numSeek = 0;
    LogDebug(VB_SEQUENCE, "seqRefreshRate        : %f\n", m_seqRefreshRate);
    LogDebug(VB_SEQUENCE, "seqMSDuration         : %d\n", m_seqMSDuration);
    LogDebug(VB_SEQUENCE, "seqMSRemaining        : %d\n", m_seqMSRemaining);
    return 1;
}
void Sequence::ProcessVariableHeaders() {
    for (auto& vh : m_seqFile->getVariableHeaders()) {
        if (vh.code[0] == 'F') {
            if (vh.code[1] == 'C' || vh.code[1] == 'E') {
                uint8_t* data = (uint8_t*)(&vh.data[0]);
                if (data[0] != 1)
                    continue; // version flag, only understand v1 right now
                uint32_t* uintData = (uint32_t*)&data[1];
                int count = *uintData;
                std::string ips = "";
                if (data[5]) {
                    ips = std::string((const char*)&data[5]);
                    // TODO - process ips to see if we are actually supposed to run these commands/effects
                    //     Not supported in xLights yet
                }
                data += 6 + ips.length();
                for (int x = 0; x < count; x++) {
                    std::string cmd = std::string((const char*)data);
                    data += cmd.length() + 1;
                    uintData = (uint32_t*)data;
                    int cc = uintData[0];
                    data += 4;
                    for (int cc1 = 0; cc1 < cc; cc1++) {
                        uintData = (uint32_t*)data;
                        uint32_t start = uintData[0];
                        uint32_t end = uintData[1];
                        if (vh.code[1] == 'C') {
                            commandPresets[start].push_back(cmd);
                            commandPresets[end].push_back(cmd + "_END");
                        } else {
                            effectsOn[start].push_back(cmd);
                            effectsOff[end].push_back(cmd);
                        }
                        data += 8;
                    }
                }
            }
        }
    }
}

void Sequence::StartSequence() {
    if (!IsSequenceRunning() && m_seqFile) {
        if (multiSync->isMultiSyncEnabled()) {
            ResetChannelOutputFrameNumber();
            multiSync->SendSeqSyncStartPacket(m_seqFilename);
        }
        m_seqStarting = 0;
        SetChannelOutputRefreshRate(m_seqRefreshRate);
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
    if ((sequence->m_seqFilename == filename) || (sequence->m_seqFilename == "fallback.fseq")) {
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
        // Going backwords but frame is cached, we'll push the old frames
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

    m_dataProcessed = false;

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

// Broken per issue #986
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
            // wait up to the step time, if we don't have the frame, bail
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
                // we'll have the read thread discard the frame
                m_lastFrameRead++;
                if (!pastFrameCache.empty()) {
                    // and copy the last frame data
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
            // on a remote, we will get a "stop" and then a "start" a short time later
            // we don't want to blank immediately (unless the master tells us to)
            // so we don't get black blinks on the remote.  We'll wait the equivalent
            // of 5 frames to then blank
            if (m_remoteBlankCount > 5) {
                BlankSequenceData();
            } else {
                m_remoteBlankCount++;
            }
        } else if (!Player::INSTANCE.IsPlaying() && (getFPPmode() & PLAYER_MODE)) {
            // Player, but not playlist running (so not between sequences)
            // yet we are likely outputting something (overlay, etc...)  Need to blank
            BlankSequenceData();
        }
    }
}

void Sequence::ProcessSequenceData(int ms) {
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

    if (ChannelTester::INSTANCE.Testing())
        ChannelTester::INSTANCE.OverlayTestData(m_seqData);

    PluginManager::INSTANCE.modifyChannelData(ms, (uint8_t*)m_seqData);

    PrepareChannelData(m_seqData);
    m_dataProcessed = true;
}

void Sequence::SendSequenceData() {
    if (m_lastFrameData) {
        uint32_t frame = m_lastFrameData->frame;
        if (!commandPresets.empty()) {
            const auto& p = commandPresets.find(frame);
            if (p != commandPresets.end()) {
                std::map<std::string, std::string> keywords({ { "SEQUENCE_NAME", m_seqFilename } });
                for (auto& cmd : p->second) {
                    CommandManager::INSTANCE.TriggerPreset(cmd, keywords);
                }
            }
        }
        if (!effectsOn.empty()) {
            const auto& eop = effectsOn.find(frame);
            if (eop != effectsOn.end()) {
                for (auto& eff : eop->second) {
                    StartEffect(eff, 0, 1);
                }
            }
        }
        if (!effectsOff.empty()) {
            const auto& efp = effectsOff.find(frame);
            if (efp != effectsOff.end()) {
                for (auto& eff : efp->second) {
                    StopEffect(eff);
                }
            }
        }
    }
    SendChannelData(m_seqData);
}

void Sequence::SendBlankingData(void) {
    LogDebug(VB_SEQUENCE, "SendBlankingData()\n");
    std::this_thread::sleep_for(5ms);

    if (multiSync->isMultiSyncEnabled())
        multiSync->SendBlankingDataPacket();

    BlankSequenceData(true);

    if (ChannelOutputThreadIsRunning() && ChannelOutputThreadIsEnabled()) {
        ForceChannelOutputNow();
    } else {
        ProcessSequenceData(0);
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
        std::unique_lock<std::mutex> fclock(frameCacheLock);
        delete m_seqFile;
        m_seqFile = nullptr;
        fclock.unlock();

        std::map<std::string, std::string> keywords;
        keywords["SEQUENCE_NAME"] = m_seqFilename;
        CommandManager::INSTANCE.TriggerPreset("SEQUENCE_STOPPED", keywords);
        commandPresets.clear();
        effectsOn.clear();
        effectsOff.clear();
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
    if (this->IsSequenceRunning()) {
        if (m_warn_if_bridging) {
            WarningHolder::AddWarningTimeout("Received bridging data while sequence is running.", 60);
        }
        if (m_prioritize_sequence_over_bridge) {
            return;
        }
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
