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

#ifndef _SEQUENCE_H
#define _SEQUENCE_H

#include <pthread.h>
#include <stdio.h>
#include <string>

#include <mutex>
#include <thread>
#include <list>
#include <atomic>
#include <condition_variable>

//1024K channels plus a few
#define FPPD_MAX_CHANNELS 1048580
#define DATA_DUMP_SIZE    28

#define SEQUENCE_CACHE_FRAMECOUNT 20

class Sequence {
  public:
	Sequence();
	~Sequence();

	int   IsSequenceRunning(void);
	int   IsSequenceRunning(char *filename);
	int   OpenSequenceFile(const char *filename, int startFrame = 0, int startSecond = -1);
	void  ProcessSequenceData(int ms, int checkControlChannels = 1);
	int   SeekSequenceFile(int frameNumber);
	void  ReadSequenceData(bool forceFirstFrame = false);
	void  SendSequenceData(void);
	void  SendBlankingData(void);
	void  CloseIfOpen(char *filename);
	void  CloseSequenceFile(void);
	void  ToggleSequencePause(void);
	void  SingleStepSequence(void);
	void  SingleStepSequenceBack(void);
	int   SequenceIsPaused(void);
    bool  isDataProcessed() const { return m_dataProcessed; }

	uint64_t      m_seqFileSize;
	int           m_seqDuration;
	int           m_seqSecondsElapsed;
	int           m_seqSecondsRemaining;
	int           m_seqMSRemaining;
	char          m_seqData[FPPD_MAX_CHANNELS] __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)));
	char          m_seqFilename[1024];

  private:
	void  BlankSequenceData(void);
	char  NormalizeControlValue(char in);
	char *CurrentSequenceFilename(void);

	FILE         *m_seqFile;
    uint64_t      m_seqChanDataOffset;
    uint64_t      m_seqFixedHeaderSize;
    uint64_t      m_seqStepSize;
    uint64_t      m_seqNumPeriods;
    
    int           m_seqStepTime;
	volatile int  m_seqStarting;
	int           m_seqPaused;
	int           m_seqSingleStep;
	int           m_seqSingleStepBack;
	int           m_seqVersionMajor;
	int           m_seqVersionMinor;
	int           m_seqVersion;
	int           m_seqRefreshRate;
	int           m_seqNumUniverses;
	int           m_seqUniverseSize;
	int           m_seqGamma;
	int           m_seqColorEncoding;
	char          m_seqLastControlMajor;
	char          m_seqLastControlMinor;
    int           m_remoteBlankCount;
    bool          m_dataProcessed;

    std::recursive_mutex m_sequenceLock;
    
    class FrameData {
        public:
        FrameData(int f, int sz) : frame(f), size(sz) { data = (uint8_t*)malloc(sz);}
        ~FrameData() { if (data) free(data); };
        uint8_t *data;
        uint32_t frame;
        uint32_t size;
    };
    std::atomic_int m_lastFrameRead;
    volatile bool m_doneRead;
    volatile bool m_shuttingDown;
    std::thread *m_readThread;
    std::list<FrameData*> frameCache;
    std::list<FrameData*> pastFrameCache;
    void clearCaches();
    std::mutex frameCacheLock;
    std::condition_variable frameLoadSignal;
    
    public:
    void ReadFramesLoop();
};

extern Sequence *sequence;

#endif /* _SEQUENCE_H */
