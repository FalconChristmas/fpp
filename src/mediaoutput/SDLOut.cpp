/*
 *   libav/SDL player driver for Falcon Player (FPP)
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <atomic>
#include <cmath>
#include <set>
#include <thread>
#include <list>
#include <mutex>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include <SDL2/SDL.h>
}


#include "channeloutputthread.h"
#include "common.h"
#include "log.h"
#include "MultiSync.h"
#include "SDLOut.h"
#include "Sequence.h"
#include "settings.h"
#include "PixelOverlay.h"

//1/2 second buffers
#define MAX_BUFFER_SIZE  44100*2
//buffer the first 5 seconds
#define INITIAL_BUFFER_MAX  10
//keep a 4-8 second buffer
#define ONGOING_BUFFER_MAX  16

//Only keep 30 frames in buffer
#define VIDEO_FRAME_MAX     30

#if defined(PLATFORM_PI)
//on the old single core Pi's, we need to increase the buffer size
//a bit or the audio stutters.
static const int DEFAULT_NUM_SAMPLES = std::thread::hardware_concurrency() <= 1 ? 4096 : 1024;
#else 
static const int DEFAULT_NUM_SAMPLES = 1024;
#endif
#define DEFAULT_RATE 44100




static bool AudioHasStalled = false;

class AudioBuffer {
public:
    AudioBuffer() {
        size = MAX_BUFFER_SIZE;
        data = (uint8_t*)malloc(size);
        pos = 0;
        next = nullptr;
    }
    ~AudioBuffer() {
        free(data);
    }
    
    uint8_t *data;
    int size;
    int pos;
    AudioBuffer *next;
};


class VideoFrame {
public:
    VideoFrame(int ms,  uint8_t *d, int s) : data(d), timestamp(ms), size(s) {
        next = nullptr;
    }
    ~VideoFrame() {
        free(data);
    }
    
    int timestamp;
    int size;
    uint8_t *data;
    VideoFrame *next;
};


void SetChannelOutputFrameNumber(int frameNumber);

static int64_t MStoDTS(int ms, int dtspersec)
{
    return (((int64_t)ms * (int64_t)dtspersec) / (int64_t)1000);
}

static int DTStoMS(int64_t dts , int dtspersec)
{
    return (int)(((int64_t)1000 * dts) / (int64_t)dtspersec);
}


class SDLInternalData {
public:
    SDLInternalData() : curPos(0) {
        formatContext = nullptr;
        audioCodecContext = nullptr;
        videoCodecContext = nullptr;
        audio_stream_idx = -1;
        video_stream_idx = -1;
        videoStream = audioStream = nullptr;
        firstBuffer = lastBuffer = fillBuffer = nullptr;
        curBuffer = nullptr;
        bufferCount = 0;
        doneRead = false;
        frame = av_frame_alloc();
        scaledFrame = nullptr;
        au_convert_ctx = nullptr;
        decodedDataLen = 0;
        swsCtx = nullptr;
        firstVideoFrame = lastVideoFrame = nullptr;
        curVideoFrame = nullptr;
        videoFrameCount = 0;
    }
    ~SDLInternalData() {
        if (frame != nullptr) {
            av_free(frame);
        }
        if (swsCtx != nullptr) {
            sws_freeContext(swsCtx);
            swsCtx = nullptr;
        }
        while (firstVideoFrame) {
            VideoFrame *t = firstVideoFrame;
            firstVideoFrame = t->next;
            delete t;
        }
        if (scaledFrame != nullptr) {
            if (scaledFrame->data[0] != nullptr) {
                av_free(scaledFrame->data[0]);
            }

            av_free(scaledFrame);
        }
        if (formatContext != nullptr) {
            avformat_close_input(&formatContext);
        }
        if (au_convert_ctx != nullptr) {
            swr_free(&au_convert_ctx);
        }

        while (firstBuffer) {
            AudioBuffer *tmp = firstBuffer;
            firstBuffer = tmp->next;
            delete tmp;
        }
    }
    pthread_t fillThread;
    volatile int stopped;
    AVFormatContext*formatContext;
    AVPacket readingPacket;
    AVFrame* frame;
    
    
    // stuff for the audio stream
    AVCodecContext *audioCodecContext;
    int audio_stream_idx = -1;
    AVStream* audioStream;
    SwrContext *au_convert_ctx;
    AudioBuffer *firstBuffer;
    AudioBuffer * volatile curBuffer;
    AudioBuffer *lastBuffer;
    AudioBuffer *fillBuffer;
    int bufferCount;
    unsigned int totalDataLen;
    unsigned int decodedDataLen;
    float totalLen;

    // stuff for the video stream
    AVCodecContext *videoCodecContext;
    int video_stream_idx = -1;
    AVStream* videoStream;
    int video_dtspersec;
    int video_frames;
    AVFrame* scaledFrame;
    SwsContext *swsCtx;
    VideoFrame *firstVideoFrame;
    VideoFrame *lastVideoFrame;
    volatile VideoFrame *curVideoFrame;
    int videoFrameCount;
    unsigned int totalVideoLen;
    long long videoStartTime;
    std::string videoOverlayModel;
    
    
    bool doneRead;
    std::atomic_uint curPos;
    
    void addVideoFrame(int ms, uint8_t *d, int sz) {
        VideoFrame *f = new VideoFrame(ms, d, sz);
        if (firstVideoFrame == nullptr) {
            curVideoFrame = f;
            firstVideoFrame = f;
            lastVideoFrame = f;
        } else {
            lastVideoFrame->next = f;
            lastVideoFrame = f;
        }
        ++videoFrameCount;
    }
    void addBuffer(AudioBuffer *b) {
        b->size = b->pos;
        b->pos = 0;
        if (b->size == 0) {
            delete b;
            return;
        }
        
        if (lastBuffer == nullptr) {
            while (firstBuffer != nullptr) {
                AudioBuffer *tmp = firstBuffer;
                --bufferCount;
                firstBuffer = tmp->next;
                delete tmp;
            }
            firstBuffer = b;
            curBuffer = b;
            lastBuffer = b;
        } else {
            lastBuffer->next = b;
            lastBuffer = b;
        }
        bufferCount++;
    }
    void removeUsedBuffers() {
        while (firstBuffer != curBuffer) {
            AudioBuffer *tmp = firstBuffer;
            firstBuffer = tmp->next;
            --bufferCount;
            delete tmp;
        }
        while (firstVideoFrame && curVideoFrame && (firstVideoFrame != curVideoFrame)) {
            VideoFrame *tmp = firstVideoFrame;
            firstVideoFrame = firstVideoFrame->next;
            --videoFrameCount;
            delete tmp;
        }
    }
    
    int buffersFull() {
        if ((bufferCount >= (ONGOING_BUFFER_MAX - 2)) || (videoFrameCount >= VIDEO_FRAME_MAX)) {
            return 2;
        }
        if (video_stream_idx == -1 && (bufferCount >= (ONGOING_BUFFER_MAX / 2))) {
            return 1;
        }
        return 0;
    }
    bool maybeFillBuffer(bool first) {
        removeUsedBuffers();
        
        if (doneRead || (bufferCount > ONGOING_BUFFER_MAX)  || videoFrameCount > VIDEO_FRAME_MAX) {
            //buffers are full, don't so anything
            if (AudioHasStalled) LogWarn(VB_MEDIAOUT, "Stalled audio, buffers are full.  %d\n", doneRead);
            return doneRead;
        }
        if (AudioHasStalled) LogWarn(VB_MEDIAOUT, "Stalled audio, buffers still filling.  %X\n", fillBuffer);


        if (fillBuffer == nullptr) {
            fillBuffer = new AudioBuffer();
        }
        bool newBuf = false;
        while (av_read_frame(formatContext, &readingPacket) == 0) {
            bool packetOk = false;
            if (readingPacket.stream_index == audio_stream_idx) {
                int packetSendCount = 0;
                int packetRecvCount = 0;
                bool failToSend = false;
                while (avcodec_send_packet(audioCodecContext, &readingPacket) && !failToSend) {
                    packetSendCount++;
                    int lastPacketRecvCount = packetRecvCount;
                    while (!avcodec_receive_frame(audioCodecContext, frame)) {
                        packetRecvCount++;
                        int sz = frame->nb_samples * 2 * 2;
                        if ((sz + fillBuffer->pos) >= fillBuffer->size) {
                            addBuffer(fillBuffer);
                            fillBuffer = new AudioBuffer();
                            newBuf = true;
                        }
                        uint8_t* out_buffer = &fillBuffer->data[fillBuffer->pos];
                        int outSamples = swr_convert(au_convert_ctx,
                                                     &out_buffer,
                                                     (fillBuffer->size - fillBuffer->pos) / 4,
                                                     (const uint8_t **)frame->extended_data, frame->nb_samples);

                        fillBuffer->pos += (outSamples * 2 * 2);
                        decodedDataLen += (outSamples * 2 * 2);
                        av_frame_unref(frame);
                    }
                    if (packetSendCount > 1000 && lastPacketRecvCount == packetRecvCount) {
                        //failed to make any progress with this packet, we'll loop out
                        failToSend = true;
                    }
                    
                    if (lastPacketRecvCount != packetRecvCount) {
                        //some work was done, reset counters
                        packetRecvCount = 0;
                        packetSendCount = 0;
                    }
                }
                packetOk = true;
            } else if (readingPacket.stream_index == video_stream_idx) {
                while (avcodec_send_packet(videoCodecContext, &readingPacket)) {
                    while (!avcodec_receive_frame(videoCodecContext, frame)) {
                        int ms = DTStoMS(frame->pkt_dts, video_dtspersec);
                       
                        if (swsCtx) {
                            sws_scale(swsCtx, frame->data, frame->linesize, 0,
                                      videoCodecContext->height, scaledFrame->data,
                                      scaledFrame->linesize);
                        
                            int sz = scaledFrame->linesize[0] * scaledFrame->height;
                            uint8_t *d = (uint8_t*)malloc(sz);
                            memcpy(d, scaledFrame->data[0], sz);
                            addVideoFrame(ms, d, sz);
                        } else {
                            int sz = frame->linesize[0] * frame->height;
                            uint8_t *d = (uint8_t*)malloc(sz);
                            memcpy(d, frame->data[0], sz);
                            addVideoFrame(ms, d, sz);
                        }
                        av_frame_unref(frame);
                    }
                }
                packetOk = true;
            }
            av_packet_unref(&readingPacket);
            
            if (packetOk) {
                if (first) {
                    if ((bufferCount > INITIAL_BUFFER_MAX || videoFrameCount > VIDEO_FRAME_MAX))  {
                        return doneRead;
                    }
                } else {
                    return doneRead;
                }
            }
        }
        
        totalDataLen = decodedDataLen;
        addBuffer(fillBuffer);
        fillBuffer = nullptr;
        doneRead = true;
        return doneRead;
    }
};

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx,
                              enum AVMediaType type,
                              const std::string &src_filename)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }
        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }
        /* Init the decoders, with or without reference counting */
        av_dict_set(&opts, "refcounted_frames", "0", 0);
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }
    return 0;
}



