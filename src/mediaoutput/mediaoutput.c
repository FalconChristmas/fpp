/*
 *   Media handler for Falcon Player (FPP)
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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <set>

#include <boost/algorithm/string.hpp>

#include "log.h"
#include "common.h"
#include "mediaoutput.h"
#include "mpg123.h"
#include "MultiSync.h"
#include "ogg123.h"
#include "omxplayer.h"
#include "SDLOut.h"
#include "Sequence.h"
#include "settings.h"


/////////////////////////////////////////////////////////////////////////////
MediaOutputBase *mediaOutput = 0;
pthread_mutex_t  mediaOutputLock;
float            masterMediaPosition = 0.0;

MediaOutputStatus mediaOutputStatus = {
	MEDIAOUTPUTSTATUS_IDLE, //status
	};

void MediaOutput_sigchld_handler(int signal)
{
	int status;
	pid_t p = waitpid(-1, &status, WNOHANG);

	pthread_mutex_lock(&mediaOutputLock);
	if (!mediaOutput) {
		pthread_mutex_unlock(&mediaOutputLock);
		return;
	}

	LogDebug(VB_MEDIAOUT,
		"MediaOutput_sigchld_handler(): pid: %d, waiting for %d\n",
		p, mediaOutput->m_childPID);

	if (p == mediaOutput->m_childPID)
	{
		mediaOutput->Close();
		mediaOutput->m_childPID = 0;

		pthread_mutex_unlock(&mediaOutputLock);

        
        if (getFPPmode() != REMOTE_MODE)  {
            if ((sequence->m_seqMSRemaining > 0) &&
                (sequence->m_seqMSRemaining < 2000)) {
                usleep(sequence->m_seqMSRemaining * 1000);
            }

            // Always sleep an extra 100ms to let the sequence finish since playlist watches the media output
            if (sequence->IsSequenceRunning())
            usleep(100000);
        }


		mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;
		CloseMediaOutput();

        //don't close the sequence in remote mode, a separate stop will come from the master for that
		if (getFPPmode() != REMOTE_MODE && sequence->IsSequenceRunning())
			sequence->CloseSequenceFile();

	} else {
		pthread_mutex_unlock(&mediaOutputLock);
	}
}

/*
 *
 */
void InitMediaOutput(void)
{
	if (pthread_mutex_init(&mediaOutputLock, NULL) != 0) {
		LogDebug(VB_MEDIAOUT, "ERROR: Media Output mutex init failed!\n");
	}

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = MediaOutput_sigchld_handler;
	sigaction(SIGCHLD, &sa, NULL);
}

/*
 *
 */
void CleanupMediaOutput(void)
{
	CloseMediaOutput();

	pthread_mutex_destroy(&mediaOutputLock);
}

std::string GetVideoFilenameForMedia(const std::string &filename, std::string &ext) {
    ext = "";
    std::string result("");
    std::size_t found = filename.find_last_of(".");
    std::string oext = filename.substr(found + 1);
    std::string lext = boost::algorithm::to_lower_copy(oext);
    std::string bfile = filename.substr(0, found + 1);
    std::string videoPath(getVideoDirectory());
    videoPath += "/";
    videoPath += bfile;

    if ((lext == "mp4") || (lext == "avi") || (lext == "mov") || (lext == "mkv")) {
        if (FileExists(videoPath + oext)) {
            ext = lext;
            result = bfile + oext;
        } else if (FileExists(videoPath + lext)) {
            ext = lext;
            result = bfile + lext;
        }
    } else if ((lext == "mp3") || (lext == "ogg") || (lext == "m4a")) {
        if (FileExists(videoPath + "mp4")) {
            ext = "mp4";
            result = bfile + "mp4";
        } else if (FileExists(videoPath + "MP4")) {
            ext = "mp4";
            result = bfile + "MP4";
        } else if (FileExists(videoPath + "avi")) {
            ext = "avi";
            result = bfile + "avi";
        } else if (FileExists(videoPath + "AVI")) {
            ext = "avi";
            result = bfile + "AVI";
        } else if (FileExists(videoPath + "mov")) {
            ext = "mov";
            result = bfile + "mov";
        } else if (FileExists(videoPath + "MOV")) {
            ext = "mov";
            result = bfile + "MOV";
        } else if (FileExists(videoPath + "mkv")) {
            ext = "mkv";
            result = bfile + "mkv";
        } else if (FileExists(videoPath + "MKV")) {
            ext = "mkv";
            result = bfile + "MKV";
        }
    }

    return result;
}

