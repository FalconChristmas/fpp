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
#include "fpp-pch.h"
#include <vlc/vlc.h>

#include "VLCOut.h"

#include "MultiSync.h"
#include "channeloutput/channeloutputthread.h"

class VLCInternalData {
public:
    VLCInternalData(const std::string &m, VLCOutput *out) : fullMediaPath(m), vlcOutput(out) {
    }
    VLCOutput *vlcOutput;
    libvlc_media_player_t *vlcPlayer = nullptr;
    libvlc_media_t *media = nullptr;
    libvlc_equalizer_t *equalizer = nullptr;
    std::string fullMediaPath;   
    
    uint64_t length = 0;
    uint64_t lastPos = 0;
    
    float currentRate = 1.0f;
    
    
    //circular buffer to record the last 10 diffs/rates so
    //we can detect trends
    static const int MAX_DIFFS = 10;
    std::array<std::pair<int, float>, MAX_DIFFS> diffs;
    int diffsSize = 0;
    int diffIdx = 0;
    int diffSum = 0;
    float rateSum = 0.0f;
    
    void push(int diff, float rate) {
        diffSum += diff;
        rateSum += rate;
        if (diffsSize < MAX_DIFFS) {
            diffIdx = diffsSize;
            diffsSize++;
        } else {
            diffIdx++;
            if (diffIdx == MAX_DIFFS) {
                diffIdx = 0;
            }
            diffSum -= diffs[diffIdx].first;
            rateSum -= diffs[diffIdx].second;
        }
        diffs[diffIdx].first = diff;
        diffs[diffIdx].second = rate;
    }
    
    
    int lastDiff = 0.0f;
    int rateDiff = 0;
};

static std::string currentMediaFilename;

void logCallback(void *data, int level, const libvlc_log_t *ctx,
                 const char *fmt, va_list args) {
    switch (level) {
        case LIBVLC_DEBUG:
            if (WillLog(LOG_EXCESSIVE, VB_MEDIAOUT)) {
                char buf[513];
                vsnprintf(buf, 512, fmt, args);
                LogExcess(VB_MEDIAOUT, "%s\n", buf);
            }
            break;
        case LIBVLC_NOTICE:
        case LIBVLC_WARNING:
            if (WillLog(LOG_DEBUG, VB_MEDIAOUT)) {
                char buf[256];
                vsnprintf(buf, 255, fmt, args);
                LogDebug(VB_MEDIAOUT, "%s\n", buf);
            }
            break;
        case LIBVLC_ERROR:
            // log at warn as nothing it's reporting as error is critical
            if (WillLog(LOG_WARN, VB_MEDIAOUT)) {
                char buf[256];
                vsnprintf(buf, 255, fmt, args);
                std::string str = buf;
                if (str == "buffer deadlock prevented") {
                    return;
                }
                if (str == "cannot estimate delay: Input/output error") {
                    return;
                }
                if (str == "cannot connect to session bus: Unable to autolaunch a dbus-daemon without a $DISPLAY for X11") {
                    return;
                }
                LogWarn(VB_MEDIAOUT, "%s\n", buf);
            }
            break;
    }
}

static void startingEventCallBack(const struct libvlc_event_t *p_event, void *p_data) {
    VLCInternalData *d = (VLCInternalData*)p_data;
    d->vlcOutput->Starting();
}
static void stoppedEventCallBack(const struct libvlc_event_t *p_event, void *p_data) {
    VLCInternalData *d = (VLCInternalData*)p_data;
    d->vlcOutput->Stopped();
}


class VLCManager {
public:
    VLCManager() {}
    ~VLCManager() {
        if (vlcInstance) {
            libvlc_release(vlcInstance);
        }
    }
    
