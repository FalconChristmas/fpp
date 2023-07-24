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

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

#include "config.h"

#ifdef USE_FRAMEBUFFER_SOCKET
#include <sys/socket.h>
#include <sys/un.h>
#else
#include <linux/fb.h>
#include <linux/kd.h>
#if __has_include(<xf86drm.h>)
#include <xf86drm.h>
#include <xf86drmMode.h>
#define HAS_DRM
#endif
#endif
#ifdef USE_X11
#include <X11/Xlib.h>
#endif

typedef enum {
    IT_Random = -2,
    IT_Default = -1,
    IT_Normal = 0,
    IT_SlideUp,         // Slide up from the bottom to the top
    IT_SlideDown,       // Slide down from the top to the bottom
    IT_SlideLeft,       // Slide in from the right to the left
    IT_SlideRight,      // Slide in from the left to the right
    IT_WipeUp,          // Draw new over old by row starting from the bottom
    IT_WipeDown,        // Draw new over old by row starting from the top
    IT_WipeLeft,        // Draw new over old by column starting from the left
    IT_WipeRight,       // Draw new over old by column starting from the right
    IT_WipeToHCenter,   // Wipe down/up to center
    IT_WipeFromHCenter, // Wipe up/down from center
    IT_HorzBlindsOpen,  // Horizontal blinds opening \__Very little difference in these two
    IT_HorzBlindsClose, // Horizontal blinds closing /
    IT_Mosaic,          // Fill in small random squares until totally done
    IT_MAX,             // PLACEHOLDER - ALL ITEMS ABOVE THIS LINE ARE ENABLED, BELOW ARE DISABLED
    IT_Wipe45,          // Wipe at 45 degree angle to upper right corner
    IT_Wipe135,         // Wipe at 135 degree angle to bottom right corner
    IT_Wipe225,         // Wipe at 225 degree angle to bottom left corner
    IT_Wipe315,         // Wipe at 315 degree angle to top left corner
    IT_WipeToVCenter,   // Wipe from left/right to vert line going down center
    IT_WipeFromVCenter, // Wipe to left/right from vert line going down center
    IT_WipeInRect,      // Wipe from outside corners inward in rect shape
    IT_WipeOutRect,     // Wipe to outside corners outward in rect shape
    IT_WipeInCorners,   // Wipe 4 rectangles from corners growing inward (4 growing rectangles in corners, shrinking + sign)
    IT_WipeOutCorners,  // Wipe 4 rectangles from center shrinking outward (creates a growing + sign in middle of screen)
    IT_WipeCW,          // CW wipe from 0 degrees top dead center
    IT_WipeCCW,         // CCW wipe from 0 degrees top dead center
    IT_WipeQuadCW,      // Quad CW wipe from 0 degrees top dead center
    IT_WipeQuadCCW,     // Quad CCW wipe from 0 degrees top dead center
    IT_WipeOctoCW,      // Octo CW wipe from 0 degrees top dead center
    IT_WipeOctoCCW,     // Octo CCW wipe from 0 degrees top dead center
    IT_VertBlindsOpen,  // Vertical blinds (8 bands?)
    IT_VertBlindsClose, // Vertical blinds (8 bands?)
    IT_Fade,            // Fade from one image to the other (copy original and slowly fade each pixel)
    IT_END
} ImageTransitionType;

class FrameBuffer {
public:
    FrameBuffer();
    ~FrameBuffer();

    int FBInit(const Json::Value& config);
    void FBCopyData(const uint8_t* buffer, int draw = 0);
    void FBStartDraw(ImageTransitionType transitionType = IT_Default);

    void Dump(void);

    void DrawLoop(void);

    int Width() { return m_width; }
    int Height() { return m_height; }
    int Page(bool producer = false) { return producer ? m_pPage : m_cPage; }
    int PageCount() { return m_pages; }
    int PageSize() { return m_pageSize; }
    int RowStride() { return m_rowStride; }
    int RowPadding() { return m_rowPadding; }
    uint8_t* Buffer() { return m_buffer; }
    uint8_t* BufferPage(int page = 0) { return (m_buffer + (m_pageSize * page)); }
    int BitsPerPixel() { return m_bpp; }
    int BytesPerPixel() { return m_bpp / 8; }
    bool IsDirty(int page) { return (m_dirtyPages[page] == 1); }
    void SetDirty(int page, uint8_t dirty = 1) { m_dirtyPages[page] = dirty; }