typedef enum SDLSTATE {
    SDLUNINITIALISED,
    SDLINITIALISED,
    SDLOPENED,
    SDLPLAYING,
    SDLNOTPLAYING
} SDLSTATE;


class SDL
{
    SDLSTATE _state;
    SDL_AudioSpec _wanted_spec;
    int _initialisedRate;
    SDL_AudioDeviceID audioDev;
    
public:
    SDL() : data(nullptr), _state(SDLSTATE::SDLUNINITIALISED) {}
    virtual ~SDL();
    
    bool Start(SDLInternalData *d) {
        if (!initSDL()) {
            return false;
        }
        if (!openAudio()) {
            return false;
        }
        if (_state != SDLSTATE::SDLINITIALISED && _state != SDLSTATE::SDLUNINITIALISED) {
            data = d;
            SDL_PauseAudioDevice(audioDev, 0);

            long long t = GetTime() / 1000;
            data->videoStartTime = t;
            _state = SDLSTATE::SDLPLAYING;
            return true;
        }
        return false;
    }
    void Stop() {
        if (_state == SDLSTATE::SDLPLAYING) {
            SDL_PauseAudioDevice(audioDev, 1);
            data = nullptr;
            _state = SDLSTATE::SDLNOTPLAYING;
        }
    }
    void Close() {
        Stop();
        if (_state != SDLSTATE::SDLINITIALISED && _state != SDLSTATE::SDLUNINITIALISED) {
            SDL_CloseAudioDevice(audioDev);
            _state = SDLSTATE::SDLINITIALISED;
        }
    }
    