bool HasVideoForMedia(char *filename) {
    std::string ext;
    std::string fp = GetVideoFilenameForMedia(filename, ext);
    if (fp != "") {
        strcpy(filename, fp.c_str());
    }
    return fp != "";
}


static std::set<std::string> alreadyWarned;
/*
 *
 */
int OpenMediaOutput(char *filename) {
	LogDebug(VB_MEDIAOUT, "OpenMediaOutput(%s)\n", filename);

	pthread_mutex_lock(&mediaOutputLock);
	if (mediaOutput) {
		pthread_mutex_unlock(&mediaOutputLock);
		CloseMediaOutput();
	}
	pthread_mutex_unlock(&mediaOutputLock);

    std::string tmpFile(filename);
    std::size_t found = tmpFile.find_last_of(".");
    if (found == std::string::npos) {
        LogDebug(VB_MEDIAOUT, "Unable to determine extension of media file %s\n",
                 tmpFile.c_str());
        return 0;
    }
    std::string ext = boost::algorithm::to_lower_copy(tmpFile.substr(found + 1));

	if (getFPPmode() == REMOTE_MODE) {
		// For v1.0 MultiSync, we can't sync audio to audio, so check for
		// a video file if the master is playing an audio file
        tmpFile = GetVideoFilenameForMedia(tmpFile, ext);
        
        if (tmpFile == "") {
            //video doesn't exist, punt
            tmpFile = filename;
            if (alreadyWarned.find(tmpFile) == alreadyWarned.end()) {
                alreadyWarned.emplace(tmpFile);
                LogInfo(VB_MEDIAOUT, "No video found for remote playing of %s\n", filename);
            }
            return 0;
        } else {
            LogDebug(VB_MEDIAOUT,
                     "Master is playing %s audio, remote will try %s Video\n",
                     filename, tmpFile.c_str());
        }
	}

    pthread_mutex_lock(&mediaOutputLock);

    std::string vOut = getSetting("VideoOutput");
    if (vOut == "") {
#if !defined(PLATFORM_BBB)
        vOut = "--HDMI--";
#else
        vOut = "--Disabled--";
#endif
    }
    
#if !defined(PLATFORM_BBB)
    // BBB doesn't have mpg123 installed
	if (getSettingInt("LegacyMediaOutputs")
        && (ext == "mp3" || ext == "ogg")) {
		if (ext == "mp3") {
			mediaOutput = new mpg123Output(tmpFile, &mediaOutputStatus);
		} else if (ext == "ogg") {
			mediaOutput = new ogg123Output(tmpFile, &mediaOutputStatus);
		}
    } else
#endif
    if ((ext == "mp3") ||
        (ext == "m4a") ||
        (ext == "ogg")) {
        mediaOutput = new SDLOutput(tmpFile, &mediaOutputStatus, "--Disabled--");
#ifdef PLATFORM_PI
	} else if (((ext == "mp4") ||
                (ext == "mkv") ||
                (ext == "mov") ||
                (ext == "avi")) && vOut == "--HDMI--") {
		mediaOutput = new omxplayerOutput(tmpFile, &mediaOutputStatus);
#endif
    } else if ((ext == "mp4") ||
               (ext == "mkv") ||
               (ext == "mov") ||
               (ext == "avi")) {
        mediaOutput = new SDLOutput(tmpFile, &mediaOutputStatus, vOut);
	} else {
		pthread_mutex_unlock(&mediaOutputLock);
		LogErr(VB_MEDIAOUT, "No Media Output handler for %s\n", tmpFile);
		return 0;
	}

	if (!mediaOutput)
	{
		pthread_mutex_unlock(&mediaOutputLock);
		return 0;
	}

	if (getFPPmode() == MASTER_MODE)
		multiSync->SendMediaSyncStartPacket(mediaOutput->m_mediaFilename.c_str());

	if (!mediaOutput->Start())
	{
        LogErr(VB_MEDIAOUT, "Could not start media %s\n", tmpFile);
		delete mediaOutput;
		mediaOutput = 0;
		pthread_mutex_unlock(&mediaOutputLock);
		return 0;
	}

	mediaOutputStatus.speedDelta = 0;
    mediaOutputStatus.speedDeltaCount = 0;

	pthread_mutex_unlock(&mediaOutputLock);

	return 1;
}

