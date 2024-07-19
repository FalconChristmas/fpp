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

#if __has_include(<linux/fb.h>)

#define HAS_IOCTL_FB
#include <linux/fb.h>
#include <linux/kd.h>

#include "FrameBuffer.h"

class IOCTLFrameBuffer : public FrameBuffer {
public:
    IOCTLFrameBuffer();
    virtual ~IOCTLFrameBuffer();

    virtual int InitializeFrameBuffer() override;
    virtual void DestroyFrameBuffer() override;
    virtual void SyncLoop() override;
    virtual void SyncDisplay(bool pageChanged = false) override;
    virtual void FBCopyData(const uint8_t* buffer, int draw) override;

    struct fb_var_screeninfo m_vInfo;
    struct fb_fix_screeninfo m_fInfo;

    int m_ttyFd = -1;

    // Support for 16-bit framebuffer output
    uint16_t*** m_rgb565map = nullptr;
};

#endif