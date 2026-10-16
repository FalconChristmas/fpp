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
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "MultiSync.h"
#include "Player.h"
#include "Timers.h"
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
#include "mediaoutput/GStreamerOut.h"
#include "overlays/PixelOverlay.h"
#include "playlist/Playlist.h"

#include "Sequence.h"

using namespace std::literals;
using namespace std::chrono_literals;
using namespace std::literals::chrono_literals;

#define SEQUENCE_CACHE_FRAMECOUNT 40

Sequence* sequence = NULL;
// The pause and single-step flags are set by commands on the main loop and
// read by the output thread.  atomic_ref rather than atomic members: this
// class's layout is compiled into plugins (see Sequence.h).
static std::atomic_ref<int> seqFlag(int& flag) {
    return std::atomic_ref<int>(flag);
}

Sequence::Sequence() :
    m_seqMSDuration(0),
    m_seqMSElapsed(0),
    m_seqMSRemaining(0),
    m_seqFile(nullptr),
    m_seqStarting(0),
    m_seqPaused(0),
    m_seqStepTime(50),
    m_seqSingleStep(0),
    m_seqSingleStepBack(0),
    m_seqRefreshRate(20),
    m_remoteBlankCount(0),
    m_numSeek(0),
    m_readThread(nullptr),
    m_lastFrameRead(-1),
    m_doneRead(false),
    m_shuttingDown(false),
    m_lastFrameData(nullptr),
    m_dataProcessed(false),
    m_seqFilename(""),
    // Allocated here, not on the first bridged packet: the receive thread
    // allocating it while the output thread tested the pointer was a race.
    // calloc'd pages cost nothing until something is written to them.
    m_bridgeData((uint8_t*)calloc(1, FPPD_MAX_CHANNEL_NUM)),
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
    setSeqFile(nullptr);
    if (m_bridgeData) {
        free(m_bridgeData);
    }
    munmap(m_seqData, FPPD_MAX_CHANNEL_NUM); // the size mmap()ed in the constructor
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
    m_lastFrameNumber = data ? (int64_t)data->frame : -1;
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
        std::shared_ptr<FSEQFile> seqFilePtr = m_seqFile.load();
        if (frameCache.size() < SEQUENCE_CACHE_FRAMECOUNT && m_seqStarting < 2 && seqFilePtr && !m_doneRead) {
            uint32_t frame = (m_lastFrameRead + 1);
            if (frame < seqFilePtr->getNumFrames()) {
                lock.unlock();

                long long start = GetTimeMS();
                std::unique_lock<std::mutex> readlock(readFileLock);
                long long lockt = GetTimeMS();
                std::shared_ptr<FSEQFile> file = m_seqFile.load();
                FSEQFile::FrameData* fd = nullptr;
                if (m_doneRead || file == nullptr) {
                    // memset(fd->data, 0, maxChanToRead);
                } else {
                    fd = file->getFrame(frame);
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

inline void checkForReplacementFile(const std::string &fname) {
    std::string replaceFileName = fname + ".replace";
    if (FileExists(replaceFileName)) {
        unlink(fname.c_str());
        rename(replaceFileName.c_str(), fname.c_str());
    }
}

// Warning ID / lifetime for "sequence file does not exist".  The warning is
// re-armed on every sync packet for the missing file, so this is the grace
// period after the player goes quiet (or after it sends a stop, which closes
// the sequence and clears the warning outright), not the total display time.
static constexpr int MISSING_SEQUENCE_WARNING_ID = 25;
static constexpr int MISSING_SEQUENCE_WARNING_TIMEOUT = 30;

// Sequence names already reported missing or invalid, so a playlist that keeps
// coming round to them (or a master that keeps naming one in sync packets)
// logs each once instead of every time: a repeating playlist of missing
// sequences wrote some 150 KB of log every 5 s.  A name leaves the set when
// it opens.  File-static (guarded by m_sequenceLock) because Sequence is
// plugin-visible.
static std::set<std::string> reportedMissingSequences;

static bool FirstReportOfMissing(const std::string& name) {
    if (reportedMissingSequences.size() > 256) {
        reportedMissingSequences.clear();
    }
    return reportedMissingSequences.insert(name).second;
}

void Sequence::SetMissingSequenceWarning(const std::string& filename, const std::string& warning) {
    ClearMissingSequenceWarning();
    m_missingSeqFilename = filename;
    m_missingSeqWarning = warning;
    WarningHolder::AddWarningTimeout(MISSING_SEQUENCE_WARNING_TIMEOUT, MISSING_SEQUENCE_WARNING_ID, warning);
}

void Sequence::RefreshMissingSequenceWarning() {
    if (m_missingSeqWarning.empty()) {
        return;
    }
    // AddWarningTimeout only ever pushes an existing warning's timeout later,
    // so this keeps it up for as long as the sync packets keep arriving.
    WarningHolder::AddWarningTimeout(MISSING_SEQUENCE_WARNING_TIMEOUT, MISSING_SEQUENCE_WARNING_ID, m_missingSeqWarning);
}

void Sequence::ClearMissingSequenceWarning() {
    if (m_missingSeqWarning.empty()) {
        return;
    }
    WarningHolder::RemoveWarning(MISSING_SEQUENCE_WARNING_ID, m_missingSeqWarning);
    m_missingSeqWarning.clear();
    m_missingSeqFilename.clear();
}

int Sequence::OpenSequenceFile(const std::string& filename, int startFrame, int startSecond) {
    LogDebug(VB_SEQUENCE, "OpenSequenceFile(%s, %d, %d)\n", filename.c_str(), startFrame, startSecond);

    if (filename == "") {
        LogErr(VB_SEQUENCE, "Empty Sequence Filename!\n", filename.c_str());
        return 0;
    }

    // Sequence names are flat files inside the sequences directory -- they
    // arrive from playlists, play commands, and multisync sync packets, so a
    // rogue remote or crafted playlist must not escape via ../ (which would
    // reach checkForReplacementFile()'s unlink/rename below), pick absolute
    // paths, or smuggle NULs past the c_str() calls. Reject up front, before
    // any teardown, the same way empty names are rejected.
    if (filename.find_first_of("/\\") != std::string::npos ||
        filename.find("..") != std::string::npos ||
        filename.find('\0') != std::string::npos) {
        // The missing-sequence warning state is guarded by m_sequenceLock.
        // Leave m_seqStarting alone: a sequence that is open but not yet
        // started must not be flipped to "running" by a rejected request.
        std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
        if (FirstReportOfMissing(filename)) {
            LogErr(VB_SEQUENCE, "Invalid Sequence Filename '%s'\n", filename.c_str());
        } else {
            LogDebug(VB_SEQUENCE, "Invalid Sequence Filename '%s'\n", filename.c_str());
        }
        SetMissingSequenceWarning(filename, "Invalid sequence filename " + filename + "\n");
        return 0;
    }

    size_t bytesRead = 0;
    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    m_endedSeqFilename.clear();
    const uint64_t openGeneration = ++m_openGeneration;

    if (m_seqFile.load()) {
        if (m_seqFilename == filename && m_seqStarting) {
            // same filename AND we haven't started yet, we can continue
            return 1;
        }
    }

    if (IsSequenceRunning())
        CloseSequenceFile();

    // Tear down any existing m_seqFile.  m_seqFile is an atomic<shared_ptr<FSEQFile>>, so
    // resetting it here cannot free the file out from under an in-flight getFrame() in the
    // ReadFramesLoop thread: that thread holds its own shared_ptr copy and the FSEQFile is
    // only destroyed once the last holder releases it.
    std::unique_lock<std::mutex> readLock(readFileLock);
    std::unique_lock<std::mutex> lock(frameCacheLock);
    if (std::shared_ptr<FSEQFile> old = m_seqFile.load()) {
        std::string fn = old->getFilename();
        setSeqFile(nullptr);
        ForgetTriggers();
        checkForReplacementFile(fn);
    }

    m_seqStarting = 2;
    m_doneRead = false;
    m_lastFrameRead = -1;
    if (startFrame) {
        m_lastFrameRead = startFrame - 1;
    }
    clearCaches();
    lock.unlock();
    readLock.unlock();

    seqFlag(m_seqPaused).store(0);
    m_seqMSDuration = 0;
    m_seqMSElapsed = 0;
    m_seqMSRemaining = 0;
    SetChannelOutputFrameNumber(m_lastFrameRead + 1);
    if (m_readThread == nullptr) {
        m_readThread = new std::thread(ReadSequenceDataThread, this);
    }

    {
        std::unique_lock<std::mutex> nameLock(m_seqFilenameLock);
        m_seqFilename = filename;
    }
    m_seqSyncName = filename;

    char tmpFilename[2048];
    unsigned char tmpData[2048];
    std::string fullPath = FPP_DIR_SEQUENCE("/" + filename);
    if (fullPath.size() >= sizeof(tmpFilename)) {
        LogErr(VB_SEQUENCE, "Sequence Filename too long (%u chars)\n", (unsigned)fullPath.size());
        SetMissingSequenceWarning(filename, "Invalid sequence filename " + filename + "\n");
        m_seqStarting = 0;
        return 0;
    }
    memcpy(tmpFilename, fullPath.c_str(), fullPath.size() + 1);

    if (getFPPmode() == REMOTE_MODE)
        CheckForHostSpecificFile(getSetting("HostName").c_str(), tmpFilename);

    checkForReplacementFile(std::string(tmpFilename));

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
                SetMissingSequenceWarning(filename, warning);
                m_seqStarting = 0;
                return 0;

            } else {
                {
                    std::unique_lock<std::mutex> nameLock(m_seqFilenameLock);
                    m_seqFilename = "fallback.fseq";
                }
                LogDebug(VB_SEQUENCE, "Playing Fallback Sequence file %s\n", tmpFilename);
            }

        } else {
            if (FirstReportOfMissing(filename)) {
                LogErr(VB_SEQUENCE, "Sequence file %s does not exist\n", tmpFilename);
            } else {
                LogDebug(VB_SEQUENCE, "Sequence file %s does not exist\n", tmpFilename);
            }
            SetMissingSequenceWarning(filename, warning);
            m_seqStarting = 0;
            return 0;
        }
    }

    // We have a file to play (the requested one, or the fallback)
    ClearMissingSequenceWarning();
    reportedMissingSequences.erase(filename);

    if (multiSync->isMultiSyncEnabled()) {
        seqLock.unlock();
        multiSync->SendSeqOpenPacket(filename);
        seqLock.lock();
        if (openGeneration != m_openGeneration) {
            LogDebug(VB_SEQUENCE, "Open of %s superseded by a later open while sending the open packet\n", filename.c_str());
            return 0;
        }
    }

    setSeqFile(nullptr);
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

    // prepareRead() copies the ranges into its own member before returning, so
    // it's safe to pass the temporary snapshot directly -- it lives through the
    // full expression.
    seqFile->prepareRead(*GetOutputRangesSnapshot(), startFrame < 0 ? 0 : startFrame);
    // Calculate duration
    m_seqMSRemaining = seqFile->getNumFrames() * seqFile->getStepTime();
    m_seqMSDuration = m_seqMSRemaining.load();
    m_seqMSElapsed = 0;
    SetChannelOutputRefreshRate(m_seqRefreshRate);

    // start reading frames
    lock.lock();
    setSeqFile(std::shared_ptr<FSEQFile>(seqFile));
    lock.unlock();
    m_seqStarting = 1; // beyond header, read loop can start reading frames
    frameLoadSignal.notify_all();
    seqFlag(m_seqPaused).store(0);
    seqFlag(m_seqSingleStep).store(0);
    seqFlag(m_seqSingleStepBack).store(0);
    m_dataProcessed = false;
    ProcessVariableHeaders();
    ReadSequenceData(true);
    seqLock.unlock();
    StartChannelOutputThread();

    m_numSeek = 0;
    LogDebug(VB_SEQUENCE, "seqRefreshRate        : %f\n", m_seqRefreshRate);
    LogDebug(VB_SEQUENCE, "seqMSDuration         : %d\n", m_seqMSDuration.load());
    LogDebug(VB_SEQUENCE, "seqMSRemaining        : %d\n", m_seqMSRemaining.load());
    return 1;
}
void Sequence::ProcessVariableHeaders() {
    std::shared_ptr<FSEQFile> seqFile = m_seqFile.load();
    if (!seqFile) {
        return;
    }
    auto trig = std::make_shared<Triggers>();
    for (auto& vh : seqFile->getVariableHeaders()) {
        if (vh.code[0] != 'F' || (vh.code[1] != 'C' && vh.code[1] != 'E')) {
            continue;
        }
        // This walks a blob that came straight out of a file, so every read is
        // bounds-checked against its end.  It used to trust the block's own
        // fields: the entry count and range count were read (unaligned) with no
        // size check and the command/IP strings with a bare std::string(char*),
        // so one truncated or non-v1 FC/FE block ran strlen() off the end of the
        // vector and killed fppd while opening the sequence, before a single
        // frame played.  Nothing here is worth crashing a show over - a header
        // that does not parse is dropped and the sequence still runs.
        const std::vector<uint8_t>& vhd = vh.getData();
        const uint8_t* data = vhd.data();
        const uint8_t* const end = data + vhd.size();

        auto readU32 = [&data, end](uint32_t& out) {
            if ((size_t)(end - data) < sizeof(out)) {
                return false;
            }
            memcpy(&out, data, sizeof(out)); // also drops the unaligned cast
            data += sizeof(out);
            return true;
        };
        auto readStr = [&data, end](std::string& out) {
            const uint8_t* nul = (const uint8_t*)memchr(data, 0, end - data);
            if (nul == nullptr) {
                return false;
            }
            out.assign((const char*)data, nul - data);
            data = nul + 1;
            return true;
        };

        uint32_t count = 0;
        std::string ips;
        // version flag, only understand v1 right now
        if (data == end || *data++ != 1 || !readU32(count) || !readStr(ips)) {
            continue;
        }
        // TODO - process ips to see if we are actually supposed to run these commands/effects
        //     Not supported in xLights yet

        bool truncated = false;
        for (uint32_t x = 0; x < count && !truncated; x++) {
            std::string cmd;
            uint32_t cc = 0;
            if (!readStr(cmd) || !readU32(cc)) {
                truncated = true;
                break;
            }
            // The name is stored once (an _END twin for commands) and each
            // range records only its index; see Triggers.
            const uint32_t idx = (uint32_t)trig->names.size();
            for (uint32_t cc1 = 0; cc1 < cc; cc1++) {
                uint32_t start = 0;
                uint32_t stop = 0;
                if (!readU32(start) || !readU32(stop)) {
                    truncated = true;
                    break;
                }
                if (cc1 == 0) {
                    trig->names.push_back(cmd);
                    if (vh.code[1] == 'C') {
                        trig->names.push_back(cmd + "_END");
                    }
                }
                if (vh.code[1] == 'C') {
                    trig->commandPresets[start].push_back(idx);
                    trig->commandPresets[stop].push_back(idx + 1);
                } else {
                    trig->effectsOn[start].push_back(idx);
                    trig->effectsOff[stop].push_back(idx);
                }
            }
        }
        if (truncated) {
            LogWarn(VB_SEQUENCE, "Truncated '%c%c' variable header in %s, ignoring the rest of it\n",
                    vh.code[0], vh.code[1], m_seqFilename.c_str());
        }
    }
    if (!trig->commandPresets.empty() || !trig->effectsOn.empty() || !trig->effectsOff.empty()) {
        m_lastTriggeredFrame = -1;
        setTriggers(trig);
    }
}

