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

#include "KMSFrameBuffer.h"

#ifdef HAS_KMS_FB
#include <libdrm/drm.h>
#include <libdrm/drm_fourcc.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cmath>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <linux/netlink.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <thread>

#include "../common.h"
#include "../common_mini.h"
#include "../log.h"
#include "../mediaoutput/mediaoutput.h"

std::atomic_int KMSFrameBuffer::FRAMEBUFFER_COUNT(0);
std::vector<CardInfo*> KMSFrameBuffer::CARDS;
std::map<uint32_t, KMSFrameBuffer*> KMSFrameBuffer::FLIP_TARGETS;
uint32_t KMSFrameBuffer::NEXT_FLIP_TOKEN = 0;

std::string KMSFrameBuffer::ConnectorFullName(int fd, drmModeConnectorPtr conn) {
    const char* typeName = drmModeGetConnectorTypeName(conn->connector_type);
    std::string name = typeName ? typeName : "Unknown";
    name += "-" + std::to_string(conn->connector_type_id);
    return name;
}

bool KMSFrameBuffer::CreateDumbBuffer(int fd, uint32_t width, uint32_t height, uint32_t format, DumbBuffer& buf) {
    uint32_t bpp;
    if (format == DRM_FORMAT_RGB888) {
        bpp = 24;
    } else if (format == DRM_FORMAT_RGB565) {
        bpp = 16;
    } else {
        bpp = 32;
    }

    struct drm_mode_create_dumb creq = {};
    creq.width = width;
    creq.height = height;
    creq.bpp = bpp;

    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
        return false;
    }

    uint32_t handles[4] = { creq.handle, 0, 0, 0 };
    uint32_t pitches[4] = { creq.pitch, 0, 0, 0 };
    uint32_t offsets[4] = { 0, 0, 0, 0 };
    uint32_t fb_id = 0;

    if (drmModeAddFB2(fd, width, height, format, handles, pitches, offsets, &fb_id, 0) < 0) {
        struct drm_mode_destroy_dumb dreq = {};
        dreq.handle = creq.handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        return false;
    }

    struct drm_mode_map_dumb mreq = {};
    mreq.handle = creq.handle;
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0) {
        drmModeRmFB(fd, fb_id);
        struct drm_mode_destroy_dumb dreq = {};
        dreq.handle = creq.handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        return false;
    }

    void* mapped = mmap(nullptr, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
    if (mapped == MAP_FAILED) {
        drmModeRmFB(fd, fb_id);
        struct drm_mode_destroy_dumb dreq = {};
        dreq.handle = creq.handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        return false;
    }

    buf.handle = creq.handle;
    buf.fb_id = fb_id;
    buf.width = width;
    buf.height = height;
    buf.stride = creq.pitch;
    buf.format = format;
    buf.size = creq.size;
    buf.mapped = mapped;
    return true;
}

void KMSFrameBuffer::DestroyDumbBuffer(int fd, DumbBuffer& buf) {
    if (buf.mapped) {
        munmap(buf.mapped, buf.size);
        buf.mapped = nullptr;
    }
    if (buf.fb_id) {
        drmModeRmFB(fd, buf.fb_id);
        buf.fb_id = 0;
    }
    if (buf.handle) {
        struct drm_mode_destroy_dumb dreq = {};
        dreq.handle = buf.handle;
        drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        buf.handle = 0;
    }
    buf = {};
}

uint32_t KMSFrameBuffer::FindPlaneForCrtc(int fd, uint32_t crtcId, uint32_t format) {
    drmModeResPtr res = drmModeGetResources(fd);
    if (!res) {
        return 0;
    }

    int crtcIndex = -1;
    for (int i = 0; i < res->count_crtcs; i++) {
        if (res->crtcs[i] == crtcId) {
            crtcIndex = i;
            break;
        }
    }
    drmModeFreeResources(res);

    if (crtcIndex < 0) {
        return 0;
    }

    drmModePlaneResPtr planes = drmModeGetPlaneResources(fd);
    if (!planes) {
        return 0;
    }

    uint32_t result = 0;
    for (uint32_t i = 0; i < planes->count_planes && result == 0; i++) {
        drmModePlanePtr plane = drmModeGetPlane(fd, planes->planes[i]);
        if (!plane) {
            continue;
        }

        if (!(plane->possible_crtcs & (1u << crtcIndex))) {
            drmModeFreePlane(plane);
            continue;
        }

        // Check if already reserved
        bool reserved = false;
        for (auto* card : CARDS) {
            for (auto pid : card->reservedPlanes) {
                if (pid == plane->plane_id) {
                    reserved = true;
                    break;
                }
            }
            if (reserved) break;
        }
        if (reserved) {
            drmModeFreePlane(plane);
            continue;
        }

        for (uint32_t j = 0; j < plane->count_formats; j++) {
            if (plane->formats[j] == format) {
                result = plane->plane_id;
                break;
            }
        }
        drmModeFreePlane(plane);
    }
    drmModeFreePlaneResources(planes);
    return result;
}

KMSFrameBuffer::KMSFrameBuffer() {
    LogDebug(VB_CHANNELOUT, "KMSFrameBuffer::KMSFrameBuffer()\n");

    if (FRAMEBUFFER_COUNT == 0) {
        for (int cn = 0; cn < 10; cn++) {
            std::string path = "/dev/dri/card" + std::to_string(cn);
            if (FileExists(path)) {
                int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
                if (fd < 0) {
                    continue;
                }
                drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
                ioctl(fd, DRM_IOCTL_DROP_MASTER, 0);
                CardInfo* info = new CardInfo();
                info->fd = fd;
                info->path = path;
                CARDS.push_back(info);
            }
        }
        StartHotplugMonitor();
    }
    ++FRAMEBUFFER_COUNT;

    std::unique_lock<std::mutex> lock(drmMasterLock);
    m_flipToken = ++NEXT_FLIP_TOKEN;
    FLIP_TARGETS[m_flipToken] = this;
}

