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

#include "log.h"
#include "common.h"
#include "mediaoutput.h"
#include "MultiSync.h"
#include "omxplayer.h"

#ifdef HASVLC
    #include "VLCOut.h"
#endif
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

/*
 *
 */
void InitMediaOutput(void)
{
	if (pthread_mutex_init(&mediaOutputLock, NULL) != 0) {
		LogDebug(VB_MEDIAOUT, "ERROR: Media Output mutex init failed!\n");
	}

    int vol = getSettingInt("volume", -1);
    if (vol < 0) {
        vol = 70;
    }
    setVolume(vol);
}

/*
 *
 */
void CleanupMediaOutput(void)
{
	CloseMediaOutput();

	pthread_mutex_destroy(&mediaOutputLock);
}

static int volume = 70;
int getVolume() {
    return volume;
}

void setVolume(int vol)
{
    char buffer [60];
    
    if ( vol < 0 )
        vol = 0;
    else if ( vol > 100 )
        vol = 100;
    volume = vol;

    const char *mixerDeviceC = getSetting("AudioMixerDevice");
    std::string mixerDevice = "PCM";
    if (mixerDeviceC) {
        mixerDevice = mixerDevice;
    }
    int   audioOutput = getSettingInt("AudioOutput");
    std::string audio0Type = getSetting("AudioCard0Type");
    
    float fvol = volume;
#ifdef PLATFORM_PI
    if (audioOutput == 0 && audio0Type == "bcm2") {
        fvol = volume;
        fvol /= 2;
        fvol += 50;
    }
#endif
    snprintf(buffer, 60, "amixer set -c %d %s -- %.2f%% >/dev/null 2>&1",
             audioOutput, mixerDevice.c_str(), fvol);
    
    LogDebug(VB_SETTING,"Volume change: %d \n", volume);
    LogDebug(VB_MEDIAOUT,"Calling amixer to set the volume: %s \n", buffer);
    system(buffer);

    pthread_mutex_lock(&mediaOutputLock);
    if (mediaOutput)
        mediaOutput->SetVolume(volume);
    
    pthread_mutex_unlock(&mediaOutputLock);
}

static std::set<std::string> AUDIO_EXTS = {
    "mp3", "ogg", "m4a", "m4p", "wav", "au", "wma",
    "MP3", "OGG", "M4A", "M4P", "WAV", "AU", "WMA"
};
static std::map<std::string, std::string> VIDEO_EXTS = {
    {"mp4", "mp4"}, {"MP4", "mp4"},
    {"avi", "avi"}, {"AVI", "avi"},
    {"mov", "mov"}, {"MOV", "mov"},
    {"mkv", "mkv"}, {"MKV", "mkv"},
    {"mpg", "mpg"}, {"MPG", "mpg"},
    {"mpeg", "mpeg"}, {"MPEG", "mpeg"}
};

bool IsExtensionVideo(const std::string &ext) {
    return VIDEO_EXTS.find(ext) != VIDEO_EXTS.end();
}
bool IsExtensionAudio(const std::string &ext) {
    return AUDIO_EXTS.find(ext) != AUDIO_EXTS.end();
}


std::string GetVideoFilenameForMedia(const std::string &filename, std::string &ext) {
    ext = "";
    std::string result("");
    std::size_t found = filename.find_last_of(".");
    std::string oext = filename.substr(found + 1);
    std::string lext = toLowerCopy(oext);
    std::string bfile = filename.substr(0, found + 1);
    std::string videoPath(getVideoDirectory());
    videoPath += "/";
    videoPath += bfile;

    if (IsExtensionVideo(lext)) {
        if (FileExists(videoPath + oext)) {
            ext = lext;
            result = bfile + oext;
        } else if (FileExists(videoPath + lext)) {
            ext = lext;
            result = bfile + lext;
        }
    } else if (IsExtensionAudio(lext)) {
        for (auto &n : VIDEO_EXTS) {
            if (FileExists(videoPath + n.first)) {
                ext = n.second;
                result = bfile + n.first;
                return result;
            }
        }
    }

    return result;
}
bool HasAudio(const std::string &mediaFilename) {
    std::string fullMediaPath = mediaFilename;
    if (!FileExists(mediaFilename)) {
        fullMediaPath = getMusicDirectory();
        fullMediaPath += "/";
        fullMediaPath += mediaFilename;
    }
    return FileExists(fullMediaPath);
}
bool HasVideoForMedia(std::string &filename) {
    std::string ext;
    std::string fp = GetVideoFilenameForMedia(filename, ext);
    if (fp != "") {
        filename = fp;
    }
    return fp != "";
}

MediaOutputBase *CreateMediaOutput(const std::string &mediaFilename, const std::string &vOut) {
    std::string tmpFile(mediaFilename);
    std::size_t found = mediaFilename.find_last_of(".");
    if (found == std::string::npos) {
        LogDebug(VB_MEDIAOUT, "Unable to determine extension of media file %s\n",
                 mediaFilename.c_str());
        return nullptr;
    }
    std::string ext = toLowerCopy(mediaFilename.substr(found + 1));

    if (IsExtensionAudio(ext)) {
        if (getFPPmode() == REMOTE_MODE) {
            #ifdef HASVLC
            return new VLCOutput(mediaFilename, &mediaOutputStatus, "--Disabled--");
            #else
            return nullptr;
            #endif
        } else {
            return new SDLOutput(mediaFilename, &mediaOutputStatus, "--Disabled--");
        }
#ifdef HASVLC
    } else if (IsExtensionVideo(ext) && vOut == "--HDMI--") {
        return new VLCOutput(mediaFilename, &mediaOutputStatus, vOut);
#endif
#ifdef PLATFORM_PI
    } else if (IsExtensionVideo(ext) && vOut == "--HDMI--") {
       return new omxplayerOutput(tmpFile, &mediaOutputStatus);
#endif
    } else if (IsExtensionVideo(ext)) {
        return new SDLOutput(mediaFilename, &mediaOutputStatus, vOut);
    }
    return nullptr;
}