    void ClearAllPages();
    void Clear();

    void NextPage(bool producer = false);
    void SyncDisplay(bool pageChanged = false);

private:
    void FBDrawNormal(void);
    void FBDrawSlideUp(void);
    void FBDrawSlideDown(void);
    void FBDrawSlideLeft(void);
    void FBDrawSlideRight(void);
    void FBDrawWipeUp(void);
    void FBDrawWipeDown(void);
    void FBDrawWipeLeft(void);
    void FBDrawWipeRight(void);
    void FBDrawWipeToHCenter(void);
    void FBDrawWipeFromHCenter(void);
    void FBDrawHorzBlindsOpen(void);
    void FBDrawHorzBlindsClose(void);
    void FBDrawMosaic(void);

    // Helpers
    void DrawSquare(int dx, int dy, int w, int h, int sx = -1, int sy = -1);

    int InitializeFrameBuffer(void);
    void DestroyFrameBuffer(void);
#ifdef USE_FRAMEBUFFER_SOCKET
    int InitializeFrameBufferSocket(void);
    void DestroyFrameBufferSocket(void);
#else
    int InitializeFrameBufferIOCTL(void);
    void DestroyFrameBufferIOCTL(void);
#ifdef HAS_DRM
    int InitializeFrameBufferDRM(void);
    void DestroyFrameBufferDRM(void);
#endif
#endif

    std::string m_name;
    std::string m_device;
    int m_fbFd = -1;
    int m_xOffset = -1;
    int m_yOffset = -1;
    int m_width = 0;
    int m_height = 0;
    int m_bpp = -1;
    uint8_t* m_buffer = nullptr;
    uint8_t* m_outputBuffer = nullptr;
    uint8_t* m_pageBuffers[3] = { nullptr, nullptr, nullptr };
    int m_pageSize = 0;

    int m_pixelSize = 1;
    int m_pixelsWide = 0;
    int m_pixelsHigh = 0;

    ImageTransitionType m_transitionType = IT_Normal;
    volatile ImageTransitionType m_nextTransitionType = IT_Normal;
    int m_transitionTime = 800;

    unsigned int m_typeSeed;

    volatile bool m_runLoop = false;
    volatile bool m_imageReady = false;

    std::thread* m_drawThread = nullptr;
    bool m_autoSync = false;
    volatile uint8_t* m_dirtyPages = nullptr;

    std::mutex m_bufferLock;
    std::mutex m_drawLock;

    std::condition_variable m_drawSignal;

    int m_cPage = 0;
    int m_pPage = 0;
    int m_pages = 2;
    int m_bufferSize = 0;
    int m_rowStride = 0;
    int m_rowPadding = 0;
    int m_frameSize = 0;
    uint8_t* m_lastFrame = nullptr;

    bool usingDRM = false;
#ifdef HAS_DRM
    drmModeConnectorPtr m_connector = nullptr;
    drmModeCrtcPtr m_crtc = nullptr;
    uint32_t m_mode = 0;
    uint32_t m_bufferHandles[2] = { 0, 0 };
#endif

#ifdef USE_FRAMEBUFFER_SOCKET
    void SyncLoopFBSocket();
    void sendFramebufferConfig();
    void sendFramebufferFrame();

    int shmemFile = -1;
    struct sockaddr_un dev_address;
    bool targetExists = false;
#else
    void SyncLoopFB();

    struct fb_var_screeninfo m_vInfo;
    struct fb_fix_screeninfo m_fInfo;

    int m_ttyFd = -1;

    // Support for 16-bit framebuffer output
    uint16_t*** m_rgb565map = nullptr;
#endif

#ifdef USE_X11
    int InitializeX11Window(void);
    void DestroyX11Window(void);
    void SyncLoopX11();

    Display* m_display = nullptr;
    int m_screen = 0;
    Window m_window;
    GC m_gc;
    Pixmap m_pixmap;
    XImage* m_xImage = nullptr;
#endif
};