    bool initSDL();
    bool openAudio();

    SDLInternalData *data;
    
    std::set<std::string> blacklisted;
};

static SDL sdlManager;

void fill_audio(void *udata, Uint8 *stream, int len) {

    if (sdlManager.data != nullptr) {
        /*
        static auto lastt = std::chrono::high_resolution_clock::now();
        auto nowt = std::chrono::high_resolution_clock::now();
        int bc = sdlManager.data->bufferCount;
        int nsc = std::chrono::duration_cast<std::chrono::nanoseconds>(nowt-lastt).count();
        printf("fill: %d  %d       %d\n", bc, len, nsc);
        lastt = nowt;
        */
        
        AudioBuffer *buf = sdlManager.data->curBuffer;
        if (buf == nullptr) {
            if (AudioHasStalled) LogWarn(VB_MEDIAOUT, "Stalled, but no buffer  %d\n", sdlManager.data->doneRead);
            SDL_memset(stream, 0, len);
            return;
        }
        int sp = 0;
        while (len > 0) {
            int nl = len;
            if (nl > (buf->size - buf->pos)) {
                nl = (buf->size - buf->pos);
            }
            memcpy(&stream[sp], &buf->data[buf->pos], nl);
            buf->pos += nl;
            sp += nl;
            len -= nl;
            sdlManager.data->curPos += nl;
            if (buf->pos == buf->size) {
                buf = buf->next;
                sdlManager.data->curBuffer = buf;
                SDL_memset(&stream[sp], 0, len);
                if (buf == nullptr) {
                    if (AudioHasStalled) LogWarn(VB_MEDIAOUT, "Stalled, but last buffer hit\n");
                    sdlManager.data->lastBuffer = nullptr;
                    return;
                }
            }
        }
    } else {
        SDL_memset(stream, 0, len);
    }
}