KMSFrameBuffer::~KMSFrameBuffer() {
    // The draw loop writes into the mapped dumb buffers and calls the virtual
    // SyncDisplay(); it has to be dead before this destructor releases any of
    // that.  ~FrameBuffer() joins far too late - by then the vtable is already
    // demoted and the card fds below may be closed.
    StopDrawLoop();

    {
        std::unique_lock<std::mutex> lock(drmMasterLock);
        // Retire our last flip while this object can still take its event.
        // Past this point one that is still queued is dropped by token.
        if (m_flipPending) {
            int vr = m_mode.vrefresh > 0 ? m_mode.vrefresh : 60;
            WaitForPendingFlip((1000 / vr) + 20);
        }
        FLIP_TARGETS.erase(m_flipToken);

        if (m_displayEnabled && m_crtcId && m_planeId) {
            LogInfo(VB_CHANNELOUT, "KMSFrameBuffer::~KMSFrameBuffer() disabling display before destruction\n");
            int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
            if (im == 0) {
                drmModeSetPlane(m_cardFd, m_planeId, m_crtcId, m_fb[m_cPage].fb_id, 0,
                                0, 0, 0, 0, 0, 0, 0, 0);
                m_displayEnabled = false;
                ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
            }
        }
    }

    // Must run here, not from ~FrameBuffer(): the base destructor can only reach
    // FrameBuffer::DestroyFrameBuffer(), so the dumb buffers and the connector /
    // CRTC / plane reservations were never released on delete.  It also has to
    // happen before the card fds are closed below.
    KMSFrameBuffer::DestroyFrameBuffer();

    --FRAMEBUFFER_COUNT;
    if (FRAMEBUFFER_COUNT == 0) {
        StopHotplugMonitor();
        for (auto* card : CARDS) {
            if (card->fd >= 0) {
                close(card->fd);
            }
            delete card;
        }
        CARDS.clear();
    }
}

int KMSFrameBuffer::InitializeFrameBuffer(void) {
    LogDebug(VB_CHANNELOUT, "KMSFrameBuffer::InitializeFrameBuffer()\n");

    for (auto* card : CARDS) {
        drmModeResPtr res = drmModeGetResources(card->fd);
        if (!res) {
            continue;
        }

        for (int ci = 0; ci < res->count_connectors; ci++) {
            drmModeConnectorPtr conn = drmModeGetConnector(card->fd, res->connectors[ci]);
            if (!conn) {
                continue;
            }

            std::string cname = ConnectorFullName(card->fd, conn);
            bool connected = (conn->connection == DRM_MODE_CONNECTED);

            if (m_device != cname || !connected) {
                drmModeFreeConnector(conn);
                continue;
            }

            // Check if already reserved
            bool alreadyReserved = false;
            for (auto cid : card->reservedConnectors) {
                if (cid == conn->connector_id) {
                    alreadyReserved = true;
                    break;
                }
            }
            if (alreadyReserved) {
                drmModeFreeConnector(conn);
                continue;
            }

            // Find a CRTC for this connector
            uint32_t crtcId = 0;

            // Try the encoder's current CRTC first
            if (conn->encoder_id) {
                drmModeEncoderPtr enc = drmModeGetEncoder(card->fd, conn->encoder_id);
                if (enc) {
                    // Check if this CRTC is already reserved
                    bool crtcReserved = false;
                    for (auto rid : card->reservedCrtcs) {
                        if (rid == enc->crtc_id) {
                            crtcReserved = true;
                            break;
                        }
                    }
                    if (!crtcReserved && enc->crtc_id) {
                        crtcId = enc->crtc_id;
                    }
                    drmModeFreeEncoder(enc);
                }
            }

            // If no CRTC yet, try all encoders for this connector
            if (crtcId == 0) {
                for (int ei = 0; ei < conn->count_encoders && crtcId == 0; ei++) {
                    drmModeEncoderPtr enc = drmModeGetEncoder(card->fd, conn->encoders[ei]);
                    if (!enc) continue;
                    for (int ci2 = 0; ci2 < res->count_crtcs && crtcId == 0; ci2++) {
                        if (enc->possible_crtcs & (1u << ci2)) {
                            bool crtcReserved = false;
                            for (auto rid : card->reservedCrtcs) {
                                if (rid == res->crtcs[ci2]) {
                                    crtcReserved = true;
                                    break;
                                }
                            }
                            if (!crtcReserved) {
                                crtcId = res->crtcs[ci2];
                            }
                        }
                    }
                    drmModeFreeEncoder(enc);
                }
            }

            if (crtcId == 0) {
                drmModeFreeConnector(conn);
                drmModeFreeResources(res);
                return 0;
            }

            // Get default mode (first preferred, or first available)
            drmModeModeInfo mode = {};
            bool foundMode = false;
            for (int mi = 0; mi < conn->count_modes; mi++) {
                if (conn->modes[mi].type & DRM_MODE_TYPE_PREFERRED) {
                    mode = conn->modes[mi];
                    foundMode = true;
                    break;
                }
            }
            if (!foundMode && conn->count_modes > 0) {
                mode = conn->modes[0];
                foundMode = true;
            }

            if (!foundMode) {
                drmModeFreeConnector(conn);
                drmModeFreeResources(res);
                return 0;
            }

            m_mode = mode;
            m_connectorId = conn->connector_id;
            m_crtcId = crtcId;
            m_crtcPipe = 0;
            for (int ci = 0; ci < res->count_crtcs; ci++) {
                if (res->crtcs[ci] == crtcId) {
                    m_crtcPipe = ci;
                }
            }
            m_connectorName = cname;
            m_cardFd = card->fd;
            m_cardInfo = card;

            // The panel refresh caps the sustainable output frame rate: page
            // flips are vblank-paced, so a sequence faster than this will be
            // limited to the refresh rate.
            LogInfo(VB_CHANNELOUT, "KMSFrameBuffer: %s mode %dx%d @ %dHz\n",
                    cname.c_str(), mode.hdisplay, mode.vdisplay, mode.vrefresh);

            card->reservedConnectors.push_back(m_connectorId);
            card->reservedCrtcs.push_back(m_crtcId);

            if (m_variableRefresh) {
                // DPI Pixels: drive our own vertical timing.  Keep the
                // connector's horizontal timing and pixel clock (they define the
                // WS281x bit timing) but set vactive to the caller's requested
                // scanline count (sized to the longest string) and start at the
                // maximum refresh rate.  DPIPixelsOutput lowers it per-sequence
                // via SetRefreshRate() by only varying the vertical blanking, so
                // the framebuffer and its contents never have to be rebuilt.
                if (m_width == 0) {
                    m_width = m_mode.hdisplay;
                }
                if (m_height == 0) {
                    m_height = m_mode.vdisplay;
                }
                m_pixelSize = 1;
                ApplyVerticalTiming(m_mode, m_height, 1000000); // clamped to max fps
            } else {
                if (m_width == 0) {
                    m_width = m_mode.hdisplay;
                }
                if (m_height == 0) {
                    m_height = m_mode.vdisplay;
                }
                if (m_pixelSize == 0) {
                    int mw = m_mode.hdisplay / m_width;
                    int mh = m_mode.vdisplay / m_height;
                    m_pixelSize = std::min(mw, mh);
                    if (m_pixelSize < 1) {
                        m_pixelSize = 1;
                    }
                    m_width *= m_pixelSize;
                    m_height *= m_pixelSize;
                }
            }

            m_bpp = 24;
            uint32_t format = DRM_FORMAT_RGB888;
            for (int x = 0; x < 2; x++) {
                if (!CreateDumbBuffer(card->fd, m_width, m_height, format, m_fb[x])) {
                    format = DRM_FORMAT_XRGB8888;
                    m_bpp = 32;
                    if (!CreateDumbBuffer(card->fd, m_width, m_height, format, m_fb[x])) {
                        LogErr(VB_CHANNELOUT, "Failed to create dumb buffer\n");
                        // Free any buffer(s) already created and undo the
                        // connector/CRTC reservations pushed above so we don't
                        // leak CMA or permanently reserve this output.
                        for (int y = 0; y < x; y++) {
                            DestroyDumbBuffer(card->fd, m_fb[y]);
                            m_pageBuffers[y] = nullptr;
                        }
                        auto& rc = card->reservedConnectors;
                        rc.erase(std::remove(rc.begin(), rc.end(), m_connectorId), rc.end());
                        auto& rcr = card->reservedCrtcs;
                        rcr.erase(std::remove(rcr.begin(), rcr.end(), m_crtcId), rcr.end());
                        m_connectorId = 0;
                        m_crtcId = 0;
                        drmModeFreeConnector(conn);
                        drmModeFreeResources(res);
                        return 0;
                    }
                }
                m_pageBuffers[x] = (uint8_t*)m_fb[x].mapped;
            }

            m_planeId = FindPlaneForCrtc(card->fd, m_crtcId, format);
            if (m_planeId) {
                card->reservedPlanes.push_back(m_planeId);
            }
            m_pages = 2;

            m_cPage = 0;
            m_pPage = 0;
            m_rowStride = m_fb[0].stride;
            m_rowPadding = m_rowStride - (m_width * m_bpp / 8);
            m_pageSize = m_fb[0].size;
            m_bufferSize = m_pageSize;

            if (m_planeId) {
                drmModeSetPlane(m_cardFd, m_planeId, m_crtcId, m_fb[0].fb_id, 0,
                                0, 0, m_mode.hdisplay, m_mode.vdisplay,
                                0, 0, (uint32_t)m_width << 16, (uint32_t)m_height << 16);
            }

            drmModeFreeConnector(conn);
            drmModeFreeResources(res);

            {
                std::unique_lock<std::mutex> lock(drmMasterLock);
                m_sinkPresent = true;
                m_hotplugReady = true;
            }
            return 1;
        }
        drmModeFreeResources(res);
    }
    return 0;
}