// Drop the current file's triggers (open/close).  Effects the file started
// are stopped from the main loop: this runs under m_sequenceLock, and on the
// output thread when a sequence reaches its end, and StopEffect() can blank
// the outputs.
void Sequence::ForgetTriggers() {
    setTriggers(nullptr);
    m_lastTriggeredFrame = -1;
    std::vector<std::string> running;
    {
        // Clearing the owner in the same critical section as the swap is what
        // closes the window for an effect FireFrameTriggers() is starting
        // right now: it either lands in `running` or sees the table closed.
        std::unique_lock<std::mutex> l(m_triggerEffectsLock);
        m_triggerEffectsOwner = nullptr;
        running.swap(m_triggerEffects);
    }
    if (running.empty())
        return;
    {
        std::unique_lock<std::mutex> l(m_pendingPresetsLock);
        m_pendingEffectStops.insert(m_pendingEffectStops.end(), running.begin(), running.end());
    }
    Timers::INSTANCE.addTimer("SequencePresets", GetTimeMS(), [this]() {
        drainPendingPresets();
    });
}

void Sequence::queuePreset(const std::string& preset, std::map<std::string, std::string>&& keywords) {
    if (!CommandManager::INSTANCE.HasPreset(preset)) {
        return;
    }
    {
        std::unique_lock<std::mutex> l(m_pendingPresetsLock);
        m_pendingPresets.emplace_back(preset, std::move(keywords));
    }
    // One shared timer name: a queue while an entry is already armed just
    // refreshes it, and the drain empties the whole FIFO, so nothing is lost
    // to the replace-same-name semantics.  The drain runs on the main loop
    // with no sequence locks held; a preset that re-enters the sequence (or
    // queues further presets) is therefore safe.
    Timers::INSTANCE.addTimer("SequencePresets", GetTimeMS(), [this]() {
        drainPendingPresets();
    });
}

