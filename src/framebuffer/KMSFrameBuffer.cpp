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
#include <fcntl.h>
#include <unistd.h>

#include "../common_mini.h"
#include "../log.h"
#include "../mediaoutput/mediaoutput.h"

std::atomic_int KMSFrameBuffer::FRAMEBUFFER_COUNT(0);
std::vector<CardInfo*> KMSFrameBuffer::CARDS;

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
    }
    ++FRAMEBUFFER_COUNT;
}

KMSFrameBuffer::~KMSFrameBuffer() {
    if (m_displayEnabled && m_crtcId && m_planeId) {
        LogInfo(VB_CHANNELOUT, "KMSFrameBuffer::~KMSFrameBuffer() disabling display before destruction\n");
        std::unique_lock<std::mutex> lock(mediaOutputLock);
        int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
        if (im == 0) {
            drmModeSetPlane(m_cardFd, m_planeId, m_crtcId, m_fb[m_cPage].fb_id, 0,
                            0, 0, 0, 0, 0, 0, 0, 0);
            m_displayEnabled = false;
            ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
        }
    }

    --FRAMEBUFFER_COUNT;
    if (FRAMEBUFFER_COUNT == 0) {
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
            m_connectorName = cname;
            m_cardFd = card->fd;
            m_cardInfo = card;

            card->reservedConnectors.push_back(m_connectorId);
            card->reservedCrtcs.push_back(m_crtcId);

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
            return 1;
        }
        drmModeFreeResources(res);
    }
    return 0;
}

void KMSFrameBuffer::DestroyFrameBuffer(void) {
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
    int dummy = 0;
    if (m_pages == 1) {
        return;
    }

    while (m_runLoop) {
        // Wait for vsync
        dummy = 0;
        // Allow the producer to catch up if needed
        if (!m_dirtyPages[m_cPage] && m_dirtyPages[(m_cPage + 1) % m_pages])
            NextPage();

        if (m_dirtyPages[m_cPage]) {
            SyncDisplay(true);

            m_dirtyPages[m_cPage] = 0;
            NextPage();
        }
    }
}

void KMSFrameBuffer::SyncDisplay(bool pageChanged) {
    if (!pageChanged | m_pages == 1)
        return;

    std::unique_lock<std::mutex> lock(mediaOutputLock);
    if (mediaOutputStatus.mediaLoading) {
        return;
    }
    if (mediaOutputStatus.output != m_connectorName) {
        int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
        if (im == 0) {
            int ret = drmModePageFlip(m_cardFd, m_crtcId, m_fb[m_cPage].fb_id,
                                      0, nullptr);
            if (ret) {
                if (!m_displayEnabled) {
                    drmModeSetCrtc(m_cardFd, m_crtcId, m_fb[m_cPage].fb_id, 0, 0,
                                   &m_connectorId, 1, &m_mode);
                    m_displayEnabled = true;
                }
                if (m_planeId) {
                    drmModeSetPlane(m_cardFd, m_planeId, m_crtcId, m_fb[m_cPage].fb_id, 0,
                                    0, 0, m_mode.hdisplay, m_mode.vdisplay,
                                    0, 0, (uint32_t)m_width << 16, (uint32_t)m_height << 16);
                }
                drmModePageFlip(m_cardFd, m_crtcId, m_fb[m_cPage].fb_id,
                                0, nullptr);
            }
            ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
        }
    }
}

void KMSFrameBuffer::DisableDisplay() {
    std::unique_lock<std::mutex> lock(mediaOutputLock);
    if (mediaOutputStatus.mediaLoading) {
        return;
    }
    if (mediaOutputStatus.output != m_connectorName) {
        int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
        if (im == 0) {
            if (m_crtcId && m_planeId) {
                drmModeSetPlane(m_cardFd, m_planeId, m_crtcId, m_fb[m_cPage].fb_id, 0,
                                0, 0, 0, 0, 0, 0, 0, 0);
                drmModeSetCrtc(m_cardFd, m_crtcId, 0, 0, 0, nullptr, 0, nullptr);
                m_displayEnabled = false;
            }
            ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
        }
    }
}

void KMSFrameBuffer::EnableDisplay() {
    LogDebug(VB_CHANNELOUT, "KMSFrameBuffer::EnableDisplay() called for %s\n", m_connectorName.c_str());
    std::unique_lock<std::mutex> lock(mediaOutputLock);
    if (mediaOutputStatus.mediaLoading) {
        LogDebug(VB_CHANNELOUT, "  Skipping: media is loading\n");
        return;
    }
    if (mediaOutputStatus.output != m_connectorName) {
        LogDebug(VB_CHANNELOUT, "  mediaOutputStatus.output='%s', proceeding with enable\n", mediaOutputStatus.output.c_str());
        int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
        if (im == 0) {
            if (m_crtcId && m_planeId) {
                int ret = drmModeSetCrtc(m_cardFd, m_crtcId, m_fb[m_cPage].fb_id, 0, 0,
                                         &m_connectorId, 1, &m_mode);
                if (ret) {
                    LogErr(VB_CHANNELOUT, "KMSFrameBuffer::EnableDisplay set_mode failed: %d\n", ret);
                } else {
                    LogInfo(VB_CHANNELOUT, "KMSFrameBuffer::EnableDisplay: set mode for %s\n", m_connectorName.c_str());
                }

                ret = drmModeSetPlane(m_cardFd, m_planeId, m_crtcId, m_fb[m_cPage].fb_id, 0,
                                      0, 0, m_mode.hdisplay, m_mode.vdisplay,
                                      0, 0, (uint32_t)m_width << 16, (uint32_t)m_height << 16);
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
    } else {
        LogDebug(VB_CHANNELOUT, "  Skipping: mediaOutputStatus.output matches connector\n");
    }
}

#endif