void KMSFrameBuffer::DestroyFrameBuffer(void) {
    {
        std::unique_lock<std::mutex> lock(drmMasterLock);
        m_hotplugReady = false;
    }
    for (int x = 0; x < 3; x++) {
        if (m_fb[x].handle) {
            DestroyDumbBuffer(m_cardFd, m_fb[x]);
            m_pageBuffers[x] = nullptr;
        }
    }
    if (m_cardInfo) {
        if (m_planeId) {
            auto& planes = m_cardInfo->reservedPlanes;
            planes.erase(std::remove(planes.begin(), planes.end(), m_planeId), planes.end());
        }
        if (m_crtcId) {
            auto& crtcs = m_cardInfo->reservedCrtcs;
            crtcs.erase(std::remove(crtcs.begin(), crtcs.end(), m_crtcId), crtcs.end());
        }
        if (m_connectorId) {
            auto& conns = m_cardInfo->reservedConnectors;
            conns.erase(std::remove(conns.begin(), conns.end(), m_connectorId), conns.end());
        }
    }

    m_connectorId = 0;
    m_crtcId = 0;
    m_planeId = 0;
    m_cardInfo = nullptr;
    FrameBuffer::DestroyFrameBuffer();
}

void KMSFrameBuffer::SyncLoop() {
    if (m_pages == 1) {
        return;
    }

    while (m_runLoop) {
        // Allow the producer to catch up if needed
        if (!m_dirtyPages[m_cPage] && m_dirtyPages[(m_cPage + 1) % m_pages])
            NextPage();

        if (m_dirtyPages[m_cPage]) {
            // SyncDisplay() blocks on the vblank completion event, so it paces
            // this loop to the panel refresh without spinning.
            SyncDisplay(true);

            m_dirtyPages[m_cPage] = 0;
            NextPage();
        } else {
            // Nothing to flip yet.  Sleep briefly instead of busy-spinning so an
            // idle output doesn't peg a CPU core (the original reason AUTO_SYNC
            // was abandoned).
            std::this_thread::sleep_for(std::chrono::microseconds(250));
        }
    }
}

// Whether media playback is starting up or showing on this connector, so this
// output must leave it alone.  Read under mediaOutputLock and released at once:
// the DRM work that follows holds drmMasterLock, can wait most of a refresh,
// and with mediaOutputLock held that long it stalled MultiSync position
// updates and status requests on every frame.
static bool MediaHoldsConnector(const std::string& connector, bool includeLoading) {
    std::unique_lock<std::mutex> lock(mediaOutputLock);
    if (includeLoading && mediaOutputStatus.mediaLoading) {
        return true;
    }
    return mediaOutputStatus.output == connector;
}