void Sequence::drainPendingPresets() {
    std::unique_lock<std::mutex> l(m_pendingPresetsLock);
    while (!m_pendingEffectStops.empty()) {
        std::string eff = std::move(m_pendingEffectStops.front());
        m_pendingEffectStops.pop_front();
        l.unlock();
        StopEffect(eff);
        l.lock();
    }
    while (!m_pendingPresets.empty()) {
        auto p = std::move(m_pendingPresets.front());
        m_pendingPresets.pop_front();
        l.unlock();
        CommandManager::INSTANCE.TriggerPreset(p.first, p.second);
        l.lock();
    }
}

void Sequence::StartSequence() {
    // Callers reach this without m_sequenceLock (the command handler and the
    // playlist), so the name has to be snapshotted rather than read raw.
    std::string seqName = GetSeqFilenameCopy();
    if (!IsSequenceRunning() && m_seqFile.load()) {
        if (multiSync->isMultiSyncEnabled()) {
            ResetChannelOutputFrameNumber();
            multiSync->SendSeqSyncStartPacket(seqName);
        }
        m_seqStarting = 0;
        SetChannelOutputRefreshRate(m_seqRefreshRate);
        StartChannelOutputThread();

        // Only on an actual start.  It used to be queued on every call, and a
        // remote whose copy of the sequence is missing calls this for every
        // sync packet: the user's SEQUENCE_STARTED preset ran several times a
        // second for the whole song, with no matching SEQUENCE_STOPPED.
        //
        // Deferred to the main loop: callers can hold m_sequenceLock
        // (recursively, via StartSequence(filename, frame)), and a preset that
        // starts/stops a playlist takes m_playlistMutex - the reverse of the
        // playlist thread's m_playlistMutex -> m_sequenceLock order.
        queuePreset("SEQUENCE_STARTED", { { "SEQUENCE_NAME", seqName } });
    }
}

