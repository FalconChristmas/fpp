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

// This #define must be before any #include's
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "E131.h"
#include "channeloutputthread.h"
#include "common.h"
#include "events.h"
#include "effects.h"
#include "fpp.h" // for FPPstatus && #define-d status values
#include "fppd.h"
#include "log.h"
#include "MultiSync.h"
#include "PixelOverlay.h"
#include "Sequence.h"
#include "settings.h"

#include "mediaoutput/SDLOut.h"

Sequence *sequence = NULL;

extern int minimumNeededChannel;
extern int maximumNeededChannel;

Sequence::Sequence()
  : m_seqFileSize(0),
    m_seqDuration(0),
    m_seqSecondsElapsed(0),
    m_seqSecondsRemaining(0),
    m_seqMSRemaining(0),
    m_seqFile(NULL),
    m_seqFilePosition(0),
    m_seqStarting(0),
    m_seqPaused(0),
    m_seqSingleStep(0),
    m_seqSingleStepBack(0),
    m_seqVersionMajor(0),
    m_seqVersionMinor(0),
    m_seqVersion(0),
    m_seqChanDataOffset(0),
    m_seqFixedHeaderSize(0),
    m_seqStepSize(8192),
    m_seqStepTime(50),
    m_seqNumPeriods(0),
    m_seqRefreshRate(20),
    m_seqNumUniverses(0),
    m_seqUniverseSize(0),
    m_seqGamma(0),
    m_seqColorEncoding(0),
    m_seqLastControlMajor(0),
    m_seqLastControlMinor(0),
    m_remoteBlankCount(0)
{
    m_seqFilename[0] = 0;

    pthread_mutexattr_init(&m_sequenceLock_attr);
    pthread_mutexattr_settype(&m_sequenceLock_attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&m_sequenceLock, &m_sequenceLock_attr);
}

Sequence::~Sequence()
{
    pthread_mutex_destroy(&m_sequenceLock);
}

/*
 *
 */

