/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <fcntl.h>
#include <mutex>
#include <stdlib.h>
#include <string>
#include <thread>
#include <unistd.h>

#include "Warnings.h"
#include "common.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"

#include "FrameBuffer.h"
#include "IOCTLFrameBuffer.h"
#include "KMSFrameBuffer.h"
#include "SocketFrameBuffer.h"
#include "X11FrameBuffer.h"

#define FB_CURRENT_PAGE_PTR (m_pageBuffers[m_cPage])

/*
 * Usage:
 *
 * AutoSync Off:
 *
 * Client -> FBCopyData();
 * Client -> FBSyncDisplay();
 * FrameBuffer::DrawLoop() calls transition effect
 * - None -> NextPage(false); copy output buffer, SyncDisplay();
 * - Others -> loop(copy output buffer, SyncDisplay())
 *   (effects only work on a single m_cPage page)
 *
 * AutoSync On:
 * Client:
 *   page = Page(true);
 *   if IsDirty(page) then can't do anything, else do...
 *   NextPage(true)
 *   copy data to BufferPage(page)
 *   SetDirty(page)
 * FrameBuffer::SyncLoop*():
 * - if page m_cPage is dirty, then output and increment else just sleep/wait
 *
 */

void StartDrawLoopThread(FrameBuffer* fb);

/*
 *
 */
FrameBuffer::FrameBuffer() {
    LogDebug(VB_CHANNELOUT, "FrameBuffer::FrameBuffer()\n");

    m_typeSeed = (unsigned int)time(NULL);
}

/*
 *
 */
FrameBuffer::~FrameBuffer() {
    if (!m_runLoop)
        return; // never initialized/run so no need to tear down

    m_runLoop = false;
    m_drawSignal.notify_all();

    if (m_drawThread) {
        m_drawThread->join();
        delete m_drawThread;
    }

    if (m_outputBuffer)
        free(m_outputBuffer);

    if (m_fbFd > -1) {
        DestroyFrameBuffer();
    }

    if (m_dirtyPages)
        delete[] m_dirtyPages;
}

/*
 *
 */
int FrameBuffer::FBInit(const Json::Value& config) {
    if (config.isMember("Name"))
        m_name = config["Name"].asString();

    if (config.isMember("X"))
        m_xOffset = config["X"].asInt();

    if (config.isMember("Y"))
        m_yOffset = config["Y"].asInt();

    if (config.isMember("Width"))
        m_width = config["Width"].asInt();

    if (config.isMember("Height"))
        m_height = config["Height"].asInt();

    if (config.isMember("PixelSize"))
        m_pixelSize = config["PixelSize"].asInt();

    if (config.isMember("Device"))
        m_device = config["Device"].asString();

    if (config.isMember("TransitionType"))
        m_transitionType = (ImageTransitionType)(atoi(config["TransitionType"].asString().c_str()));

    if (config.isMember("Pages"))
        m_pages = config["Pages"].asInt();

    if (config.isMember("BitsPerPixel"))
        m_bpp = config["BitsPerPixel"].asInt();

    if (config.isMember("AutoSync"))
        m_autoSync = config["AutoSync"].asBool();

    if (m_pixelSize > 0) {
        m_pixelsWide = m_width / m_pixelSize;
        m_pixelsHigh = m_height / m_pixelSize;

        // Adjust our Width/Height to be whole numbers
        m_width = m_pixelSize * m_pixelsWide;
        m_height = m_pixelSize * m_pixelsHigh;
    } else {
        m_pixelsWide = m_width;
        m_pixelsHigh = m_height;
    }

    int result = InitializeFrameBuffer();
    if (m_pixelSize == 0) {
        m_pixelSize = 1;
    }
    if (!result)
        return 0;

    if (m_autoSync) {
        m_dirtyPages = new volatile uint8_t[m_pages];
        for (int i = 0; i < m_pages; i++)
            m_dirtyPages[i] = 0;
    }

    m_outputBuffer = (uint8_t*)malloc(m_pageSize);
    if (!m_outputBuffer) {
        LogErr(VB_CHANNELOUT, "Error, unable to allocate outputBuffer buffer\n");
        DestroyFrameBuffer();
        return 0;
    }
    memset(m_outputBuffer, 0, m_pageSize);

    LogDebug(VB_CHANNELOUT, "Initialized %s frame buffer at %dx%d resolution.\n",
             m_device.c_str(), m_width, m_height);

    Dump();
    m_runLoop = true;
    m_drawThread = new std::thread(StartDrawLoopThread, this);
    return 1;
}