void Sequence::StartSequence(const std::string& filename, int frameNumber) {
    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if ((frameNumber > 0) && (filename == m_endedSeqFilename)) {
        // A sync packet for a sequence that already played to its end here
        // (see m_endedSeqFilename).  A start (frame 0) or an open replays it.
        return;
    }
    if ((sequence->m_seqFilename != filename) && (m_seqSyncName != filename)) {
        CloseSequenceFile();
        OpenSequenceFile(filename, frameNumber);
    } else if (!m_seqFile.load() && m_missingSeqFilename == filename) {
        // The file is still missing and the player is still syncing to it, so
        // keep the warning up instead of letting it time out mid-sequence.
        RefreshMissingSequenceWarning();
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
                // Timed: it is re-raised while the skipping continues and
                // otherwise ages out, rather than staying up until a restart.
                WarningHolder::AddWarningTimeout(600, 17, "Multiple Frame Skips During Playback - Likely Slow Network");
            }
        }
    }
    lock.unlock();
    frameLoadSignal.notify_all();
}

std::string Sequence::GetSeqFilenameCopy() const {
    std::unique_lock<std::mutex> nameLock(m_seqFilenameLock);
    return m_seqFilename;
}

// The flag is set after its pointer is stored and cleared before the pointer
// is dropped, so a reader that sees it set finds the pointer set too, as far
// as any unlocked check can promise that.
void Sequence::setSeqFile(std::shared_ptr<FSEQFile> f) {
    if (f) {
        m_seqFile.store(std::move(f));
        m_haveSeqFile.store(true, std::memory_order_release);
    } else {
        m_haveSeqFile.store(false, std::memory_order_release);
        m_seqFile.store(nullptr);
    }
}

