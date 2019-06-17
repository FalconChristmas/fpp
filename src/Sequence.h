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

#include "fseq/FSEQFile.h"


//1024K channels plus a few
#define FPPD_MAX_CHANNELS (4196*1024+4)
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

	FSEQFile     *m_seqFile;

    volatile int  m_seqStarting;
	int           m_seqPaused;
    int           m_seqStepTime;
	int           m_seqSingleStep;
	int           m_seqSingleStepBack;
	int           m_seqRefreshRate;
	int           m_seqControlRawIDs;
	char          m_seqLastControlMajor;
	char          m_seqLastControlMinor;
    int           m_remoteBlankCount;
    bool          m_dataProcessed;

    std::recursive_mutex m_sequenceLock;
    
    std::atomic_int m_lastFrameRead;
    volatile bool m_doneRead;
    volatile bool m_shuttingDown;
    std::thread *m_readThread;
    std::list<FSEQFile::FrameData*> frameCache;
    std::list<FSEQFile::FrameData*> pastFrameCache;
    void clearCaches();
    std::mutex frameCacheLock;
    std::mutex readFileLock; //lock for just the stuff needed to read from the file (m_seqFile variable)
    std::condition_variable frameLoadSignal;
    std::condition_variable frameLoadedSignal;

    public:
    void ReadFramesLoop();
};

extern Sequence *sequence;

#endif /* _SEQUENCE_H */
