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

#define FB_CURRENT_PAGE_PTR (m_pageBuffers[m_cPage])

#ifdef HAS_DRM
static std::string GetConnectorType(uint32_t i) {
    switch (i) {
    case DRM_MODE_CONNECTOR_VGA:
        return "VGA";
    case DRM_MODE_CONNECTOR_DVII:
        return "DVII";
    case DRM_MODE_CONNECTOR_DVID:
        return "DVID";
    case DRM_MODE_CONNECTOR_DVIA:
        return "DVIA";
    case DRM_MODE_CONNECTOR_Composite:
        return "Composite";
    case DRM_MODE_CONNECTOR_SVIDEO:
        return "SVIDEO";
    case DRM_MODE_CONNECTOR_LVDS:
        return "LVDS";
    case DRM_MODE_CONNECTOR_Component:
        return "Component";
    case DRM_MODE_CONNECTOR_9PinDIN:
        return "9PinDIN";
    case DRM_MODE_CONNECTOR_DisplayPort:
        return "DisplayPort";
    case DRM_MODE_CONNECTOR_HDMIA:
        return "HDMI-A";
    case DRM_MODE_CONNECTOR_HDMIB:
        return "HDMI-B";
    case DRM_MODE_CONNECTOR_TV:
        return "TV";
    case DRM_MODE_CONNECTOR_eDP:
        return "eDP";
    case DRM_MODE_CONNECTOR_VIRTUAL:
        return "VIRTUAL";
    case DRM_MODE_CONNECTOR_DSI:
        return "DSI";
    case DRM_MODE_CONNECTOR_DPI:
        return "DPI";
    case DRM_MODE_CONNECTOR_WRITEBACK:
        return "WRITEBACK";
    case DRM_MODE_CONNECTOR_SPI:
        return "SPI";
    }
    return "unknown";
}
#endif

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
#ifdef USE_X11
    if (m_device == "x11") {
        DestroyX11Window();
    }
#endif

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

    m_pixelsWide = m_width / m_pixelSize;
    m_pixelsHigh = m_height / m_pixelSize;

    // Adjust our Width/Height to be whole numbers
    m_width = m_pixelSize * m_pixelsWide;
    m_height = m_pixelSize * m_pixelsHigh;

    int result = 0;

    if (m_device != "x11") {
        result = InitializeFrameBuffer();
    } else {
#ifdef USE_X11
        result = InitializeX11Window();
#else
        std::string errStr = "X11 FrameBuffer configured, but FPP is not compiled with X11 support";
        WarningHolder::AddWarning(errStr);
        errStr += "\n";
        LogErr(VB_CHANNELOUT, errStr.c_str());
#endif
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

    if (m_bpp == 16) {
        m_frameSize = m_width * m_height * 3; // Input always RGB
        m_lastFrame = new uint8_t[m_frameSize];
        if (!m_lastFrame) {
            LogErr(VB_CHANNELOUT, "Error, unable to allocate lastFrame buffer\n");
            DestroyFrameBuffer();
            return 0;
        }
        memset(m_lastFrame, 0, m_frameSize);
    }

    LogDebug(VB_CHANNELOUT, "Initialized %s frame buffer at %dx%d resolution.\n",
             m_device.c_str(), m_width, m_height);

    Dump();

    m_runLoop = true;

    m_drawThread = new std::thread(StartDrawLoopThread, this);

    return 1;
}

/*
 *
 */
int FrameBuffer::InitializeFrameBuffer(void) {
    LogDebug(VB_CHANNELOUT, "Using FrameBuffer device %s\n", m_device.c_str());
#ifndef USE_FRAMEBUFFER_SOCKET
#ifdef HAS_DRM
    if (InitializeFrameBufferDRM()) {
        return 1;
    }
#endif
    return InitializeFrameBufferIOCTL();
#else
    return InitializeFrameBufferSocket();
#endif
}

void FrameBuffer::DestroyFrameBuffer(void) {
    if (m_buffer) {
        memset(m_buffer, 0, m_bufferSize);
        munmap(m_buffer, m_bufferSize);
    }

#ifdef USE_FRAMEBUFFER_SOCKET
    DestroyFrameBufferSocket();
#else
#ifdef HAS_DRM
    DestroyFrameBufferDRM();
#endif
    if (m_fbFd != -1) {
        close(m_fbFd);
        m_fbFd = -1;
    }
    DestroyFrameBufferIOCTL();

    if (m_rgb565map) {
        for (int r = 0; r < 32; r++) {
            for (int g = 0; g < 64; g++) {
                delete[] m_rgb565map[r][g];
            }

            delete[] m_rgb565map[r];
        }

        delete[] m_rgb565map;
    }

    if (m_lastFrame)
        delete[] m_lastFrame;
#endif
}