void Sequence::setTriggers(std::shared_ptr<Triggers> t) {
    if (t) {
        {
            std::unique_lock<std::mutex> l(m_triggerEffectsLock);
            m_triggerEffectsOwner = t.get();
        }
        m_triggers.store(std::move(t));
        m_haveTriggers.store(true, std::memory_order_release);
    } else {
        m_haveTriggers.store(false, std::memory_order_release);
        m_triggers.store(nullptr);
    }
}

int Sequence::IsSequenceRunning(void) {
    if (m_haveSeqFile.load(std::memory_order_acquire) && !m_seqStarting.load(std::memory_order_acquire))
        return 1;

    return 0;
}

int Sequence::IsSequenceRunning(const std::string& filename) {
    int result = 0;

    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if ((sequence->m_seqFilename == filename) || (!filename.empty() && (sequence->m_seqSyncName == filename)))
        result = sequence->IsSequenceRunning();

    return result;
}

void Sequence::BlankSequenceData(bool clearBridge) {
    LogExcess(VB_SEQUENCE, "BlankSequenceData()\n");
    auto ranges = GetOutputRangesSnapshot();
    for (auto& a : *ranges) {
        memset(&m_seqData[a.first], 0, a.second);
    }
    if (m_bridgeData && clearBridge) {
        std::unique_lock<std::mutex> lock(m_bridgeRangesLock);
        for (auto& a : *ranges) {
            memset(&m_bridgeData[a.first], 0, a.second);
        }
        m_bridgeRanges.clear();
        m_hasBridgeRanges.store(false, std::memory_order_relaxed);
    }

    m_dataProcessed = false;

    std::unique_lock<std::mutex> lock(frameCacheLock);
    SetLastFrameData(nullptr);
}

int Sequence::SequenceIsPaused(void) {
    return seqFlag(m_seqPaused).load();
}

void Sequence::ToggleSequencePause(void) {
    seqFlag(m_seqPaused).fetch_xor(1);
}

void Sequence::SingleStepSequence(void) {
    seqFlag(m_seqSingleStep).store(1);
}

// Broken per issue #986
void Sequence::SingleStepSequenceBack(void) {
    seqFlag(m_seqSingleStepBack).store(1);
}

