#pragma once
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

#include <stdio.h>
#include <string>

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

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
    void ProcessSequenceData(int ms, int checkControlChannels = 1);
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
    unsigned char m_seqLastControlValue;
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

public:
    void ReadFramesLoop();
};

extern Sequence* sequence;
