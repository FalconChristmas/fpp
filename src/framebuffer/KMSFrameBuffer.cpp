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
#include <sys/ioctl.h>

#include <sys/mman.h>

#include "../common_mini.h"
#include "../log.h"
#include "../mediaoutput/mediaoutput.h"

std::atomic_int KMSFrameBuffer::FRAMEBUFFER_COUNT(0);
std::map<kms::Card*, kms::ResourceManager*> KMSFrameBuffer::CARDS;

KMSFrameBuffer::KMSFrameBuffer() {
    LogDebug(VB_CHANNELOUT, "KMSFrameBuffer::KMSFrameBuffer()\n");

    if (FRAMEBUFFER_COUNT == 0) {
        for (int cn = 0; cn < 10; cn++) {
            if (FileExists("/dev/dri/card" + std::to_string(cn))) {
                kms::Card* card = new kms::Card("/dev/dri/card" + std::to_string(cn));
                CARDS[card] = new kms::ResourceManager(*card);
                card->drop_master();
            }
        }
    }
    ++FRAMEBUFFER_COUNT;
}

KMSFrameBuffer::~KMSFrameBuffer() {
    // Ensure display is disabled before destroying framebuffer
    if (m_displayEnabled && m_crtc && m_plane) {
        LogInfo(VB_CHANNELOUT, "KMSFrameBuffer::~KMSFrameBuffer() disabling display before destruction\n");
        std::unique_lock<std::mutex> lock(mediaOutputLock);
        int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
        if (im == 0) {
            try {
                m_crtc->set_plane(m_plane, *m_fb[m_cPage], 0, 0, 0, 0, 0, 0, 0, 0);
                m_crtc->disable_mode();
                m_displayEnabled = false;
            } catch (const std::exception& ex) {
                LogErr(VB_CHANNELOUT, "KMSFrameBuffer::~KMSFrameBuffer() exception during disable: %s\n", ex.what());
            }
            ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
        }
    }
    
    --FRAMEBUFFER_COUNT;
    if (FRAMEBUFFER_COUNT == 0) {
        for (auto& i : CARDS) {
            delete i.first;
            delete i.second;
        }
        CARDS.clear();
    }
}
int KMSFrameBuffer::InitializeFrameBuffer(void) {
    LogDebug(VB_CHANNELOUT, "KMSFrameBuffer::InitializeFrameBuffer()\n");

    for (auto& card : CARDS) {
        for (auto& conn : card.first->get_connectors()) {
            if (m_device == conn->fullname() && conn->connected()) {
                m_resourceManager = card.second;
                m_connector = m_resourceManager->reserve_connector(conn);
                if (m_connector == nullptr) {
                    continue;
                }
                m_crtc = m_resourceManager->reserve_crtc(conn);
                if (m_crtc == nullptr) {
                    m_crtc = m_resourceManager->reserve_crtc(m_connector);
                }
                if (m_crtc == nullptr) {
                    return 0;
                }
                m_mode = m_connector->get_default_mode();

                if (m_width == 0) {
                    m_width = m_mode.hdisplay;
                }
                if (m_height == 0) {
                    m_height = m_mode.vdisplay;
                }
                if (m_pixelSize == 0) {
                    // find a suitable pixel size to make it less "fuzzy" looking
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
                for (int x = 0; x < 2; x++) {
                    try {
                        m_fb[x] = new kms::DumbFramebuffer(*card.first, m_width, m_height, kms::PixelFormat::RGB888);
                    } catch (...) {
                        m_fb[x] = new kms::DumbFramebuffer(*card.first, m_width, m_height, kms::PixelFormat::XRGB8888);
                        m_bpp = 32;
                    }
                    m_pageBuffers[x] = m_fb[x]->map(0);
                }
                m_plane = m_resourceManager->reserve_generic_plane(m_crtc, m_fb[0]->format());
                m_pages = 2;

                m_cPage = 0;
                m_pPage = 0;
                m_rowStride = m_fb[0]->stride(0);
                m_rowPadding = m_rowStride - (m_width * m_bpp / 8);
                m_pageSize = m_fb[0]->size(0);
                m_bufferSize = m_pageSize;
                
                // Initialize display as disabled - it will be enabled when needed
                m_displayEnabled = false;
                
                // Disable CRTC/plane from previous session to prevent old data output
                int im = ioctl(card.first->fd(), DRM_IOCTL_SET_MASTER, 0);
                if (im == 0) {
                    try {
                        m_crtc->set_plane(m_plane, *m_fb[0], 0, 0, 0, 0, 0, 0, 0, 0);
                        m_crtc->disable_mode();
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    } catch (...) {
                    }
                    ioctl(card.first->fd(), DRM_IOCTL_DROP_MASTER, 0);
                }
                
                m_cardFd = card.first->fd();
                
                // Clear framebuffer pages
                for (int x = 0; x < 2; x++) {
                    memset(m_pageBuffers[x], 0, m_pageSize);
                }
                
                m_cardFd = card.first->fd();
                return 1;
            }
        }
    }
    return 0;
}

void KMSFrameBuffer::DestroyFrameBuffer(void) {
    for (int x = 0; x < 3; x++) {
        if (m_fb[x]) {
            delete m_fb[x];
            m_pageBuffers[x] = nullptr;
            m_fb[x] = nullptr;
        }
    }
    if (m_plane) {
        m_resourceManager->release_plane(m_plane);
    }
    if (m_crtc) {
        m_resourceManager->release_crtc(m_crtc);
    }
    if (m_connector) {
        m_resourceManager->release_connector(m_connector);
    }

    m_connector = nullptr;
    m_crtc = nullptr;
    m_plane = nullptr;
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
        // LogInfo(VB_CHANNELOUT, "%s - %d - waiting for vsync\n", m_device.c_str(), m_cPage);
        // Allow the producer to catch up if needed
        if (!m_dirtyPages[m_cPage] && m_dirtyPages[(m_cPage + 1) % m_pages])
            NextPage();

        if (m_dirtyPages[m_cPage]) {
            // LogInfo(VB_CHANNELOUT, "%s - %d - flipping to cPage\n", m_device.c_str(), m_cPage);

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
        // we cannot do anything to the KMS connectors while VLC is getting setup
        return;
    }
    if (mediaOutputStatus.output != m_connector->fullname()) {
        // if there isn't media being output on this connector, we can display the page
        int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
        if (im == 0) {
            int i = m_crtc->page_flip(*m_fb[m_cPage], m_pageBuffers[m_cPage]);
            if (i) {
                if (m_displayEnabled) {
                    m_crtc->set_plane(m_plane, *m_fb[m_cPage], 0, 0, m_mode.hdisplay, m_mode.vdisplay, 0, 0, m_width, m_height);
                    m_crtc->page_flip(*m_fb[m_cPage], m_pageBuffers[m_cPage]);
                }
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
    if (mediaOutputStatus.output != m_connector->fullname()) {
        int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
        if (im == 0) {
            if (m_crtc && m_plane) {
                m_crtc->set_plane(m_plane, *m_fb[m_cPage], 0, 0, 0, 0, 0, 0, 0, 0);
                try {
                    m_crtc->disable_mode();
                } catch (...) {
                }
                m_displayEnabled = false;
            }
            ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
        }
    }
}

void KMSFrameBuffer::EnableDisplay() {
    std::unique_lock<std::mutex> lock(mediaOutputLock);
    if (mediaOutputStatus.mediaLoading) {
        return;
    }
    if (mediaOutputStatus.output != m_connector->fullname()) {
        int im = ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
        if (im == 0) {
            if (m_crtc && m_plane) {
                try {
                    m_crtc->set_mode(m_connector, m_mode);
                } catch (const std::exception& ex) {
                    LogErr(VB_CHANNELOUT, "KMSFrameBuffer::EnableDisplay set_mode exception: %s\n", ex.what());
                }
                try {
                    m_crtc->set_plane(m_plane, *m_fb[m_cPage], 0, 0, m_mode.hdisplay, m_mode.vdisplay, 0, 0, m_width, m_height);
                    m_displayEnabled = true;
                } catch (const std::exception& ex) {
                    LogErr(VB_CHANNELOUT, "KMSFrameBuffer::EnableDisplay set_plane exception: %s\n", ex.what());
                }
            }
            ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
        }
    }
}

#endif
