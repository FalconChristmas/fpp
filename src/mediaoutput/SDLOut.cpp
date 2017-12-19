/*
 *   mpg123 player driver for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#include <list>
#include <mutex>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
    
#include <SDL2/SDL.h>
}


#include "channeloutputthread.h"
#include "common.h"
#include "controlsend.h"
#include "log.h"
#include "SDLOut.h"
#include "Sequence.h"
#include "settings.h"

//1/2 second buffers
#define MAX_BUFFER_SIZE  44100*2
//buffer the first 5 seconds
#define INITIAL_BUFFER_MAX  10
//keep a 5 second buffer
#define ONGOING_BUFFER_MAX  10

#define DEFAULT_NUM_SAMPLES 1024
#define DEFAULT_RATE 44100

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



class SDLInternalData {
public:
    SDLInternalData() : curPos(0) {
        formatContext = nullptr;
        codecContext = nullptr;
        audio_stream_idx = -1;
        firstBuffer = lastBuffer = fillBuffer = nullptr;
        curBuffer = nullptr;
        bufferCount = 0;
        doneRead = false;
        frame = av_frame_alloc();
        au_convert_ctx = nullptr;
        decodedDataLen = 0;
    }
    ~SDLInternalData() {
        if (frame != nullptr) {
            av_free(frame);
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
    AVFormatContext* formatContext;
    AVCodecContext *codecContext;
    int audio_stream_idx = -1;
    AVPacket readingPacket;
    AVFrame* frame;
    AVStream* audioStream;
    SwrContext *au_convert_ctx;

    AudioBuffer *firstBuffer;
    AudioBuffer * volatile curBuffer;
    AudioBuffer *lastBuffer;
    AudioBuffer *fillBuffer;
    int bufferCount;
    bool doneRead;
    std::atomic_uint curPos;
    unsigned int totalDataLen;
    unsigned int decodedDataLen;
    float totalLen;
    
    void addBuffer(AudioBuffer *b) {
        b->size = b->pos;
        b->pos = 0;
        if (b->size == 0) {
            delete b;
            return;
        }
        
        if (lastBuffer == nullptr) {
            firstBuffer = b;
            curBuffer = b;
            lastBuffer = b;
        } else {
            lastBuffer->next = b;
            lastBuffer = b;
        }
        bufferCount++;
    }
    bool maybeFillBuffer(bool first) {
        while (firstBuffer != curBuffer) {
            AudioBuffer *tmp = firstBuffer;
            --bufferCount;
            firstBuffer = tmp->next;
            delete tmp;
        }
        if (doneRead || bufferCount > 30) {
            //have ~30 seconds of audio already buffered, don't so anything
            return doneRead;
        }
        
        if (fillBuffer == nullptr) {
            fillBuffer = new AudioBuffer();
        }
        bool newBuf = false;
        while (av_read_frame(formatContext, &readingPacket) == 0) {
            if (readingPacket.stream_index == audioStream->index) {
                while (avcodec_send_packet(codecContext, &readingPacket)) {
                    while (!avcodec_receive_frame(codecContext, frame)) {
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
                }
                if (bufferCount > (first ? INITIAL_BUFFER_MAX : ONGOING_BUFFER_MAX)) {
                    av_packet_unref(&readingPacket);
                    return doneRead;
                }
            }
            av_packet_unref(&readingPacket);
        }
        while (!avcodec_receive_frame(codecContext, frame)) {
            int sz = frame->nb_samples * 2 * 2;
            if ((sz + fillBuffer->pos) >= fillBuffer->size) {
                addBuffer(fillBuffer);
                fillBuffer = new AudioBuffer();
            }

            uint8_t* out_buffer = &fillBuffer->data[fillBuffer->pos];
            int outSamples = swr_convert(au_convert_ctx,
                                         &out_buffer,
                                         (fillBuffer->size - fillBuffer->pos) / 4,
                                         (const uint8_t **)frame->extended_data, frame->nb_samples);
            av_frame_unref(frame);
            fillBuffer->pos += (outSamples * 2 * 2);
            decodedDataLen += (outSamples * 2 * 2);
            if (bufferCount > (first ? INITIAL_BUFFER_MAX : ONGOING_BUFFER_MAX)) {
                return doneRead;
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
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename.c_str());
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

public:
    SDL() : hasStarted(false), data(nullptr), _state(SDLSTATE::SDLUNINITIALISED) {}
    virtual ~SDL();
    
    void Start(SDLInternalData *d) {
        initAudio();
        data = d;
        SDL_PauseAudio(0);
        _state = SDLSTATE::SDLPLAYING;
    }
    void Stop() {
        SDL_PauseAudio(1);
        data = nullptr;
        _state = SDLSTATE::SDLNOTPLAYING;
    };
    
    void initAudio();
    
    bool hasStarted;
    SDLInternalData *data;
    
    std::set<std::string> blacklisted;
};

static SDL sdlManager;


void fill_audio(void *udata, Uint8 *stream, int len) {
    if (sdlManager.data != nullptr) {
        AudioBuffer *buf = sdlManager.data->curBuffer;
        if (buf == nullptr) {
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
                    return;
                }
            }
        }
    } else {
        SDL_memset(stream, 0, len);
    }
}


void SDL::initAudio()  {
    if (hasStarted) {
        return;
    }
    
    if (!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE")) {
        SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE", "1", true);
    }
    
    hasStarted = true;
    _state = SDLSTATE::SDLUNINITIALISED;
    
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        return;
    }
    
    _state = SDLSTATE::SDLINITIALISED;
    _initialisedRate = DEFAULT_RATE;
    
    _wanted_spec.freq = _initialisedRate;
    _wanted_spec.format = AUDIO_S16SYS;
    _wanted_spec.channels = 2;
    _wanted_spec.silence = 0;
    _wanted_spec.samples = DEFAULT_NUM_SAMPLES;
    _wanted_spec.callback = fill_audio;
    _wanted_spec.userdata = nullptr;
    
    if (SDL_OpenAudio(&_wanted_spec, nullptr) < 0)
    {
        return;
    }
    
    _state = SDLSTATE::SDLOPENED;
}
SDL::~SDL() {
    if (_state != SDLSTATE::SDLOPENED && _state != SDLSTATE::SDLINITIALISED && _state != SDLSTATE::SDLUNINITIALISED)
    {
        Stop();
    }
    if (_state != SDLSTATE::SDLINITIALISED && _state != SDLSTATE::SDLUNINITIALISED)
    {
        SDL_CloseAudio();
    }
    if (_state != SDLSTATE::SDLUNINITIALISED)
    {
        SDL_Quit();
    }
}

void CancelRoutine(void *arg) {
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
        spec.tv_nsec = 10000000; //10 ms
        if (nanosleep(&spec, nullptr)) {
            data->stopped++;
        }
    }
    data->stopped++;
    pthread_cleanup_pop(0);
    return nullptr;
}

static std::string currentMediaFilename;
static void LogCallback(void *     avcl,
                        int     level,
                        const char *     fmt,
                        va_list     vl) {
    pthread_testcancel();
    
    static int print_prefix = 1;
    static char lastBuf[256] = "";
    char buf[256];
    av_log_format_line(avcl, level, fmt, vl, buf, 256, &print_prefix);
    if (strcmp(buf, lastBuf) != 0) {
        strcpy(lastBuf, buf);
        if (level >= AV_LOG_DEBUG) {
            LogExcess(VB_MEDIAOUT, "\"%s\" - %s", currentMediaFilename.c_str(), buf);
        } else if (level >= AV_LOG_VERBOSE ) {
            LogDebug(VB_MEDIAOUT, "\"%s\" - %s", currentMediaFilename.c_str(), buf);
        } else if (level >= AV_LOG_INFO ) {
            LogInfo(VB_MEDIAOUT, "\"%s\" - %s", currentMediaFilename.c_str(), buf);
        } else if (level >= AV_LOG_WARNING) {
            LogWarn(VB_MEDIAOUT, "\"%s\" - %s", currentMediaFilename.c_str(), buf);
        } else {
            LogErr(VB_MEDIAOUT, "\"%s\" - %s", currentMediaFilename.c_str(), buf);
        }
    }
    pthread_testcancel();
}
/*
 *
 */