int Sequence::OpenSequenceFile(const char *filename, int startSeconds) {
    LogDebug(VB_SEQUENCE, "OpenSequenceFile(%s, %d)\n", filename, startSeconds);

    if (!filename || !filename[0])
    {
        LogErr(VB_SEQUENCE, "Empty Sequence Filename!\n", filename);
        return 0;
    }

    size_t bytesRead = 0;

    pthread_mutex_lock(&m_sequenceLock);

    m_seqFileSize = 0;

    if (IsSequenceRunning())
        CloseSequenceFile();

    m_seqStarting = 1;
    m_seqPaused   = 0;
    m_seqDuration = 0;
    m_seqSecondsElapsed = 0;
    m_seqSecondsRemaining = 0;
    SetChannelOutputFrameNumber(0);

    strcpy(m_seqFilename, filename);

    char tmpFilename[2048];
    unsigned char tmpData[2048];
    strcpy(tmpFilename,(const char *)getSequenceDirectory());
    strcat(tmpFilename,"/");
    strcat(tmpFilename, filename);

    if (getFPPmode() == REMOTE_MODE)
        CheckForHostSpecificFile(getSetting("HostName"), tmpFilename);

    if (!FileExists(tmpFilename))
    {
        if (getFPPmode() == REMOTE_MODE)
            LogDebug(VB_SEQUENCE, "Sequence file %s does not exist\n", tmpFilename);
        else
            LogErr(VB_SEQUENCE, "Sequence file %s does not exist\n", tmpFilename);

        m_seqStarting = 0;

        pthread_mutex_unlock(&m_sequenceLock);

        return 0;
    }

    m_seqFile = fopen((const char *)tmpFilename, "r");
    if (m_seqFile == NULL) 
    {
        LogErr(VB_SEQUENCE, "Error opening sequence file: %s. fopen returned NULL\n",
            tmpFilename);
        m_seqStarting = 0;

        pthread_mutex_unlock(&m_sequenceLock);

        return 0;
    }
    
    posix_fadvise(fileno(m_seqFile), 0, 0, POSIX_FADV_SEQUENTIAL);
    // Preload a chunk
    posix_fadvise(fileno(m_seqFile), 0, 1024*1024, POSIX_FADV_WILLNEED);


    if (getFPPmode() == MASTER_MODE)
    {
        pthread_mutex_unlock(&m_sequenceLock);
        multiSync->SendSeqSyncStartPacket(filename);

        // Give the remotes a head start spining up so they are ready
        usleep(100000);
        pthread_mutex_lock(&m_sequenceLock);
    }

    ///////////////////////////////////////////////////////////////////////
    // Check 4-byte File format identifier
    char seqFormatID[5];
    strcpy(seqFormatID, "    ");
    bytesRead = fread(seqFormatID, 1, 4, m_seqFile);
    seqFormatID[4] = 0;
    if ((bytesRead != 4) || (strcmp(seqFormatID, "PSEQ") && strcmp(seqFormatID, "FSEQ")))
    {
        LogErr(VB_SEQUENCE, "Error opening sequence file: %s. Incorrect File Format header: '%s', bytesRead: %d\n",
            filename, seqFormatID, bytesRead);

        fseeko(m_seqFile, 0L, SEEK_SET);
        bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, m_seqFile);
        HexDump("Sequence File head:", tmpData, bytesRead);

        fclose(m_seqFile);
        m_seqFile = NULL;
        m_seqStarting = 0;

        pthread_mutex_unlock(&m_sequenceLock);

        return 0;
    }

    ///////////////////////////////////////////////////////////////////////
    // Get Channel Data Offset
    bytesRead = fread(tmpData, 1, 2, m_seqFile);
    if (bytesRead != 2)
    {
        LogErr(VB_SEQUENCE, "Sequence file %s too short, unable to read channel data offset value\n", filename);

        fseeko(m_seqFile, 0L, SEEK_SET);
        bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, m_seqFile);
        HexDump("Sequence File head:", tmpData, bytesRead);

        fclose(m_seqFile);
        m_seqFile = NULL;
        m_seqStarting = 0;

        pthread_mutex_unlock(&m_sequenceLock);

        return 0;
    }
    m_seqChanDataOffset = tmpData[0] + (tmpData[1] << 8);

    ///////////////////////////////////////////////////////////////////////
    // Now that we know the header size, read the whole header in one shot
    fseeko(m_seqFile, 0L, SEEK_SET);
    bytesRead = fread(tmpData, 1, m_seqChanDataOffset, m_seqFile);
    if (bytesRead != m_seqChanDataOffset)
    {
        LogErr(VB_SEQUENCE, "Sequence file %s too short, unable to read fixed header size value\n", filename);

        fseeko(m_seqFile, 0L, SEEK_SET);
        bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, m_seqFile);
        HexDump("Sequence File head:", tmpData, bytesRead);

        fclose(m_seqFile);
        m_seqFile = NULL;
        m_seqStarting = 0;

        pthread_mutex_unlock(&m_sequenceLock);

        return 0;
    }

    m_seqVersionMinor = tmpData[6];
    m_seqVersionMajor = tmpData[7];
    m_seqVersion      = (m_seqVersionMajor * 256) + m_seqVersionMinor;

    m_seqFixedHeaderSize =
        (tmpData[8])        + (tmpData[9] << 8);

    m_seqStepSize =
        (tmpData[10])       + (tmpData[11] << 8) +
        (tmpData[12] << 16) + (tmpData[13] << 24);

    m_seqNumPeriods =
        (tmpData[14])       + (tmpData[15] << 8) +
        (tmpData[16] << 16) + (tmpData[17] << 24);

    m_seqStepTime =
        (tmpData[18])       + (tmpData[19] << 8);

    m_seqNumUniverses = 
        (tmpData[20])       + (tmpData[21] << 8);

    m_seqUniverseSize = 
        (tmpData[22])       + (tmpData[23] << 8);

    m_seqGamma         = tmpData[24];
    m_seqColorEncoding = tmpData[25];

    // End of v1.0 fields
    if (m_seqVersion > 0x0100)
    {
    }

    m_seqRefreshRate = 1000 / m_seqStepTime;

    fseeko(m_seqFile, 0L, SEEK_END);
    m_seqFileSize = ftello(m_seqFile);

    // Calculate actual duration based on file size, not m_seqNumPeriods
    m_seqMSRemaining = (int)(((float)(m_seqFileSize - m_seqChanDataOffset)
        / (float)m_seqStepSize) * m_seqStepTime);
    m_seqDuration = (int)((float)(m_seqFileSize - m_seqChanDataOffset)
        / ((float)m_seqStepSize * (float)m_seqRefreshRate));

    m_seqSecondsRemaining = m_seqDuration;
    fseeko(m_seqFile, m_seqChanDataOffset, SEEK_SET);
    m_seqFilePosition = m_seqChanDataOffset;

    LogDebug(VB_SEQUENCE, "Sequence File Information\n");
    LogDebug(VB_SEQUENCE, "seqFilename           : %s\n", m_seqFilename);
    LogDebug(VB_SEQUENCE, "seqVersion            : %d.%d\n",
        m_seqVersionMajor, m_seqVersionMinor);
    LogDebug(VB_SEQUENCE, "seqFormatID           : %s\n", seqFormatID);
    LogDebug(VB_SEQUENCE, "seqChanDataOffset     : %d\n", m_seqChanDataOffset);
    LogDebug(VB_SEQUENCE, "seqFixedHeaderSize    : %d\n", m_seqFixedHeaderSize);
    LogDebug(VB_SEQUENCE, "seqStepSize           : %d\n", m_seqStepSize);
    LogDebug(VB_SEQUENCE, "seqNumPeriods         : %d\n", m_seqNumPeriods);
    LogDebug(VB_SEQUENCE, "seqStepTime           : %dms\n", m_seqStepTime);
    LogDebug(VB_SEQUENCE, "seqNumUniverses       : %d *\n", m_seqNumUniverses);
    LogDebug(VB_SEQUENCE, "seqUniverseSize       : %d *\n", m_seqUniverseSize);
    LogDebug(VB_SEQUENCE, "seqGamma              : %d *\n", m_seqGamma);
    LogDebug(VB_SEQUENCE, "seqColorEncoding      : %d *\n", m_seqColorEncoding);
    LogDebug(VB_SEQUENCE, "seqRefreshRate        : %d\n", m_seqRefreshRate);
    LogDebug(VB_SEQUENCE, "seqFileSize           : %lld\n", m_seqFileSize);
    LogDebug(VB_SEQUENCE, "seqDuration           : %d\n", m_seqDuration);
    LogDebug(VB_SEQUENCE, "seqMSRemaining        : %d\n", m_seqMSRemaining);
    LogDebug(VB_SEQUENCE, "'*' denotes field is currently ignored by FPP\n");

    m_seqPaused = 0;
    m_seqSingleStep = 0;
    m_seqSingleStepBack = 0;

    int frameNumber = 0;

    if (startSeconds)
    {
        int frameNumber = startSeconds * m_seqRefreshRate;
        off_t newPos = m_seqChanDataOffset;
        newPos += (frameNumber * m_seqStepSize);
        LogDebug(VB_SEQUENCE, "Seeking to byte %d in %s\n", (int)newPos, m_seqFilename);

        fseeko(m_seqFile, newPos, SEEK_SET);
        m_seqFilePosition = newPos;
        m_seqMSRemaining -= (startSeconds * 1000);
    }

    ReadSequenceData();

    SetChannelOutputFrameNumber(frameNumber);

    SetChannelOutputRefreshRate(m_seqRefreshRate);
    StartChannelOutputThread();

    m_seqStarting = 0;

    pthread_mutex_unlock(&m_sequenceLock);

    return 1;
}

