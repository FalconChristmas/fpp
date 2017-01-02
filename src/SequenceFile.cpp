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

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "log.h"
#include "SequenceFile.h"

SequenceFile::SequenceFile(char *seqData)
  : m_file(NULL),
	m_filePosition(0),
	m_chanDataOffset(0),
	m_fixedHeaderSize(0),
	m_stepSize(0),
	m_stepTime(0),
	m_refreshRate(0),
	m_numSteps(0),
	m_versionMajor(0),
	m_versionMinor(0),
	m_version(0),
	m_duration(0),
	m_secondsElapsed(0),
	m_secondsRemaining(0),
	m_fileSize(0),
	m_numUniverses(0),
	m_universeSize(0),
	m_gamma(1),
	m_colorEncoding(2)
{
	// Only allow external seqData during conversion
	if (seqData)
		m_seqData = seqData;
	else
		m_seqData = m_seqDataPrivate;
}

SequenceFile::~SequenceFile()
{
	if (IsOpen())
		Close();
}

/*
 *
 */
int SequenceFile::OpenForRead(void)
{
	LogDebug(VB_SEQUENCE, "OpenForRead()\n");

	if (!FileExists(m_filename.c_str()))
	{
		LogErr(VB_SEQUENCE, "Sequence file %s does not exist\n", m_filename.c_str());
		return 0;
	}

	m_file = fopen(m_filename.c_str(), "rb");
	if (m_file == NULL) 
	{
		LogErr(VB_SEQUENCE, "Error opening sequence file: %s. fopen returned NULL\n",
			m_filename.c_str());
		return 0;
	}

	unsigned char tmpData[20480];

	///////////////////////////////////////////////////////////////////////
	// Check 4-byte File format identifier
	char seqFormatID[5];
	strcpy(seqFormatID, "    ");

	int bytesRead = fread(seqFormatID, 1, 4, m_file);

	if ((bytesRead != 4) || (strcmp(seqFormatID, "PSEQ") && strcmp(seqFormatID, "FSEQ")))
	{
		LogErr(VB_SEQUENCE, "Error opening sequence file: %s. Incorrect File Format header: '%s', bytesRead: %d\n",
			m_filename.c_str(), seqFormatID, bytesRead);

		fseek(m_file, 0L, SEEK_SET);
		bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, m_file);
		HexDump("Sequence File head:", tmpData, bytesRead);

		fclose(m_file);
		m_file = NULL;
		return 0;
	}

	///////////////////////////////////////////////////////////////////////
	// Get Channel Data Offset
	bytesRead = fread(tmpData, 1, 2, m_file);
	if (bytesRead != 2)
	{
		LogErr(VB_SEQUENCE, "Sequence file %s too short, unable to read channel data offset value\n", m_filename.c_str());

		fseek(m_file, 0L, SEEK_SET);
		bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, m_file);
		HexDump("Sequence File head:", tmpData, bytesRead);

		fclose(m_file);
		m_file = NULL;
		return 0;
	}
	m_chanDataOffset = tmpData[0] + (tmpData[1] << 8);

	///////////////////////////////////////////////////////////////////////
	// Now that we know the header size, read the whole header in one shot
	fseek(m_file, 0L, SEEK_SET);
	bytesRead = fread(tmpData, 1, m_chanDataOffset, m_file);
	if (bytesRead != m_chanDataOffset)
	{
		LogErr(VB_SEQUENCE, "Sequence file %s too short, unable to read fixed header size value\n", m_filename.c_str());

		fseek(m_file, 0L, SEEK_SET);
		bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, m_file);
		HexDump("Sequence File head:", tmpData, bytesRead);

		fclose(m_file);
		m_file = NULL;
		return 0;
	}

	m_versionMinor = tmpData[6];
	m_versionMajor = tmpData[7];
	m_version      = (m_versionMajor * 256) + m_versionMinor;

	m_fixedHeaderSize =
		(tmpData[8])        + (tmpData[9] << 8);

	m_stepSize =
		(tmpData[10])       + (tmpData[11] << 8) +
		(tmpData[12] << 16) + (tmpData[13] << 24);

	m_numSteps =
		(tmpData[14])       + (tmpData[15] << 8) +
		(tmpData[16] << 16) + (tmpData[17] << 24);

	m_stepTime =
		(tmpData[18])       + (tmpData[19] << 8);

	m_numUniverses = 
		(tmpData[20])       + (tmpData[21] << 8);

	m_universeSize = 
		(tmpData[22])       + (tmpData[23] << 8);

	m_gamma         = tmpData[24];
	m_colorEncoding = tmpData[25];

	m_seqIndex.clear();

	// End of v1.0 fields

	// Only parse variable length headers in 1.x files
	if ((m_versionMajor == 0x01) && (m_versionMinor > 0x00))
	{
		char variableType[3];
		int curHeaderPos = m_fixedHeaderSize;
		unsigned char *vData = NULL;

		while (m_chanDataOffset >= (curHeaderPos + 4))
		{
			int variableLen = tmpData[curHeaderPos]
							+ (tmpData[curHeaderPos+1] << 8);
			strncpy(variableType, (char *)&tmpData[curHeaderPos+2], 2);
			variableType[2] = 0x00;

			vData = tmpData + curHeaderPos + 4;

			if (!strcmp(variableType, "mf"))
			{
				// Media Filename
				m_mediaFilename = (char *)vData;
			}
			else if (!strcmp(variableType, "si"))
			{
				// Sparse Index
				int indices = vData[0] + (vData[1] << 8);
				vData += 2;

				for (int i = 0; (i < indices) &&
						(vData < &tmpData[m_chanDataOffset]); i++)
				{
					int offset = 
						(vData[0])       + (vData[1] << 8) +
						(vData[2] << 16) + (vData[3] << 24);
					vData += 4;

					int count = 
						(vData[0])       + (vData[1] << 8) +
						(vData[2] << 16) + (vData[3] << 24);
					vData += 4;

					m_seqIndex.push_back((SparseChannelIndex){ offset, count });
				}
			}

			curHeaderPos += variableLen;
		}
	}

	if (m_seqIndex.empty())
		m_seqIndex.push_back((SparseChannelIndex){ 0, m_stepSize });

	m_refreshRate = 1000 / m_stepTime;

	fseek(m_file, 0L, SEEK_END);
	m_fileSize = ftell(m_file);
	m_duration = (int)((float)(m_fileSize - m_chanDataOffset)
		/ ((float)m_stepSize * (float)m_refreshRate));
	m_secondsRemaining = m_duration;
	fseek(m_file, m_chanDataOffset, SEEK_SET);
	m_filePosition = m_chanDataOffset;