SDLOutput::SDLOutput(const std::string &mediaFilename, MediaOutputStatus *status)
{
	LogDebug(VB_MEDIAOUT, "SDLOutput::SDLOutput(%s)\n",
		mediaFilename.c_str());
    data = nullptr;
    
    std::string fullAudioPath = mediaFilename;
    if (!FileExists(mediaFilename.c_str())) {
        fullAudioPath = getMusicDirectory();
        fullAudioPath += "/";
        fullAudioPath += mediaFilename;
    }

    if (!FileExists(fullAudioPath.c_str()))
    {
        LogErr(VB_MEDIAOUT, "%s does not exist!\n", fullAudioPath.c_str());
        return;
    }
    
    if (sdlManager.blacklisted.find(fullAudioPath) != sdlManager.blacklisted.end()) {
        LogErr(VB_MEDIAOUT, "%s has been blacklisted!\n", fullAudioPath.c_str());
        return;
    }
    currentMediaFilename = mediaFilename;
	m_mediaFilename = fullAudioPath;
	m_mediaOutputStatus = status;
    
    av_log_set_flags(AV_LOG_SKIP_REPEATED);
    av_log_set_callback(LogCallback);
    
    data = new SDLInternalData();

    
    // Initialize FFmpeg codecs
    av_register_all();
    int res = avformat_open_input(&data->formatContext, m_mediaFilename.c_str(), nullptr, nullptr);
    if (avformat_find_stream_info(data->formatContext, nullptr) < 0)
    {
        LogErr(VB_MEDIAOUT, "Could not find suitable input stream!\n");
        avformat_close_input(&data->formatContext);
        data->formatContext = nullptr;
        return;
    }

    if (open_codec_context(&data->audio_stream_idx, &data->codecContext, data->formatContext, AVMEDIA_TYPE_AUDIO, m_mediaFilename) >= 0) {
        data->audioStream = data->formatContext->streams[data->audio_stream_idx];
    }
    //av_dump_format(data->formatContext, 0, m_mediaFilename.c_str(), 0);
    
    int64_t duration = data->formatContext->duration + (data->formatContext->duration <= INT64_MAX - 5000 ? 5000 : 0);
    int secs  = duration / AV_TIME_BASE;
    int us    = duration % AV_TIME_BASE;
    int mins  = secs / 60;
    secs %= 60;
    
    m_mediaOutputStatus->secondsTotal = secs;
    m_mediaOutputStatus->minutesTotal = mins;
    
    int64_t in_channel_layout = av_get_default_channel_layout(data->codecContext->channels);
    
    uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
    AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
    int out_sample_rate = DEFAULT_RATE;
    
    data->au_convert_ctx = swr_alloc_set_opts(nullptr,
                                        out_channel_layout, out_sample_fmt, out_sample_rate,
                                        in_channel_layout, data->codecContext->sample_fmt, data->codecContext->sample_rate, 0, nullptr);
    swr_init(data->au_convert_ctx);
    
    //get an estimate of the total length
    float d = duration / AV_TIME_BASE;
    float usf = (100 * us);
    usf /= (float)AV_TIME_BASE;
    usf /= 100.0f;
    d += usf;
    data->totalLen = d;
    data->totalDataLen = d * DEFAULT_RATE * 2 * 2;
    data->maybeFillBuffer(true);
    data->stopped = 0;
    
    pthread_create(&data->fillThread, nullptr, BufferFillThread, (void *)data);
}