static std::set<std::string> alreadyWarned;
/*
 *
 */
int OpenMediaOutput(const char *filename) {
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
    std::string ext = toLowerCopy(tmpFile.substr(found + 1));

	if (getFPPmode() == REMOTE_MODE) {
        std::string orgTmp = tmpFile;
        tmpFile = GetVideoFilenameForMedia(tmpFile, ext);
        #ifdef HASVLC
        if (tmpFile == "") {
            if (HasAudio(orgTmp)) {
                tmpFile = orgTmp;
            }
        }
        #endif
        
        if (tmpFile == "") {
            // For v1.0 MultiSync, we can't sync audio to audio, so check for
            // a video file if the master is playing an audio file
            //video doesn't exist, punt
            tmpFile = filename;
            if (alreadyWarned.find(tmpFile) == alreadyWarned.end()) {
                alreadyWarned.emplace(tmpFile);
                LogInfo(VB_MEDIAOUT, "No video found for remote playing of %s\n", filename);
            }
            return 0;
        } else {
            LogDebug(VB_MEDIAOUT,
                     "Master is playing %s audio, remote will try %s\n",
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
    
    mediaOutput = CreateMediaOutput(tmpFile, vOut);
	if (!mediaOutput) {
		pthread_mutex_unlock(&mediaOutputLock);
        LogErr(VB_MEDIAOUT, "No Media Output handler for %s\n", tmpFile);
		return 0;
	}

	if (getFPPmode() == MASTER_MODE)
		multiSync->SendMediaOpenPacket(mediaOutput->m_mediaFilename);
    
#ifdef PLATFORM_PI
    // at this point, OMX takes a long time to actually start, we'll just start it
    // there is a patch to add a --start-paused which we could eventually use
    if (dynamic_cast<omxplayerOutput*>(mediaOutput) != nullptr) {
        if (getFPPmode() == MASTER_MODE)
            multiSync->SendMediaSyncStartPacket(mediaOutput->m_mediaFilename);
        if (!mediaOutput->Start()) {
            LogErr(VB_MEDIAOUT, "Could not start media %s\n", tmpFile);
            delete mediaOutput;
            mediaOutput = 0;
            pthread_mutex_unlock(&mediaOutputLock);
            return 0;
        }
    }
#endif
	pthread_mutex_unlock(&mediaOutputLock);

	return 1;
}

bool MatchesRunningMediaFilename(const char *filename) {
    if (mediaOutput) {
        std::string tmpFile = filename;
        if (mediaOutput->m_mediaFilename == tmpFile
            || !strcmp(mediaOutput->m_mediaFilename.c_str(), filename)) {
            return true;
        }
        if (HasVideoForMedia(tmpFile)) {
            if (mediaOutput->m_mediaFilename == tmpFile
                || !strcmp(mediaOutput->m_mediaFilename.c_str(), filename)) {
                return true;
            }
        }
    }
    return false;
}

int StartMediaOutput(const char *filename) {
    if (!MatchesRunningMediaFilename(filename)) {
        CloseMediaOutput();
    }
#ifdef PLATFORM_PI
    omxplayerOutput *omx = dynamic_cast<omxplayerOutput*>(mediaOutput);
    if (omx) {
        //its already started
        return 1;
    }
#endif
    
    if (mediaOutput && mediaOutput->IsPlaying()) {
        CloseMediaOutput();
    }
    if (!mediaOutput) {
        OpenMediaOutput(filename);
    }
    if (!mediaOutput) {
        return 0;
    }
    pthread_mutex_lock(&mediaOutputLock);
    if (getFPPmode() == MASTER_MODE)
        multiSync->SendMediaSyncStartPacket(mediaOutput->m_mediaFilename);

    if (!mediaOutput->Start()) {
        LogErr(VB_MEDIAOUT, "Could not start media %s\n", mediaOutput->m_mediaFilename.c_str());
        delete mediaOutput;
        mediaOutput = 0;
        pthread_mutex_unlock(&mediaOutputLock);
        return 0;
    }

    pthread_mutex_unlock(&mediaOutputLock);
    return 1;
}
void CloseMediaOutput() {
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
		multiSync->SendMediaSyncStopPacket(mediaOutput->m_mediaFilename);

	delete mediaOutput;
	mediaOutput = 0;

	pthread_mutex_unlock(&mediaOutputLock);
}

void UpdateMasterMediaPosition(const char *filename, float seconds)
{
    if (getFPPmode() != REMOTE_MODE) {
		return;
    }
    
    if (MatchesRunningMediaFilename(filename)) {
        masterMediaPosition = seconds;
        pthread_mutex_lock(&mediaOutputLock);
        if (!mediaOutput) {
            pthread_mutex_unlock(&mediaOutputLock);
            return;
        }
        mediaOutput->AdjustSpeed(seconds);
        pthread_mutex_unlock(&mediaOutputLock);
        return;
    #ifdef HASVLC
    } else {
        // with VLC, we can jump forward a bit and get close
        OpenMediaOutput(filename);
        StartMediaOutput(filename);
        masterMediaPosition = seconds;
        pthread_mutex_lock(&mediaOutputLock);
        if (!mediaOutput) {
            pthread_mutex_unlock(&mediaOutputLock);
            return;
        }
        mediaOutput->AdjustSpeed(seconds);
        pthread_mutex_unlock(&mediaOutputLock);
        return;
    #endif
    }
}