LogDebug(VB_SEQUENCE, "Read %d bytes, total %d\n", m_filePosition, m_filePosition);

	LogDebug(VB_SEQUENCE, "Sequence File Information\n");
	LogDebug(VB_SEQUENCE, "seqFilename           : %s\n", m_filename.c_str());
	LogDebug(VB_SEQUENCE, "seqVersion            : %d.%d\n",
		m_versionMajor, m_versionMinor);
	LogDebug(VB_SEQUENCE, "seqFormatID           : %s\n", seqFormatID);
	LogDebug(VB_SEQUENCE, "seqChanDataOffset     : %d\n", m_chanDataOffset);
	LogDebug(VB_SEQUENCE, "seqFixedHeaderSize    : %d\n", m_fixedHeaderSize);
	LogDebug(VB_SEQUENCE, "seqStepSize           : %d\n", m_stepSize);
	LogDebug(VB_SEQUENCE, "seqNumSteps           : %d\n", m_numSteps);
	LogDebug(VB_SEQUENCE, "seqStepTime           : %dms\n", m_stepTime);
	LogDebug(VB_SEQUENCE, "seqNumUniverses       : %d *\n", m_numUniverses);
	LogDebug(VB_SEQUENCE, "seqUniverseSize       : %d *\n", m_universeSize);
	LogDebug(VB_SEQUENCE, "seqGamma              : %d *\n", m_gamma);
	LogDebug(VB_SEQUENCE, "seqColorEncoding      : %d *\n", m_colorEncoding);
	LogDebug(VB_SEQUENCE, "seqRefreshRate        : %d\n", m_refreshRate);
	LogDebug(VB_SEQUENCE, "seqFileSize           : %lu\n", m_fileSize);
	LogDebug(VB_SEQUENCE, "seqDuration           : %d\n", m_duration);

	if (m_mediaFilename.size())
		LogDebug(VB_SEQUENCE, "mediaFilename         : %s\n", m_mediaFilename.c_str());

	if (m_seqIndex.size())
	{
		LogDebug(VB_SEQUENCE, "seqIndex              : \n");

		//for (SparseChannelIndex se : m_seqIndex)
		for (unsigned int i = 0; i < m_seqIndex.size(); i++)
		{
			SparseChannelIndex *se = &m_seqIndex[i];

			LogDebug(VB_SEQUENCE, "                        : %d @ %d\n",
				se->count, se->offset);
		}
	}

	LogDebug(VB_SEQUENCE, "'*' denotes field is currently ignored by FPP\n");

	return m_fileSize;
}