FrameBuffer* FrameBuffer::createFrameBuffer(const Json::Value& config) {
    FrameBuffer* fb = nullptr;
    std::string device;
    if (config.isMember("Device")) {
        device = config["Device"].asString();
    }
    
    LogDebug(VB_CHANNELOUT, "FrameBuffer::createFrameBuffer for device='%s'\n", device.c_str());
    
#ifdef USE_X11
    fb = new X11FrameBuffer();
    if (fb->FBInit(config)) {
        return fb;
    }
    delete fb;
    fb = nullptr;
#endif
#ifdef USE_FRAMEBUFFER_SOCKET
    fb = new SocketFrameBuffer();
    if (fb->FBInit(config)) {
        return fb;
    }
    delete fb;
    fb = nullptr;
#endif
#ifdef HAS_KMS_FB
    // DPI needs KMS for direct pixel output
    // For HDMI: Try KMS first for full-resolution displays (better performance)
    // Small models will fall through to IOCTL which has auto-scaling
    bool isDPI = (device.find("DPI") != std::string::npos);
    bool isSmallModel = false;
    
    // Check if this is a small model that needs scaling
    if (config.isMember("Width") && config.isMember("Height")) {
        int width = config["Width"].asInt();
        int height = config["Height"].asInt();
        // Consider anything smaller than 1280x720 as "small" needing potential scaling
        isSmallModel = (width < 1280 || height < 720);
    }
    
    bool tryKMS = isDPI || device.empty() || !isSmallModel;
    
    if (tryKMS) {
        fb = new KMSFrameBuffer();
        if (fb->FBInit(config)) {
            return fb;
        }
        delete fb;
        fb = nullptr;
    }
#endif
#ifdef HAS_IOCTL_FB
    fb = new IOCTLFrameBuffer();
    if (fb->FBInit(config)) {
        return fb;
    }
    delete fb;
    fb = nullptr;
#endif
    return nullptr;
}

void FrameBuffer::DestroyFrameBuffer(void) {
    if (m_buffer) {
        memset(m_buffer, 0, m_bufferSize);
        munmap(m_buffer, m_bufferSize);
    }

    if (m_fbFd != -1) {
        close(m_fbFd);
        m_fbFd = -1;
    }
}

/*
 *
 */
void FrameBuffer::FBCopyData(const uint8_t* buffer, int draw) {
    int drow = 0;
    const uint8_t* sR = buffer + 0; // Input always RGB
    const uint8_t* sG = buffer + 1;
    const uint8_t* sB = buffer + 2;
    uint8_t* ob = m_outputBuffer;

    if (draw && (m_pixelSize = 1)) {
        m_bufferLock.lock();
        ob = FB_CURRENT_PAGE_PTR;
    }
    uint8_t* dR;
    uint8_t* dG;
    uint8_t* dB;
    int add = m_bpp / 8;

    for (int y = 0; y < m_pixelsHigh; y++) {
        // Output is BGR(A)
        dB = ob + (drow * m_pixelSize * m_rowStride);
        dG = dB + 1;
        dR = dB + 2;

        for (int x = 0; x < m_pixelsWide; x++) {
            for (int sc = 0; sc < m_pixelSize; sc++) {
                *dR = *sR;
                *dG = *sG;
                *dB = *sB;

                dR += add;
                dG += add;
                dB += add;
            }

            sR += 3;
            sG += 3;
            sB += 3;
        }

        dB = ob + (drow * m_pixelSize * m_rowStride);
        for (int sc = 1; sc < m_pixelSize; sc++) {
            memcpy(dB + (sc * m_rowStride), dB, m_rowStride);
        }

        drow++;
    }

    if (draw) {
        m_bufferLock.unlock();
    } else {
        m_imageReady = true;
    }
}