/*
 *
 */
SDLOutput::~SDLOutput()
{
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
	LogDebug(VB_MEDIAOUT, "SDLOutput::Start()\n");
    if (data) {
        sdlManager.Start(data);
        m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_PLAYING;
    } else {
        m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
    }
	return 1;
}

/*
 *
 */
int SDLOutput::Process(void)
{
    if (!data) {
        return 0;
    }
    static int lastRemoteSync = 0;
    
    float curtime = data->curPos;
    if (curtime > 0) {
        //we've sent DEFAULT_NUM_SAMPLES to the audio, but on
        //average, only half have been played yet, but no way to really
        //tell so we'll use the average
        curtime -= ((DEFAULT_NUM_SAMPLES * 2 * 2)/ 2);
    }
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

    
    if (data->curBuffer == nullptr) {
        m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
    }
    if (getFPPmode() == MASTER_MODE)
    {
        if ((m_mediaOutputStatus->secondsElapsed > 0) &&
            (lastRemoteSync != m_mediaOutputStatus->secondsElapsed))
        {
            SendMediaSyncPacket(m_mediaFilename.c_str(), 0,
                                m_mediaOutputStatus->mediaSeconds);
            lastRemoteSync = m_mediaOutputStatus->secondsElapsed;
        }
    }
    
    if ((sequence->IsSequenceRunning()) &&
        (m_mediaOutputStatus->secondsElapsed > 0))
    {
        LogExcess(VB_MEDIAOUT,
                  "Elapsed: %.2d.%.2d  Remaining: %.2d Total %.2d:%.2d.\n",
                  m_mediaOutputStatus->secondsElapsed,
                  m_mediaOutputStatus->subSecondsElapsed,
                  m_mediaOutputStatus->secondsRemaining,
                  m_mediaOutputStatus->minutesTotal,
                  m_mediaOutputStatus->secondsTotal);
        
        CalculateNewChannelOutputDelay(m_mediaOutputStatus->mediaSeconds);
    }
	return 1;
}
int SDLOutput::IsPlaying(void) {
    return m_mediaOutputStatus->status == MEDIAOUTPUTSTATUS_PLAYING;
}

int  SDLOutput::Close(void)
{
    Stop();
}

/*
 *
 */
int SDLOutput::Stop(void)
{
	LogDebug(VB_MEDIAOUT, "SDLOutput::Stop()\n");
    sdlManager.Stop();
    if (data && !data->stopped) {
        data->stopped++;
        timespec tv;
        time(&tv.tv_sec);
        
        tv.tv_sec += 1;
        tv.tv_nsec = 0;
        //wait up to a second
        if (pthread_timedjoin_np( data->fillThread, NULL, &tv) == ETIMEDOUT) {
            printf("joining didn't work\n");
            //cancel the thread, nothing we can do now
            pthread_cancel(data->fillThread);
            sdlManager.blacklisted.insert(m_mediaFilename);
            LogWarn(VB_MEDIAOUT, "Problems decoding %d, blacklisting", m_mediaFilename.c_str());
        }
    }
	m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
	return 1;
}

