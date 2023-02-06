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

#include <stdio.h>
#include <string>

#include <atomic>
#include <condition_variable>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "fseq/FSEQFile.h"

#define FPPD_MAX_CHANNELS (8192 * 1024)
#define DATA_DUMP_SIZE 28

//reserve 4 channels of 0 and 4 channels of 0xFF for indexes
//that require one or the other
#define FPPD_OFF_CHANNEL FPPD_MAX_CHANNELS
#define FPPD_WHITE_CHANNEL (FPPD_MAX_CHANNELS + 4)
#define FPPD_MAX_CHANNEL_NUM (FPPD_WHITE_CHANNEL + 4)

class Sequence {
public:
    Sequence();
    ~Sequence();

    int IsSequenceRunning(void);
    int IsSequenceRunning(const std::string& filename);
    int OpenSequenceFile(const std::string& filename, int startFrame = 0, int startSecond = -1);
    void StartSequence(const std::string& filename, int startFrame);
    void StartSequence();
    void ProcessSequenceData(int ms);
    void SeekSequenceFile(int frameNumber);
    void ReadSequenceData(bool forceFirstFrame = false);
    void SendSequenceData(void);
    void SendBlankingData(void);
    void CloseIfOpen(const std::string& filename);
    void CloseSequenceFile(void);
    void ToggleSequencePause(void);
    void SingleStepSequence(void);
    void SingleStepSequenceBack(void);
    int SequenceIsPaused(void);

    bool hasBridgeData();
    bool isDataProcessed() const { return m_dataProcessed; }
    void setDataNotProcessed() { m_dataProcessed = false; }

    int m_seqMSDuration;
    int m_seqMSElapsed;
    int m_seqMSRemaining;
    char m_seqData[FPPD_MAX_CHANNEL_NUM] __attribute__((aligned(__BIGGEST_ALIGNMENT__)));
    std::string m_seqFilename;

    int GetSeqStepTime() const { return m_seqStepTime; }
    void BlankSequenceData(bool clearBridge = false);

    void SetBridgeData(uint8_t* data, int startChannel, int len, uint64_t expireMS);

private:
    void ProcessVariableHeaders();
    void SetLastFrameData(FSEQFile::FrameData* data);
    bool m_prioritize_sequence_over_bridge;

    class BridgeRangeData {
    public:
        BridgeRangeData() :
            startChannel(0) {}

        uint32_t startChannel;
        // map of len -> ms when it expires, in MOST cases, this will be a single len (like 512 for e1.31)
        std::map<uint32_t, uint64_t> expires;
    };
    std::map<uint64_t, BridgeRangeData> m_bridgeRanges;
    std::mutex m_bridgeRangesLock;
    uint8_t* m_bridgeData;

    FSEQFile* m_seqFile;

    volatile int m_seqStarting;
    int m_seqPaused;
    int m_seqStepTime;
    int m_seqSingleStep;
    int m_seqSingleStepBack;
    float m_seqRefreshRate;
    int m_remoteBlankCount;
    bool m_dataProcessed;
    int m_numSeek;

    int m_blankBetweenSequences;

    std::recursive_mutex m_sequenceLock;

    std::atomic_int m_lastFrameRead;
    volatile bool m_doneRead;
    volatile bool m_shuttingDown;
    std::thread* m_readThread;
    std::list<FSEQFile::FrameData*> frameCache;
    std::list<FSEQFile::FrameData*> pastFrameCache;
    FSEQFile::FrameData* m_lastFrameData;
    void clearCaches();
    std::mutex frameCacheLock;
    std::mutex readFileLock; //lock for just the stuff needed to read from the file (m_seqFile variable)
    std::condition_variable frameLoadSignal;
    std::condition_variable frameLoadedSignal;

    std::map<uint32_t, std::vector<std::string>> commandPresets;
    std::map<uint32_t, std::vector<std::string>> effectsOn;
    std::map<uint32_t, std::vector<std::string>> effectsOff;
public:
    void ReadFramesLoop();
};

extern Sequence* sequence;
