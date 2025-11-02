#pragma once
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

#if __has_include(<kms++/kms++.h>)
#include "FrameBuffer.h"
#include <kms++/kms++.h>
#include <kms++util/kms++util.h>
#define HAS_KMS_FB

#include <map>

class KMSFrameBuffer : public FrameBuffer {
public:
    KMSFrameBuffer();
    virtual ~KMSFrameBuffer();

    virtual int InitializeFrameBuffer() override;
    virtual void DestroyFrameBuffer() override;
    virtual void SyncLoop() override;
    virtual void SyncDisplay(bool pageChanged = false) override;
    
    virtual void EnableDisplay() override;
    virtual void DisableDisplay() override;

    int m_cardFd = -1;
    kms::Connector* m_connector = nullptr;
    kms::Crtc* m_crtc = nullptr;
    kms::Videomode m_mode;
    kms::DumbFramebuffer* m_fb[3] = { nullptr, nullptr, nullptr };
    kms::Plane* m_plane = nullptr;
    kms::ResourceManager* m_resourceManager = nullptr;
    
    bool m_displayEnabled = false;  // Track if display/CRTC is intentionally enabled

    static std::atomic_int FRAMEBUFFER_COUNT;
    static std::map<kms::Card*, kms::ResourceManager*> CARDS;
};

#endif
