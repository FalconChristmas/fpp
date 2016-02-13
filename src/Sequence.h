/*
 *   Sequence Class for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#include <string>

#include <stdio.h>

#define FPPD_MAX_CHANNELS 524288
#define DATA_DUMP_SIZE    28

class Sequence {
  public:
	Sequence(int priority, int startChannel = 0, int blockSize = -1);
	~Sequence();

	int   OpenSequenceFile(std::string filename, int startSeconds = 0);
	int   SeekSequenceFile(int frameNumber);
	int   ReadSequenceData(void);
	void  OverlayNextFrame(char *outputBuffer);
	void  CloseSequenceFile(void);
	void  ToggleSequencePause(void);
	void  SetPauseState(int pause = 1);
	void  SingleStepSequence(void);
	void  SingleStepSequenceBack(void);
	int   SequenceIsPaused(void);

	int   SequenceFileOpen(void)        { return m_seqFile ? 1 : 0; }
	int   GetPriority(void)             { return m_priority; }
	void  SetAutoRepeat(void)           { m_autoRepeat = 1; }
	int   GetAutoRepeat(void)           { return m_autoRepeat; }
	int   GetRefreshRate(void)          { return m_seqRefreshRate; }

	long long     GetSequenceID(void)         { return m_sequenceID; }
	void          SetSequenceID(long long id) { m_sequenceID = id; }

	long long     m_sequenceID;
	unsigned long m_seqFileSize;
	int           m_seqDuration;
	int           m_seqSecondsElapsed;
	int           m_seqSecondsRemaining;
	char          m_seqData[FPPD_MAX_CHANNELS] __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)));
	char          m_seqFilename[1024];

  private:
	char *CurrentSequenceFilename(void);

	FILE         *m_seqFile;
	unsigned long m_seqFilePosition;
	int           m_priority;
	int           m_autoRepeat;
	int           m_startChannel;
	int           m_blockSize;
	int           m_seqStarting;
	int           m_seqPaused;
	int           m_seqSingleStep;
	int           m_seqSingleStepBack;
	int           m_seqVersionMajor;
	int           m_seqVersionMinor;
	int           m_seqVersion;
	int           m_seqChanDataOffset;
	int           m_seqFixedHeaderSize;
	int           m_seqStepSize;
	int           m_seqStepTime;
	int           m_seqNumPeriods;
	int           m_seqRefreshRate;
	int           m_seqNumUniverses;
	int           m_seqUniverseSize;
	int           m_seqGamma;
	int           m_seqColorEncoding;
	char          m_seqLastControlMajor;
	char          m_seqLastControlMinor;
};

#endif /* _SEQUENCE_H */
