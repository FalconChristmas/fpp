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

#include "X11FrameBuffer.h"

#ifdef USE_X11
X11FrameBuffer::X11FrameBuffer() {
}

/*
 *
 */
X11FrameBuffer::~X11FrameBuffer() {
    DestroyFrameBuffer();
}

int X11FrameBuffer::InitializeFrameBuffer(void) {
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
    m_pageBuffers[0] = m_buffer;

    m_xImage = XCreateImage(m_display, CopyFromParent, 24, ZPixmap, 0,
                            (char*)m_buffer, m_width, m_height, 32, m_width * 4);

    XSetWindowAttributes attributes;

    attributes.background_pixel = BlackPixel(m_display, m_screen);

    XGCValues values;

    m_pixmap = XCreatePixmap(m_display, XDefaultRootWindow(m_display), m_width, m_height, 24);

    m_gc = XCreateGC(m_display, m_pixmap, 0, &values);
    if (!m_gc) {
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

void X11FrameBuffer::DestroyFrameBuffer(void) {
    if (m_display != nullptr) {
        XDestroyWindow(m_display, m_window);
        XFreePixmap(m_display, m_pixmap);
        XFreeGC(m_display, m_gc);
        XCloseDisplay(m_display);
        m_display = nullptr;
    }
    if (m_buffer) {
        delete[] m_buffer;
        m_buffer = nullptr;
    }
}

void X11FrameBuffer::SyncLoop() {
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

void X11FrameBuffer::SyncDisplay(bool pageChanged) {
    XLockDisplay(m_display);
    XPutImage(m_display, m_window, m_gc, m_xImage, 0, 0, 0, 0, m_width, m_height);
    XSync(m_display, True);
    XFlush(m_display);
    XUnlockDisplay(m_display);
}
#endif