void KMSFrameBuffer::PageFlipHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void* data) {
    // data is the flip token of the instance that queued this flip, so even if
    // we drain an event that belongs to another KMSFrameBuffer sharing the same
    // card fd, the correct instance's pending flag is cleared. Runs inside
    // drmHandleEvent under drmMasterLock, which also guards FLIP_TARGETS.
    uint32_t token = (uint32_t)(uintptr_t)data;
    auto it = FLIP_TARGETS.find(token);
    if (it == FLIP_TARGETS.end()) {
        LogWarn(VB_CHANNELOUT, "KMSFrameBuffer: dropped the flip event of a deleted framebuffer (token %u)\n", token);
        return;
    }
    KMSFrameBuffer* self = it->second;
    {
        self->m_flipPending = false;
        // "frame" is the vblank sequence number at flip completion; track the
        // cadence.  Runs inside drmHandleEvent under drmMasterLock.
        if (self->m_lastFlipSeq != 0 && frame > self->m_lastFlipSeq) {
            uint32_t delta = frame - self->m_lastFlipSeq;
            self->m_vblankDeltaCounts[delta > 4 ? 4 : delta]++;
            if (self->m_prevVblankDelta != 0 && delta != self->m_prevVblankDelta) {
                self->m_vblankCadenceBreaks++;
            }
            self->m_prevVblankDelta = delta;
        }
        self->m_lastFlipSeq = frame;
        self->m_lastFlipTimeUS = (uint64_t)sec * 1000000ULL + usec;
    }
}

void KMSFrameBuffer::WaitForPendingFlip(int timeoutMs) {
    if (!m_flipPending) {
        return;
    }
    struct pollfd pfd = {};
    pfd.fd = m_cardFd;
    pfd.events = POLLIN;

    drmEventContext evctx = {};
    evctx.version = 2;
    evctx.page_flip_handler = &KMSFrameBuffer::PageFlipHandler;

    // Normally the vblank event is already queued (or arrives within one refresh
    // interval) so poll() returns almost immediately.  The timeout is only a
    // safety net so a dropped/lost event can never wedge the output thread.
    int r = poll(&pfd, 1, timeoutMs);
    if (r > 0 && (pfd.revents & POLLIN)) {
        drmHandleEvent(m_cardFd, &evctx);
    } else {
        m_flipWaitTimeouts++;
    }
    // If we timed out (or errored) give up on this flip so the next frame can
    // proceed rather than blocking forever.
    m_flipPending = false;
}

void KMSFrameBuffer::WaitForPageFree() {
    if (m_pages == 1) {
        return;
    }
    std::unique_lock<std::mutex> lock(drmMasterLock);
    if (m_flipPending) {
        int vr = m_mode.vrefresh > 0 ? m_mode.vrefresh : 60;
        uint32_t timeoutsBefore = m_flipWaitTimeouts;
        WaitForPendingFlip((1000 / vr) + 20);
        if (m_flipWaitTimeouts != timeoutsBefore) {
            // The completion event never came, so it is unknown whether the
            // hardware has switched pages yet.  A queued flip lands at the next
            // vblank at the latest, so wait one out before the caller writes
            // into the page it is about to leave.  Ask for the vblank as an
            // event on OUR crtc (the pipe bits select it; without them this
            // would be pipe 0, which on a Pi 4 with HDMI up is not the DPI) and
            // poll for it with the same bound as the flip wait, so a display
            // whose vblanks have stopped cannot wedge the output thread here.
            drmVBlank vbl = {};
            unsigned int type = DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT;
            if (m_crtcPipe > 0) {
                type |= (unsigned int)((m_crtcPipe << _DRM_VBLANK_HIGH_CRTC_SHIFT) & _DRM_VBLANK_HIGH_CRTC_MASK);
            }
            vbl.request.type = (drmVBlankSeqType)type;
            vbl.request.sequence = 1;
            vbl.request.signal = 0;
            if (drmWaitVBlank(m_cardFd, &vbl) == 0) {
                struct pollfd pfd = {};
                pfd.fd = m_cardFd;
                pfd.events = POLLIN;
                if (poll(&pfd, 1, (1000 / vr) + 20) > 0 && (pfd.revents & POLLIN)) {
                    drmEventContext evctx = {};
                    evctx.version = 2;
                    evctx.page_flip_handler = &KMSFrameBuffer::PageFlipHandler;
                    evctx.vblank_handler = &KMSFrameBuffer::VBlankHandler;
                    drmHandleEvent(m_cardFd, &evctx);
                }
            }
        }
    }
}

void KMSFrameBuffer::VBlankHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void* data) {
    // Nothing to record; the wait in WaitForPageFree() only needs the event
    // to have been delivered.
}