bool SDL::initSDL()  {
    if (_state == SDLSTATE::SDLUNINITIALISED) {
        if (SDL_Init(SDL_INIT_AUDIO))  {
            LogErr(VB_MEDIAOUT, "Could not initialize SDL - %s\n", SDL_GetError());
            return false;
        }
        _state = SDLSTATE::SDLINITIALISED;
    }
    return true;
}
bool SDL::openAudio() {
    if (_state == SDLSTATE::SDLINITIALISED) {
        _initialisedRate = DEFAULT_RATE;
        
        SDL_memset(&_wanted_spec, 0, sizeof(_wanted_spec));
        _wanted_spec.freq = _initialisedRate;
        _wanted_spec.format = AUDIO_S16SYS;
        _wanted_spec.channels = 2;
        _wanted_spec.silence = 0;
        _wanted_spec.samples = DEFAULT_NUM_SAMPLES;
        _wanted_spec.callback = fill_audio;
        _wanted_spec.userdata = nullptr;
        
        SDL_AudioSpec have;
        audioDev = SDL_OpenAudioDevice(NULL, 0, &_wanted_spec, &have, 0);
        if (audioDev == 0) {
            LogErr(VB_MEDIAOUT, "Could not open audio device - %s\n", SDL_GetError());
            return false;
        }
        
        _state = SDLSTATE::SDLOPENED;
    }
    return true;
}
SDL::~SDL() {
    Stop();
    Close();
    if (_state != SDLSTATE::SDLUNINITIALISED) {
        SDL_Quit();
    }
}

bool SDLOutput::IsOverlayingVideo() {
    return sdlManager.data && sdlManager.data->video_stream_idx != -1 && !sdlManager.data->stopped;
}
bool SDLOutput::ProcessVideoOverlay(unsigned int msTimestamp) {
    if (sdlManager.data && !sdlManager.data->stopped && sdlManager.data->curVideoFrame) {
        while (sdlManager.data->curVideoFrame->next
               && sdlManager.data->curVideoFrame->next->timestamp <= msTimestamp) {
            sdlManager.data->curVideoFrame = sdlManager.data->curVideoFrame->next;
        }
        if (msTimestamp <= sdlManager.data->totalVideoLen) {
            VideoFrame *vf = (VideoFrame*)sdlManager.data->curVideoFrame;
            
            long long t = GetTime() / 1000;
            int t2 = ((int)t) - sdlManager.data->videoStartTime;
            
            //printf("v:  %d  %d      %d        %d\n", msTimestamp, vf->timestamp, t2, sdlManager.data->videoFrameCount);
            SetPixelOverlayData(sdlManager.data->videoOverlayModel, vf->data);
        }
    }
}