    int load(VLCInternalData *data) {
        if (vlcInstance == nullptr) {
            //const char *args[] {"-A", "alsa", "-V", "mmal_vout", nullptr};
#ifdef PLATFORM_UNKNOWN
            const char *dsp = getenv("DISPLAY");
            if (dsp == nullptr) {
                setenv("DISPLAY", ":0", true);
            }
#endif
            const char *args[] {"-A", "alsa", "--no-osd",
#ifdef PLATFORM_PI
                "-V", "mmal_vout",
#elif defined(PLATFORM_UNKNOWN)
                "-I", "dummy",
#endif
                nullptr};
            int argc = 0;
            while (args[argc]) {
                argc++;
            }
            vlcInstance = libvlc_new(argc, args);
            
            libvlc_log_set(vlcInstance, logCallback, this);
        }
        data->media = libvlc_media_new_path(vlcInstance, data->fullMediaPath.c_str());
        data->vlcPlayer = libvlc_media_player_new_from_media(data->media);
        libvlc_event_attach(libvlc_media_player_event_manager(data->vlcPlayer), libvlc_MediaPlayerEndReached, stoppedEventCallBack, data);
        libvlc_event_attach(libvlc_media_player_event_manager(data->vlcPlayer), libvlc_MediaPlayerOpening, startingEventCallBack, data);
        data->length = libvlc_media_player_get_length(data->vlcPlayer);
        
        return 0;
    }
    int start(VLCInternalData *data, int startPos) {
        libvlc_media_player_play(data->vlcPlayer);
        if (startPos) {
            libvlc_media_player_set_time(data->vlcPlayer, startPos, false);
        }
        data->length = libvlc_media_player_get_length(data->vlcPlayer);
        return 0;
    }
    int restart(VLCInternalData *data) {
        if (data->vlcPlayer) {
            libvlc_media_player_set_media(data->vlcPlayer, data->media);
            libvlc_media_player_play(data->vlcPlayer);
            libvlc_media_player_set_time(data->vlcPlayer, 0, false);
        }
        return 0;
    }

    int stop(VLCInternalData *data) {
        if (data->vlcPlayer) {
            libvlc_media_player_stop_async(data->vlcPlayer);
            libvlc_media_player_release(data->vlcPlayer);
            data->vlcPlayer = nullptr;
            libvlc_media_release(data->media);
            data->media = nullptr;
            
            if (data->equalizer) {
                libvlc_audio_equalizer_release(data->equalizer);
                data->equalizer = nullptr;
            }
        }
        return 0;
    }
    
    libvlc_instance_t *vlcInstance = nullptr;
};

static VLCManager vlcManager;
static constexpr int RATE_AVERAGE_COUNT = 20;
static std::list<float> lastRates;
static float rateSum = 0;

VLCOutput::VLCOutput(const std::string &mediaFilename, MediaOutputStatus *status, const std::string &videoOut) {
    LogDebug(VB_MEDIAOUT, "VLCOutput::VLCOutput(%s)\n",
        mediaFilename.c_str());
    data = nullptr;
    m_allowSpeedAdjust = getSettingInt("remoteIgnoreSync") == 0;
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
    data = new VLCInternalData(fullMediaPath, this);
    vlcManager.load(data);
}
VLCOutput::~VLCOutput() {
    LogDebug(VB_MEDIAOUT, "VLCOutput::~VLCOutput() %X\n", data);
    Close();
    if (data) {
        delete data;
    }
}