void KMSFrameBuffer::SyncDisplay(bool pageChanged) {
    if (!pageChanged || m_pages == 1)
        return;

    if (MediaHoldsConnector(m_connectorName, true)) {
        return;
    }
    std::unique_lock<std::mutex> lock(drmMasterLock);
    {
        long long syncStart = GetTimeMS();
        int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
        if (im == 0) {
            // If the previous flip hasn't retired yet, wait for its vblank
            // completion event instead of letting this flip return EBUSY and
            // fall into the expensive SetPlane/SetCrtc modeset path.  poll()
            // sleeps until the flip completes, so this paces us to the panel's
            // refresh with no busy-waiting.
            if (m_flipPending) {
                int vr = m_mode.vrefresh > 0 ? m_mode.vrefresh : 60;
                int timeoutMs = (1000 / vr) + 20; // one refresh interval + slack
                WaitForPendingFlip(timeoutMs);
            }

            // Phase alignment: fppd's software frame clock and the panel's
            // vblank clock are never exactly the same rate, so the submission
            // time slowly drifts across the vblank grid.  When it straddles a
            // boundary, scheduling jitter makes alternate frames catch/miss
            // the vblank and frames display for e.g. 25ms/75ms instead of a
            // steady 50ms - a visible judder burst until the drift clears the
            // boundary (confirmed via the vblank-delta counters: 1,3,1,3
            // bursts exactly at observed stutters).  A "nudge only near the
            // boundary" zone just moves the discontinuity to the zone edge
            // (confirmed in testing: crossings leaked at the edge while the
            // phase transited the zone), so instead align EVERY submission to
            // the middle of the vblank interval - scheduling jitter can never
            // reach a boundary from there, and the clock drift shows up as
            // this sleep slowly varying.  When the sleep wraps, the
            // unavoidable one-vblank phase slip happens once, cleanly, per
            // beat cycle instead of oscillating.  Align unconditionally:
            // gating this on "slack since the last completion" feeds back on
            // itself (an aligned flip completes later in the cycle, the gate
            // then skips the next frame, which lands early - producing a
            // permanent 1,3,1,3 oscillation; confirmed in testing).  The
            // completion timestamps are vblank-quantized, so aligning every
            // frame pins submissions to the true grid midpoint and is stable
            // at any frame rate at or below the refresh rate.
            long long alignSleepUS = 0;
            if (m_lastFlipTimeUS != 0 && m_mode.vrefresh > 0) {
                struct timespec tsm;
                clock_gettime(CLOCK_MONOTONIC, &tsm);
                uint64_t nowUS = (uint64_t)tsm.tv_sec * 1000000ULL + tsm.tv_nsec / 1000;
                uint64_t periodUS = 1000000ULL / m_mode.vrefresh;
                // The 60s cap also skips alignment if the driver reported
                // non-monotonic timestamps - elapsed would be implausible.
                if (nowUS > m_lastFlipTimeUS &&
                    (nowUS - m_lastFlipTimeUS) < 60000000ULL) {
                    uint64_t phaseUS = (nowUS - m_lastFlipTimeUS) % periodUS;
                    uint64_t sleepUS = (periodUS / 2 + periodUS - phaseUS) % periodUS;
                    // Already within 2ms past mid-interval: close enough,
                    // don't sleep a nearly full period chasing the target.
                    if (sleepUS > 0 && sleepUS < periodUS - 2000) {
                        m_phaseNudges++;
                        alignSleepUS = sleepUS;
                        std::this_thread::sleep_for(std::chrono::microseconds(sleepUS));
                    }
                }
            }

            bool wasEnabled = m_displayEnabled;
            int ret = -1;
            if (m_displayEnabled) {
                ret = drmModePageFlip(m_cardFd, m_crtcId, m_fb[m_cPage].fb_id,
                                      DRM_MODE_PAGE_FLIP_EVENT, (void*)(uintptr_t)m_flipToken);
            }
            if (ret) {
                if (wasEnabled) {
                    // Flip rejected on an already-enabled display; the modeset
                    // fallback below costs multiple vblanks.
                    m_flipRejects++;
                }
                // First flip after enable, or the flip was rejected: bring the
                // display up with a full modeset/plane commit, then queue a flip.
                if (!m_displayEnabled) {
                    ModesetLocked();
                    m_displayEnabled = true;
                } else if (m_planeId) {
                    drmModeSetPlane(m_cardFd, m_planeId, m_crtcId, m_fb[m_cPage].fb_id, 0,
                                    0, 0, m_mode.hdisplay, m_mode.vdisplay,
                                    0, 0, (uint32_t)m_width << 16, (uint32_t)m_height << 16);
                }
                ret = drmModePageFlip(m_cardFd, m_crtcId, m_fb[m_cPage].fb_id,
                                      DRM_MODE_PAGE_FLIP_EVENT, (void*)(uintptr_t)m_flipToken);
            }
            m_flipPending = (ret == 0);
            ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);

            // Track flip-path health.  A single sync should normally complete
            // well inside one refresh interval; count the ones that don't and
            // surface them (rate-limited) at Warn level so output stutters can
            // be correlated with (or ruled out of) the KMS flip path without
            // needing debug logging.
            long long syncEnd = GetTimeMS();
            // The phase-alignment sleep is intentional; don't count it as a
            // flip-path delay.
            int tookMS = (int)(syncEnd - syncStart) - (int)(alignSleepUS / 1000);
            if (tookMS > 20) {
                m_slowSyncs++;
                m_lastSlowSyncMS = tookMS;
                if (tookMS > m_maxSyncMS) {
                    m_maxSyncMS = tookMS;
                }
            }
            // Warn only when a new delay event has occurred since the last
            // warning (rate-limited), so each warning line timestamps actual
            // trouble rather than repeating stale totals.
            uint64_t eventTotal = (uint64_t)m_slowSyncs + m_flipWaitTimeouts + m_flipRejects + m_vblankCadenceBreaks;
            if (eventTotal != m_warnedEventTotal &&
                (syncEnd - m_lastFlipWarnMS) > 10000) {
                m_lastFlipWarnMS = syncEnd;
                m_warnedEventTotal = eventTotal;
                LogWarn(VB_CHANNELOUT, "KMSFrameBuffer %s flip-path event; totals since start: %u syncs >20ms (max %dms, last %dms), %u wait timeouts, %u flip rejects, %u vblank cadence breaks (deltas 1/2/3/4+: %u/%u/%u/%u), %u phase nudges\n",
                        m_connectorName.c_str(), m_slowSyncs, m_maxSyncMS, m_lastSlowSyncMS, m_flipWaitTimeouts, m_flipRejects,
                        m_vblankCadenceBreaks,
                        m_vblankDeltaCounts[1], m_vblankDeltaCounts[2], m_vblankDeltaCounts[3], m_vblankDeltaCounts[4],
                        m_phaseNudges);
            }
        }
    }
}

void KMSFrameBuffer::DisableDisplay() {
    if (MediaHoldsConnector(m_connectorName, true)) {
        return;
    }
    std::unique_lock<std::mutex> lock(drmMasterLock);
    {
        int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
        if (im == 0) {
            if (m_crtcId && m_planeId) {
                drmModeSetPlane(m_cardFd, m_planeId, m_crtcId, m_fb[m_cPage].fb_id, 0,
                                0, 0, 0, 0, 0, 0, 0, 0);
                drmModeSetCrtc(m_cardFd, m_crtcId, 0, 0, 0, nullptr, 0, nullptr);
                m_displayEnabled = false;
                m_flipPending = false;
            }
            ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
        }
    }
}