/*
 *
 */
int SequenceFile::OpenForWrite(void)
{
	LogDebug(VB_SEQUENCE, "OpenForWrite()\n");

	m_file = fopen(m_filename.c_str(), "wb");
	if (m_file == NULL) 
	{
		LogErr(VB_SEQUENCE, "Error opening sequence file: %s. fopen returned NULL\n",
			m_filename.c_str());
		return 0;
	}

	return 1;
}


/*
 *
 */
int SequenceFile::Open(std::string filename, SequenceFile *source)
{
	LogDebug(VB_SEQUENCE, "Open(%s, %s)\n", filename.c_str(), source->GetFilename().c_str());

	if (filename.empty())
	{
		LogErr(VB_SEQUENCE, "Tried to open for write with no filename.\n");
		return 0;
	}

	m_filename = filename;

	if (!OpenForWrite())
		return 0;

	SetStepSize(source->GetStepSize());
	SetStepTime(source->GetStepTime());

	m_mediaFilename = source->GetMediaFilename();
	m_seqIndex = source->m_seqIndex;

	WriteHeader();

	return 1;
}


/*
 *
 */

int SequenceFile::Open(std::string filename, int write) {
	LogDebug(VB_SEQUENCE, "Open(%s, %d)\n", filename.c_str(), write);

	if (filename.empty())
	{
		LogErr(VB_SEQUENCE, "Empty Sequence Filename!\n");
		return 0;
	}

	if (IsOpen())
		Close();

	size_t bytesRead = 0;

	m_fileSize = 0;

	m_duration = 0;
	m_secondsElapsed = 0;
	m_secondsRemaining = 0;

	m_filename = filename;

	if (write)
		return OpenForWrite();

	return OpenForRead();
}

/*
 *
 */
int SequenceFile::Close(void)
{
	if (m_writeMode)
		UpdateNumSteps();

	if (m_file)
		fclose(m_file);
	
	m_file = NULL;

	return 1;
}

/*
 *
 */
int SequenceFile::SetStepSize(int channels)
{
	if (channels % 4)
		m_stepSize = channels + 4 - (channels % 4);
	else
		m_stepSize = channels;

	LogDebug(VB_SEQUENCE, "SetStepSize(%d) => %d\n", channels, m_stepSize);

	return m_stepSize;
}


/*
 *
 */
int SequenceFile::SetStepTime(int stepTime)
{
	LogDebug(VB_SEQUENCE, "SetStepTime(%d)\n", stepTime);

	m_stepTime = stepTime;

	m_refreshRate = 1000 / m_stepTime;

	return m_stepTime;
}


/*
 *
 */
int SequenceFile::SeekSeconds(int seconds)
{
	int frameNumber = seconds * m_refreshRate;

	return SeekFrame(frameNumber);
}


/*
 *
 */
int SequenceFile::SeekFrame(int frameNumber) {
	LogDebug(VB_SEQUENCE, "SeekFrame(%d)\n", frameNumber);

	if (!IsOpen())
	{
		LogErr(VB_SEQUENCE, "No sequence file is open\n");
		return 0;
	}

	int newPos = m_chanDataOffset + (frameNumber * m_stepSize);
	LogDebug(VB_SEQUENCE, "Seeking to byte %d in %s\n", newPos, m_filename.c_str());

	fseek(m_file, newPos, SEEK_SET);
}


/*
 *
 */
int SequenceFile::ReadFrame(void)
{
	LogDebug(VB_SEQUENCE, "ReadFrame()\n");

	if (!IsOpen())
	{
		LogErr(VB_SEQUENCE, "Error, ReadFrame() called when no file open.\n");
		return 0;
	}

	if (m_writeMode)
	{
		LogErr(VB_SEQUENCE, "Error, tried to read from file open in write mode: %s\n", m_filename.c_str());
		return 0;
	}

	size_t  bytesRead = 0;

	if(m_filePosition <= (m_fileSize - m_stepSize))
	{
		//for (SparseChannelIndex se : m_seqIndex)
		for (unsigned int i = 0; i < m_seqIndex.size(); i++)
		{
			SparseChannelIndex *se = &m_seqIndex[i];

			bytesRead += fread(m_seqData + se->offset, 1, se->count, m_file);
		}

		m_filePosition += bytesRead;
LogDebug(VB_SEQUENCE, "Read %d bytes, total %d\n", bytesRead, m_filePosition);
	}

	if (bytesRead != m_stepSize)
	{
		Close();
		return 0;
	}

	m_secondsElapsed = (int)((float)(m_filePosition - m_chanDataOffset)/((float)m_stepSize*(float)m_refreshRate));
	m_secondsRemaining = m_duration - m_secondsElapsed;

	return bytesRead;
}