/*
 *
 */
void FrameBuffer::Dump(void) {
    LogDebug(VB_CHANNELOUT, "Device         : %s\n", m_device.c_str());
    LogDebug(VB_CHANNELOUT, "Width          : %d\n", m_width);
    LogDebug(VB_CHANNELOUT, "Height         : %d\n", m_height);
    LogDebug(VB_CHANNELOUT, "PixelSize      : %d\n", m_pixelSize);
    LogDebug(VB_CHANNELOUT, "Pixels Wide    : %d\n", m_pixelsWide);
    LogDebug(VB_CHANNELOUT, "Pixels High    : %d\n", m_pixelsHigh);
    LogDebug(VB_CHANNELOUT, "bpp            : %d\n", m_bpp);
    LogDebug(VB_CHANNELOUT, "Transition Type: %d\n", (int)m_transitionType);
}

/*
 *
 */
void FrameBuffer::FBStartDraw(ImageTransitionType transitionType) {
    if (transitionType == IT_Default)
        transitionType = m_transitionType;

    if (transitionType == IT_Random)
        transitionType = (ImageTransitionType)(rand_r(&m_typeSeed) % ((int)IT_MAX - 1) + 1);

    m_nextTransitionType = transitionType;

    m_drawSignal.notify_all();
}

/*
 *
 */
void StartDrawLoopThread(FrameBuffer* fb) {
    fb->DrawLoop();
}

void FrameBuffer::DrawLoop(void) {
    SetThreadName("FPP-FBDrawLoop");
    if (m_autoSync) {
        SyncLoop();
        return;
    }

    std::unique_lock<std::mutex> lock(m_bufferLock);
    while (m_runLoop) {
        if (m_imageReady) {
            lock.unlock();
            // m_nextTransitionType = IT_WipeFromHCenter;
            switch (m_nextTransitionType) {
            case IT_Random:
                m_nextTransitionType = (ImageTransitionType)(rand_r(&m_typeSeed) % ((int)IT_MAX - 1) + 1);
                lock.lock();
                continue;
                break;

            case IT_SlideUp:
                FBDrawSlideUp();
                break;

            case IT_SlideDown:
                FBDrawSlideDown();
                break;

            case IT_SlideLeft:
                FBDrawSlideLeft();
                break;

            case IT_SlideRight:
                FBDrawSlideRight();
                break;

            case IT_WipeUp:
                FBDrawWipeUp();
                break;

            case IT_WipeDown:
                FBDrawWipeDown();
                break;

            case IT_WipeLeft:
                FBDrawWipeLeft();
                break;

            case IT_WipeRight:
                FBDrawWipeRight();
                break;

            case IT_WipeToHCenter:
                FBDrawWipeToHCenter();
                break;

            case IT_WipeFromHCenter:
                FBDrawWipeFromHCenter();
                break;

            case IT_HorzBlindsOpen:
                FBDrawHorzBlindsOpen();
                break;

            case IT_HorzBlindsClose:
                FBDrawHorzBlindsClose();
                break;

            case IT_Mosaic:
                FBDrawMosaic();
                break;

            default:
                FBDrawNormal();
                break;
            }

            lock.lock();

            m_imageReady = false;
        }

        m_drawSignal.wait(lock);
    }
}

void FrameBuffer::NextPage(bool producer) {
    if (m_pages == 1)
        return;

    // LogInfo(VB_CHANNELOUT, "%s - %d - NextPage() for %s\n", m_device.c_str(), m_cPage, producer ? "producer" : "consumer");
    if (producer)
        m_pPage = (m_pPage + 1) % m_pages;
    else
        m_cPage = (m_cPage + 1) % m_pages;
}

/*
 *
 */
