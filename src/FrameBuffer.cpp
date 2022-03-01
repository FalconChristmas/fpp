/*
 *   FrameBuffer Class for Falcon Player (FPP)
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
#include "fpp-pch.h"

#include "config.h"

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "FrameBuffer.h"
#include "common.h"
#include "log.h"

#define X11_WINDOW_WIDTH 720
#define X11_WINDOW_HEIGHT 400

#define FB_CURRENT_PAGE_PTR (m_buffer + (m_screenSize * m_page))

void StartDrawLoopThread(FrameBuffer* fb);

/*
 *
 */
FrameBuffer::FrameBuffer() {
    LogDebug(VB_PLAYLIST, "FrameBuffer::FrameBuffer()\n");

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
#ifdef USE_X11
    if (m_device == "x11") {
        DestroyX11Window();
    }
#endif

    if (m_rgb565map) {
        for (int r = 0; r < 32; r++) {
            for (int g = 0; g < 64; g++) {
                delete[] m_rgb565map[r][g];
            }

            delete[] m_rgb565map[r];
        }

        delete[] m_rgb565map;
    }
}

/*
 *
 */
int FrameBuffer::FBInit(const Json::Value& config) {
    if (config.isMember("Name"))
        m_name = config["Name"].asString();

    if (config.isMember("Width"))
        m_width = config["Width"].asInt();

    if (config.isMember("Height"))
        m_height = config["Height"].asInt();

    if (config.isMember("Inverted"))
        m_inverted = config["Inverted"].asInt();

    if (config.isMember("Device"))
        m_device = config["Device"].asString();

    if (config.isMember("TransitionType"))
        m_transitionType = (ImageTransitionType)(atoi(config["TransitionType"].asString().c_str()));

    if (config.isMember("DataFormat"))
        m_dataFormat = config["DataFormat"].asString();

    if (m_dataFormat == "RGB")
        m_useRGB = 1;

    int result = 0;

    LogDebug(VB_CHANNELOUT, "Initializing %s frame buffer at %dx%d resolution.\n",
             m_device.c_str(), m_width, m_height);

#ifdef USE_X11
    if (m_device == "x11")
        result = InitializeX11Window();
    else
#endif
        result = InitializeFrameBuffer();

    if (!result)
        return 0;

    m_outputBuffer = (uint8_t*)malloc(m_screenSize);
    if (!m_outputBuffer) {
        LogErr(VB_PLAYLIST, "Error, unable to allocate outputBuffer buffer\n");
        DestroyFrameBuffer();
        return 0;
    }
    memset(m_outputBuffer, 0, m_screenSize);

    m_frameSize = m_width * m_height * 3; // Input always RGB
    m_lastFrame = (uint8_t*)malloc(m_frameSize);
    if (!m_lastFrame) {
        LogErr(VB_PLAYLIST, "Error, unable to allocate lastFrame buffer\n");
        DestroyFrameBuffer();
        return 0;
    }
    memset(m_lastFrame, 0, m_frameSize);

    std::string colorOrder = m_dataFormat.substr(0, 3);

    if (colorOrder == "RGB") {
        m_rOffset = 0;
        m_gOffset = 1;
        m_bOffset = 2;
    } else if (colorOrder == "RBG") {
        m_rOffset = 0;
        m_gOffset = 2;
        m_bOffset = 1;
    } else if (colorOrder == "GRB") {
        m_rOffset = 1;
        m_gOffset = 0;
        m_bOffset = 2;
    } else if (colorOrder == "GBR") {
        m_rOffset = 2;
        m_gOffset = 0;
        m_bOffset = 1;
    } else if (colorOrder == "BRG") {
        m_rOffset = 1;
        m_gOffset = 2;
        m_bOffset = 0;
    } else if (colorOrder == "BGR") {
        m_rOffset = 2;
        m_gOffset = 1;
        m_bOffset = 0;
    }

    m_runLoop = true;

    m_drawThread = new std::thread(StartDrawLoopThread, this);

    return 1;
}

/*
 *
 */
