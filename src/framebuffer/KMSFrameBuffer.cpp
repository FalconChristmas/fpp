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
#include <sys/ioctl.h>
#include <drm/drm.h>

#include <sys/mman.h>

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
                m_crtc = m_resourceManager->reserve_crtc(conn);
                if (m_crtc == nullptr) {
                    m_crtc = m_resourceManager->reserve_crtc(m_connector);
                }
                if (m_crtc == nullptr) {
                    return 0;
                }
                bool foundMode = false;
                bool foundMultiMode = false;
                kms::Videomode multiMode;

                for (auto& m : m_connector->get_modes()) {
                    if (m.hdisplay >= m_width && m.vdisplay >= m_height) {
                        m_mode = m;
                        foundMode = true;
                    }
                    if (m_width > 0 && m_height > 0 && ((m.hdisplay % m_width) == 0) && ((m.vdisplay % m_height) == 0)) {
                        foundMultiMode = true;
                        multiMode = m;
                    }
                }
                if (!foundMode) {
                    return 0;
                }
                if (foundMultiMode) {
                    m_mode = multiMode;
                }
                LogDebug(VB_CHANNELOUT, "KMSFrameBuffer:   Connector: %s   Mode: %dx%d\n", conn->fullname().c_str(), m_mode.hdisplay, m_mode.vdisplay);
                m_crtc->set_mode(m_connector, m_mode);

                m_bpp = 24;
                for (int x = 0; x < 2; x++) {
                    try {
                        m_fb[x] = new kms::DumbFramebuffer(*card.first, m_mode.hdisplay, m_mode.vdisplay, kms::PixelFormat::RGB888);
                    } catch (...) {
                        m_fb[x] = new kms::DumbFramebuffer(*card.first, m_mode.hdisplay, m_mode.vdisplay, kms::PixelFormat::XRGB8888);
                        m_bpp = 32;
                    }
                    m_pageBuffers[x] = m_fb[x]->map(0);
                }
                m_plane = m_resourceManager->reserve_generic_plane(m_crtc, m_fb[0]->format());
                m_pages = 2;

                if (m_width == 0) {
                    m_width = m_mode.hdisplay;
                }
                if (m_height == 0) {
                    m_height = m_mode.vdisplay;
                }
                m_cPage = 0;
                m_pPage = 0;
                m_rowStride = m_fb[0]->stride(0);
                m_rowPadding = m_rowStride - (m_width * m_bpp / 8);
                m_pageSize = m_fb[0]->size(0);
                m_bufferSize = m_pageSize;
                m_crtc->set_plane(m_plane, *m_fb[0], 0, 0, m_mode.hdisplay, m_mode.vdisplay, 0, 0, m_width, m_height);
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

    ioctl(m_cardFd, DRM_IOCTL_SET_MASTER, 0);
    int i = m_crtc->page_flip(*m_fb[m_cPage], m_pageBuffers[m_cPage]);
    if (i) {
        m_crtc->set_plane(m_plane, *m_fb[m_cPage], 0, 0, m_mode.hdisplay, m_mode.vdisplay, 0, 0, m_width, m_height);
        m_crtc->page_flip(*m_fb[m_cPage], m_pageBuffers[m_cPage]);
    }
    ioctl(m_cardFd, DRM_IOCTL_DROP_MASTER, 0);
}

#endif