void Sequence::ReadSequenceData(bool forceFirstFrame) {
    LogExcess(VB_SEQUENCE, "ReadSequenceData()\n");
    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if (!forceFirstFrame && m_seqStarting) {
        return;
    }

    if ((IsSequenceRunning()) && seqFlag(m_seqPaused).load(std::memory_order_relaxed)) {
        if (seqFlag(m_seqSingleStep).exchange(0)) {
            // one frame forward: read it as if playing
        } else if (seqFlag(m_seqSingleStepBack).exchange(0)) {
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
            // Values for status and the playlist; nothing is published
            // through them, so no ordering is needed.
            const int elapsed = data->frame * m_seqStepTime;
            m_seqMSElapsed.store(elapsed, std::memory_order_relaxed);
            m_seqMSRemaining.store(m_seqMSDuration.load(std::memory_order_relaxed) - elapsed, std::memory_order_relaxed);
            m_dataProcessed.store(false, std::memory_order_release);
        } else if (m_doneRead) {
            lock.unlock();
            m_seqMSElapsed = m_seqMSDuration.load();
            m_seqMSRemaining = 0;
            // Remember it by the name sync packets use (see m_seqSyncName).
            std::string ended;
            {
                std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
                ended = m_seqSyncName.empty() ? m_seqFilename : m_seqSyncName;
            }
            CloseSequenceFile();
            std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
            m_endedSeqFilename = ended;
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
    if (m_dataProcessed.load(std::memory_order_acquire)) {
        // we shouldn't normally be reprocessing the same data, so
        // if we are then see if we can start with a pristine copy
        std::unique_lock<std::mutex> lock(frameCacheLock);
        if (m_lastFrameData)
            m_lastFrameData->readFrame((uint8_t*)m_seqData, FPPD_MAX_CHANNELS);
    }

    if (m_bridgeData && hasBridgeData()) {
        // Prune expired ranges and merge the live ones, all under the lock.
        // m_bridgeRanges is keyed by startChannel so the scratch vector comes
        // out sorted. WriteBridgeData() copies each packet in under the same
        // lock, so a packet can no longer land halfway through this copy and
        // send a universe half old, half new; the receive path waits at most
        // for one merge's memcpys.
        uint64_t nt = GetMonotonicTimeMS(); // expiries are monotonic (CommitBridgeRanges)
        // The scratch vector is shared by every caller (the output thread and
        // the inline SendBlankingData path), so it is cleared, filled and read
        // only under the lock: a clear() racing another caller's emplace_back
        // or iteration corrupts the vector.
        std::unique_lock<std::mutex> bridgesLock(m_bridgeRangesLock);
        m_bridgeRangeScratch.clear();
        auto rit = m_bridgeRanges.begin();
        while (rit != m_bridgeRanges.end()) {
            BridgeRangeData& rd = rit->second;
            uint32_t len = 0;
            auto it = rd.expires.begin();
            while (it != rd.expires.end()) {
                if (it->second < nt) {
                    it = rd.expires.erase(it);
                } else {
                    len = std::max(len, it->first);
                    ++it;
                }
            }
            if (len > 0) {
                m_bridgeRangeScratch.emplace_back(rd.startChannel, len);
                ++rit;
            } else {
                rit = m_bridgeRanges.erase(rit);
            }
        }
        m_hasBridgeRanges.store(!m_bridgeRanges.empty(), std::memory_order_relaxed);

        // copy the latest bridge data to the sequence data
        uint32_t curStart = 0xFFFFFFFF;
        uint32_t nextStart = 0xFFFFFFFF;
        for (auto& a : m_bridgeRangeScratch) {
            if (curStart == 0xFFFFFFFF) {
                curStart = a.first;
                nextStart = a.first + a.second;
            } else if (a.first > nextStart) {
                memcpy(&m_seqData[curStart], &m_bridgeData[curStart], nextStart - curStart);
                curStart = a.first;
                nextStart = a.first + a.second;
            } else {
                // Adjacent or overlapping: extend the run, but never shrink it.
                // This used to set it to the end of 'a', which cut the run
                // short when 'a' lay entirely inside it (one input's universe
                // inside another's range), dropping the earlier range's tail.
                nextStart = std::max(nextStart, a.first + a.second);
            }
        }
        if (curStart != 0xFFFFFFFF) {
            memcpy(&m_seqData[curStart], &m_bridgeData[curStart], nextStart - curStart);
        }
        bridgesLock.unlock();
    }
    PluginManager::INSTANCE.modifySequenceData(ms, (uint8_t*)m_seqData);

    if (IsEffectRunning())
        OverlayEffects(m_seqData);

    if (
#ifdef HAS_GSTREAMER
        GStreamerOutput::IsOverlayingVideo()
#else
        false
#endif
        ) {
#ifdef HAS_GSTREAMER
        GStreamerOutput::ProcessVideoOverlay(ms);
#endif
    }
    if (PixelOverlayManager::INSTANCE.hasActiveOverlays()) {
        PixelOverlayManager::INSTANCE.doOverlays((uint8_t*)m_seqData);
    }

    if (ChannelTester::INSTANCE.Testing())
        ChannelTester::INSTANCE.OverlayTestData(m_seqData);

    PluginManager::INSTANCE.modifyChannelData(ms, (uint8_t*)m_seqData);

    PrepareChannelData(m_seqData);
    m_dataProcessed.store(true, std::memory_order_release);
}

void Sequence::FireFrameTriggers() {
    // Triggers belong to frames, not to sends.  This runs on every output
    // send, and used to fire whatever frame was current: the first frame's
    // triggers went off two or three times (it is sent before the sequence
    // starts, and a remote resends it until the start packet arrives), a held
    // or paused frame re-fired every send, and a frame skipped by a catch-up
    // jump -- often the one carrying an _END preset -- never fired.  Fire each
    // frame's triggers once, only once the sequence has started, including the
    // frames a small jump skipped; a large jump is a seek (a remote joining
    // mid-song) and fires only the frame it lands on.
    // Most files have no triggers: skip the locked load on every frame.
    if (!m_haveTriggers.load(std::memory_order_acquire))
        return;
    std::shared_ptr<Triggers> trig = m_triggers.load();
    int64_t frame = m_lastFrameNumber.load();
    if (!trig || (frame < 0) || m_seqStarting)
        return;
    int64_t last = m_lastTriggeredFrame.load();
    if (frame == last)
        return;
    int64_t maxCatchup = (m_seqStepTime > 0) ? (2000 / m_seqStepTime) : 40;
    int64_t from = ((frame < last) || ((frame - last) > maxCatchup)) ? frame : last + 1;
    m_lastTriggeredFrame = frame;

    for (auto p = trig->commandPresets.lower_bound((uint32_t)from);
         (p != trig->commandPresets.end()) && (p->first <= (uint64_t)frame); ++p) {
        // This runs on the channel output thread while the open/close
        // paths can be reassigning the name on another thread.
        std::string seqName = GetSeqFilenameCopy();
        std::map<std::string, std::string> keywords({ { "SEQUENCE_NAME", seqName } });
        uint32_t elapsedMS = p->first * (uint32_t)m_seqStepTime;
        uint32_t totalSeconds = elapsedMS / 1000;
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u.%03u",
                 totalSeconds / 60, totalSeconds % 60, elapsedMS % 1000);
        for (uint32_t idx : p->second) {
            const std::string& cmd = trig->names[idx];
            LogDebug(VB_COMMAND, "Sequence \"%s\" @ %s triggering preset \"%s\"\n", seqName.c_str(), timeBuf, cmd.c_str());
            // Deferred to the main loop: this thread holds
            // outputThreadLock for the whole frame, so running a
            // command here couples the output thread to every lock
            // any command can take (playlist, scheduler via
            // %VAR:fpp_*%, media...). The preset fires on the next
            // main-loop tick, a few ms after the frame it was
            // authored on.
            queuePreset(cmd, std::map<std::string, std::string>(keywords));
        }
    }
    for (auto e = trig->effectsOn.lower_bound((uint32_t)from);
         (e != trig->effectsOn.end()) && (e->first <= (uint64_t)frame); ++e) {
        for (uint32_t idx : e->second) {
            const std::string& eff = trig->names[idx];
            StartEffect(eff, 0, 1);
            std::unique_lock<std::mutex> l(m_triggerEffectsLock);
            if (m_triggerEffectsOwner == trig.get()) {
                m_triggerEffects.push_back(eff);
            } else {
                // The file closed (ForgetTriggers) while this frame was
                // firing, after it took the running list: nothing else will
                // stop this effect, so stop it here, as effectsOff does.
                l.unlock();
                StopEffect(eff);
            }
        }
    }
    for (auto e = trig->effectsOff.lower_bound((uint32_t)from);
         (e != trig->effectsOff.end()) && (e->first <= (uint64_t)frame); ++e) {
        for (uint32_t idx : e->second) {
            const std::string& eff = trig->names[idx];
            StopEffect(eff);
            std::unique_lock<std::mutex> l(m_triggerEffectsLock);
            auto it = std::find(m_triggerEffects.begin(), m_triggerEffects.end(), eff);
            if (it != m_triggerEffects.end())
                m_triggerEffects.erase(it);
        }
    }
}

void Sequence::SendSequenceData() {
    FireFrameTriggers();
    SendChannelData(m_seqData);
}

void Sequence::SendBlankingData(void) {
    LogDebug(VB_SEQUENCE, "SendBlankingData()\n");
    std::this_thread::sleep_for(5ms);

    if (multiSync->isMultiSyncEnabled())
        multiSync->SendBlankingDataPacket();

    BlankSequenceData(true);

    if (!ChannelOutputThreadIsEnabled() || !ForceChannelOutputNow()) {
        // Driving the outputs from this thread: hold off an output reload's
        // teardown, which would otherwise free an output mid-send (it only
        // excluded the output thread).
        std::unique_lock<std::recursive_mutex> inlineLock(inlineOutputLock);
        ProcessSequenceData(0);
        SendSequenceData();
    }
}

void Sequence::CloseIfOpen(const std::string& filename) {
    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);
    if ((m_seqFilename == filename) || (m_seqSyncName == filename))
        CloseSequenceFile();
}