int KMSFrameBuffer::ModesetLocked() {
    if (!m_planeId) {
        return drmModeSetCrtc(m_cardFd, m_crtcId, m_fb[m_cPage].fb_id, 0, 0,
                              &m_connectorId, 1, &m_mode);
    }

    // Legacy SetCrtc scans the CRTC's primary plane out at the full mode size,
    // so it rejects (ENOSPC) a framebuffer smaller than the mode - and ours is
    // only the model times its pixel size, which is often short of the panel.
    // Without this, a CRTC that went off could never be turned back on: every
    // earlier "modeset" here failed and only worked because the console had
    // left the CRTC running.  Light it with a blank mode-sized buffer instead,
    // put the picture on our (scaling) plane, then take the blank one away.
    DumbBuffer blank;
    bool useBlank = m_fb[m_cPage].width < m_mode.hdisplay || m_fb[m_cPage].height < m_mode.vdisplay;
    if (useBlank && !CreateDumbBuffer(m_cardFd, m_mode.hdisplay, m_mode.vdisplay, DRM_FORMAT_RGB565, blank) &&
        !CreateDumbBuffer(m_cardFd, m_mode.hdisplay, m_mode.vdisplay, DRM_FORMAT_XRGB8888, blank)) {
        useBlank = false;
    }
    if (useBlank) {
        memset(blank.mapped, 0, blank.size);
    }
    int ret = drmModeSetCrtc(m_cardFd, m_crtcId, useBlank ? blank.fb_id : m_fb[m_cPage].fb_id, 0, 0,
                             &m_connectorId, 1, &m_mode);
    if (ret) {
        // Still place the plane: that is enough when the CRTC is already
        // running this mode, which is all the old code ever managed.
        LogWarn(VB_CHANNELOUT, "KMSFrameBuffer %s: modeset failed: %d (%s)\n", m_connectorName.c_str(), ret, strerror(errno));
    }
    ret = drmModeSetPlane(m_cardFd, m_planeId, m_crtcId, m_fb[m_cPage].fb_id, 0,
                          0, 0, m_mode.hdisplay, m_mode.vdisplay,
                          0, 0, (uint32_t)m_width << 16, (uint32_t)m_height << 16);
    if (useBlank) {
        // Unless our plane is the primary (and has just replaced it), the blank
        // buffer is still on the primary plane.  Take it off explicitly: removing
        // an in-use framebuffer can make the kernel turn the whole CRTC off.
        drmModePlaneResPtr planes = drmModeGetPlaneResources(m_cardFd);
        for (uint32_t i = 0; planes && i < planes->count_planes; i++) {
            drmModePlanePtr plane = drmModeGetPlane(m_cardFd, planes->planes[i]);
            if (plane && plane->fb_id == blank.fb_id) {
                drmModeSetPlane(m_cardFd, plane->plane_id, m_crtcId, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            }
            if (plane) {
                drmModeFreePlane(plane);
            }
        }
        if (planes) {
            drmModeFreePlaneResources(planes);
        }
        DestroyDumbBuffer(m_cardFd, blank);
    }
    return ret;
}

void KMSFrameBuffer::EnableDisplay() {
    LogDebug(VB_CHANNELOUT, "KMSFrameBuffer::EnableDisplay() called for %s\n", m_connectorName.c_str());
    if (MediaHoldsConnector(m_connectorName, true)) {
        LogDebug(VB_CHANNELOUT, "  Skipping: media is loading or playing on this connector\n");
        return;
    }
    std::unique_lock<std::mutex> lock(drmMasterLock);
    {
        int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
        if (im == 0) {
            if (m_crtcId && m_planeId) {
                int ret = ModesetLocked();
                if (ret) {
                    LogErr(VB_CHANNELOUT, "KMSFrameBuffer::EnableDisplay set_plane failed: %d\n", ret);
                } else {
                    m_displayEnabled = true;
                    LogInfo(VB_CHANNELOUT, "KMSFrameBuffer::EnableDisplay: SUCCESS for %s\n", m_connectorName.c_str());
                }
            }
            ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
        } else {
            LogWarn(VB_CHANNELOUT, "KMSFrameBuffer::EnableDisplay: Failed to get DRM master (ioctl returned %d)\n", im);
        }
    }
}

// Minimum vertical blanking (scanlines) to keep between vactive and vtotal.  The
// RP1 DPI generates a low signal on the data pins during blanking, so this also
// contributes to the WS281x reset gap; the caller (DPIPixelsOutput) additionally
// pads vactive with blank scanlines so the reset is guaranteed regardless.
static constexpr int KMS_MIN_VBLANK_LINES = 3;

void KMSFrameBuffer::ApplyVerticalTiming(drmModeModeInfo& mode, int vactive, int fps) {
    if (mode.htotal == 0) {
        return;
    }
    int minVtotal = vactive + KMS_MIN_VBLANK_LINES;
    int vtotal = minVtotal;
    if (fps > 0) {
        // pixel clock is kHz; vtotal = clockHz / (htotal * fps)
        long long v = llround((double)mode.clock * 1000.0 / ((double)mode.htotal * (double)fps));
        if (v > minVtotal) {
            vtotal = (int)v;
        }
    }
    mode.vdisplay = vactive;
    mode.vsync_start = vactive + 1;
    mode.vsync_end = vactive + 2;
    mode.vtotal = vtotal;
    mode.vrefresh = (uint32_t)llround((double)mode.clock * 1000.0 / ((double)mode.htotal * (double)vtotal));
    snprintf(mode.name, sizeof(mode.name), "%dx%d", mode.hdisplay, vactive);
}

int KMSFrameBuffer::GetMaxRefreshRate() {
    if (!m_variableRefresh || m_mode.htotal == 0) {
        return 0;
    }
    int minVtotal = m_mode.vdisplay + KMS_MIN_VBLANK_LINES;
    return (int)((double)m_mode.clock * 1000.0 / ((double)m_mode.htotal * (double)minVtotal));
}

bool KMSFrameBuffer::SetRefreshRate(int fps) {
    if (!m_variableRefresh || fps <= 0 || m_mode.htotal == 0) {
        return false;
    }

    int minVtotal = m_mode.vdisplay + KMS_MIN_VBLANK_LINES;
    long long v = llround((double)m_mode.clock * 1000.0 / ((double)m_mode.htotal * (double)fps));
    int newVtotal = (v > minVtotal) ? (int)v : minVtotal;
    if (newVtotal == m_mode.vtotal) {
        return true; // already at this rate
    }

    // Only the vertical blanking changes; horizontal timing and the pixel clock
    // (and therefore the WS bit timing and the framebuffer layout) are untouched.
    // Built in a copy: m_mode is what the CRTC is running, which the check
    // above and the flip timeouts depend on.  Updating it before the modeset
    // happened made a refused attempt look applied - the next call returned
    // "already at this rate" and the new rate was never set.
    drmModeModeInfo mode = m_mode;
    mode.vsync_start = mode.vdisplay + 1;
    mode.vsync_end = mode.vdisplay + 2;
    mode.vtotal = newVtotal;
    mode.vrefresh = (uint32_t)llround((double)mode.clock * 1000.0 / ((double)mode.htotal * (double)newVtotal));

    if (MediaHoldsConnector(m_connectorName, false)) {
        // The connector is currently presenting media playback; don't fight it.
        // Report "not applied" so the caller's idea of the live rate stays
        // honest and it asks again next frame.
        return false;
    }
    std::unique_lock<std::mutex> lock(drmMasterLock);
    int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
    if (im != 0) {
        // Couldn't grab master right now.  As above, not applied; the caller
        // asks again.
        return false;
    }
    // Drain any in-flight flip so the modeset starts from a known state.
    if (m_flipPending) {
        int vr = m_mode.vrefresh > 0 ? m_mode.vrefresh : 60;
        WaitForPendingFlip((1000 / vr) + 20);
    }
    int ret = drmModeSetCrtc(m_cardFd, m_crtcId, m_fb[m_cPage].fb_id, 0, 0,
                             &m_connectorId, 1, &mode);
    if (ret == 0) {
        m_mode = mode;
        m_displayEnabled = true;
        if (m_planeId) {
            drmModeSetPlane(m_cardFd, m_planeId, m_crtcId, m_fb[m_cPage].fb_id, 0,
                            0, 0, m_mode.hdisplay, m_mode.vdisplay,
                            0, 0, (uint32_t)m_width << 16, (uint32_t)m_height << 16);
        }
        // A mode change restarts the vblank stream; clear the cadence trackers so
        // the transition isn't miscounted as a stutter.
        m_flipPending = false;
        m_lastFlipSeq = 0;
        m_prevVblankDelta = 0;
        LogInfo(VB_CHANNELOUT, "KMSFrameBuffer %s: refresh set to %dHz (vtotal=%d)\n",
                m_connectorName.c_str(), m_mode.vrefresh, newVtotal);
    } else {
        LogErr(VB_CHANNELOUT, "KMSFrameBuffer::SetRefreshRate modeset failed: %d (%s)\n", ret, strerror(errno));
    }
    ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
    return ret == 0;
}

// ---------------------------------------------------------------------------
// Display hotplug
// ---------------------------------------------------------------------------

// Quiet time after the last uevent before acting on a batch.  A sink powering
// up can bounce HPD a few times, and acting once on the settled state avoids a
// modeset per bounce.
static constexpr int HOTPLUG_SETTLE_MS = 1000;

static std::mutex s_hotplugLock; // guards the three below
static std::thread* s_hotplugThread = nullptr;
static int s_hotplugSock = -1;
static int s_hotplugWakeFd = -1;

void KMSFrameBuffer::StartHotplugMonitor() {
    std::unique_lock<std::mutex> lock(s_hotplugLock);
    if (s_hotplugThread) {
        return;
    }
    // The kernel's own uevent multicast group (1), not udevd's re-broadcast:
    // it needs no udevd and no libudev, and fppd runs as root.
    int sock = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, NETLINK_KOBJECT_UEVENT);
    if (sock < 0) {
        LogWarn(VB_CHANNELOUT, "KMSFrameBuffer: no uevent socket, display hotplug will not be handled: %s\n", strerror(errno));
        return;
    }
    struct sockaddr_nl addr = {};
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = 1;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LogWarn(VB_CHANNELOUT, "KMSFrameBuffer: could not bind the uevent socket, display hotplug will not be handled: %s\n", strerror(errno));
        close(sock);
        return;
    }
    int wakeFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wakeFd < 0) {
        close(sock);
        return;
    }
    s_hotplugSock = sock;
    s_hotplugWakeFd = wakeFd;
    s_hotplugThread = new std::thread(HotplugMonitorLoop, sock, wakeFd);
}