/*
 *
 */
int SequenceFile::WriteHeader(void)
{
	LogDebug(VB_SEQUENCE, "WriteHeader()\n");

	unsigned char header[FSEQ_FIXED_HEADER_LEN];

	if (!m_file)
	{
		LogErr(VB_SEQUENCE, "Error, file not open in WriteHeader()\n");
		return 0;
	}

	// Magic
	header[0]  = 'F';
	header[1]  = 'S';
	header[2]  = 'E';
	header[3]  = 'Q';

	// Channel Data Offset (will be updated later)
	header[4]  = 0;    // LSB
	header[5]  = 0;    // MSB

	// FSEQ Version
	header[6]  = FSEQ_MINOR_VERSION;
	header[7]  = FSEQ_MAJOR_VERSION;

	// Fixed Header Length
	header[8]  = (unsigned char)(FSEQ_FIXED_HEADER_LEN % 256);
	header[9]  = (unsigned char)(FSEQ_FIXED_HEADER_LEN / 256);

	// Step Size (channel count rounded up to multiple of 4 bytes)
	header[10] = (unsigned char)((m_stepSize)       & 0xFF);
	header[11] = (unsigned char)((m_stepSize >>  8) & 0xFF);
	header[12] = (unsigned char)((m_stepSize >> 16) & 0xFF);
	header[13] = (unsigned char)((m_stepSize >> 24) & 0xFF);

	// Number of Steps
	header[14] = (unsigned char)((m_numSteps)       & 0xFF);
	header[15] = (unsigned char)((m_numSteps >>  8) & 0xFF);
	header[16] = (unsigned char)((m_numSteps >> 16) & 0xFF);
	header[17] = (unsigned char)((m_numSteps >> 24) & 0xFF);

	// Step time in ms
	header[18] = (unsigned char)((m_stepTime)       & 0xFF);
	header[19] = (unsigned char)((m_stepTime >>  8) & 0xFF);

	// Universe Count
	header[20] = (unsigned char)((m_numUniverses)       & 0xFF);
	header[21] = (unsigned char)((m_numUniverses >>  8) & 0xFF);

	// Universe Size
	header[22] = (unsigned char)((m_universeSize)       & 0xFF);
	header[23] = (unsigned char)((m_universeSize >>  8) & 0xFF);

	// Gamma
	header[24] = m_gamma;

	// Color Encoding
	header[25] = m_colorEncoding;

	// Unused bytes in v1.x header
	header[26] = 0;
	header[27] = 0;

	int bytesWritten = 0;
	
	bytesWritten = fwrite(header, 1, FSEQ_FIXED_HEADER_LEN, m_file);

	if (bytesWritten != FSEQ_FIXED_HEADER_LEN)
	{
		LogErr(VB_SEQUENCE, "Error, unable to write fixed header for FSEQ file: %s\n",
			strerror(errno));
		return 0;
	}

	m_filePosition += bytesWritten;
LogDebug(VB_SEQUENCE, "Wrote %d bytes, total %d\n", bytesWritten, m_filePosition);

	// Write media filename if we know it
	if (!m_mediaFilename.empty())
	{
		int mlen = strlen(m_mediaFilename.c_str()) + 1;
		int len = mlen + 4;
		unsigned char header[4];

		header[0] = (unsigned char)((len)      & 0xFF);
		header[1] = (unsigned char)((len >> 8) & 0xFF);
		header[2] = 'm';
		header[3] = 'f';

		bytesWritten = fwrite(header, 1, 4, m_file);

		if (bytesWritten != 4)
		{
			LogErr(VB_SEQUENCE, "Error, unable to write fixed header for FSEQ file: %s\n",
				strerror(errno));
			return 0;
		}

		bytesWritten = fwrite(m_mediaFilename.c_str(), 1, mlen, m_file);

		if (bytesWritten != mlen)
		{
			LogErr(VB_SEQUENCE, "Error, unable to write media filename in FSEQ file: %s\n",
				strerror(errno));
			return 0;
		}

		m_filePosition += len;
LogDebug(VB_SEQUENCE, "Wrote %d bytes, total %d\n", len, m_filePosition);
	}

	// If we have a sparse index and it isn't just channels 0 - m_stepSize
	if ((m_seqIndex.size()) &&
		((m_seqIndex.size() > 1) ||
		 ((m_seqIndex[0].offset != 0) || (m_seqIndex[0].count != m_stepSize))))
	{
		int len = 6 + (8 * m_seqIndex.size());

		header[0] = (unsigned char)((len)      & 0xFF);
		header[1] = (unsigned char)((len >> 8) & 0xFF);
		header[2] = 's';
		header[3] = 'i';
		header[4] = (unsigned char)((m_seqIndex.size())      & 0xFF);
		header[5] = (unsigned char)((m_seqIndex.size() >> 8) & 0xFF);
		
		bytesWritten = fwrite(header, 1, 6, m_file);

		if (bytesWritten != 6)
		{
			LogErr(VB_SEQUENCE, "Error, unable to write sparse index header for FSEQ file: %s\n",
				strerror(errno));
			return 0;
		}

		for (unsigned int i = 0; i < m_seqIndex.size(); i++)
		{
			SparseChannelIndex *se = &m_seqIndex[i];

			header[0] = (unsigned char)((se->offset)       & 0xFF);
			header[1] = (unsigned char)((se->offset >>  8) & 0xFF);
			header[2] = (unsigned char)((se->offset >> 16) & 0xFF);
			header[3] = (unsigned char)((se->offset >> 24) & 0xFF);

			header[4] = (unsigned char)((se->count)       & 0xFF);
			header[5] = (unsigned char)((se->count >>  8) & 0xFF);
			header[6] = (unsigned char)((se->count >> 16) & 0xFF);
			header[7] = (unsigned char)((se->count >> 24) & 0xFF);

			bytesWritten = fwrite(header, 1, 8, m_file);

			if (bytesWritten != 8)
			{
				LogErr(VB_SEQUENCE, "Error writing sparse index data: %s\n",
					strerror(errno));

				return 0;
			}
		}

		m_filePosition += len;
	}

	m_chanDataOffset = m_filePosition;

	if (fseek(m_file, FSEQ_CHAN_DATA_OFFSET_OFFSET, SEEK_SET))
	{
		LogErr(VB_SEQUENCE, "Error seeking to header to update Channel Data Offset: %s\n",
			strerror(errno));
		return 0;
	}

	unsigned char cdo[2];
	cdo[0] = (unsigned char)((m_chanDataOffset)      & 0xFF);
	cdo[1] = (unsigned char)((m_chanDataOffset >> 8) & 0xFF);

	bytesWritten = fwrite(cdo, 1, 2, m_file);

	if (bytesWritten != 2)
	{
		LogErr(VB_SEQUENCE, "Error updating Channel Data Offset: %s\n",
			strerror(errno));
		return 0;
	}

	if (fseek(m_file, m_chanDataOffset, SEEK_SET))
	{
		LogErr(VB_SEQUENCE, "Error seeking to Channel Data Offset: %s\n",
			strerror(errno));
		return 0;
	}

	LogDebug(VB_SEQUENCE, "Wrote FSEQ header:\n");
	LogFSEQInfo();

	return 1;
}


