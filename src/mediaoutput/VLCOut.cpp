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
    VLCInternalData(const std::string& m, VLCOutput* out) :
        fullMediaPath(m),
        vlcOutput(out) {
    }
    VLCOutput* vlcOutput;
    libvlc_media_player_t* vlcPlayer = nullptr;
    libvlc_media_t* media = nullptr;
    libvlc_equalizer_t* equalizer = nullptr;
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

    int lastDiff = -1; //initalize at -1ms so speedup/slowdown logic initally assumes we are slightly behind the master (most common)
    int rateDiff = 0;
};

static std::string currentMediaFilename;

void logCallback(void* data, int level, const libvlc_log_t* ctx,
                 const char* fmt, va_list args) {
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

static void startingEventCallBack(const struct libvlc_event_t* p_event, void* p_data) {
    VLCInternalData* d = (VLCInternalData*)p_data;
    d->vlcOutput->Starting();
}
static void stoppedEventCallBack(const struct libvlc_event_t* p_event, void* p_data) {
    VLCInternalData* d = (VLCInternalData*)p_data;
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

    int load(VLCInternalData* data) {
        if (vlcInstance == nullptr) {
            //const char *args[] {"-A", "alsa", "-V", "mmal_vout", nullptr};
#ifdef PLATFORM_UNKNOWN
            const char* dsp = getenv("DISPLAY");
            if (dsp == nullptr) {
                setenv("DISPLAY", ":0", true);
            }
#endif
            const char* args[]{ "-A", "alsa", "--no-osd",
#ifdef PLATFORM_PI
                                "-V", "mmal_vout",
#elif defined(PLATFORM_UNKNOWN)
                                "-I", "dummy",
#endif
                                nullptr };
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

        std::string cardType = getSetting("AudioCardType");
        if (cardType.find("Dummy") == 0) {
            WarningHolder::AddWarningTimeout("Outputting Audio to Dummy device.", 60);
        }

        return 0;
    }
    int start(VLCInternalData* data, int startPos) {
        libvlc_media_player_play(data->vlcPlayer);
        if (startPos) {
            libvlc_media_player_set_time(data->vlcPlayer, startPos, false);
        }
        data->length = libvlc_media_player_get_length(data->vlcPlayer);
        return 0;
    }
    int restart(VLCInternalData* data) {
        if (data->vlcPlayer) {
            libvlc_media_player_set_media(data->vlcPlayer, data->media);
            libvlc_media_player_play(data->vlcPlayer);
            libvlc_media_player_set_time(data->vlcPlayer, 0, false);
        }
        return 0;
    }

    int stop(VLCInternalData* data) {
        if (data->vlcPlayer) {
            libvlc_media_player_stop_async(data->vlcPlayer);
            libvlc_media_player_release(data->vlcPlayer);
            data->vlcPlayer = nullptr;
            /*
             * Per https://mailman.videolan.org/pipermail/vlc-devel/2008-May/043240.html
             *
             * This isn't needed.  libvlc_media_player_release will do it.   
             * I *think* we need to wait for  libvlc_MediaPlayerStopped event
             * Before freeing.    I don't have time to test this now
             * But will soon.
             */
            //libvlc_media_release(data->media);
            data->media = nullptr;

            if (data->equalizer) {
                libvlc_audio_equalizer_release(data->equalizer);
                data->equalizer = nullptr;
            }
        }
        return 0;
    }

    libvlc_instance_t* vlcInstance = nullptr;
};

static VLCManager vlcManager;
static constexpr int RATE_AVERAGE_COUNT = 20;
static std::list<float> lastRates;
static float rateSum = 0.0f;

VLCOutput::VLCOutput(const std::string& mediaFilename, MediaOutputStatus* status, const std::string& videoOut) {
    LogExcess(VB_MEDIAOUT, "VLCOutput::VLCOutput(%s)\n", mediaFilename.c_str());
    data = nullptr;
    m_allowSpeedAdjust = getSettingInt("remoteIgnoreSync") == 0;
    m_mediaOutputStatus = status;
    m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;

    m_mediaOutputStatus->mediaSeconds = 0.0;
    m_mediaOutputStatus->secondsElapsed = 0;
    m_mediaOutputStatus->subSecondsElapsed = 0;

    std::string fullMediaPath = mediaFilename;
    if (!FileExists(mediaFilename)) {
        fullMediaPath = FPP_DIR_MUSIC;
        fullMediaPath += "/";
        fullMediaPath += mediaFilename;
    }
    if (!FileExists(fullMediaPath)) {
        fullMediaPath = FPP_DIR_VIDEO;
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
    LogExcess(VB_MEDIAOUT, "VLCOutput::~VLCOutput(%X) %0.3f\n", data, m_mediaOutputStatus->mediaSeconds);
    Close();
    if (data) {
        delete data;
    }
}

int VLCOutput::Start(int msTime) {
    LogDebug(VB_MEDIAOUT, "VLCOutput::Start(%X, %d) %0.3f\n", data, msTime, m_mediaOutputStatus->mediaSeconds);
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
    LogDebug(VB_MEDIAOUT, "VLCOutput::Stop(%X) %0.3f\n", data, m_mediaOutputStatus->mediaSeconds);
    if (data) {
        vlcManager.stop(data);
    }
    m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
    return 1;
}

int VLCOutput::Process(void) {
    //LogExcess(VB_MEDIAOUT, "VLCOutput::Process(%X) %0.3f\n", data, m_mediaOutputStatus->mediaSeconds);
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

    if (multiSync->isMultiSyncEnabled()) {
        multiSync->SendMediaSyncPacket(m_mediaFilename, m_mediaOutputStatus->mediaSeconds);
    }
    LogExcess(VB_MEDIAOUT,
              "Elapsed: %.2d.%.3d  Remaining: %.2d  Total %.2d:%.2d\n",
              m_mediaOutputStatus->secondsElapsed,
              m_mediaOutputStatus->subSecondsElapsed,
              m_mediaOutputStatus->secondsRemaining,
              m_mediaOutputStatus->minutesTotal,
              m_mediaOutputStatus->secondsTotal);
    CalculateNewChannelOutputDelay(m_mediaOutputStatus->mediaSeconds);
    return m_mediaOutputStatus->status == MEDIAOUTPUTSTATUS_PLAYING;
}
int VLCOutput::Close(void) {
    LogDebug(VB_MEDIAOUT, "VLCOutput::Close() %0.3f\n", m_mediaOutputStatus->mediaSeconds);
    Stop();

    return 0;
}
int VLCOutput::IsPlaying(void) {
    LogExcess(VB_MEDIAOUT, "VLCOutput::IsPlaying() %0.3f\n", m_mediaOutputStatus->mediaSeconds);
    return m_mediaOutputStatus->status == MEDIAOUTPUTSTATUS_PLAYING;
}
int VLCOutput::Restart() {
    LogDebug(VB_MEDIAOUT, "VLCOutput::Restart(%X) %0.3f\n", data, m_mediaOutputStatus->mediaSeconds);
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
            LogDebug(VB_MEDIAOUT, "Can't adjust speed if not playing yet (%0.3f/%0.3f)\n", masterMediaPosition, m_mediaOutputStatus->mediaSeconds);
            return 1;
        }
        float rate = data->currentRate;

        if (lastRates.size() == 0) {
            // preload rate list with normal (1.0) rate
            lastRates.push_back(1.0);
            rateSum = 1.0;
        }

        int rawdiff = (int)(m_mediaOutputStatus->mediaSeconds * 1000) - (int)(masterMediaPosition * 1000);
        int diff = rawdiff;
        int sign = 1;
        if (diff < 0) {
            sign = -1;
            diff = -diff;
        }
        if ((m_mediaOutputStatus->mediaSeconds < 1) | (diff > 3000)) {
            LogDebug(VB_MEDIAOUT, "Diff: %d	Master: %0.3f  Local: %0.3f  Rate: %0.3f\n", rawdiff, masterMediaPosition, m_mediaOutputStatus->mediaSeconds, data->currentRate);
        } else {
            LogExcess(VB_MEDIAOUT, "Diff: %d	Master: %0.3f  Local: %0.3f  Rate: %0.3f\n", rawdiff, masterMediaPosition, m_mediaOutputStatus->mediaSeconds, data->currentRate);
        }
        data->push(rawdiff, data->currentRate);

        int oldSign = data->lastDiff < 0 ? -1 : 1;
        if ((oldSign != sign) && (data->lastDiff != 0) && (data->currentRate != 1.0)) {
            //last time was slightly behind and we are now slightly ahead
            //or vice versa, reset current rate to normal speed
            LogDebug(VB_MEDIAOUT, "Diff: %d	Flipped, reset speed to normal	(%0.3f)\n", rawdiff, 1.0);
            libvlc_media_player_set_rate(data->vlcPlayer, 1.0);
            // reset rate average list to 1.0
            // this is necessary or else the average rate could still be > 1 even when we need to go slower than 1
            // that would cause vlc to keep playing faster even after its caught up and needs to play slower
            lastRates.push_back(1.0);
            rateSum += 1.0;
            while (lastRates.size() > 1) {
                rateSum -= lastRates.front();
                lastRates.pop_front();
            }
            data->currentRate = 1.0;
            data->rateDiff = 0;
            data->lastDiff = rawdiff;
            return 1;
        }
        if (diff < 30) {
            // close enough
            if (data->currentRate != 1.0) {
                // only adjust if the current rate is not 1.0
                rate = 1.0;
                LogDebug(VB_MEDIAOUT, "Diff: %d	Very close, use normal rate	(%0.3f)\n", rawdiff, rate);
                libvlc_media_player_set_rate(data->vlcPlayer, rate);
                lastRates.push_back(rate);
                rateSum += rate;
                while (lastRates.size() > RATE_AVERAGE_COUNT) {
                    rateSum -= lastRates.front();
                    lastRates.pop_front();
                }
                data->currentRate = rate;
                data->rateDiff = 0;
                data->lastDiff = rawdiff;
            }
            return 1;
        } else if (diff > 10000) {
            // more than 10 seconds off, just jump to the new position. (should never be that far off unles fppd restarts)
            // **NOTE** jumping in vlc is problematic and causes video freeze (audio continues)
            // **note** if VLC 'seeking' ever gets fixed, this threshold should be lowered to around 3 seconds
            int ms = std::round(masterMediaPosition * 1000);
            LogDebug(VB_MEDIAOUT, "Diff: %d	Very far, jumping to: %0.3f	(currently at %0.3f)\n", rawdiff, masterMediaPosition, m_mediaOutputStatus->mediaSeconds);
            libvlc_media_player_set_time(data->vlcPlayer, ms, false);
            libvlc_media_player_set_rate(data->vlcPlayer, 1.0);
            lastRates.push_back(1.0);
            rateSum += 1.0;
            data->currentRate = 1.0;
            data->rateDiff = 0;
            data->lastDiff = -1; //after seeking, vlc is going to be slightly behind master, so this initializes the ahead/behind logic properly
            return 1;
        } else if (diff < 100) {
            // very close, but the diff could just be transient network issues
            // so we'll delay adjusting the rate until the next sync cycle
            if (!data->lastDiff) {
                LogDebug(VB_MEDIAOUT, "Diff: %d	Very close but could be transient, wait till next time\n", rawdiff);
                data->lastDiff = rawdiff;
                return 1;
            }
        }
        float rateDiff = diff;

        if (m_mediaOutputStatus->mediaSeconds > 10) {
            rateDiff /= 100.0f;
            if (rateDiff > 10) {
                rateDiff = 10;
            }
        } else {
            //in first 10 seconds, use larger rate changes
            // to try and get on track sooner
            rateDiff /= 50.0f;
            if (rateDiff > 20) {
                rateDiff = 20;
            }
        }

        rateDiff *= sign;
        int rateDiffI = (int)std::round(rateDiff);
        int oldrateDiffI = (int)std::round(data->rateDiff);
        //rate = data->currentRate;

        LogExcess(VB_MEDIAOUT, "Diff: %d	rateDiffI: %d  data->rateDiff: %d\n", rawdiff, rateDiffI, data->rateDiff);
        if (rateDiffI < data->rateDiff) {
            for (int r = rateDiffI; r < data->rateDiff; r++) {
                rate = rate * 1.02;
            }
            LogDebug(VB_MEDIAOUT, "Diff: %d	SpeedUp  %0.3f/%0.3f [goal/current]\n", rawdiff, rate, data->currentRate);
        } else if (rateDiffI > data->rateDiff) {
            for (int r = rateDiffI; r > data->rateDiff; r--) {
                rate = rate * 0.98;
            }
            LogDebug(VB_MEDIAOUT, "Diff: %d	SlowDown %0.3f/%0.3f [goal/current]\n", rawdiff, rate, data->currentRate);
        } else {
            //no rate change
            LogExcess(VB_MEDIAOUT, "Diff: %d	no rate change\n");
            return 1;
        }

        // add new rate to rate history so we can calculate a running average rate
        lastRates.push_back(rate);
        rateSum += rate;
        if (lastRates.size() > RATE_AVERAGE_COUNT) {
            rateSum -= lastRates.front();
            lastRates.pop_front();
        }
        // final check if we flipped from behind/ahead or ahead/behind, and we weren't already at normal speed
        // then reset to normal (1.0) speed before making further adjustments.
        if (((rate > 1.0) && (data->currentRate < 1.0)) | ((rate < 1.0) && (data->currentRate > 1.0))) {
            rate = 1.0;
            data->rateDiff = 0;
        }

        LogExcess(VB_MEDIAOUT, "Diff: %d	oldDiff: %d	newRate: %0.3f oldRate: %0.3f avgRate: %0.3f rateSum: %0.3f/%d \n",
                  rawdiff, data->lastDiff, lastRates.back(), data->currentRate, rate, rateSum, (int)lastRates.size());

        if (rate > 2.0)
            rate = 2.0; // limit max to double-speed
        if (rate < 0.5)
            rate = 0.5; // limit min to half-speed

        //if (rate > 0.991 && rate < 1.009) {
        //    rate = 1.0;
        //}

        if ((int)(rate * 1000) != (int)(data->currentRate * 1000)) {
            LogDebug(VB_MEDIAOUT, "Diff: %d	libvlc_media_player_set_rate	(%0.3f)\n", rawdiff, rate);
            libvlc_media_player_set_rate(data->vlcPlayer, rate);
            data->currentRate = rate;
            if (rate == 1.0) {
                data->rateDiff = 0;
            } else {
                data->rateDiff = rateDiffI;
            }
        }
        data->lastDiff = rawdiff;
    }
    return 1;
}
void VLCOutput::SetVolumeAdjustment(int volAdj) {
    if (data && volAdj) {
        LogDebug(VB_MEDIAOUT, "Volume Adjustment:	%d\n", volAdj);
        if (!data->equalizer) {
            data->equalizer = libvlc_audio_equalizer_new_from_preset(0);
        }
        float ampv = libvlc_audio_equalizer_get_preamp(data->equalizer);
        float adj = (volAdj > 0) ? 20.0 - ampv : 20.0 + ampv;
        adj *= volAdj;
        adj /= 100;
        ampv += adj;
        libvlc_audio_equalizer_set_preamp(data->equalizer, ampv);
        libvlc_media_player_set_equalizer(data->vlcPlayer, data->equalizer);
    }
}