void KMSFrameBuffer::StopHotplugMonitor() {
    std::unique_lock<std::mutex> lock(s_hotplugLock);
    if (!s_hotplugThread) {
        return;
    }
    uint64_t one = 1;
    write(s_hotplugWakeFd, &one, sizeof(one));
    s_hotplugThread->join();
    delete s_hotplugThread;
    s_hotplugThread = nullptr;
    close(s_hotplugSock);
    close(s_hotplugWakeFd);
    s_hotplugSock = -1;
    s_hotplugWakeFd = -1;
}

void KMSFrameBuffer::HotplugMonitorLoop(int sock, int wakeFd) {
    SetThreadName("FPP-DRMHotplug");

    // card path ("/dev/dri/card1") -> connector ids named by the batch's
    // events; 0 for an event that named none (a sysfs status write, polled
    // connectors).  Any event re-probes every output on its card.
    std::map<std::string, std::set<uint32_t>> pending;
    long long deadline = 0;
    char buf[8192];

    while (true) {
        struct pollfd pfds[2] = {};
        pfds[0].fd = sock;
        pfds[0].events = POLLIN;
        pfds[1].fd = wakeFd;
        pfds[1].events = POLLIN;
        int timeout = -1;
        if (deadline) {
            timeout = (int)std::max(0LL, deadline - GetTimeMS());
        }
        int r = poll(pfds, 2, timeout);
        if (r < 0 && errno != EINTR) {
            LogWarn(VB_CHANNELOUT, "KMSFrameBuffer: uevent poll failed, display hotplug will no longer be handled: %s\n", strerror(errno));
            return;
        }
        if (pfds[1].revents & POLLIN) {
            return;
        }
        if (pfds[0].revents & POLLIN) {
            ssize_t len;
            while ((len = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
                // "change@/devices/...\0ACTION=change\0SUBSYSTEM=drm\0HOTPLUG=1\0..."
                buf[len] = 0;
                std::string subsystem, devname;
                bool hotplug = false;
                uint32_t connectorId = 0;
                for (char* p = buf; p < buf + len; p += strlen(p) + 1) {
                    if (!strncmp(p, "SUBSYSTEM=", 10)) {
                        subsystem = p + 10;
                    } else if (!strncmp(p, "DEVNAME=", 8)) {
                        devname = p + 8;
                    } else if (!strcmp(p, "HOTPLUG=1")) {
                        hotplug = true;
                    } else if (!strncmp(p, "CONNECTOR=", 10)) {
                        connectorId = (uint32_t)strtoul(p + 10, nullptr, 10);
                    }
                }
                if (subsystem != "drm" || !hotplug || devname.empty()) {
                    continue;
                }
                LogDebug(VB_CHANNELOUT, "KMSFrameBuffer: hotplug uevent on %s connector %u\n", devname.c_str(), connectorId);
                pending["/dev/" + devname].insert(connectorId);
                deadline = GetTimeMS() + HOTPLUG_SETTLE_MS;
            }
        }
        if (deadline && GetTimeMS() >= deadline) {
            for (auto& [cardPath, connectorIds] : pending) {
                HandleHotplug(cardPath, connectorIds);
            }
            pending.clear();
            deadline = 0;
        }
    }
}

void KMSFrameBuffer::HandleHotplug(const std::string& cardPath, const std::set<uint32_t>& connectorIds) {
    // Media state is read before drmMasterLock, as the other callers do, so
    // collect this card's outputs by token first.
    std::vector<std::pair<uint32_t, std::string>> outputs;
    {
        std::unique_lock<std::mutex> lock(drmMasterLock);
        for (auto& [token, fb] : FLIP_TARGETS) {
            if (fb->m_hotplugReady && fb->m_cardInfo->path == cardPath) {
                outputs.emplace_back(token, fb->m_connectorName);
            }
        }
    }
    std::map<uint32_t, bool> mediaHeld;
    for (auto& [token, name] : outputs) {
        mediaHeld[token] = MediaHoldsConnector(name, true);
    }

    std::unique_lock<std::mutex> lock(drmMasterLock);
    for (auto& [token, name] : outputs) {
        auto it = FLIP_TARGETS.find(token);
        if (it == FLIP_TARGETS.end() || !it->second->m_hotplugReady) {
            continue;
        }
        KMSFrameBuffer* fb = it->second;

        // Re-probe the connector.  Only the DRM master gets a real probe: for
        // anyone else the kernel demotes it to reading back its cached state,
        // and after a reconnect that is "connected" with no modes, because the
        // HPD interrupt updates the status but not the mode list.  So this
        // holds master - and drmMasterLock, so every output's flips wait out
        // the EDID read, once per hotplug.  The real probe also refreshes the
        // sysfs mode list that was emptied while the sink was away.
        bool master = ioctl(fb->m_cardFd, DRM_IOCTL_SET_MASTER, 0) == 0;
        drmModeConnectorPtr conn = drmModeGetConnector(fb->m_cardFd, fb->m_connectorId);
        bool present = conn && conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0;
        if (conn) {
            drmModeFreeConnector(conn);
        }
        if (master) {
            ioctl(fb->m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
        }

        bool wasPresent = fb->m_sinkPresent;
        fb->m_sinkPresent = present;
        if (!present) {
            if (wasPresent) {
                LogInfo(VB_CHANNELOUT, "KMSFrameBuffer %s: display disconnected\n", name.c_str());
            }
            continue;
        }
        // Back after we saw it gone.  An event naming this connector while it
        // looks present also counts: the sink went away and came back before
        // the batch settled, or (with force_hotplug) its EDID changed.  A
        // card-wide event alone does not: it may be about another connector,
        // and a needless modeset blanks this one (or glitches DPI pixels).
        if (wasPresent && !connectorIds.count(fb->m_connectorId)) {
            continue;
        }
        LogInfo(VB_CHANNELOUT, "KMSFrameBuffer %s: display %s\n", name.c_str(),
                wasPresent ? "re-announced itself" : "reconnected");
        if (mediaHeld[token]) {
            LogInfo(VB_CHANNELOUT, "KMSFrameBuffer %s: media owns the connector, not restarting the display\n", name.c_str());
        } else if (!fb->m_displayEnabled) {
            // Not lit yet; EnableDisplay() or SyncDisplay() will do it.
        } else {
            fb->RestartDisplayLocked();
        }
    }
}

void KMSFrameBuffer::RestartDisplayLocked() {
    int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
    if (im != 0) {
        LogWarn(VB_CHANNELOUT, "KMSFrameBuffer %s: could not get DRM master to restart the display\n", m_connectorName.c_str());
        return;
    }
    if (m_flipPending) {
        int vr = m_mode.vrefresh > 0 ? m_mode.vrefresh : 60;
        WaitForPendingFlip((1000 / vr) + 20);
    }
    // Off first.  SetCrtc with the mode the CRTC already runs is not a modeset
    // to the atomic helpers - it just swaps the framebuffer - and it is the
    // modeset that restarts the HDMI link the sink has to lock onto.
    if (m_planeId) {
        drmModeSetPlane(m_cardFd, m_planeId, m_crtcId, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    drmModeSetCrtc(m_cardFd, m_crtcId, 0, 0, 0, nullptr, 0, nullptr);

    int ret = ModesetLocked();
    if (ret == 0) {
        LogInfo(VB_CHANNELOUT, "KMSFrameBuffer %s: display restarted at %dx%d @ %dHz\n",
                m_connectorName.c_str(), m_mode.hdisplay, m_mode.vdisplay, m_mode.vrefresh);
    } else {
        // Leave it disabled so the next SyncDisplay() tries the modeset again.
        LogWarn(VB_CHANNELOUT, "KMSFrameBuffer %s: display restart failed: %d (%s)\n",
                m_connectorName.c_str(), ret, strerror(errno));
    }
    m_displayEnabled = (ret == 0);
    // The modeset restarts the vblank stream; don't count it as a stutter.
    m_lastFlipSeq = 0;
    m_prevVblankDelta = 0;
    m_lastFlipTimeUS = 0;
    ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
}

#endif