int VLCOutput::Start(int msTime) {
    LogDebug(VB_MEDIAOUT, "VLCOutput::Start() %X\n", data);
    if (data) {
        vlcManager.start(data, msTime);

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
        //printf("cur: %d    len: %d     Pos: %f        %ld\n", (int)cur, (int)data->length, libvlc_media_player_get_position(data->vlcPlayer), GetTimeMS() % 100000);
        if (cur != data->lastPos) {
            //printf("cur: %d    len: %d     Pos: %f        %ld\n", (int)cur, (int)data->length, libvlc_media_player_get_position(data->vlcPlayer), GetTimeMS() % 100000);
            data->lastPos = cur;
        }
        
        if ((cur > 0 && !libvlc_media_player_is_playing(data->vlcPlayer)) || (cur == 0 && libvlc_media_player_get_position(data->vlcPlayer) < -0.5f)) {
            cur = data->length;
            m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
        }

        float seconds = cur;
        seconds /= 1000.0f;
        int subSeconds = cur / 10;
        subSeconds = subSeconds % 100;
        m_mediaOutputStatus->mediaSeconds = seconds;
        m_mediaOutputStatus->secondsElapsed = std::floor(seconds);
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
int VLCOutput::Restart() {
    LogDebug(VB_MEDIAOUT, "VLCOutput::Start() %X\n", data);
    if (data) {
        vlcManager.restart(data);

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


int VLCOutput::AdjustSpeed(float masterMediaPosition) {
    if (data->vlcPlayer && m_allowSpeedAdjust) {
        // Can't adjust speed if not playing yet
        if (m_mediaOutputStatus->mediaSeconds < 0.01) {
            return 1;
        }
        
        int rawdiff = (int)(m_mediaOutputStatus->mediaSeconds * 1000) - (int)(masterMediaPosition * 1000);
        LogExcess(VB_MEDIAOUT, "Master %0.3f    Local:  %0.3f    Diff: %dms\n", masterMediaPosition, m_mediaOutputStatus->mediaSeconds, rawdiff);
        
        int diff = rawdiff;
        int sign = 1;
        if (diff < 0) {
            sign = -1;
            diff = -diff;
        }
        data->push(rawdiff, data->currentRate);
        if (diff < 30) {
            // close enough
            data->lastDiff = 0;
            if (data->currentRate != 1.0) {
                LogDebug(VB_MEDIAOUT, "Diff: %d    Very close, using rate of 1.0\n", rawdiff);
                
                lastRates.push_back(1.0);
                lastRates.push_back(1.0);
                rateSum += 2.0;
                while (lastRates.size() > RATE_AVERAGE_COUNT) {
                    rateSum -= lastRates.front();
                    lastRates.pop_front();
                }
                float rate = rateSum / lastRates.size();

                libvlc_media_player_set_rate(data->vlcPlayer, rate);
                data->currentRate = rate;
                data->rateDiff = 0;
            }
            return 1;
        } else if (diff > 1500) {
            LogDebug(VB_MEDIAOUT, "Diff: %d    Very far, jumping\n", rawdiff);
            // more than 1.5 seconds off, just jump to the new position
            int ms = std::round(masterMediaPosition * 1000);
            libvlc_media_player_set_time(data->vlcPlayer, ms, false);
            libvlc_media_player_set_rate(data->vlcPlayer, 1.0);
            data->currentRate = 1.0;
            data->rateDiff = 0;
            data->lastDiff = 0;
            return 1;
        } else if (diff < 100) {
            // very close, but the diff could just be transient network issues
            // so we'll delay adjusting the rate
            if (!data->lastDiff) {
                //LogDebug(VB_MEDIAOUT, "Diff: %d    Close, wait till next time\n", rawdiff);
                data->lastDiff = diff * sign;
                return 1;
            }
            int oldSign = data->lastDiff < 0 ? -1 : 1;
            if (oldSign != sign) {
                //LogDebug(VB_MEDIAOUT, "Diff: %d    Close but flipped, wait till next time\n", rawdiff);
                //last time was slightly behind and we are now slightly ahead
                //or vice versa, again delay until next to avoid toggle back/forth
                data->lastDiff = diff * sign;
                return 1;
            }
        }
        data->lastDiff = 0;
        
        float rateDiff = diff;
        if (masterMediaPosition > 10) {
            rateDiff /= 100.0f;
            if (rateDiff > 5) {
                rateDiff = 5;
            }
        } else {
            // in first 10 seconds, use larger rate changes
            // to try and get on track sooner
            rateDiff /= 50.0f;
            if (rateDiff > 15) {
                rateDiff = 15;
            }
        }
        rateDiff *= sign;
        float rate = data->currentRate;
        int rateDiffI = (int)std::round(rateDiff);
        if (rateDiffI < data->rateDiff) {
            for (int r = rateDiffI; r < data->rateDiff; r++) {
                rate = rate * 1.02;
            }
        } else if (rateDiffI > data->rateDiff) {
            for (int r = data->rateDiff; r < rateDiffI; r++) {
                rate = rate * 0.98;
            }
        } else {
            //no rate change
            return 1;
        }
        if (rate > 0.991 && rate < 1.009) {
            rate = 1.0;
        }
        if (rate > 1.1) rate = 1.1;
        if (rate < 0.9) rate = 0.9;
        if (rate > 1.0 && data->currentRate < 1.0) rate = 1.0;
        if (rate < 1.0 && data->currentRate > 1.0) rate = 1.0;
        lastRates.push_back(rate);
        rateSum += rate;
        if (lastRates.size() > RATE_AVERAGE_COUNT) {
            rateSum -= lastRates.front();
            lastRates.pop_front();
        }
        rate = rateSum / lastRates.size();
        
        LogDebug(VB_MEDIAOUT, "Diff: %d     RateDiff:  %0.3f / %d  New rate: %0.3f/%0.3f\n", rawdiff, rateDiff, data->rateDiff, rate, data->currentRate);
        libvlc_media_player_set_rate(data->vlcPlayer, rate);
        data->rateDiff = rateDiffI;
        data->currentRate = rate;
    }
    return 1;
}
void VLCOutput::SetVolumeAdjustment(int volAdj) {
    if (data && volAdj) {
        if (!data->equalizer) {
            data->equalizer = libvlc_audio_equalizer_new_from_preset(0);
        }
        float ampv = libvlc_audio_equalizer_get_preamp(data->equalizer);
        float adj = (volAdj > 0) ? 20.0 - ampv : 20.0 + ampv;
        adj *= volAdj;
        adj /= 100;
        ampv += adj;
        libvlc_audio_equalizer_set_preamp(data->equalizer,  ampv);
        libvlc_media_player_set_equalizer(data->vlcPlayer, data->equalizer);
    }
}