int FrameBuffer::InitializeFrameBuffer(void) {
    LogDebug(VB_PLAYLIST, "Using FrameBuffer device %s\n", m_device.c_str());

    std::string devString = getSetting("framebufferControlSocketPath", "/dev") + "/" + m_device;
#ifndef USE_FRAMEBUFFER_SOCKET
    m_fbFd = open(devString.c_str(), O_RDWR);
    if (!m_fbFd) {
        LogErr(VB_PLAYLIST, "Error opening FrameBuffer device: %s\n", devString.c_str());
        return 0;
    }

    if (ioctl(m_fbFd, FBIOGET_VSCREENINFO, &m_vInfo)) {
        LogErr(VB_PLAYLIST, "Error getting FrameBuffer info\n");
        close(m_fbFd);
        return 0;
    }

    if (m_vInfo.bits_per_pixel == 32)
        m_vInfo.bits_per_pixel = 24;

    m_bpp = m_vInfo.bits_per_pixel;
    LogDebug(VB_PLAYLIST, "FrameBuffer is using %d BPP\n", m_bpp);

    if ((m_bpp != 24) && (m_bpp != 16)) {
        LogErr(VB_PLAYLIST, "Do not know how to handle %d BPP\n", m_bpp);
        close(m_fbFd);
        return 0;
    }

    if (m_bpp == 16) {
        LogExcess(VB_PLAYLIST, "Current Bitfield offset info:\n");
        LogExcess(VB_PLAYLIST, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
        LogExcess(VB_PLAYLIST, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
        LogExcess(VB_PLAYLIST, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);

        // RGB565
        m_vInfo.red.offset = 11;
        m_vInfo.red.length = 5;
        m_vInfo.green.offset = 5;
        m_vInfo.green.length = 6;
        m_vInfo.blue.offset = 0;
        m_vInfo.blue.length = 5;
        m_vInfo.transp.offset = 0;
        m_vInfo.transp.length = 0;

        LogExcess(VB_PLAYLIST, "New Bitfield offset info should be:\n");
        LogExcess(VB_PLAYLIST, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
        LogExcess(VB_PLAYLIST, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
        LogExcess(VB_PLAYLIST, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);
    }

    m_isDoubleBuffered = true;
    m_pages = 2;

    if (m_width && m_height) {
        m_vInfo.xres = m_width;
        m_vInfo.yres = m_height;
    } else {
        m_width = m_vInfo.xres;
        m_height = m_vInfo.yres;
    }
    m_vInfo.xres_virtual = m_width;
    m_vInfo.yres_virtual = m_height * m_pages;

    if (ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfo)) {
        m_isDoubleBuffered = false;
        m_vInfo.yres_virtual = m_height;
        m_pages = 1;
        if (ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfo)) {
            LogErr(VB_PLAYLIST, "Error setting FrameBuffer info\n");
            close(m_fbFd);
            return 0;
        } else {
            LogWarn(VB_CHANNELOUT, "Unable to allocate enough memory for double buffering, using single buffered framebuffer\n");
        }
    }

    if (ioctl(m_fbFd, FBIOGET_FSCREENINFO, &m_fInfo)) {
        LogErr(VB_PLAYLIST, "Error getting fixed FrameBuffer info\n");
        close(m_fbFd);
        return 0;
    }

    if (devString == "/dev/fb0") {
        m_ttyFd = open("/dev/console", O_RDWR);
        if (!m_ttyFd) {
            LogErr(VB_PLAYLIST, "Error, unable to open /dev/console\n");
            DestroyFrameBuffer();
            return 0;
        }

        // Hide the text console
        ioctl(m_ttyFd, KDSETMODE, KD_GRAPHICS);

        // Shouldn't need this anymore.
        close(m_ttyFd);
        m_ttyFd = -1;
    }

    m_rowStride = m_fInfo.line_length;
    m_rowPadding = m_rowStride - (m_width * 3);
    m_screenSize = m_vInfo.yres * m_fInfo.line_length;
    m_bufferSize = m_screenSize * m_pages;
    m_buffer = (uint8_t*)mmap(0, m_bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fbFd, 0);

    if ((char*)m_buffer == (char*)-1) {
        LogErr(VB_PLAYLIST, "Error, unable to map /dev/fb0\n");
        DestroyFrameBuffer();
        return 0;
    }

    memset(m_buffer, 0, m_bufferSize);

    if (m_bpp == 16) {
        LogExcess(VB_PLAYLIST, "Generating RGB565Map for Bitfield offset info:\n");
        LogExcess(VB_PLAYLIST, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
        LogExcess(VB_PLAYLIST, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
        LogExcess(VB_PLAYLIST, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);

        int rShift = m_vInfo.red.offset - (8 + (8 - m_vInfo.red.length));
        int gShift = m_vInfo.green.offset - (8 + (8 - m_vInfo.green.length));
        int bShift = m_vInfo.blue.offset - (8 + (8 - m_vInfo.blue.length));

#if 0
        uint8_t rMask = (0xFF ^ (0xFF >> m_vInfo.red.length));
        uint8_t gMask = (0xFF ^ (0xFF >> m_vInfo.green.length));
        uint8_t bMask = (0xFF ^ (0xFF >> m_vInfo.blue.length));
        LogDebug(VB_PLAYLIST, "rM/rS: 0x%02x/%d, gM/gS: 0x%02x/%d, bM/bS: 0x%02x/%d\n", rMask, rShift, gMask, gShift, bMask, bShift);
#endif

        uint16_t o;
        m_rgb565map = new uint16_t**[32];

        for (uint16_t b = 0; b < 32; b++) {
            m_rgb565map[b] = new uint16_t*[64];
            for (uint16_t g = 0; g < 64; g++) {
                m_rgb565map[b][g] = new uint16_t[32];
                for (uint16_t r = 0; r < 32; r++) {
                    o = 0;

                    if (rShift >= 0)
                        o |= r >> rShift;
                    else
                        o |= r << abs(rShift);

                    if (gShift >= 0)
                        o |= g >> gShift;
                    else
                        o |= g << abs(gShift);

                    if (bShift >= 0)
                        o |= b >> bShift;
                    else
                        o |= b << abs(bShift);

                    m_rgb565map[b][g][r] = o;
                }
            }
        }
    }
    return 1;
#else
    m_isDoubleBuffered = true;
    m_pages = 2;
    m_bpp = 24;
    m_rowStride = m_width * 3;
    m_rowPadding = 0;
    m_screenSize = m_width * m_height * 3;
    m_bufferSize = m_screenSize * m_pages;

    m_fbFd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (m_fbFd < 0) {
        LogErr(VB_PLAYLIST, "Error opening FrameBuffer device: %s\n", devString.c_str());
        return 0;
    }

    std::string shmFile = "/fpp/" + m_device + "-buffer";
    shmemFile = shm_open(shmFile.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (shmemFile == -1) {
        LogErr(VB_PLAYLIST, "Error creating shared memory buffer: %s   %s\n", shmFile.c_str(), strerror(errno));
        close(m_fbFd);
        m_fbFd = -1;
        return 0;
    }
    ftruncate(shmemFile, m_bufferSize);
    m_buffer = (uint8_t*)mmap(NULL, m_bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmemFile, 0);
    if (m_buffer == MAP_FAILED) {
        LogErr(VB_PLAYLIST, "Error mmap buffer: %s    %s\n", shmFile.c_str(), strerror(errno));
        close(m_fbFd);
        m_fbFd = -1;
        close(shmemFile);
        shmemFile = -1;
        shm_unlink(shmFile.c_str());
        m_buffer = nullptr;
        return 0;
    }
    memset(m_buffer, 0, m_bufferSize);

    memset(&dev_address, 0, sizeof(struct sockaddr_un));
    dev_address.sun_family = AF_UNIX;
    strcpy(dev_address.sun_path, devString.c_str());

    // CMD 1 : send the w/h/pages so the framebuffer can setup
    uint16_t data[4] = { 1, (uint16_t)m_width, (uint16_t)m_height, (uint16_t)m_pages };

    struct msghdr msg = { 0 };
    char buf[CMSG_SPACE(sizeof(shmemFile))];
    memset(buf, '\0', sizeof(buf));

    struct iovec io = { .iov_base = data, .iov_len = sizeof(data) };

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(shmemFile));

    memmove(CMSG_DATA(cmsg), &shmemFile, sizeof(shmemFile));
    msg.msg_controllen = CMSG_SPACE(sizeof(shmemFile));

    msg.msg_name = &dev_address;
    msg.msg_namelen = sizeof(dev_address);
    sendmsg(m_fbFd, &msg, 0);
    return 1;
#endif
}

void FrameBuffer::DestroyFrameBuffer(void) {
    if (m_buffer) {
        memset(m_buffer, 0, m_bufferSize);
        munmap(m_buffer, m_bufferSize);
    }

    if (m_fbFd > -1) {
        close(m_fbFd);
    }
    m_fbFd = -1;

#ifdef USE_FRAMEBUFFER_SOCKET
    if (shmemFile != -1) {
        close(shmemFile);
        std::string shmFile = m_device + "-buffer";
        shm_unlink(shmFile.c_str());
        shmemFile = -1;
    }
#endif
}

#ifdef USE_X11
int FrameBuffer::InitializeX11Window(void) {
    m_display = XOpenDisplay(getenv("DISPLAY"));
    if (!m_display) {
        LogErr(VB_PLAYLIST, "Unable to connect to X Server\n");
        return 0;
    }

    m_screen = DefaultScreen(m_display);

    m_isDoubleBuffered = false;
    m_pages = 1;
    m_bpp = 32;
    m_rowStride = m_width * m_bpp / 8;
    m_rowPadding = 0;
    m_screenSize = m_width * m_height * m_bpp / 8;
    m_bufferSize = m_screenSize;
    m_buffer = new uint8_t[m_bufferSize];

    m_xImage = XCreateImage(m_display, CopyFromParent, 24, ZPixmap, 0,
                            (char*)m_buffer, m_width, m_height, 32, m_width * 4);

    XSetWindowAttributes attributes;

    attributes.background_pixel = BlackPixel(m_display, m_screen);

    XGCValues values;

    m_pixmap = XCreatePixmap(m_display, XDefaultRootWindow(m_display), m_width, m_height, 24);

    m_gc = XCreateGC(m_display, m_pixmap, 0, &values);
    if (m_gc < 0) {
        LogErr(VB_PLAYLIST, "Unable to create GC\n");
        return 0;
    }

    m_window = XCreateWindow(
        m_display, RootWindow(m_display, m_screen), m_width, m_height,
        m_width, m_height, 5, 24, InputOutput,
        DefaultVisual(m_display, m_screen), CWBackPixel, &attributes);

    XMapWindow(m_display, m_window);

    XStoreName(m_display, m_window, m_name.c_str());
    XSetIconName(m_display, m_window, m_name.c_str());

    XFlush(m_display);

    return 1;
}

void FrameBuffer::DestroyX11Window(void) {
    XDestroyWindow(m_display, m_window);
    XFreePixmap(m_display, m_pixmap);
    XFreeGC(m_display, m_gc);
    XCloseDisplay(m_display);

    if (m_buffer)
        delete[] m_buffer;
}
#endif

/*
 *
 */
void FrameBuffer::FBCopyData(const uint8_t* buffer, int draw) {
    int srow = 0;
    int drow = m_inverted ? m_height - 1 : 0;
    const uint8_t* s = buffer;
    uint8_t* d;
    uint8_t* l = m_lastFrame;
    const uint8_t* sR = buffer + m_rOffset;
    const uint8_t* sG = buffer + m_gOffset;
    const uint8_t* sB = buffer + m_bOffset;
    int sBpp = 3;
    int skipped = 0;
    uint8_t* ob = m_outputBuffer;

    if (draw) {
        m_bufferLock.lock();
        ob = FB_CURRENT_PAGE_PTR;
    }

    if (m_bpp == 16) {
        for (int y = 0; y < m_height; y++) {
            d = ob + (drow * m_rowStride);
            for (int x = 0; x < m_width; x++) {
                if (memcmp(l, sR, sBpp)) {
                    if (skipped) {
                        sG += skipped * sBpp;
                        sB += skipped * sBpp;
                        d += skipped * 2;
                        skipped = 0;
                    }

                    if (m_useRGB) // RGB data to BGR framebuffer
                        *((uint16_t*)d) = m_rgb565map[*sR >> 3][*sG >> 2][*sB >> 3];
                    else // BGR data to BGR framebuffer
                        *((uint16_t*)d) = m_rgb565map[*sB >> 3][*sG >> 2][*sR >> 3];

                    sG += sBpp;
                    sB += sBpp;
                    d += 2;
                } else {
                    skipped++;
                }

                sR += sBpp;
                l += sBpp;
            }

            srow++;
            drow += m_inverted ? -1 : 1;
        }
    } else if (m_useRGB || (m_bpp == 32)) {
        uint8_t* dR;
        uint8_t* dG;
        uint8_t* dB;
        int add = m_bpp / 8;

        for (int y = 0; y < m_height; y++) {
            if (m_useRGB) {
                dR = ob + (drow * m_rowStride) + 2;
                dG = ob + (drow * m_rowStride) + 1;
                dB = ob + (drow * m_rowStride) + 0;
            } else {
                dR = ob + (drow * m_rowStride) + 0;
                dG = ob + (drow * m_rowStride) + 1;
                dB = ob + (drow * m_rowStride) + 2;
            }

            for (int x = 0; x < m_width; x++) {
                *dR = *sR;
                *dG = *sG;
                *dB = *sB;

                sR += 3;
                sG += 3;
                sB += 3;

                dR += add;
                dG += add;
                dB += add;
            }

            srow++;
            drow += m_inverted ? -1 : 1;
        }
    } else {
        int istride = m_width * 3;
        const uint8_t* src = buffer;
        uint8_t* dst = ob;

        if (m_inverted)
            dst = ob + (m_rowStride * (m_height - 1));

        for (int y = 0; y < m_height; y++) {
            memcpy(dst, src, istride);
            src += istride;

            if (m_inverted)
                dst -= m_rowStride;
            else
                dst += m_rowStride;
        }
    }

    memcpy(m_lastFrame, buffer, m_frameSize);

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
    LogDebug(VB_PLAYLIST, "Transition Type: %d\n", (int)m_transitionType);
    LogDebug(VB_PLAYLIST, "Width          : %d\n", m_width);
    LogDebug(VB_PLAYLIST, "Height         : %d\n", m_height);
    LogDebug(VB_PLAYLIST, "Device         : %s\n", m_device.c_str());
    LogDebug(VB_PLAYLIST, "DataFormat     : %s\n", m_dataFormat.c_str());
    LogDebug(VB_PLAYLIST, "Inverted       : %d\n", m_inverted);
    LogDebug(VB_PLAYLIST, "bpp            : %d\n", m_bpp);
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

void FrameBuffer::NextPage() {
    if (m_pages == 1)
        return;

    m_page = (m_page + 1) % m_pages;
}

void FrameBuffer::SyncDisplay(bool pageChanged) {
    if (m_device == "x11") {
#ifdef USE_X11
        XLockDisplay(m_display);
        XPutImage(m_display, m_window, m_gc, m_xImage, 0, 0, 0, 0, m_width, m_height);
        XSync(m_display, True);
        XFlush(m_display);
        XUnlockDisplay(m_display);
#endif
    } else if (m_fbFd != -1) {
        if (m_pages == 1)
            return;

        if (!pageChanged)
            return;

#ifdef USE_FRAMEBUFFER_SOCKET
        // CMD 2 - sync, param is page#
        uint16_t data[2] = { 2, (uint16_t)m_page };
        sendto(m_fbFd, data, sizeof(data), 0, (struct sockaddr*)&dev_address, sizeof(dev_address));
#else
        // Pan the display to the new page
        m_vInfo.yoffset = m_vInfo.yres * m_page;
        ioctl(m_fbFd, FBIOPAN_DISPLAY, &m_vInfo);
#endif
    }
}

/*
 *
 */
void FrameBuffer::FBDrawNormal(void) {
    m_bufferLock.lock();

    NextPage();

    memcpy(FB_CURRENT_PAGE_PTR, m_outputBuffer, m_screenSize);

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
    int squares[ySquares][xSquares];
    int count = xSquares * ySquares;
    int sleepTime = 800 * 1000 / count;

    memset(squares, 0, sizeof(squares));

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
