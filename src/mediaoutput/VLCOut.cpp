/*
 *   libvlc player driver for Falcon Player (FPP)
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

#include <vlc/vlc.h>

#include "VLCOut.h"

#include "common.h"
#include "log.h"
#include "MultiSync.h"
#include "Sequence.h"
#include "channeloutput/channeloutputthread.h"


class VLCInternalData {
public:
    VLCInternalData(const std::string &m) : fullMediaPath(m) {
    }
    libvlc_media_player_t *vlcPlayer = nullptr;
    std::string fullMediaPath;
    
    uint64_t length = 0;
    uint64_t lastPos = 0;
};

static std::string currentMediaFilename;

class VLCManager {
public:
    VLCManager() {}
    ~VLCManager() {
        if (vlcInstance) {
            libvlc_release(vlcInstance);
        }
    }
    
    int start(VLCInternalData *data) {
        if (vlcInstance == nullptr) {
            const char *args[] {"-A", "alsa"};
            vlcInstance = libvlc_new(2, args);
        }
        libvlc_media_t *media = libvlc_media_new_path(vlcInstance, data->fullMediaPath.c_str());
        data->vlcPlayer = libvlc_media_player_new_from_media(media);
        libvlc_media_release(media);
        libvlc_media_player_play(data->vlcPlayer);
        data->length = libvlc_media_player_get_length(data->vlcPlayer);
        return 0;
    }
    
    int stop(VLCInternalData *data) {
        if (data->vlcPlayer) {
            libvlc_media_player_stop(data->vlcPlayer);
            libvlc_media_player_release(data->vlcPlayer);
            data->vlcPlayer = nullptr;
        }
        return 0;
    }
    
    libvlc_instance_t *vlcInstance = nullptr;
};

static VLCManager vlcManager;

VLCOutput::VLCOutput(const std::string &mediaFilename, MediaOutputStatus *status, const std::string &videoOut) {
    LogDebug(VB_MEDIAOUT, "VLCOutput::VLCOutput(%s)\n",
        mediaFilename.c_str());
    data = nullptr;
    m_mediaOutputStatus = status;
    m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
    
    
    m_mediaOutputStatus->mediaSeconds = 0.0;
    m_mediaOutputStatus->secondsElapsed = 0;
    m_mediaOutputStatus->subSecondsElapsed = 0;
    
    std::string fullMediaPath = mediaFilename;
    if (!FileExists(mediaFilename)) {
        fullMediaPath = getMusicDirectory();
        fullMediaPath += "/";
        fullMediaPath += mediaFilename;
    }
    if (!FileExists(fullMediaPath)) {
        fullMediaPath = getVideoDirectory();
        fullMediaPath += "/";
        fullMediaPath += mediaFilename;
    }
    if (!FileExists(fullMediaPath)) {
        LogErr(VB_MEDIAOUT, "%s does not exist!\n", fullMediaPath.c_str());
        currentMediaFilename = "";
        return;
    }
    currentMediaFilename = mediaFilename;
    m_mediaFilename = mediaFilename;
    data = new VLCInternalData(fullMediaPath);
}
VLCOutput::~VLCOutput() {
    LogDebug(VB_MEDIAOUT, "VLCOutput::~VLCOutput() %X\n", data);
    Close();
    if (data) {
        delete data;
    }
}

int VLCOutput::Start(void) {
    LogDebug(VB_MEDIAOUT, "VLCOutput::Start() %X\n", data);
    if (data) {
        vlcManager.start(data);
        
        int seconds = data->length / 1000;
        m_mediaOutputStatus->secondsTotal = seconds / 60;
        m_mediaOutputStatus->minutesTotal = seconds % 60;

        m_mediaOutputStatus->secondsRemaining = seconds;
        m_mediaOutputStatus->subSecondsRemaining = 0;
        
        m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_PLAYING;
        return 1;
    }
    m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
    return 0;
}
int VLCOutput::Stop(void) {
    LogDebug(VB_MEDIAOUT, "VLCOutput::Stop()\n");
    if (data) {
        vlcManager.stop(data);
    }
    m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
    return 1;
}
int VLCOutput::Process(void) {
    if (!data) {
        return 0;
    }
    
    if (data->vlcPlayer) {
        uint64_t cur = libvlc_media_player_get_time(data->vlcPlayer);
        if (!data->length) {
            data->length = libvlc_media_player_get_length(data->vlcPlayer);
            int seconds = data->length / 1000;
            m_mediaOutputStatus->secondsTotal = seconds / 60;
            m_mediaOutputStatus->minutesTotal = seconds % 60;

            m_mediaOutputStatus->secondsRemaining = seconds;
            m_mediaOutputStatus->subSecondsRemaining = 0;
        }
        if (cur != data->lastPos) {
            //printf("cur: %d    len: %d     Pos: %f\n", (int)cur, (int)data->length, libvlc_media_player_get_position(data->vlcPlayer));
            data->lastPos = cur;
        }
        
        if (cur > 0 && !libvlc_media_player_is_playing(data->vlcPlayer)) {
            cur = data->length;
            m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
        }

        int seconds = cur / 1000;
        int subSeconds = cur / 10;
        subSeconds = subSeconds % 100;
        m_mediaOutputStatus->mediaSeconds = seconds;
        m_mediaOutputStatus->secondsElapsed = seconds;
        m_mediaOutputStatus->subSecondsElapsed = subSeconds;

        cur = data->length - cur;
        seconds = cur / 1000;
        subSeconds = cur / 10;
        subSeconds = subSeconds % 100;
        m_mediaOutputStatus->secondsRemaining = seconds;
        m_mediaOutputStatus->subSecondsRemaining = subSeconds;
    }
    
    
    if (getFPPmode() == MASTER_MODE) {
        multiSync->SendMediaSyncPacket(m_mediaFilename, m_mediaOutputStatus->mediaSeconds);
    }
    if (sequence->IsSequenceRunning()) {
        LogExcess(VB_MEDIAOUT,
                  "Elapsed: %.2d.%.3d  Remaining: %.2d Total %.2d:%.2d.\n",
                  m_mediaOutputStatus->secondsElapsed,
                  m_mediaOutputStatus->subSecondsElapsed,
                  m_mediaOutputStatus->secondsRemaining,
                  m_mediaOutputStatus->minutesTotal,
                  m_mediaOutputStatus->secondsTotal);
        CalculateNewChannelOutputDelay(m_mediaOutputStatus->mediaSeconds);
    }
    return m_mediaOutputStatus->status == MEDIAOUTPUTSTATUS_PLAYING;
}
int VLCOutput::Close(void) {
    LogDebug(VB_MEDIAOUT, "VLCOutput::Close()\n");
    Stop();

    return 0;
}
int VLCOutput::IsPlaying(void) {
    return m_mediaOutputStatus->status == MEDIAOUTPUTSTATUS_PLAYING;
}