/*
 *
 */
int SequenceFile::WriteFrame(void)
{
	LogDebug(VB_SEQUENCE, "WriteFrame()\n");
	int bytesWritten = 0;

	for (unsigned int i = 0; i < m_seqIndex.size(); i++)
	{
		SparseChannelIndex *se = &m_seqIndex[i];

		if (m_scaleBrightness)
		{
			unsigned char *c = (unsigned char *)m_seqData + se->offset;
			for (int i = 0; i < se->count; i++)
			{
				*c = m_bLookup[*c];
				c++;
			}
		}

		bytesWritten = fwrite(m_seqData + se->offset, 1, se->count, m_file);

		if (bytesWritten != se->count)
		{
			LogErr(VB_SEQUENCE, "Error writing sequence data: %s\n",
				strerror(errno));

			return 0;
		}

		m_filePosition += bytesWritten;
LogDebug(VB_SEQUENCE, "Wrote %d bytes, total %d\n", bytesWritten, m_filePosition);
	}

	m_numSteps++;

	return 1;
}


/*
 *
 */
int SequenceFile::Copy(SequenceFile *source)
{
	LogDebug(VB_SEQUENCE, "Copy(%s)\n", source->GetFilename().c_str());
	int writeResult = 1;

	while (source->ReadFrame() && writeResult)
	{
		for (unsigned int i = 0; i < m_seqIndex.size(); i++)
		{
			SparseChannelIndex *se = &m_seqIndex[i];

			memcpy(m_seqData + se->offset, source->m_seqData + se->offset,
				se->count);
		}

		writeResult = WriteFrame();
	}

	return writeResult;
}