void FrameBuffer::FBDrawNormal(void) {
    m_bufferLock.lock();

    NextPage();
    memcpy(FB_CURRENT_PAGE_PTR, m_outputBuffer, m_pageSize);
    SyncDisplay(true);
    m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawSlideUp(void) {
    int rEach = 2;
    int sleepTime = 800 * 1000 * rEach / m_height;

    if ((m_height % 4) == 0)
        rEach = 4;

    m_bufferLock.lock();
    for (int i = m_height - rEach; i >= 0;) {
        memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * i), m_outputBuffer, m_rowStride * (m_height - i));
        SyncDisplay();

        usleep(sleepTime);

        i -= rEach;
    }
    m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawSlideDown(void) {
    int rEach = 2;
    int sleepTime = 800 * 1000 * rEach / m_height;

    if ((m_height % 4) == 0)
        rEach = 4;

    m_bufferLock.lock();
    for (int i = rEach; i <= m_height;) {
        memcpy(FB_CURRENT_PAGE_PTR, m_outputBuffer + (m_rowStride * (m_height - i)), m_rowStride * i);
        SyncDisplay();

        usleep(sleepTime);

        i += rEach;
    }
    m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawSlideLeft(void) {
    int colsEach = 2;
    int sleepTime = 800 * 1000 * colsEach / m_width;

    for (int x = m_width - colsEach; x >= 0; x -= colsEach) {
        DrawSquare(x, 0, m_width - x, m_height, 0, 0);
        SyncDisplay();

        usleep(sleepTime);
    }
}

/*
 *
 */
void FrameBuffer::FBDrawSlideRight(void) {
    int colsEach = 2;
    int sleepTime = 800 * 1000 * colsEach / m_width;

    for (int x = 2; x <= m_width; x += colsEach) {
        DrawSquare(0, 0, x, m_height, m_width - x, 0);
        SyncDisplay();

        usleep(sleepTime);
    }
}

/*
 *
 */
void FrameBuffer::FBDrawWipeUp(void) {
    int rEach = 2;

    if ((m_height % 4) == 0)
        rEach = 4;

    int sleepTime = 800 * 1000 * rEach / m_height;

    m_bufferLock.lock();
    for (int i = m_height - rEach; i >= 0;) {
        memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * i), m_outputBuffer + (m_rowStride * i), m_rowStride * rEach);
        SyncDisplay();

        usleep(sleepTime);

        i -= rEach;
    }
    m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawWipeDown(void) {
    int rEach = 2;

    if ((m_height % 4) == 0)
        rEach = 4;

    int sleepTime = 800 * 1000 * rEach / m_height;

    m_bufferLock.lock();
    for (int i = 0; i < m_height;) {
        memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * i), m_outputBuffer + (m_rowStride * i), m_rowStride * rEach);
        SyncDisplay();

        usleep(sleepTime);

        i += rEach;
    }
    m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawWipeLeft(void) {
    int BPP = m_bpp / 8;

    m_bufferLock.lock();

    for (int x = m_width - 1; x >= 0; x--) {
        for (int y = 0; y < m_height; y++) {
            memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * y) + (x * BPP),
                   m_outputBuffer + (m_rowStride * y) + (x * BPP), BPP);
        }
        SyncDisplay();
    }

    m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawWipeRight(void) {
    int BPP = m_bpp / 8;

    m_bufferLock.lock();

    for (int x = 0; x < m_width; x++) {
        for (int y = 0; y < m_height; y++) {
            memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * y) + (x * BPP),
                   m_outputBuffer + (m_rowStride * y) + (x * BPP), BPP);
        }
        SyncDisplay();
    }

    m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawWipeToHCenter(void) {
    int rEach = 2;
    int mid = m_height / 2;

    if ((m_height % 4) == 0)
        rEach = 4;

    int sleepTime = 800 * 1000 * rEach / m_height * 2;

    m_bufferLock.lock();
    for (int i = 0; i < mid; i += rEach) {
        memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * i), m_outputBuffer + (m_rowStride * i), m_rowStride * rEach);
        memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * (m_height - i - rEach)),
               m_outputBuffer + (m_rowStride * (m_height - i - rEach)),
               m_rowStride * rEach);
        SyncDisplay();

        usleep(sleepTime);
    }
    m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawWipeFromHCenter(void) {
    int mid = m_height / 2;
    int rowsEachUpdate = 2;
    int sleepTime = 800 * 1000 * rowsEachUpdate / m_height * 2;

    m_bufferLock.lock();

    int t = mid - 2;
    for (int b = mid; b < m_height; b += rowsEachUpdate, t -= rowsEachUpdate) {
        memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * b), m_outputBuffer + (m_rowStride * b),
               m_rowStride * rowsEachUpdate);

        if (t >= 0)
            memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * t), m_outputBuffer + (m_rowStride * t),
                   m_rowStride * rowsEachUpdate);

        SyncDisplay();

        usleep(sleepTime);
    }

    m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawHorzBlindsOpen(void) {
    int rowsEachUpdate = 2;
    int blindSize = 32;
    int sleepTime = 800 * 1000 * rowsEachUpdate / blindSize;

    m_bufferLock.lock();
    for (int i = (blindSize - 1); i >= 0; i -= rowsEachUpdate) {
        for (int y = i; y < m_height; y += blindSize) {
            memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * y), m_outputBuffer + (m_rowStride * y),
                   m_rowStride * rowsEachUpdate);
        }

        SyncDisplay();

        usleep(sleepTime);
    }
    m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawHorzBlindsClose(void) {
    int rowsEachUpdate = 2;
    int blindSize = 32;
    int sleepTime = 800 * 1000 * rowsEachUpdate / blindSize;

    m_bufferLock.lock();
    for (int i = 0; i < blindSize; i += rowsEachUpdate) {
        for (int y = i; y < m_height; y += blindSize) {
            memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * y), m_outputBuffer + (m_rowStride * y),
                   m_rowStride * rowsEachUpdate);
        }

        SyncDisplay();

        usleep(sleepTime);
    }
    m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawMosaic(void) {
    int squareSize = 32;
    int xSquares = m_width / squareSize + 1;
    int ySquares = m_height / squareSize + 1;
    std::vector<std::vector<int>> squares;
    squares.resize(ySquares, std::vector<int>(xSquares, 0));
    
    int count = xSquares * ySquares;
    int sleepTime = 800 * 1000 / count;


    m_bufferLock.lock();

    for (int i = 0; i < (count * 10); i++) {
        int square = rand_r(&m_typeSeed) % (count - 1);
        int x = square % xSquares;
        int y = square / xSquares;

        if (!squares[y][x]) {
            DrawSquare(x * squareSize, y * squareSize, squareSize, squareSize);
            squares[y][x] = 1;

            SyncDisplay();

            usleep(sleepTime);
        }
    }

    for (int y = 0; y < ySquares; y++) {
        for (int x = 0; x < xSquares; x++) {
            if (!squares[y][x]) {
                DrawSquare(x * squareSize, y * squareSize, squareSize,
                           squareSize);
                squares[y][x] = 1;

                SyncDisplay();

                usleep(sleepTime);
            }
        }
    }

    m_bufferLock.unlock();
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void FrameBuffer::DrawSquare(int dx, int dy, int w, int h, int sx, int sy) {
    int dRowOffset = dx * m_bpp / 8;
    int sRowOffset = ((sx == -1) ? dx : sx) * m_bpp / 8;
    int bytesWide = w * m_bpp / 8;

    if (sy == -1)
        sy = dy;

    for (int i = 0; i < h; i++) {
        memcpy(FB_CURRENT_PAGE_PTR + (m_rowStride * (dy + i)) + dRowOffset,
               m_outputBuffer + (m_rowStride * (sy + i)) + sRowOffset, bytesWide);
    }
}

void FrameBuffer::ClearAllPages() {
    for (int x = 0; x < 3; x++) {
        if (m_pageBuffers[x]) {
            memset(m_pageBuffers[x], 0, m_pageSize);
        }
    }
}

void FrameBuffer::Clear() {
    memset(FB_CURRENT_PAGE_PTR, 0, m_pageSize);
}

uint8_t* FrameBuffer::BufferPage(int page) {
    return m_pageBuffers[page];
}
