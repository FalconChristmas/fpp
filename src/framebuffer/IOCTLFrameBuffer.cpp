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

#include "IOCTLFrameBuffer.h"

#ifdef HAS_IOCTL_FB

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

/*
 *
 */
IOCTLFrameBuffer::IOCTLFrameBuffer() {
    LogDebug(VB_CHANNELOUT, "IOCTLFrameBuffer::IOCTLFrameBuffer()\n");
}

/*
 *
 */
IOCTLFrameBuffer::~IOCTLFrameBuffer() {
}

int IOCTLFrameBuffer::InitializeFrameBuffer() {
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

    if (m_bpp != -1) {
        m_vInfo.bits_per_pixel = m_bpp;
    } else if (m_vInfo.bits_per_pixel == 32) {
        m_vInfo.bits_per_pixel = 24;
    }

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
    if (m_pages >= 2) {
        m_pageBuffers[1] = m_buffer + m_pageSize;
    }
    if (m_pages >= 3) {
        m_pageBuffers[2] = m_buffer + 2 * m_pageSize;
    }

    if (m_bpp == 16) {
        LogExcess(VB_CHANNELOUT, "Generating RGB565Map for Bitfield offset info:\n");
        LogExcess(VB_CHANNELOUT, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
        LogExcess(VB_CHANNELOUT, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
        LogExcess(VB_CHANNELOUT, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);

        int rShift = m_vInfo.red.offset - (8 + (8 - m_vInfo.red.length));
        int gShift = m_vInfo.green.offset - (8 + (8 - m_vInfo.green.length));
        int bShift = m_vInfo.blue.offset - (8 + (8 - m_vInfo.blue.length));

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

void IOCTLFrameBuffer::DestroyFrameBuffer(void) {
    if (m_ttyFd != -1) {
        // Show the text console
        ioctl(m_ttyFd, KDSETMODE, KD_TEXT);

        // Shouldn't need this anymore.
        close(m_ttyFd);
        m_ttyFd = -1;
    }
    if (m_rgb565map) {
        for (int r = 0; r < 32; r++) {
            for (int g = 0; g < 64; g++) {
                delete[] m_rgb565map[r][g];
            }

            delete[] m_rgb565map[r];
        }

        delete[] m_rgb565map;
    }
    FrameBuffer::DestroyFrameBuffer();
}

/*
 *
 */
void IOCTLFrameBuffer::FBCopyData(const uint8_t* buffer, int draw) {
    if (m_bpp == 16) {
        int sBpp = 3;
        uint8_t* d;

        const uint8_t* sR = buffer + 0; // Input always RGB
        const uint8_t* sG = buffer + 1;
        const uint8_t* sB = buffer + 2;
        uint8_t* ob = m_outputBuffer;
        int drow = 0;

        if (draw) {
            m_bufferLock.lock();
        }
        if (m_pixelSize == 1) {
            ob = m_pageBuffers[m_cPage];
        }

        for (int y = 0; y < m_pixelsHigh; y++) {
            d = ob + (drow * m_pixelSize * m_rowStride);
            for (int x = 0; x < m_pixelsWide; x++) {
                for (int sc = 0; sc < m_pixelSize; sc++) {
                    *((uint16_t*)d) = m_rgb565map[*sR >> 3][*sG >> 2][*sB >> 3];
                    d += 2;
                }

                sG += sBpp;
                sB += sBpp;
                sR += sBpp;
            }

            d = ob + (drow * m_pixelSize * m_rowStride);
            for (int sc = 1; sc < m_pixelSize; sc++) {
                memcpy(d + (sc * m_rowStride), d, m_rowStride);
            }

            drow++;
        }

        if (draw) {
            m_bufferLock.unlock();
        } else {
            m_imageReady = true;
        }
    } else {
        FrameBuffer::FBCopyData(buffer, draw);
    }
}

void IOCTLFrameBuffer::SyncLoop() {
    int dummy = 0;

    if (m_pages == 1) {
        // using /dev/fb* and only one page so writes are immediate, no need to sync
        return;
    }

    while (m_runLoop) {
        // Wait for vsync
        dummy = 0;
        // LogInfo(VB_CHANNELOUT, "%s - %d - waiting for vsync\n", m_device.c_str(), m_cPage);
        ioctl(m_fbFd, FBIO_WAITFORVSYNC, &dummy);

        // Allow the producer to catch up if needed
        if (!m_dirtyPages[m_cPage] && m_dirtyPages[(m_cPage + 1) % m_pages])
            NextPage();

        if (m_dirtyPages[m_cPage]) {
            // Pan the display to the page
            // LogInfo(VB_CHANNELOUT, "%s - %d - flipping to cPage\n", m_device.c_str(), m_cPage);

            m_vInfo.yoffset = m_vInfo.yres * m_cPage;
            ioctl(m_fbFd, FBIOPAN_DISPLAY, &m_vInfo);

            // LogInfo(VB_CHANNELOUT, "%s - %d - marking clear and bumping cPage\n", m_device.c_str(), m_cPage);
            m_dirtyPages[m_cPage] = 0;
            NextPage();
        }
    }
}

void IOCTLFrameBuffer::SyncDisplay(bool pageChanged) {
    if (m_fbFd != -1) {
        if (!pageChanged)
            return;

        if (m_pages == 1)
            return;

        // Wait for vsync
        int dummy = 0;
        // LogInfo(VB_CHANNELOUT, "%s - %d - SyncDisplay() waiting for vsync\n", m_device.c_str(), m_cPage);

        // FBIO_WAITFORVSYNC causes DPIPixels on fb1 to not work when fb0 is in use for Virtual Matrix
        // or Virtual Display.  It shouldn't be needed since we are using page flipping.
        // ioctl(m_fbFd, FBIO_WAITFORVSYNC, &dummy);

        // Pan the display to the new page
        // LogInfo(VB_CHANNELOUT, "%s - %d - SyncDisplay() flipping to cPage\n", m_device.c_str(), m_cPage);
        m_vInfo.yoffset = m_vInfo.yres * m_cPage;
        ioctl(m_fbFd, FBIOPAN_DISPLAY, &m_vInfo);
    }
}

#endif