void Sequence::CloseSequenceFile(void) {
    // These two reads happen before m_sequenceLock is taken below, so they need
    // the snapshot; callers arrive here from the playlist, MultiSync and the
    // API threads.
    std::string seqName = GetSeqFilenameCopy();
    LogDebug(VB_SEQUENCE, "CloseSequenceFile() %s\n", seqName.c_str());

    if (multiSync->isMultiSyncEnabled())
        multiSync->SendSeqSyncStopPacket(seqName);

    std::unique_lock<std::recursive_mutex> seqLock(m_sequenceLock);

    // The player told us to stop (or we're moving on); a file that never
    // opened is no longer worth warning about.
    ClearMissingSequenceWarning();

    std::unique_lock<std::mutex> readLock(readFileLock);
    if (std::shared_ptr<FSEQFile> old = m_seqFile.load()) {
        std::string fn = old->getFilename();
        std::unique_lock<std::mutex> fclock(frameCacheLock);
        setSeqFile(nullptr);
        fclock.unlock();
        checkForReplacementFile(fn);

        // Deferred to the main loop: this runs under m_sequenceLock +
        // readFileLock (and on the channel output thread when the sequence
        // ends via ReadSequenceData), so firing the preset here inverts
        // against the playlist thread's m_playlistMutex -> m_sequenceLock.
        if (!m_seqStarting) {
            // Only for a sequence that started, to pair with SEQUENCE_STARTED
            // (which fires only on an actual start).  Closing one that was
            // opened and never started -- the next playlist entry prepared,
            // then a stop -- used to fire a STOPPED with no STARTED.
            queuePreset("SEQUENCE_STOPPED", { { "SEQUENCE_NAME", m_seqFilename } });
        }
        ForgetTriggers();
    }
    readLock.unlock();

    std::unique_lock<std::mutex> lock(frameCacheLock);
    clearCaches();
    m_doneRead = true;
    m_lastFrameRead = -1;
    lock.unlock();
    frameLoadedSignal.notify_all();

    {
        std::unique_lock<std::mutex> nameLock(m_seqFilenameLock);
        m_seqFilename = "";
    }
    m_seqSyncName.clear();
    seqFlag(m_seqPaused).store(0);

    if ((!IsEffectRunning()) &&
            ((getFPPmode() != REMOTE_MODE) &&
             (Player::INSTANCE.GetStatus() != FPP_STATUS_PLAYLIST_PLAYING)) ||
        (m_blankBetweenSequences)) {
        SendBlankingData();
    }
}

