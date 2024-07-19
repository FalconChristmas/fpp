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

#ifdef USE_X11
#include "FrameBuffer.h"
#include <X11/Xlib.h>

class X11FrameBuffer : public FrameBuffer {
public:
    X11FrameBuffer();
    virtual ~X11FrameBuffer();

    virtual int InitializeFrameBuffer() override;
    virtual void DestroyFrameBuffer() override;
    virtual void SyncLoop() override;
    virtual void SyncDisplay(bool pageChanged = false) override;

    Display* m_display = nullptr;
    int m_screen = 0;
    Window m_window;
    GC m_gc;
    Pixmap m_pixmap;
    XImage* m_xImage = nullptr;
};

#endif