static void CancelRoutine(void *arg) {
}
void *BufferFillThread(void *d) {
    int old;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old);
    pthread_cleanup_push(CancelRoutine, nullptr);
    
    SDLInternalData *data = (SDLInternalData *)d;
    while (!data->stopped && !data->maybeFillBuffer(false)) {
        timespec spec;
        spec.tv_sec = 0;
        int i = data->buffersFull();
        switch (i) {
            case 0:
                //not even half full, don't sleep very long
                spec.tv_nsec = 1000000; //1 ms
                break;
            case 1:
                //half full,
                spec.tv_nsec = 15000000; //15 ms
                break;
            case 2:
                // completely full, we can sleep for a while
                spec.tv_nsec = 100000000; //100 ms
                break;
        }
        if (nanosleep(&spec, nullptr)) {
            data->stopped++;
        }
    }
    LogDebug(VB_MEDIAOUT, "Reading thread done, exiting\n");
    data->stopped++;
    pthread_cleanup_pop(0);
    return nullptr;
}

static std::string currentMediaFilename;

static void LogCallback(void *     avcl,
                        int     level,
                        const char *     fmt,
                        va_list     vl)
{
    pthread_testcancel();
    
    static int print_prefix = 1;
    static char lastBuf[256] = "";
    char buf[256];
    av_log_format_line(avcl, level, fmt, vl, buf, 256, &print_prefix);
    if (strcmp(buf, lastBuf) != 0) {
        strcpy(lastBuf, buf);
        if (level >= AV_LOG_DEBUG) {
            LogExcess(VB_MEDIAOUT, "Debug: \"%s\" - %s", currentMediaFilename.c_str(), buf);
        } else if (level >= AV_LOG_VERBOSE ) {
            LogDebug(VB_MEDIAOUT, "Verbose: \"%s\" - %s", currentMediaFilename.c_str(), buf);
        } else if (level >= AV_LOG_INFO ) {
            LogInfo(VB_MEDIAOUT, "Info: \"%s\" - %s", currentMediaFilename.c_str(), buf);
        } else if (level >= AV_LOG_WARNING) {
            if (strstr(buf, "Could not update timestamps") != nullptr) {
                //these are really ignorable
                LogDebug(VB_MEDIAOUT, "Verbose: \"%s\" - %s", currentMediaFilename.c_str(), buf);
            } else {
                LogWarn(VB_MEDIAOUT, "Warn: \"%s\" - %s", currentMediaFilename.c_str(), buf);
            }
        } else {
            LogErr(VB_MEDIAOUT, "\"%s\" - %s", currentMediaFilename.c_str(), buf);
        }
    }
    pthread_testcancel();
}
/*
 *
 */