void CloseMediaOutput(void) {
	LogDebug(VB_MEDIAOUT, "CloseMediaOutput()\n");

	mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;

	pthread_mutex_lock(&mediaOutputLock);
	if (!mediaOutput) {
		pthread_mutex_unlock(&mediaOutputLock);
		return;
	}

	if (mediaOutput->IsPlaying())
	{
		pthread_mutex_unlock(&mediaOutputLock);
		mediaOutput->Stop();
		pthread_mutex_lock(&mediaOutputLock);
	}

	if (getFPPmode() == MASTER_MODE)
		multiSync->SendMediaSyncStopPacket(mediaOutput->m_mediaFilename.c_str());

	delete mediaOutput;
	mediaOutput = 0;

	pthread_mutex_unlock(&mediaOutputLock);
}

void CheckCurrentPositionAgainstMaster(void)
{
	int diff = (int)(mediaOutputStatus.mediaSeconds * 1000)
				- (int)(masterMediaPosition * 1000);
	int i = 0;

	if (!mediaOutput)
		return;

	// Allow faster sync in first 10 seconds
	int maxDelta = (mediaOutputStatus.mediaSeconds < 10) ? 15 : 3;
	int desiredDelta = diff / -50;

	if (desiredDelta > maxDelta)
		desiredDelta = maxDelta;
	else if (desiredDelta < (0 - maxDelta))
		desiredDelta = 0 - maxDelta;


	LogDebug(VB_MEDIAOUT, "Master: %.2f, Local: %.2f, Diff: %dms, delta: %d, new: %d\n",
		masterMediaPosition, mediaOutputStatus.mediaSeconds, diff,
		mediaOutputStatus.speedDelta, desiredDelta);

	// Can't adjust speed if not playing yet
	if (mediaOutputStatus.mediaSeconds < 0.01)
		return;
    
    if ((desiredDelta == -1 || desiredDelta == 1) && mediaOutputStatus.speedDelta == 0 && mediaOutputStatus.speedDeltaCount < 3) {
        //a small change, but lets delay implementing slightly as it could just be
        //transient network issue or similar, if still need a delta at next sync, then do it
        mediaOutputStatus.speedDeltaCount++;
        desiredDelta = 0;
    } else if (desiredDelta < 0 && mediaOutputStatus.speedDelta > 0) {
        //if going from too slow to to too fast (or vice versa), only do a small step across
        //to not overshoot
        desiredDelta = -1;
    } else if (desiredDelta > 0 && mediaOutputStatus.speedDelta < 0) {
        desiredDelta = 1;
    } else {
        mediaOutputStatus.speedDeltaCount = 0;
    }
    

	if (desiredDelta)
	{
		if (mediaOutputStatus.speedDelta < desiredDelta)
		{
			while (mediaOutputStatus.speedDelta < desiredDelta)
			{
				pthread_mutex_lock(&mediaOutputLock);
				if (!mediaOutput)
				{
					pthread_mutex_unlock(&mediaOutputLock);
					return;
				}
				mediaOutput->AdjustSpeed(1);
				pthread_mutex_unlock(&mediaOutputLock);
				mediaOutputStatus.speedDelta++;

				if (mediaOutputStatus.speedDelta < desiredDelta)
					usleep(30000);
			}
		}
		else if (mediaOutputStatus.speedDelta > desiredDelta)
		{
			while (mediaOutputStatus.speedDelta > desiredDelta)
			{
				pthread_mutex_lock(&mediaOutputLock);
				if (!mediaOutput)
				{
					pthread_mutex_unlock(&mediaOutputLock);
					return;
				}
				mediaOutput->AdjustSpeed(-1);
				pthread_mutex_unlock(&mediaOutputLock);
				mediaOutputStatus.speedDelta--;

				if (mediaOutputStatus.speedDelta > desiredDelta)
					usleep(30000);
			}
		}
	}
	else
	{
		pthread_mutex_lock(&mediaOutputLock);
		if (!mediaOutput)
		{
			pthread_mutex_unlock(&mediaOutputLock);
			return;
		}

		if (mediaOutputStatus.speedDelta != 0)
			mediaOutput->AdjustSpeed(0);

		pthread_mutex_unlock(&mediaOutputLock);

		mediaOutputStatus.speedDelta = 0;
	}
}

void UpdateMasterMediaPosition(float seconds)
{
	if (getFPPmode() != REMOTE_MODE)
		return;

	masterMediaPosition = seconds;

	CheckCurrentPositionAgainstMaster();
}