int Sequence::SeekSequenceFile(int frameNumber) {
    LogDebug(VB_SEQUENCE, "SeekSequenceFile(%d)\n", frameNumber);

    pthread_mutex_lock(&m_sequenceLock);

    if (!IsSequenceRunning())
    {
        LogErr(VB_SEQUENCE, "No sequence is running\n");
        pthread_mutex_unlock(&m_sequenceLock);
        return 0;
    }

    off_t newPos = m_seqChanDataOffset;
    newPos += (frameNumber * m_seqStepSize);
    LogDebug(VB_SEQUENCE, "Seeking to byte %d in %s\n", (int)newPos, m_seqFilename);

    fseeko(m_seqFile, newPos, SEEK_SET);

    m_seqFilePosition = newPos;
    m_seqMSRemaining = (int)(((float)(m_seqFileSize - newPos)
        / (float)m_seqStepSize) * m_seqStepTime);

    ReadSequenceData();

    SetChannelOutputFrameNumber(frameNumber);

    pthread_mutex_unlock(&m_sequenceLock);
}


char *Sequence::CurrentSequenceFilename(void) {
    return m_seqFilename;
}

int Sequence::IsSequenceRunning(void) {
    if (m_seqFile && !m_seqStarting)
        return 1;

    return 0;
}