SDLOutput::SDLOutput(const std::string &mediaFilename,
                     MediaOutputStatus *status,
                     const std::string &videoOutput)
{
	LogDebug(VB_MEDIAOUT, "SDLOutput::SDLOutput(%s)\n",
		mediaFilename.c_str());
    data = nullptr;
    m_mediaOutputStatus = status;
    m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
    
    if (sdlManager.blacklisted.find(mediaFilename) != sdlManager.blacklisted.end()) {
        currentMediaFilename = "";
        LogErr(VB_MEDIAOUT, "%s has been blacklisted!\n", mediaFilename.c_str());
        return;
    }
    std::string fullAudioPath = mediaFilename;
    if (!FileExists(mediaFilename.c_str())) {
        fullAudioPath = getMusicDirectory();
        fullAudioPath += "/";
        fullAudioPath += mediaFilename;
    }
    if (!FileExists(fullAudioPath.c_str())) {
        fullAudioPath = getVideoDirectory();
        fullAudioPath += "/";
        fullAudioPath += mediaFilename;
    }
    if (!FileExists(fullAudioPath.c_str())) {
        LogErr(VB_MEDIAOUT, "%s does not exist!\n", fullAudioPath.c_str());
        currentMediaFilename = "";
        return;
    }
    if (sdlManager.blacklisted.find(fullAudioPath) != sdlManager.blacklisted.end()) {
        currentMediaFilename = "";
        LogErr(VB_MEDIAOUT, "%s has been blacklisted!\n", mediaFilename.c_str());
        return;
    }
    currentMediaFilename = mediaFilename;
	m_mediaFilename = fullAudioPath;
    
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    av_log_set_callback(LogCallback);
    
    data = new SDLInternalData();
    
    // Initialize FFmpeg codecs
    av_register_all();
    int res = avformat_open_input(&data->formatContext, m_mediaFilename.c_str(), nullptr, nullptr);
    if (avformat_find_stream_info(data->formatContext, nullptr) < 0) {
        LogErr(VB_MEDIAOUT, "Could not find suitable input stream!\n");
        avformat_close_input(&data->formatContext);
        data->formatContext = nullptr;
        currentMediaFilename = "";
        return;
    }

    if (open_codec_context(&data->audio_stream_idx, &data->audioCodecContext, data->formatContext, AVMEDIA_TYPE_AUDIO, m_mediaFilename) >= 0) {
        data->audioStream = data->formatContext->streams[data->audio_stream_idx];
    } else {
        data->audioStream = nullptr;
        data->audio_stream_idx = -1;
    }
    int videoOverlayWidth, videoOverlayHeight;
    if (videoOutput != "--Disabled--" && videoOutput != "" && videoOutput != "--HDMI--") {
        data->videoOverlayModel = videoOutput;
        if (GetPixelOverlayModelSize(videoOutput, videoOverlayWidth, videoOverlayHeight) &&
            open_codec_context(&data->video_stream_idx, &data->videoCodecContext, data->formatContext, AVMEDIA_TYPE_VIDEO, m_mediaFilename) >= 0) {
            data->videoStream = data->formatContext->streams[data->video_stream_idx];
        } else {
            data->videoStream = nullptr;
            data->video_stream_idx = -1;
        }
    } else {
        data->videoStream = nullptr;
        data->video_stream_idx = -1;
    }
    //av_dump_format(data->formatContext, 0, m_mediaFilename.c_str(), 0);
    
    int64_t duration = data->formatContext->duration + (data->formatContext->duration <= INT64_MAX - 5000 ? 5000 : 0);
    int secs  = duration / AV_TIME_BASE;
    int us    = duration % AV_TIME_BASE;
    int mins  = secs / 60;
    secs %= 60;
    
    m_mediaOutputStatus->secondsTotal = secs;
    m_mediaOutputStatus->minutesTotal = mins;
    
    if (data->audio_stream_idx != -1) {
        int64_t in_channel_layout = av_get_default_channel_layout(data->audioCodecContext->channels);

        uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
        AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
        int out_sample_rate = DEFAULT_RATE;
        
        data->au_convert_ctx = swr_alloc_set_opts(nullptr,
                                            out_channel_layout, out_sample_fmt, out_sample_rate,
                                            in_channel_layout, data->audioCodecContext->sample_fmt, data->audioCodecContext->sample_rate, 0, nullptr);
        swr_init(data->au_convert_ctx);
        
        //get an estimate of the total length
        float d = duration / AV_TIME_BASE;
        float usf = (100 * us);
        usf /= (float)AV_TIME_BASE;
        usf /= 100.0f;
        d += usf;
        data->totalLen = d;
        data->totalDataLen = d * DEFAULT_RATE * 2 * 2;
    }
    if (data->video_stream_idx != -1) {
        data->video_frames = (long)data->videoStream->nb_frames;
        data->video_dtspersec = (int)(((int64_t)data->videoStream->duration * (int64_t)data->videoStream->avg_frame_rate.num) / ((int64_t)data->video_frames * (int64_t)data->videoStream->avg_frame_rate.den));
        
        int lengthMS = (int)(((uint64_t)data->video_frames * (uint64_t)data->videoStream->avg_frame_rate.den * 1000) / (uint64_t)data->videoStream->avg_frame_rate.num);
        if ((lengthMS <= 0 || data->video_frames <= 0) && data->videoStream->avg_frame_rate.den != 0) {
            lengthMS = (int)((uint64_t)data->formatContext->duration * (uint64_t)data->videoStream->avg_frame_rate.num / (uint64_t)data->videoStream->avg_frame_rate.den);
            data->video_frames = lengthMS  * (uint64_t)data->videoStream->avg_frame_rate.num / (uint64_t)(data->videoStream->avg_frame_rate.den) / 1000;
        }

        data->totalVideoLen = lengthMS;

        SetPixelOverlayState(data->videoOverlayModel, "Enabled");
        
        data->scaledFrame = av_frame_alloc();
    
    
        data->scaledFrame->width = videoOverlayWidth;
        data->scaledFrame->height = videoOverlayHeight;
    
        data->scaledFrame->linesize[0] = data->scaledFrame->width * 3;
        data->scaledFrame->data[0] = (uint8_t *)av_malloc(data->scaledFrame->width * data->scaledFrame->height * 3 * sizeof(uint8_t));
    
        data->swsCtx = sws_getContext(data->videoCodecContext->width,
                                      data->videoCodecContext->height,
                                      data->videoCodecContext->pix_fmt,
                                      data->scaledFrame->width, data->scaledFrame->height,
                                      AVPixelFormat::AV_PIX_FMT_RGB24, SWS_BICUBIC, nullptr,
                                      nullptr, nullptr);
    }

    data->maybeFillBuffer(true);
    data->stopped = 0;

    pthread_attr_t attr;
    int s = pthread_attr_init(&attr);
    if (s != 0)
        LogWarn(VB_MEDIAOUT, "pthread_attr_init: %d\n", s);
    s = pthread_attr_setstacksize(&attr, 2*1024*1024); //2MB
    if (s != 0)
        LogWarn(VB_MEDIAOUT, "pthread_attr_setstacksize: %d\n", s);
    s = pthread_create(&data->fillThread, &attr, BufferFillThread, (void *)data);
    if (s) {
        data->fillThread = -1;
        LogWarn(VB_MEDIAOUT, "Decode thread not started   %X\n", s);
    } else {
        pthread_setname_np(data->fillThread, "Decode Thread");
    }
    pthread_attr_destroy(&attr);
}