bool Sequence::WriteBridgeData(uint8_t* data, int startChannel, int len) {
    if (this->IsSequenceRunning()) {
        if (m_warn_if_bridging) {
            WarningHolder::AddWarningTimeout(60 ,11, "Received bridging data while sequence is running.");
        }
        if (m_prioritize_sequence_over_bridge) {
            return false;
        }
    }

    if (!m_bridgeData) {
        return false;
    }
    // Guard against out-of-range bridging data.  The bound is the real
    // channels, FPPD_MAX_CHANNELS: the bytes after them are the reserved OFF
    // and WHITE channels, which outputs read as "always 0" for null pixels and
    // smart-receiver lead-in/out, and bridged data merged into them turned
    // those on.  Unsigned arithmetic avoids overflow.
    if (startChannel < 0 || len < 0 ||
        ((unsigned long long)startChannel + (unsigned long long)len) > (unsigned long long)FPPD_MAX_CHANNELS) {
        return false;
    }
    // Under the ranges lock, which ProcessSequenceData() holds while it
    // copies the bridge buffer out: a packet landing mid-copy sent a universe
    // half old, half new.
    std::unique_lock<std::mutex> lock(m_bridgeRangesLock);
    memcpy(&m_bridgeData[startChannel], data, len);
    return true;
}

void Sequence::CommitBridgeRanges(const std::vector<std::pair<uint32_t, uint32_t>>& ranges, uint64_t expireMS) {
    if (ranges.empty()) {
        return;
    }
    std::unique_lock<std::mutex> lock(m_bridgeRangesLock);
    for (auto& r : ranges) {
        auto& a = m_bridgeRanges[r.first];
        a.startChannel = r.first;
        a.expires[r.second] = expireMS;
    }
    m_hasBridgeRanges.store(true, std::memory_order_relaxed);
    lock.unlock();

    setDataNotProcessed();
}

void Sequence::SetBridgeData(uint8_t* data, int startChannel, int len, uint64_t expireMS) {
    if (!WriteBridgeData(data, startChannel, len)) {
        return;
    }
    std::unique_lock<std::mutex> lock(m_bridgeRangesLock);
    auto& a = m_bridgeRanges[startChannel];
    a.startChannel = startChannel;
    a.expires[len] = expireMS;
    m_hasBridgeRanges.store(true, std::memory_order_relaxed);
    lock.unlock();

    setDataNotProcessed();
}

bool Sequence::hasBridgeData() {
    return m_hasBridgeRanges.load(std::memory_order_relaxed);
}
