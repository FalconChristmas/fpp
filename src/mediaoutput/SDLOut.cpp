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


#include "common.h"
#include "log.h"
#include "MultiSync.h"
#include "SDLOut.h"
#include "Sequence.h"
#include "settings.h"
#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"
#include "Warnings.h"
#include "channeloutput/channeloutputthread.h"

//Only keep 30 frames in buffer
#define VIDEO_FRAME_MAX     30

#if defined(PLATFORM_PI)
//on the old single core Pi's, we need to increase the buffer size
//a bit or the audio stutters.
static const int DEFAULT_NUM_SAMPLES = std::thread::hardware_concurrency() <= 1 ? 2048 : 1024;
#else
static const int DEFAULT_NUM_SAMPLES = 1024;
#endif

static bool AudioHasStalled = false;


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
    SDLInternalData(int rate, int bps, bool flt) : curPos(0) {
        formatContext = nullptr;
        audioCodecContext = nullptr;
        videoCodecContext = nullptr;
        audio_stream_idx = -1;
        video_stream_idx = -1;
        videoStream = audioStream = nullptr;
        doneRead = false;
        frame = av_frame_alloc();
        scaledFrame = nullptr;
        au_convert_ctx = nullptr;
        decodedDataLen = 0;
        swsCtx = nullptr;
        firstVideoFrame = lastVideoFrame = nullptr;
        curVideoFrame = nullptr;
        videoFrameCount = 0;
        audioDev = 0;
        outBufferPos = 0;
        currentRate = rate;
        bytesPerSample = bps;
        isSamplesFloat = flt;
        
        minQueueSize = rate*bps*2*2;  // 2 seconds of 2 channel audio
        maxQueueSize = minQueueSize * 2;
        outBuffer = new uint8_t[maxQueueSize];
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

        delete [] outBuffer;
    }

    volatile int stopped;
    AVFormatContext*formatContext;
    AVPacket readingPacket;
    AVFrame* frame;
    
    
    // stuff for the audio stream
    AVCodecContext *audioCodecContext;
    int audio_stream_idx = -1;
    AVStream* audioStream;
    SwrContext *au_convert_ctx;
    unsigned int totalDataLen;
    unsigned int decodedDataLen;
    float totalLen;
    SDL_AudioDeviceID audioDev;
    int outBufferPos = 0;
    uint8_t *outBuffer;
    int currentRate;
    int bytesPerSample;
    bool isSamplesFloat;
    int minQueueSize;
    int maxQueueSize;

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
    volatile VideoFrame * volatile curVideoFrame;
    int videoFrameCount;
    unsigned int totalVideoLen;
    long long videoStartTime;
    PixelOverlayModel *videoOverlayModel = nullptr;
    
    
    bool doneRead;
    unsigned int curPos;
    std::mutex curPosLock;
    
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

    int buffersFull(bool flushaudio) {
        int retVal = -1;
        if (video_stream_idx != -1) {
            //if video
            while (firstVideoFrame && (firstVideoFrame != curVideoFrame)) {
                videoFrameCount--;
                auto tmp = firstVideoFrame->next;
                delete firstVideoFrame;
                firstVideoFrame = tmp;
            }
            retVal = (doneRead || (videoFrameCount >= VIDEO_FRAME_MAX)) ? 2
                : ((videoFrameCount >= (VIDEO_FRAME_MAX - 6)) ? 1 : 0);
            if (!flushaudio) {
                return retVal;
            }
        }
        if (audioDev == 0) {
            //no audio device, clear the audio buffer
            curPosLock.lock();
            curPos += outBufferPos;
            outBufferPos = 0;
            curPosLock.unlock();
            return retVal >= 0 ? retVal : 2;
        }
        unsigned int queue = SDL_GetQueuedAudioSize(audioDev);
        //if we have data and are either below the queue threshold or we've finished reading
        if (outBufferPos && ((queue < minQueueSize) || doneRead)) {
            curPosLock.lock();
            SDL_QueueAudio(audioDev, outBuffer, outBufferPos);
            curPos += outBufferPos;
            outBufferPos = 0;
            curPosLock.unlock();
            queue = SDL_GetQueuedAudioSize(audioDev);
        }
        if (retVal >= 0) {
            return retVal;
        }
        if (doneRead) {
            //done reading, they are as full as they will get
            return 2;
        }
        queue += outBufferPos;
        if (queue < minQueueSize) {
            return 0;
        } else if (queue < maxQueueSize) {
            return 1;
        }
        return 2;
    }
    int maybeFillBuffer(bool first) {
        if (doneRead || videoFrameCount > VIDEO_FRAME_MAX) {
            //buffers are full, don't so anything
            if (AudioHasStalled) LogWarn(VB_MEDIAOUT, "Stalled audio, buffers are full.  %d\n", doneRead);
            return 0;
        }
        if (AudioHasStalled) LogWarn(VB_MEDIAOUT, "Stalled audio, buffers still filling.\n");
        int orig = outBufferPos;
        bool vidPacket = false;
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
                        
                        uint8_t* out_buffer = &outBuffer[outBufferPos];
                        int max = maxQueueSize - outBufferPos;
                        int outSamples = swr_convert(au_convert_ctx,
                                                     &out_buffer,
                                                     max / 4,
                                                     (const uint8_t **)frame->extended_data,
                                                     frame->nb_samples);

                        outBufferPos += (outSamples * bytesPerSample * 2);
                        if (outBufferPos > maxQueueSize) {
                            AudioHasStalled = true;
                        }
                        decodedDataLen += (outSamples * bytesPerSample * 2);
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
                        vidPacket = true;
                        av_frame_unref(frame);
                    }
                }
                packetOk = true;
            }
            av_packet_unref(&readingPacket);
            
            if (packetOk) {
                if (first) {
                    if ((outBufferPos > minQueueSize || videoFrameCount > VIDEO_FRAME_MAX))  {
                        return outBufferPos - orig;
                    }
                } else if (video_stream_idx != -1 && !vidPacket) {
                    //didn't get a video packet, we'll keep going
                } else {
                    return outBufferPos - orig;
                }
            }
        }
        
        totalDataLen = decodedDataLen;
        doneRead = true;
        return outBufferPos - orig;
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
    SDLNOTPLAYING,
    SDLDESTROYED
} SDLSTATE;

