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

#include <sys/mman.h>

std::atomic_int KMSFrameBuffer::FRAMEBUFFER_COUNT(0);
kms::Card* KMSFrameBuffer::CARD = nullptr;
kms::ResourceManager* KMSFrameBuffer::RESOURCE_MANAGER = nullptr;

KMSFrameBuffer::KMSFrameBuffer() {
    LogDebug(VB_CHANNELOUT, "KMSFrameBuffer::KMSFrameBuffer()\n");

    if (FRAMEBUFFER_COUNT == 0) {
        CARD = new kms::Card();
        RESOURCE_MANAGER = new kms::ResourceManager(*CARD);
    }
    ++FRAMEBUFFER_COUNT;
}

KMSFrameBuffer::~KMSFrameBuffer() {
    --FRAMEBUFFER_COUNT;
    if (FRAMEBUFFER_COUNT == 0) {
        delete RESOURCE_MANAGER;
        RESOURCE_MANAGER = nullptr;
        delete CARD;
        CARD = nullptr;
    }
}
int KMSFrameBuffer::InitializeFrameBuffer(void) {
    LogDebug(VB_CHANNELOUT, "KMSFrameBuffer::InitializeFrameBuffer()\n");
    if (!CARD->has_kms() || !CARD->has_atomic()) {
        return 0;
    }
    for (auto& conn : CARD->get_connectors()) {
        if (m_device == conn->fullname() && conn->connected()) {
            m_connector = RESOURCE_MANAGER->reserve_connector(conn);
            m_crtc = RESOURCE_MANAGER->reserve_crtc(conn);
            if (m_crtc == nullptr) {
                m_crtc = RESOURCE_MANAGER->reserve_crtc(m_connector);
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

            for (int x = 0; x < 2; x++) {
                m_fb[x] = new kms::DumbFramebuffer(*CARD, m_mode.hdisplay, m_mode.vdisplay, kms::PixelFormat::RGB888);
                m_pageBuffers[x] = m_fb[x]->map(0);
            }
            m_plane = RESOURCE_MANAGER->reserve_generic_plane(m_crtc, m_fb[0]->format());
            m_pages = 2;

            if (m_width == 0) {
                m_width = m_mode.hdisplay;
            }
            if (m_height == 0) {
                m_height = m_mode.vdisplay;
            }
            m_cPage = 0;
            m_pPage = 0;
            m_bpp = 24;
            m_rowStride = m_fb[0]->stride(0);
            m_rowPadding = m_rowStride - (m_width * m_bpp / 8);
            m_pageSize = m_fb[0]->size(0);
            m_bufferSize = m_pageSize;
            m_crtc->set_plane(m_plane, *m_fb[0], 0, 0, m_mode.hdisplay, m_mode.vdisplay, 0, 0, m_width, m_height);
            return 1;
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
        RESOURCE_MANAGER->release_plane(m_plane);
    }
    if (m_crtc) {
        RESOURCE_MANAGER->release_crtc(m_crtc);
    }
    if (m_connector) {
        RESOURCE_MANAGER->release_connector(m_connector);
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
    if (CARD->is_master()) {
        CARD->drop_master();
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

    m_crtc->page_flip(*m_fb[m_cPage], m_pageBuffers[m_cPage]);
    if (CARD->is_master()) {
        CARD->drop_master();
    }
}

#endif