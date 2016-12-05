/*
 *   SequenceFile Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Player Developers
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

#ifndef _SEQUENCEFILE_H
#define _SEQUENCEFILE_H

#include <string>
#include <vector>

#define FPPD_MAX_CHANNELS  524288
#define DATA_DUMP_SIZE     28

// Version info for the FSEQ version this class creates
#define FSEQ_MAJOR_VERSION 1
#define FSEQ_MINOR_VERSION 1

// Defines for this FSEQ version
#define FSEQ_FIXED_HEADER_LEN 28
#define FSEQ_CHAN_DATA_OFFSET_OFFSET 4
#define FSEQ_DURATION_OFFSET 14

typedef struct sparseChannelIndex {
	int offset;
	int count;
} SparseChannelIndex;

class SequenceFile {
  public:
	SequenceFile(char *seqData = NULL);
	~SequenceFile();

	// Misc. Routines
	int  Open(std::string filename, int write = 0);
	int  IsOpen(void)                  { return m_file ? 1 : 0; }
	int  Close(void);
	void LogFSEQInfo(void);

	int  SetStepSize(int channels);
	int  SetStepTime(int stepTime);

	int  GetStepSize(void)             { return m_stepSize; }
	int  GetNumSteps(void)           { return m_numSteps; }
	int  GetStepTime(void)             { return m_stepTime; }
	int  GetMajorVersion(void)         { return m_versionMajor; }
	int  GetMinorVersion(void)         { return m_versionMinor; }
	int  GetSecondsElapsed(void)       { return m_secondsElapsed; }
	int  GetSecondsRemaining(void)     { return m_secondsRemaining; }
	int  GetDuration(void)             { return m_duration; }
	int  GetFileSize(void)             { return m_fileSize; }

	std::string GetFilename(void)      { return m_filename; }
	std::string GetMediaFilename(void) { return m_mediaFilename; }

	// Routines for reading a FESQ file
	int  SeekSeconds(int seconds);
	int  SeekFrame(int frameNumber);
	int  ReadFrame(void);

	// Routines for creating a FSEQ file
	int  Open(std::string filename, SequenceFile *source);
	int  WriteHeader(void);
	int  WriteFrame(void);
	int  Copy(SequenceFile *source);
	int  UpdateNumSteps(void);
	void ScaleBrightness(int scale);

	char  *m_seqData;
	char   m_seqDataPrivate[FPPD_MAX_CHANNELS] __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)));

	std::vector<SparseChannelIndex> m_seqIndex;


  private:
	int  OpenForRead(void);
	int  OpenForWrite(void);

	int           m_writeMode;
	FILE         *m_file;
	unsigned long m_filePosition;
	int           m_chanDataOffset;
	int           m_fixedHeaderSize;
	int           m_stepSize;
	int           m_stepTime;
	int           m_refreshRate;
	int           m_numSteps;
	unsigned char m_versionMajor;
	unsigned char m_versionMinor;
	int           m_version;
	int           m_duration;
	int           m_secondsElapsed;
	int           m_secondsRemaining;
	unsigned long m_fileSize;
	int           m_scaleBrightness;
	std::string   m_filename;
	std::string   m_mediaFilename;
	unsigned char m_bLookup[256];


	// Unused fields with hard-coded values in FSEQ v1.x
	int           m_numUniverses;
	int           m_universeSize;
	unsigned char m_gamma;
	unsigned char m_colorEncoding;

};

#endif /* _SEQUENCEFILE_H */