class SDL
{
    volatile SDLSTATE _state;
    SDL_AudioSpec _wanted_spec;
    int _initialisedRate;
    int _bytesPerSample;
    bool _isSampleFloat;
    SDL_AudioDeviceID audioDev;
    std::atomic_bool decoding;
    
public:
    SDL() : data(nullptr), _state(SDLSTATE::SDLUNINITIALISED), decodeThread(nullptr) {}
    virtual ~SDL();
    
    int getRate() { return _initialisedRate; }
    int getBytesPerSample() { return _bytesPerSample; }
    bool isSamplesFloat() { return _isSampleFloat; }

    static void decodeThreadEntry(SDL *sdl) {
        sdl->runDecode();
    }
    bool Start(SDLInternalData *d) {
        if (!initSDL()) {
            return false;
        }
        if (!openAudio()) {
            return false;
        }
        if (!decodeThread) {
            decodeThread = new std::thread(decodeThreadEntry, this);
        }
        if (_state != SDLSTATE::SDLINITIALISED && _state != SDLSTATE::SDLUNINITIALISED) {
            data = d;
            if (audioDev) {
                data->curPosLock.lock();
                SDL_ClearQueuedAudio(audioDev);
                SDL_QueueAudio(audioDev, data->outBuffer, data->outBufferPos);
                data->curPos = data->outBufferPos;
                data->outBufferPos = 0;
                data->curPosLock.unlock();
                SDL_PauseAudioDevice(audioDev, 0);
            } else {
                data->curPos = 0;
                data->outBufferPos = 0;
            }
            data->audioDev = audioDev;

            long long t = GetTime() / 1000;
            data->videoStartTime = t;
            _state = SDLSTATE::SDLPLAYING;
            return true;
        }
        return false;
    }
    void Stop() {
        if (_state == SDLSTATE::SDLPLAYING) {
            if (audioDev) {
                SDL_PauseAudioDevice(audioDev, 1);
                SDL_ClearQueuedAudio(audioDev);
            }
            SDLInternalData *d = data;
            data = nullptr;
            _state = SDLSTATE::SDLNOTPLAYING;
            while (decoding) {
                //wait for decoding thread to be done with it
                std::this_thread::yield();
            }
        }
    }
    void Close() {
        Stop();
        if (_state != SDLSTATE::SDLINITIALISED && _state != SDLSTATE::SDLUNINITIALISED) {
            if (audioDev) {
                SDL_CloseAudioDevice(audioDev);
            }
            _state = SDLSTATE::SDLINITIALISED;
        }
    }
    