int Sequence::IsSequenceRunning(char *filename) {
    int result = 0;

    pthread_mutex_lock(&m_sequenceLock);

    if ((!strcmp(sequence->m_seqFilename, filename)) && m_seqFile)
        result = 1;

    pthread_mutex_unlock(&m_sequenceLock);

    return result;
}

void Sequence::BlankSequenceData(void) {
    memset(m_seqData, 0, sizeof(m_seqData));
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

void Sequence::SingleStepSequenceBack(void) {
    m_seqSingleStepBack = 1;
}

void Sequence::ReadSequenceData(void) {
    LogExcess(VB_SEQUENCE, "ReadSequenceData()\n");
    size_t  bytesRead = 0;

    pthread_mutex_lock(&m_sequenceLock);

    if (m_seqStarting) {
        pthread_mutex_unlock(&m_sequenceLock);
        return;
    }

    if ((IsSequenceRunning()) && (m_seqPaused)) {
        if (m_seqSingleStep) {
            m_seqSingleStep = 0;
        } else if (m_seqSingleStepBack) {
            m_seqSingleStepBack = 0;

            int offset = m_seqStepSize * 2;
            if (m_seqFilePosition > offset) {
                fseeko(m_seqFile, 0 - offset, SEEK_CUR);
                m_seqMSRemaining += m_seqStepTime;
            }
        }
        else
        {
            pthread_mutex_unlock(&m_sequenceLock);

            return;
        }
    }

    if (IsSequenceRunning()) {
        m_remoteBlankCount = 0;
        bytesRead = 0;
        if(m_seqFilePosition <= m_seqFileSize - m_seqStepSize) {
            int maxChanToRead = m_seqStepSize - 1;
            if (maximumNeededChannel > 0 && maximumNeededChannel < m_seqStepSize) {
                maxChanToRead = maximumNeededChannel;
            }

            if (maximumNeededChannel <= 0) {
                fseeko(m_seqFile, m_seqStepSize, SEEK_CUR);
                bytesRead = m_seqStepSize;
            } else {
                if (minimumNeededChannel) {
                    fseeko(m_seqFile, minimumNeededChannel, SEEK_CUR);
                    bytesRead = minimumNeededChannel;
                }
                bytesRead += fread(&m_seqData[minimumNeededChannel], 1, maxChanToRead - minimumNeededChannel + 1, m_seqFile);
                if (bytesRead < m_seqStepSize) {
                    fseeko(m_seqFile, m_seqStepSize - bytesRead, SEEK_CUR);
                    bytesRead += m_seqStepSize - bytesRead;
                }
            }
            off_t fsz = ftello(m_seqFile);
            if (fsz > 4096) {
                //make sure the current memory page stays so we don't reload it
                //don't need up to this anymore, discard it
                posix_fadvise(fileno(m_seqFile), 0, fsz - 4096, POSIX_FADV_DONTNEED);
            }
            
            //let the kernel know we're going to need the next few frames
            int sizeToRead = maxChanToRead - minimumNeededChannel + 1;
            for (int x = 0; x < 3; x++) {
                posix_fadvise(fileno(m_seqFile), fsz + x * m_seqStepSize + minimumNeededChannel, sizeToRead, POSIX_FADV_WILLNEED);
            }
            m_seqFilePosition += bytesRead;
        }

        if (bytesRead != m_seqStepSize)
            CloseSequenceFile();
        else
            m_seqMSRemaining -= m_seqStepTime;

        m_seqSecondsElapsed = (int)((float)(m_seqFilePosition - m_seqChanDataOffset)/((float)m_seqStepSize*(float)m_seqRefreshRate));
        m_seqSecondsRemaining = m_seqDuration - m_seqSecondsElapsed;
    } else {
        if (getFPPmode() != REMOTE_MODE || getSettingInt("blankBetweenSequences")) {
            BlankSequenceData();
        } else {
            //on a remote, we will get a "stop" and then a "start" a short time later
            //we don't want to blank immediately (unless the master tells us to)
            //so we don't get black blinks on the remote.  We'll wait the equivalent
            //of 5 frames to then blank
            if (m_remoteBlankCount > 5) {
                BlankSequenceData();
            } else {
                m_remoteBlankCount++;
            }
        }
    }

    pthread_mutex_unlock(&m_sequenceLock);
}

void Sequence::ProcessSequenceData(int ms, int checkControlChannels) {
    if (IsEffectRunning())
        OverlayEffects(m_seqData);

    if (SDLOutput::IsOverlayingVideo()) {
        SDLOutput::ProcessVideoOverlay(ms);
    }
    if (UsingMemoryMapInput())
        OverlayMemoryMap(m_seqData);

    if (checkControlChannels && getControlMajor() && getControlMinor())
    {
        char thisMajor = NormalizeControlValue(m_seqData[getControlMajor()-1]);
        char thisMinor = NormalizeControlValue(m_seqData[getControlMinor()-1]);

        if ((m_seqLastControlMajor != thisMajor) ||
            (m_seqLastControlMinor != thisMinor))
        {
            m_seqLastControlMajor = thisMajor;
            m_seqLastControlMinor = thisMinor;

            if (m_seqLastControlMajor && m_seqLastControlMinor)
                TriggerEvent(m_seqLastControlMajor, m_seqLastControlMinor);
        }
    }

    if (channelTester->Testing())
        channelTester->OverlayTestData(m_seqData);
    
    PrepareChannelData(m_seqData);
}

void Sequence::SendSequenceData(void) {
    SendChannelData(m_seqData);
}

void Sequence::SendBlankingData(void) {
    LogDebug(VB_SEQUENCE, "SendBlankingData()\n");
    usleep(100000);

    if (getFPPmode() == MASTER_MODE)
        multiSync->SendBlankingDataPacket();

    BlankSequenceData();
    ProcessSequenceData(0, 0);
    SendSequenceData();
}

void Sequence::CloseIfOpen(char *filename) {
    pthread_mutex_lock(&m_sequenceLock);

    if (!strcmp(m_seqFilename, filename))
        CloseSequenceFile();

    pthread_mutex_unlock(&m_sequenceLock);
}

void Sequence::CloseSequenceFile(void) {
    LogDebug(VB_SEQUENCE, "CloseSequenceFile() %s\n", m_seqFilename);

    if (getFPPmode() == MASTER_MODE)
        multiSync->SendSeqSyncStopPacket(m_seqFilename);

    pthread_mutex_lock(&m_sequenceLock);

    if (m_seqFile) {
        fclose(m_seqFile);
        m_seqFile = NULL;
    }

    m_seqFilename[0] = '\0';
    m_seqPaused = 0;

    if ((!IsEffectRunning()) &&
        ((getFPPmode() != REMOTE_MODE) &&
         (FPPstatus != FPP_STATUS_PLAYLIST_PLAYING)) ||
        (getSettingInt("blankBetweenSequences")))
        SendBlankingData();

    pthread_mutex_unlock(&m_sequenceLock);
}

/*
 * Normalize control channel values into buckets
 */
char Sequence::NormalizeControlValue(char in) {
    char result = (char)(((unsigned char)in + 5) / 10);

    if (result == 26)
        return 25;

    return result;
}