/*
 *
 */
SDLOutput::~SDLOutput()
{
    LogDebug(VB_MEDIAOUT, "SDLOutput::~SDLOutput() %X\n", data);
    Stop();
    if (data) {
        delete data;
    }
}

/*
 *
 */
int SDLOutput::Start(void)
{
	LogDebug(VB_MEDIAOUT, "SDLOutput::Start() %X\n", data);
    if (data) {
        SetChannelOutputFrameNumber(0);
        if (!sdlManager.Start(data)) {
            m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
            Stop();
            return 0;
        }
        m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_PLAYING;
        return 1;
    }
    m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
    return 0;
}

/*
 *
 */
static int ProcessCount = 0;
static float lastCurTime = 0;

int SDLOutput::Process(void)
{
    if (!data) {
        return 0;
    }
    static int lastRemoteSync = 0;
    
    if (data->stopped) {
        //read thread is done, we need to do any frame cleanups
        data->removeUsedBuffers();
        
        if (data->fillThread) {
            //need to "join" the thread to release resources
            pthread_join(data->fillThread, NULL);
            data->fillThread = 0;
        }
    }

    if (data->audio_stream_idx != -1) {
        //if we have an audio stream, that drives everything
        float curtime = data->curPos;
        if (curtime > 0) {
            //the last block sent is buffered, but not at the audio hardware yet
            curtime -= DEFAULT_NUM_SAMPLES;
            if (curtime > 0) {
                //we've sent DEFAULT_NUM_SAMPLES to the audio hardware, but on
                //average, only half have been played yet, but no way to really
                //tell so we'll use the average
                curtime -= ((DEFAULT_NUM_SAMPLES * 2 * 2)/ 2);
            }
        }
        
        if (lastCurTime == curtime) {
            ProcessCount++;
            if (ProcessCount >= 50) {
                LogWarn(VB_MEDIAOUT, "Audio has stalled   %d\n", data->doneRead);
                AudioHasStalled = true;
                ProcessCount = 0;
            }
        } else {
            AudioHasStalled = false;
            ProcessCount = 0;
        }
        lastCurTime = curtime;
        
        curtime /= DEFAULT_RATE; //samples per sec
        curtime /= 4; //4 bytes per sample
        
        m_mediaOutputStatus->mediaSeconds = curtime;

        float ss, s;
        ss = std::modf( m_mediaOutputStatus->mediaSeconds, &s);
        m_mediaOutputStatus->secondsElapsed = s;
        ss *= 100;
        m_mediaOutputStatus->subSecondsElapsed = ss;

        float rem = data->totalLen - m_mediaOutputStatus->mediaSeconds;
        ss = std::modf( rem, &s);
        m_mediaOutputStatus->secondsRemaining = s;
        ss *= 100;
        m_mediaOutputStatus->subSecondsRemaining = ss;

        if (data->curBuffer == nullptr && data->doneRead) {
            m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
        }
    } else if (data->video_stream_idx != -1) {
        //no audio stream, attempt data from video stream
        float total = data->totalVideoLen;
        total /= 1000.0;

        long long curTime = GetTime() / 1000;
        float elapsed = curTime - data->videoStartTime;
        elapsed /= 1000;
        float remaining = total - elapsed;
        
        m_mediaOutputStatus->mediaSeconds = elapsed;
        
        float ss, s;
        ss = std::modf(elapsed, &s);
        ss *= 100;
        m_mediaOutputStatus->secondsElapsed = s;
        m_mediaOutputStatus->subSecondsElapsed = ss;
        
        ss = std::modf(remaining, &s);
        ss *= 100;
        m_mediaOutputStatus->secondsRemaining = s;
        m_mediaOutputStatus->subSecondsRemaining = ss;
        
        if (remaining < 0.0 && data->doneRead) {
            m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
        }
    }
    if (getFPPmode() == MASTER_MODE) {
        if ((m_mediaOutputStatus->secondsElapsed > 0) &&
            (lastRemoteSync != m_mediaOutputStatus->secondsElapsed)) {
            multiSync->SendMediaSyncPacket(m_mediaFilename.c_str(), 0,
                                m_mediaOutputStatus->mediaSeconds);
            lastRemoteSync = m_mediaOutputStatus->secondsElapsed;
        }
    }
    
    if ((sequence->IsSequenceRunning()) &&
        (m_mediaOutputStatus->secondsElapsed > 0)) {
        LogExcess(VB_MEDIAOUT,
                  "Elapsed: %.2d.%.2d  Remaining: %.2d Total %.2d:%.2d.\n",
                  m_mediaOutputStatus->secondsElapsed,
                  m_mediaOutputStatus->subSecondsElapsed,
                  m_mediaOutputStatus->secondsRemaining,
                  m_mediaOutputStatus->minutesTotal,
                  m_mediaOutputStatus->secondsTotal);
        CalculateNewChannelOutputDelay(m_mediaOutputStatus->mediaSeconds);
    }
	return m_mediaOutputStatus->status == MEDIAOUTPUTSTATUS_PLAYING;
}
int SDLOutput::IsPlaying(void)
{
    return m_mediaOutputStatus->status == MEDIAOUTPUTSTATUS_PLAYING;
}