/*
 *
 */
int SequenceFile::UpdateNumSteps(void)
{
	LogDebug(VB_SEQUENCE, "UpdateNumSteps(): = %d\n", m_numSteps);

	unsigned char header[4];

	// Number of Steps (duration)
	header[0] = (unsigned char)((m_numSteps)       & 0xFF);
	header[1] = (unsigned char)((m_numSteps >>  8) & 0xFF);
	header[2] = (unsigned char)((m_numSteps >> 16) & 0xFF);
	header[3] = (unsigned char)((m_numSteps >> 24) & 0xFF);

	if (fseek(m_file, FSEQ_DURATION_OFFSET, SEEK_SET))
	{
		LogErr(VB_SEQUENCE, "Error seeking to header to update Channel Data Offset: %s\n",
			strerror(errno));
		return 0;
	}

	int bytesWritten = fwrite(header, 1, 4, m_file);

	if (bytesWritten != 4)
	{
		LogErr(VB_SCHEDULE, "Error updating NumSteps in FSEQ header: %s\n",
			strerror(errno));
		return 0;
	}

	return 1;
}


/*
 *
 */
void SequenceFile::ScaleBrightness(int scale)
{
	m_scaleBrightness = scale;

	for (int i = 0; i < 256; i++)
	{
		m_bLookup[i] = (unsigned char)(i * 1.0 * scale / 100.0);
	}
}

/*
 *
 */
void SequenceFile::LogFSEQInfo(void)
{
	LogDebug(VB_SEQUENCE, "Sequence File Information\n");
	LogDebug(VB_SEQUENCE, "seqFilename           : %s\n", m_filename.c_str());
	LogDebug(VB_SEQUENCE, "seqVersion            : %d.%d\n",
		m_versionMajor, m_versionMinor);
	LogDebug(VB_SEQUENCE, "seqChanDataOffset     : %d\n", m_chanDataOffset);
	LogDebug(VB_SEQUENCE, "seqFixedHeaderSize    : %d\n", m_fixedHeaderSize);
	LogDebug(VB_SEQUENCE, "seqStepSize           : %d\n", m_stepSize);
	LogDebug(VB_SEQUENCE, "seqNumSteps           : %d\n", m_numSteps);
	LogDebug(VB_SEQUENCE, "seqStepTime           : %dms\n", m_stepTime);
	LogDebug(VB_SEQUENCE, "seqNumUniverses       : %d *\n", m_numUniverses);
	LogDebug(VB_SEQUENCE, "seqUniverseSize       : %d *\n", m_universeSize);
	LogDebug(VB_SEQUENCE, "seqGamma              : %d *\n", m_gamma);
	LogDebug(VB_SEQUENCE, "seqColorEncoding      : %d *\n", m_colorEncoding);
	LogDebug(VB_SEQUENCE, "seqRefreshRate        : %d\n", m_refreshRate);
	LogDebug(VB_SEQUENCE, "seqFileSize           : %lu\n", m_fileSize);
	LogDebug(VB_SEQUENCE, "seqDuration           : %d\n", m_duration);

	if (m_version > 0x0100)
	{
		if (m_mediaFilename.size())
			LogDebug(VB_SEQUENCE, "mediaFilename         : %s\n", m_mediaFilename.c_str());

		if (m_seqIndex.size())
		{
			LogDebug(VB_SEQUENCE, "seqIndex              : \n");

			//for (SparseChannelIndex se : m_seqIndex)
			for (unsigned int i = 0; i < m_seqIndex.size(); i++)
			{
				SparseChannelIndex *se = &m_seqIndex[i];

				LogDebug(VB_SEQUENCE, "                        : %d @ %d\n",
					se->count, se->offset);
			}
		}
	}

	LogDebug(VB_SEQUENCE, "'*' denotes field is currently ignored by FPP\n");
}