    bool initSDL();
    bool openAudio();
    void runDecode();

    SDLInternalData * volatile data;
    std::thread *decodeThread;
    std::set<std::string> blacklisted;
};

static SDL sdlManager;

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

void SDL::runDecode() {
    while (_state != SDLSTATE::SDLUNINITIALISED) {
        decoding = true;
        SDLInternalData *data = this->data;
        if (data == nullptr) {
            decoding = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        } else {
            int bufFull = data->buffersFull(true);
            bool bufFillWas0 = false;
            int count = 0;
            while (bufFull == 0 && count < 5) {
                //critical, the SDL queue is < max
                data->maybeFillBuffer(false);
                bufFull = data->buffersFull(false);
                bufFillWas0 = true;
                count++;
            }
            count = 0;
            int countRead = 0;
            while (bufFull != 2 && count < 5) {
                count++;
                if (data->outBufferPos > data->minQueueSize > 2) {
                    //single packet
                    countRead += data->maybeFillBuffer(false);
                    bufFull = 2;
                } else {
                    //read a little more than single
                    countRead += data->maybeFillBuffer(false);
                    if (countRead > (data->currentRate * data->bytesPerSample * 2/10)) {
                        // read a 1/10 of a second, move on
                        bufFull = 2;
                    } else {
                        bufFull = data->buffersFull(false);
                    }
                }
            }
            if (data->video_stream_idx != -1 && data->videoFrameCount < 15) {
                //we won't sleep, need to keep decoding
                decoding = false;
            } else {
                decoding = false;
                if (bufFillWas0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                }
            }
        }
    }
    _state = SDLSTATE::SDLDESTROYED;
}