#ifdef USE_FRAMEBUFFER_SOCKET
int FrameBuffer::InitializeFrameBufferSocket(void) {
    std::string devString = getSetting("framebufferControlSocketPath", "/dev") + "/" + m_device;
    m_pages = 3;
    m_bpp = 32;
    m_rowStride = m_width * 4;
    m_rowPadding = 0;
    m_pageSize = m_width * m_height * 4;
    m_bufferSize = m_pageSize * m_pages;

    m_fbFd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (m_fbFd < 0) {
        LogErr(VB_CHANNELOUT, "Error opening FrameBuffer device: %s\n", devString.c_str());
        return 0;
    }

    std::string shmFile = "/fpp/" + m_device + "-buffer";
    shmemFile = shm_open(shmFile.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (shmemFile == -1) {
        LogErr(VB_CHANNELOUT, "Error creating shared memory buffer: %s   %s\n", shmFile.c_str(), strerror(errno));
        close(m_fbFd);
        m_fbFd = -1;
        return 0;
    }
    if (ftruncate(shmemFile, m_bufferSize) == -1) {
        close(shmemFile);
        shm_unlink(shmFile.c_str());
        shmemFile = shm_open(shmFile.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        ftruncate(shmemFile, m_bufferSize);
    }
    m_buffer = (uint8_t*)mmap(NULL, m_bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmemFile, 0);
    if (m_buffer == MAP_FAILED) {
        LogErr(VB_CHANNELOUT, "Error mmap buffer: %s    %s\n", shmFile.c_str(), strerror(errno));
        close(m_fbFd);
        m_fbFd = -1;
        close(shmemFile);
        shmemFile = -1;
        shm_unlink(shmFile.c_str());
        m_buffer = nullptr;
        return 0;
    }
    memset(m_buffer, 0, m_bufferSize);
    m_pageBuffers[0] = m_buffer;
    m_pageBuffers[1] = m_buffer + m_pageSize;
    m_pageBuffers[2] = m_buffer + m_pageSize + m_pageSize;

    memset(&dev_address, 0, sizeof(struct sockaddr_un));
    dev_address.sun_family = AF_UNIX;
    strcpy(dev_address.sun_path, devString.c_str());

    targetExists = FileExists(dev_address.sun_path);
    sendFramebufferConfig();
    return 1;
}
void FrameBuffer::sendFramebufferConfig() {
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
}

void FrameBuffer::sendFramebufferFrame() {
    // CMD 2 - sync, param is page#
    bool fe = FileExists(dev_address.sun_path);
    if (!targetExists && fe) {
        sendFramebufferConfig();
        targetExists = true;
    } else if (!fe) {
        targetExists = false;
    }
    if (targetExists) {
        uint16_t data[2] = { 2, (uint16_t)m_cPage };
        sendto(m_fbFd, data, sizeof(data), 0, (struct sockaddr*)&dev_address, sizeof(dev_address));
    }
}
void FrameBuffer::DestroyFrameBufferSocket(void) {
    if (shmemFile != -1) {
        close(shmemFile);
        std::string shmFile = m_device + "-buffer";
        shm_unlink(shmFile.c_str());
        shmemFile = -1;
    }
}

#else
int FrameBuffer::InitializeFrameBufferIOCTL(void) {
    std::string devString = getSetting("framebufferControlSocketPath", "/dev") + "/" + m_device;
    m_fbFd = open(devString.c_str(), O_RDWR);
    if (!m_fbFd) {
        LogErr(VB_CHANNELOUT, "Error opening FrameBuffer device: %s\n", devString.c_str());
        return 0;
    }

    if (ioctl(m_fbFd, FBIOGET_VSCREENINFO, &m_vInfo)) {
        LogErr(VB_CHANNELOUT, "Error getting FrameBuffer info for device: %s\n", devString.c_str());
        close(m_fbFd);
        return 0;
    }

    if (m_bpp != -1)
        m_vInfo.bits_per_pixel = m_bpp;
    else if (m_vInfo.bits_per_pixel == 32)
        m_vInfo.bits_per_pixel = 24;

    m_bpp = m_vInfo.bits_per_pixel;

    if ((m_bpp != 24) && (m_bpp != 16)) {
        LogErr(VB_CHANNELOUT, "Do not know how to handle %d BPP\n", m_bpp);
        close(m_fbFd);
        return 0;
    }

    if (m_bpp == 16) {
        LogExcess(VB_CHANNELOUT, "Current Bitfield offset info:\n");
        LogExcess(VB_CHANNELOUT, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
        LogExcess(VB_CHANNELOUT, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
        LogExcess(VB_CHANNELOUT, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);

        // RGB565
        m_vInfo.red.offset = 11;
        m_vInfo.red.length = 5;
        m_vInfo.green.offset = 5;
        m_vInfo.green.length = 6;
        m_vInfo.blue.offset = 0;
        m_vInfo.blue.length = 5;
        m_vInfo.transp.offset = 0;
        m_vInfo.transp.length = 0;

        LogExcess(VB_CHANNELOUT, "New Bitfield offset info should be:\n");
        LogExcess(VB_CHANNELOUT, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
        LogExcess(VB_CHANNELOUT, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
        LogExcess(VB_CHANNELOUT, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);
    }

    if (m_width && m_height) {
        m_vInfo.xres = m_width;
        m_vInfo.yres = m_height;
    } else {
        m_width = m_vInfo.xres;
        m_height = m_vInfo.yres;
    }
    m_vInfo.xres_virtual = m_width;
    m_vInfo.yres_virtual = m_height * m_pages;
    m_vInfo.yoffset = 0;

    if (m_autoSync) {
        m_cPage = 1; // Drawing will be on the second page first
        m_pPage = 1; // Producer gets Page(true) then NextPage(true) after checking if page is clean
    }

    if (ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfo)) {
        m_vInfo.yres_virtual = m_height;
        m_cPage = 0;
        m_pPage = 0;
        m_pages = 1;

        if (ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfo)) {
            LogErr(VB_CHANNELOUT, "Error setting FrameBuffer info\n");
            close(m_fbFd);
            return 0;
        } else {
            LogWarn(VB_CHANNELOUT, "Unable to allocate enough memory for double buffering, using single buffered framebuffer\n");
        }
    }

    if (ioctl(m_fbFd, FBIOGET_FSCREENINFO, &m_fInfo)) {
        LogErr(VB_CHANNELOUT, "Error getting fixed FrameBuffer info\n");
        close(m_fbFd);
        return 0;
    }
    m_bpp = m_vInfo.bits_per_pixel;
    if (devString == "/dev/fb0") {
        m_ttyFd = open("/dev/console", O_RDWR);
        if (!m_ttyFd) {
            LogErr(VB_CHANNELOUT, "Error, unable to open /dev/console\n");
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
    m_rowPadding = m_rowStride - (m_width * m_bpp / 8);
    m_pageSize = m_vInfo.yres * m_fInfo.line_length;
    m_bufferSize = m_pageSize * m_pages;
    m_buffer = (uint8_t*)mmap(0, m_bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fbFd, 0);

    if ((char*)m_buffer == (char*)-1) {
        m_buffer = nullptr;
        LogErr(VB_CHANNELOUT, "Error, unable to map framebuffer device %s\n", devString.c_str());
        DestroyFrameBuffer();
        return 0;
    }

    memset(m_buffer, 0, m_bufferSize);
    m_pageBuffers[0] = m_buffer;
    if (m_pages == 2) {
        m_pageBuffers[1] = m_buffer + m_pageSize;
    }

    if (m_bpp == 16) {
        LogExcess(VB_CHANNELOUT, "Generating RGB565Map for Bitfield offset info:\n");
        LogExcess(VB_CHANNELOUT, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
        LogExcess(VB_CHANNELOUT, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
        LogExcess(VB_CHANNELOUT, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);

        int rShift = m_vInfo.red.offset - (8 + (8 - m_vInfo.red.length));
        int gShift = m_vInfo.green.offset - (8 + (8 - m_vInfo.green.length));
        int bShift = m_vInfo.blue.offset - (8 + (8 - m_vInfo.blue.length));

#if 0
        uint8_t rMask = (0xFF ^ (0xFF >> m_vInfo.red.length));
        uint8_t gMask = (0xFF ^ (0xFF >> m_vInfo.green.length));
        uint8_t bMask = (0xFF ^ (0xFF >> m_vInfo.blue.length));
        LogDebug(VB_CHANNELOUT, "rM/rS: 0x%02x/%d, gM/gS: 0x%02x/%d, bM/bS: 0x%02x/%d\n", rMask, rShift, gMask, gShift, bMask, bShift);
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
}

void FrameBuffer::DestroyFrameBufferIOCTL(void) {
    if (m_ttyFd != -1) {
        // Show the text console
        ioctl(m_ttyFd, KDSETMODE, KD_TEXT);

        // Shouldn't need this anymore.
        close(m_ttyFd);
        m_ttyFd = -1;
    }
}

#ifdef HAS_DRM
int FrameBuffer::InitializeFrameBufferDRM(void) {
    if (!drmAvailable()) {
        return 0;
    }
    for (int x = 0; x < 10; x++) {
        std::string dev = "/dev/dri/card";
        dev += std::to_string(x);
        if (FileExists(dev)) {
            int fd = open(dev.c_str(), O_RDWR);
            drmSetMaster(fd);
            auto res = drmModeGetResources(fd);
            if (res == nullptr) {
                close(fd);
                continue;
            }
            drmModeConnectorPtr connector = 0;
            for (int i = 0; i < res->count_connectors; i++) {
                char name[32];

                connector = drmModeGetConnectorCurrent(fd, res->connectors[i]);
                if (!connector) {
                    continue;
                }
                std::string fbd = GetConnectorType(connector->connector_type) + "-" + std::to_string(connector->connector_type_id);
                if (fbd == m_device) {
                    LogDebug(VB_CHANNELOUT, "Using FrameBuffer for %s\n", fbd.c_str());
                    LogDebug(VB_CHANNELOUT, "  Modes:\n");
                    m_connector = connector;

                    drmModeEncoderPtr encoder = drmModeGetEncoder(fd, connector->encoder_id);
                    if (encoder == nullptr) {
                        LogWarn(VB_CHANNELOUT, "DRM Could not get encoder.\n");
                        WarningHolder::AddWarning("Could not initialize framebuffer for " + m_device);
                        return 0;
                    }
                    m_crtc = drmModeGetCrtc(fd, encoder->crtc_id);

                    drmModeModeInfoPtr resolution = 0;
                    m_mode = 0;
                    for (int i = 0; i < connector->count_modes; i++) {
                        resolution = &connector->modes[i];
                        LogDebug(VB_CHANNELOUT, "    %d x %d   %d   %s\n", resolution->hdisplay, resolution->vdisplay, resolution->vrefresh,
                                 (resolution->type & DRM_MODE_TYPE_PREFERRED) ? "(preferred)" : "");

                        if (resolution->hdisplay >= m_width && resolution->vdisplay >= m_height) {
                            m_mode = i;
                        }
                    }

                    drm_mode_create_dumb dumb_framebuffer;
                    memset(&dumb_framebuffer, 0, sizeof(dumb_framebuffer));
                    dumb_framebuffer.width = connector->modes[m_mode].hdisplay;
                    dumb_framebuffer.height = connector->modes[m_mode].vdisplay;
                    dumb_framebuffer.bpp = 32;
                    dumb_framebuffer.flags = 0;

                    int err = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb_framebuffer);
                    if (err) {
                        LogWarn(VB_CHANNELOUT, "DRM Create framebuffer failed (err=%d)\n", err);
                        WarningHolder::AddWarning("Could not initialize framebuffer for " + m_device);
                        return 0;
                    }
                    m_bufferHandles[0] = 0;
                    err = drmModeAddFB(fd, dumb_framebuffer.width, dumb_framebuffer.height, 24, 32,
                                       dumb_framebuffer.pitch, dumb_framebuffer.handle, &m_bufferHandles[0]);
                    struct drm_mode_map_dumb mapReq = { dumb_framebuffer.handle, 0, 0 };
                    err = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mapReq);
                    if (err) {
                        LogWarn(VB_CHANNELOUT, "DRM map framebuffer failed (err=%d)\n", err);
                        WarningHolder::AddWarning("Could not initialize framebuffer for " + m_device);
                        return 0;
                    }

                    m_pageBuffers[0] = (uint8_t*)mmap(0, dumb_framebuffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapReq.offset);
                    if (m_pageBuffers[0] == MAP_FAILED) {
                        err = errno;
                        LogWarn(VB_CHANNELOUT, "DRM mmap framebuffer failed (err=%d)\n", err);
                        return 0;
                    }
                    m_pages = 2;

                    memset(&dumb_framebuffer, 0, sizeof(dumb_framebuffer));
                    dumb_framebuffer.width = connector->modes[m_mode].hdisplay;
                    dumb_framebuffer.height = connector->modes[m_mode].vdisplay;
                    dumb_framebuffer.bpp = 32;
                    dumb_framebuffer.flags = 0;
                    err = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb_framebuffer);
                    if (err) {
                        m_pages = 1;
                        LogWarn(VB_CHANNELOUT, "DRM Create framebuffer failed (err=%d). Using single page.\n", err);
                    } else {
                        m_bufferHandles[1] = 0;
                        err = drmModeAddFB(fd, dumb_framebuffer.width, dumb_framebuffer.height, 24, 32,
                                           dumb_framebuffer.pitch, dumb_framebuffer.handle, &m_bufferHandles[1]);
                        struct drm_mode_map_dumb mapReq2 = { dumb_framebuffer.handle, 0, 0 };
                        err = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mapReq2);
                        if (err) {
                            m_pages = 1;
                            LogWarn(VB_CHANNELOUT, "DRM mmap framebuffer failed (err=%d). Using single page.\n", err);
                        }

                        m_pageBuffers[1] = (uint8_t*)mmap(0, dumb_framebuffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapReq2.offset);
                        if (m_pageBuffers[1] == MAP_FAILED) {
                            m_pages = 1;
                            err = errno;
                            LogWarn(VB_CHANNELOUT, "DRM mmap framebuffer failed (err=%d). Using single page.\n", err);
                        }
                    }

                    usingDRM = true;

                    m_cPage = 0;
                    m_pPage = 0;
                    m_bpp = 32;
                    m_rowStride = dumb_framebuffer.pitch;
                    m_rowPadding = m_rowStride - (m_width * m_bpp / 8);
                    m_pageSize = dumb_framebuffer.size;
                    m_bufferSize = dumb_framebuffer.size;
                    drmModeSetCrtc(m_fbFd, m_crtc->crtc_id, m_bufferHandles[0], 0, 0, &m_connector->connector_id, 1, &m_connector->modes[m_mode]);

                } else {
                    drmModeFreeConnector(connector);
                }
            }
            drmModeFreeResources(res);
            drmDropMaster(fd);
            if (!usingDRM) {
                close(fd);
            } else {
                m_fbFd = fd;
                return 1;
            }
        }
    }
    return 0;
}
void FrameBuffer::DestroyFrameBufferDRM(void) {
    if (usingDRM) {
        if (m_pageBuffers[0]) {
            memset(m_pageBuffers[0], 0, m_bufferSize);
            munmap(m_pageBuffers[0], m_bufferSize);
            struct drm_mode_destroy_dumb d = { m_bufferHandles[0] };
            ioctl(m_fbFd, DRM_IOCTL_MODE_DESTROY_DUMB, &d);
        }
        if (m_pageBuffers[1]) {
            memset(m_pageBuffers[1], 0, m_bufferSize);
            munmap(m_pageBuffers[1], m_bufferSize);
            struct drm_mode_destroy_dumb d2 = { m_bufferHandles[1] };
            ioctl(m_fbFd, DRM_IOCTL_MODE_DESTROY_DUMB, &d2);
        }

        drmModeFreeConnector(m_connector);
    }
}
#endif
#endif

#ifdef USE_X11
int FrameBuffer::InitializeX11Window(void) {
    if ((m_width == 0) || (m_height == 0)) {
        LogErr(VB_CHANNELOUT, "Invalid X11 FrameBuffer width/height of %dx%d\n", m_width, m_height);
        return 0;
    }

    m_display = XOpenDisplay(getenv("DISPLAY"));
    if (!m_display) {
        LogErr(VB_CHANNELOUT, "Unable to connect to X Server\n");
        return 0;
    }

    m_screen = DefaultScreen(m_display);

    m_pages = 1;
    m_bpp = 32;
    m_rowStride = m_width * m_bpp / 8;
    m_rowPadding = 0;
    m_pageSize = m_width * m_height * m_bpp / 8;
    m_bufferSize = m_pageSize;
    m_buffer = new uint8_t[m_bufferSize];

    m_xImage = XCreateImage(m_display, CopyFromParent, 24, ZPixmap, 0,
                            (char*)m_buffer, m_width, m_height, 32, m_width * 4);

    XSetWindowAttributes attributes;

    attributes.background_pixel = BlackPixel(m_display, m_screen);

    XGCValues values;

    m_pixmap = XCreatePixmap(m_display, XDefaultRootWindow(m_display), m_width, m_height, 24);

    m_gc = XCreateGC(m_display, m_pixmap, 0, &values);
    if (m_gc < 0) {
        LogErr(VB_CHANNELOUT, "Unable to create GC\n");
        return 0;
    }

    m_window = XCreateWindow(
        m_display, RootWindow(m_display, m_screen), 0, 0,
        m_width, m_height, 5, 24, InputOutput,
        DefaultVisual(m_display, m_screen), CWBackPixel, &attributes);

    XMapWindow(m_display, m_window);

    XStoreName(m_display, m_window, m_name.c_str());
    XSetIconName(m_display, m_window, m_name.c_str());

    XFlush(m_display);

    if ((m_xOffset != -1) && (m_yOffset != -1)) {
        XMoveWindow(m_display, m_window, m_xOffset, m_yOffset);
        XSync(m_display, True);
        XFlush(m_display);
    }

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
    int drow = 0;
    const uint8_t* sR = buffer + 0; // Input always RGB
    const uint8_t* sG = buffer + 1;
    const uint8_t* sB = buffer + 2;
    uint8_t* ob = m_outputBuffer;

    if (draw && (m_pixelSize == 1)) {
        m_bufferLock.lock();
        ob = FB_CURRENT_PAGE_PTR;
    }

#ifndef USE_FRAMEBUFFER_SOCKET
    if (m_bpp == 16) {
        uint8_t* l = m_lastFrame;
        int skipped = 0;
        int sBpp = 3;
        uint8_t* d;

        for (int y = 0; y < m_pixelsHigh; y++) {
            d = ob + (drow * m_pixelSize * m_rowStride);
            for (int x = 0; x < m_pixelsWide; x++) {
                if (memcmp(l, sR, sBpp)) {
                    if (skipped) {
                        sG += skipped * sBpp * m_pixelSize;
                        sB += skipped * sBpp * m_pixelSize;
                        d += skipped * 2 * m_pixelSize;
                        skipped = 0;
                    }

                    for (int sc = 0; sc < m_pixelSize; sc++) {
                        *((uint16_t*)d) = m_rgb565map[*sR >> 3][*sG >> 2][*sB >> 3];
                        d += 2;
                    }

                    sG += sBpp;
                    sB += sBpp;
                } else {
                    skipped++;
                }

                sR += sBpp;
                l += sBpp;
            }

            d = ob + (drow * m_pixelSize * m_rowStride);
            for (int sc = 1; sc < m_pixelSize; sc++) {
                memcpy(d + (sc * m_rowStride), d, m_rowStride);
            }

            drow++;
        }

        memcpy(m_lastFrame, buffer, m_frameSize);
    } else
#endif
    {
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

#ifdef USE_X11
void FrameBuffer::SyncLoopX11() {
    while (m_runLoop) {
        if (m_dirtyPages[m_cPage]) {
            XLockDisplay(m_display);
            XPutImage(m_display, m_window, m_gc, m_xImage, 0, 0, 0, 0, m_width, m_height);
            XSync(m_display, True);
            XFlush(m_display);
            XUnlockDisplay(m_display);

            m_dirtyPages[m_cPage] = 0;
            NextPage();
        }

        usleep(25000);
    }
}
#endif

#ifdef USE_FRAMEBUFFER_SOCKET
void FrameBuffer::SyncLoopFBSocket() {
    while (m_runLoop) {
        if (m_dirtyPages[m_cPage]) {
            sendFramebufferFrame();

            m_dirtyPages[m_cPage] = 0;
            NextPage();
        }

        usleep(25000);
    }
}
#else
void FrameBuffer::SyncLoopFB() {
    int dummy = 0;

    if (m_pages == 1) {
        // using /dev/fb* and only one page so writes are immediate, no need to sync
        return;
    }

    while (m_runLoop) {
        // Wait for vsync
        dummy = 0;
        //LogInfo(VB_CHANNELOUT, "%s - %d - waiting for vsync\n", m_device.c_str(), m_cPage);
        if (!usingDRM) {
            ioctl(m_fbFd, FBIO_WAITFORVSYNC, &dummy);
        }

        // Allow the producer to catch up if needed
        if (!m_dirtyPages[m_cPage] && m_dirtyPages[(m_cPage + 1) % m_pages])
            NextPage();

        if (m_dirtyPages[m_cPage]) {
            // Pan the display to the page
            //LogInfo(VB_CHANNELOUT, "%s - %d - flipping to cPage\n", m_device.c_str(), m_cPage);
            if (usingDRM) {
#ifdef HAS_DRM
                static int first = true;
                drmSetMaster(m_fbFd);

                if (first) {
                    drmModeSetCrtc(m_fbFd, m_crtc->crtc_id, m_bufferHandles[m_cPage], 0, 0, &m_connector->connector_id, 1, &m_connector->modes[m_mode]);
                    first = false;
                } else {
                    drmModePageFlip(m_fbFd, m_crtc->crtc_id, m_bufferHandles[m_cPage], 0, nullptr);
                }
                drmDropMaster(m_fbFd);
#endif
            } else {
                m_vInfo.yoffset = m_vInfo.yres * m_cPage;
                ioctl(m_fbFd, FBIOPAN_DISPLAY, &m_vInfo);

                //LogInfo(VB_CHANNELOUT, "%s - %d - marking clear and bumping cPage\n", m_device.c_str(), m_cPage);
            }
            m_dirtyPages[m_cPage] = 0;
            NextPage();
        }
    }
}
#endif

void FrameBuffer::DrawLoop(void) {
    if (m_autoSync) {
        if (m_device == "x11") {
#ifdef USE_X11
            SyncLoopX11();
#endif
        } else {
#ifdef USE_FRAMEBUFFER_SOCKET
            SyncLoopFBSocket();
#else
            SyncLoopFB();
#endif
        }
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

    //LogInfo(VB_CHANNELOUT, "%s - %d - NextPage() for %s\n", m_device.c_str(), m_cPage, producer ? "producer" : "consumer");
    if (producer)
        m_pPage = (m_pPage + 1) % m_pages;
    else
        m_cPage = (m_cPage + 1) % m_pages;
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
        if (!pageChanged)
            return;

        if (m_pages == 1)
            return;
#ifdef USE_FRAMEBUFFER_SOCKET
        sendFramebufferFrame();
#else
#ifdef HAS_DRM
        if (usingDRM) {
            static int first = true;
            drmSetMaster(m_fbFd);

            if (first) {
                drmModeSetCrtc(m_fbFd, m_crtc->crtc_id, m_bufferHandles[m_cPage], 0, 0, &m_connector->connector_id, 1, &m_connector->modes[m_mode]);
                first = false;
            } else {
                drmModePageFlip(m_fbFd, m_crtc->crtc_id, m_bufferHandles[m_cPage], 0, nullptr);
            }
            drmDropMaster(m_fbFd);
            return;
        }
#endif

        // Wait for vsync
        int dummy = 0;
        //LogInfo(VB_CHANNELOUT, "%s - %d - SyncDisplay() waiting for vsync\n", m_device.c_str(), m_cPage);

        // FBIO_WAITFORVSYNC causes DPIPixels on fb1 to not work when fb0 is in use for Virtual Matrix
        // or Virtual Display.  It shouldn't be needed since we are using page flipping.
        // ioctl(m_fbFd, FBIO_WAITFORVSYNC, &dummy);

        // Pan the display to the new page
        //LogInfo(VB_CHANNELOUT, "%s - %d - SyncDisplay() flipping to cPage\n", m_device.c_str(), m_cPage);
        m_vInfo.yoffset = m_vInfo.yres * m_cPage;
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

void FrameBuffer::ClearAllPages() {
    memset(m_buffer, 0, m_bufferSize);
}

void FrameBuffer::Clear() {
    memset(FB_CURRENT_PAGE_PTR, 0, m_pageSize);
}