int  SDLOutput::Close(void)
{
    LogDebug(VB_MEDIAOUT, "SDLOutput::Close()\n");
    Stop();
    sdlManager.Close();
    return 0;
}

/*
 *
 */
int SDLOutput::Stop(void)
{
	LogDebug(VB_MEDIAOUT, "SDLOutput::Stop()\n");
    sdlManager.Stop();
    if (data) {
        data->stopped++;
        if (data->video_stream_idx >= 0) {
            data->video_stream_idx = -1;
            FillPixelOverlayModel(data->videoOverlayModel, 0, 0, 0);
            SetPixelOverlayState(data->videoOverlayModel, "Disabled");
        }
        
        timespec tv;
        time(&tv.tv_sec);
        
        tv.tv_sec += 15;
        tv.tv_nsec = 0;
        //wait up to 15 seconds
        if (pthread_timedjoin_np(data->fillThread, NULL, &tv) == ETIMEDOUT) {
            LogWarn(VB_MEDIAOUT, "Joining thread didn't work\n");
            //cancel the thread, nothing we can do now
            pthread_cancel(data->fillThread);
            sdlManager.blacklisted.insert(m_mediaFilename);
            LogWarn(VB_MEDIAOUT, "Problems decoding %s, blacklisting\n", m_mediaFilename.c_str());
        }
    }
	m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
	return 1;
}