static bool noDeviceWarning = false;
static std::string noDeviceError;
bool SDL::openAudio() {
    if (_state == SDLSTATE::SDLINITIALISED) {
        int tp = getSettingInt("AudioFormat");
        
        SDL_memset(&_wanted_spec, 0, sizeof(_wanted_spec));
        switch (tp) {
            case 0:
#if defined(PLATFORM_BBB) || defined(PLATFORM_PI)
                _wanted_spec.freq = 48000;
#else
                _wanted_spec.freq = 44100;
#endif
                _wanted_spec.format = AUDIO_S16;
                break;
            
            case 1:
                _wanted_spec.freq = 44100;
                _wanted_spec.format = AUDIO_S16;
            break;
            case 2:
                _wanted_spec.freq = 44100;
                _wanted_spec.format = AUDIO_S32;
            break;
            case 3:
                _wanted_spec.freq = 44100;
                _wanted_spec.format = AUDIO_F32;
            break;
            
            case 4:
                _wanted_spec.freq = 48000;
                _wanted_spec.format = AUDIO_S16;
            break;
            case 5:
                _wanted_spec.freq = 48000;
                _wanted_spec.format = AUDIO_S32;
            break;
            case 6:
                _wanted_spec.freq = 48000;
                _wanted_spec.format = AUDIO_F32;
            break;
            
            case 7:
                _wanted_spec.freq = 96000;
                _wanted_spec.format = AUDIO_S16;
            break;
            case 8:
                _wanted_spec.freq = 96000;
                _wanted_spec.format = AUDIO_S32;
            break;
            case 9:
                _wanted_spec.freq = 96000;
                _wanted_spec.format = AUDIO_F32;
            break;
        }
        
        _wanted_spec.channels = 2;
        _wanted_spec.silence = 0;
        _wanted_spec.samples = DEFAULT_NUM_SAMPLES;
        _wanted_spec.callback = nullptr;
        _wanted_spec.userdata = nullptr;
        
        SDL_AudioSpec have;
        audioDev = SDL_OpenAudioDevice(NULL, 0, &_wanted_spec, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE);
        if (audioDev == 0 && !noDeviceWarning) {
            noDeviceError = "Could not open audio device - ";
            noDeviceError += SDL_GetError();
            LogErr(VB_MEDIAOUT, "%s\n", noDeviceError.c_str());
            noDeviceWarning = true;
        } else {
            LogDebug(VB_MEDIAOUT, "Opened Audio Device -  Rates:  %d -> %d     AudioFormat:  %X -> %X \n", _wanted_spec.freq, have.freq, _wanted_spec.format, have.format);
            if (have.format != AUDIO_S32 && have.format != AUDIO_S16 && have.format != AUDIO_F32) {
                //we'll only support these
                LogDebug(VB_MEDIAOUT, "    Format not supported, will reopen\n");
                SDL_CloseAudioDevice(audioDev);
                audioDev = SDL_OpenAudioDevice(NULL, 0, &_wanted_spec, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
                if (audioDev == 0 && !noDeviceWarning) {
                    noDeviceError = "Could not open audio device - ";
                    noDeviceError += SDL_GetError();
                    LogErr(VB_MEDIAOUT, "%s\n", noDeviceError.c_str());
                    noDeviceWarning = true;
                }
                LogDebug(VB_MEDIAOUT, "Repened Audio Device -  Rates:  %d -> %d     AudioFormat:  %X -> %X \n", _wanted_spec.freq, have.freq, _wanted_spec.format, have.format);
            }
        }
        _initialisedRate = have.freq;
        _bytesPerSample = (have.format == AUDIO_S16) ? 2 : 4;
        _isSampleFloat = (have.format == AUDIO_F32);
        
        _state = SDLSTATE::SDLOPENED;
    }
    return true;
}
SDL::~SDL() {
    Stop();
    Close();
    if (_state != SDLSTATE::SDLUNINITIALISED) {
        SDL_Quit();
        _state = SDLSTATE::SDLUNINITIALISED;
    }
    if (decodeThread) {
        int count = 0;
        while (count < 30) {
            if (_state == SDLSTATE::SDLDESTROYED) {
                count = 30;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        decodeThread->detach();
        delete decodeThread;
    }
}

bool SDLOutput::IsOverlayingVideo() {
    SDLInternalData *data = sdlManager.data;
    return data && data->video_stream_idx != -1 && !data->stopped;
}
bool SDLOutput::ProcessVideoOverlay(unsigned int msTimestamp) {
    SDLInternalData *data = sdlManager.data;
    if (data && !data->stopped && data->curVideoFrame && data->videoOverlayModel) {
        while (data->curVideoFrame->next
               && data->curVideoFrame->next->timestamp <= msTimestamp) {
            data->curVideoFrame = data->curVideoFrame->next;
        }
        if (msTimestamp <= data->totalVideoLen) {
            VideoFrame *vf = (VideoFrame*)data->curVideoFrame;
            
            long long t = GetTime() / 1000;
            int t2 = ((int)t) - data->videoStartTime;
            
            //printf("v:  %d  %d      %d        %d\n", msTimestamp, vf->timestamp, t2, sdlManager.data->videoFrameCount);
            data->videoOverlayModel->setData(vf->data);
        }
    }
    return false;
}

static std::string currentMediaFilename;

static void LogCallback(void *     avcl,
                        int     level,
                        const char *     fmt,
                        va_list     vl)
{
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
            if (strstr(buf, "Could not update timestamps") != nullptr
                || strstr(buf, "Estimating duration from bitrate") != nullptr) {
                //these are really ignorable
                LogDebug(VB_MEDIAOUT, "Verbose: \"%s\" - %s", currentMediaFilename.c_str(), buf);
            } else {
                LogWarn(VB_MEDIAOUT, "Warn: \"%s\" - %s", currentMediaFilename.c_str(), buf);
            }
        } else {
            LogErr(VB_MEDIAOUT, "\"%s\" - %s", currentMediaFilename.c_str(), buf);
        }
    }
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
    
    
    m_mediaOutputStatus->mediaSeconds = 0.0;
    m_mediaOutputStatus->secondsElapsed = 0;
    m_mediaOutputStatus->subSecondsElapsed = 0;
    
    if (sdlManager.blacklisted.find(mediaFilename) != sdlManager.blacklisted.end()) {
        currentMediaFilename = "";
        LogErr(VB_MEDIAOUT, "%s has been blacklisted!\n", mediaFilename.c_str());
        return;
    }
    std::string fullAudioPath = mediaFilename;
    if (!FileExists(mediaFilename)) {
        fullAudioPath = getMusicDirectory();
        fullAudioPath += "/";
        fullAudioPath += mediaFilename;
    }
    if (!FileExists(fullAudioPath)) {
        fullAudioPath = getVideoDirectory();
        fullAudioPath += "/";
        fullAudioPath += mediaFilename;
    }
    if (!FileExists(fullAudioPath)) {
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
	m_mediaFilename = mediaFilename;
    
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    av_log_set_callback(LogCallback);
    
    
    sdlManager.initSDL();
    sdlManager.openAudio();
    
    data = new SDLInternalData(sdlManager.getRate(), sdlManager.getBytesPerSample(), sdlManager.isSamplesFloat());

    // Initialize FFmpeg codecs
#if LIBAVFORMAT_VERSION_MAJOR < 58
    av_register_all();
#endif
    int res = avformat_open_input(&data->formatContext, fullAudioPath.c_str(), nullptr, nullptr);
    if (avformat_find_stream_info(data->formatContext, nullptr) < 0) {
        LogErr(VB_MEDIAOUT, "Could not find suitable input stream!\n");
        avformat_close_input(&data->formatContext);
        data->formatContext = nullptr;
        currentMediaFilename = "";
        return;
    }

    if (open_codec_context(&data->audio_stream_idx, &data->audioCodecContext, data->formatContext, AVMEDIA_TYPE_AUDIO, fullAudioPath.c_str()) >= 0) {
        data->audioStream = data->formatContext->streams[data->audio_stream_idx];
    } else {
        data->audioStream = nullptr;
        data->audio_stream_idx = -1;
    }
    int videoOverlayWidth, videoOverlayHeight;
    if (videoOutput != "--Disabled--" && videoOutput != "" && videoOutput != "--HDMI--") {
        data->videoOverlayModel = PixelOverlayManager::INSTANCE.getModel(videoOutput);
        
        if (data->videoOverlayModel &&
            open_codec_context(&data->video_stream_idx, &data->videoCodecContext, data->formatContext, AVMEDIA_TYPE_VIDEO, fullAudioPath.c_str()) >= 0) {
            data->videoOverlayModel->getSize(videoOverlayWidth, videoOverlayHeight);
            data->videoStream = data->formatContext->streams[data->video_stream_idx];
        } else {
            data->videoStream = nullptr;
            data->video_stream_idx = -1;
        }
    } else {
        data->videoStream = nullptr;
        data->video_stream_idx = -1;
    }
    //av_dump_format(data->formatContext, 0, fullAudioPath.c_str(), 0);
    
    int64_t duration = data->formatContext->duration + (data->formatContext->duration <= INT64_MAX - 5000 ? 5000 : 0);
    int secs  = duration / AV_TIME_BASE;
    int us    = duration % AV_TIME_BASE;
    int mins  = secs / 60;
    secs %= 60;
    
    m_mediaOutputStatus->secondsTotal = secs;
    m_mediaOutputStatus->minutesTotal = mins;
    
    m_mediaOutputStatus->secondsRemaining = mins * 60 + secs;
    m_mediaOutputStatus->subSecondsRemaining = 0;

    if (data->audio_stream_idx != -1) {
        int64_t in_channel_layout = av_get_default_channel_layout(data->audioCodecContext->channels);

        uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
        AVSampleFormat out_sample_fmt = (data->bytesPerSample == 2) ? AV_SAMPLE_FMT_S16 : (data->isSamplesFloat ? AV_SAMPLE_FMT_FLT :AV_SAMPLE_FMT_S32 );
        int out_sample_rate = data->currentRate;
        
        data->au_convert_ctx = swr_alloc_set_opts(nullptr,
                                            out_channel_layout, out_sample_fmt, out_sample_rate,
                                            in_channel_layout, data->audioCodecContext->sample_fmt,
                                            data->audioCodecContext->sample_rate, 0, nullptr);
        swr_init(data->au_convert_ctx);
        
        //get an estimate of the total length
        float d = duration / AV_TIME_BASE;
        float usf = (100 * us);
        usf /= (float)AV_TIME_BASE;
        usf /= 100.0f;
        d += usf;
        data->totalLen = d;
        data->totalDataLen = d * data->currentRate * data->bytesPerSample * 2;
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

        if (data->videoOverlayModel) {
            data->videoOverlayModel->setState(PixelOverlayState::Enabled);
        }
        
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

    data->stopped = 0;
    data->maybeFillBuffer(true);
}

/*
 *
 */
SDLOutput::~SDLOutput()
{
    LogDebug(VB_MEDIAOUT, "SDLOutput::~SDLOutput() %X\n", data);
    Close();
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
            if (noDeviceWarning) {
                WarningHolder::AddWarning(noDeviceError);
            }
            m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
            Stop();
            return 0;
        }
        if (data->audioDev == 0 && data->video_stream_idx == -1) {
            //no audio device so audio data is useless and no video stream so not useful either,
            //bail
            if (noDeviceWarning) {
                WarningHolder::AddWarning(noDeviceError);
            }
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
    
    if (data->audio_stream_idx != -1 && data->audioDev) {
        //if we have an audio stream, that drives everything
        data->curPosLock.lock();
        float qas = SDL_GetQueuedAudioSize(data->audioDev);
        long cp = data->curPos;
        data->curPosLock.unlock();
        float curtime = cp - qas;
        curtime -= (DEFAULT_NUM_SAMPLES * 2);
        
        if (curtime < 0) curtime = 0;

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
        
        curtime /= data->currentRate; //samples per sec
        curtime /= 2 * data->bytesPerSample; //bytes per sample * 2 channels
        
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

        if (data->doneRead && SDL_GetQueuedAudioSize(data->audioDev) == 0) {
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
        multiSync->SendMediaSyncPacket(m_mediaFilename,
                            m_mediaOutputStatus->mediaSeconds);
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
            if (data->videoOverlayModel) {
                data->videoOverlayModel->clearOverlayBuffer();
                data->videoOverlayModel->flushOverlayBuffer();
                data->videoOverlayModel->setState(PixelOverlayState::Disabled);
            }
        }
    }
	m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
	return 1;
}



