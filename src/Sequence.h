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

#include <pthread.h>
#include <stdio.h>

#define FPPD_MAX_CHANNELS 524288
#define DATA_DUMP_SIZE    28

class Sequence {
  public:
	Sequence();
	~Sequence();

	int   IsSequenceRunning(void);
	int   IsSequenceRunning(char *filename);
	int   OpenSequenceFile(const char *filename, int startSeconds = 0);
	void  ProcessSequenceData(int checkControlChannels = 1);
	int   SeekSequenceFile(int frameNumber);
	void  ReadSequenceData(void);
	void  SendSequenceData(void);
	void  SendBlankingData(void);
	void  CloseIfOpen(char *filename);
	void  CloseSequenceFile(void);
	void  ToggleSequencePause(void);
	void  SingleStepSequence(void);
	void  SingleStepSequenceBack(void);
	int   SequenceIsPaused(void);

	unsigned long m_seqFileSize;
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
	unsigned long m_seqFilePosition;
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

	pthread_mutex_t      m_sequenceLock;
	pthread_mutexattr_t  m_sequenceLock_attr;
};

extern Sequence *sequence;

#endif /* _SEQUENCE_H */
