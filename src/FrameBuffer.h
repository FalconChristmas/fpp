/*
 *   FrameBuffer Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _FRAMEBUFFER_H
#define _FRAMEBUFFER_H

#include <mutex>
#include <thread>
#include <list>
#include <atomic>
#include <condition_variable>

#include <linux/fb.h>

#ifdef USE_X11
#   include <X11/Xlib.h>
#endif

#include <jsoncpp/json/json.h>

typedef enum {
	IT_Random = -2,
	IT_Default = -1,
	IT_Normal = 0,
	IT_SlideUp,          // Slide up from the bottom to the top
	IT_SlideDown,        // Slide down from the top to the bottom
	IT_SlideLeft,        // Slide in from the right to the left
	IT_SlideRight,       // Slide in from the left to the right
	IT_WipeUp,           // Draw new over old by row starting from the bottom
	IT_WipeDown,         // Draw new over old by row starting from the top
	IT_WipeLeft,         // Draw new over old by column starting from the left
	IT_WipeRight,        // Draw new over old by column starting from the right
	IT_WipeToHCenter,    // Wipe down/up to center
	IT_WipeFromHCenter,  // Wipe up/down from center
	IT_HorzBlindsOpen,   // Horizontal blinds opening \__Very little difference in these two
	IT_HorzBlindsClose,  // Horizontal blinds closing /
	IT_Mosaic,           // Fill in small random squares until totally done
	IT_MAX,              // PLACEHOLDER - ALL ITEMS ABOVE THIS LINE ARE ENABLED, BELOW ARE DISABLED
	IT_Wipe45,           // Wipe at 45 degree angle to upper right corner
	IT_Wipe135,          // Wipe at 135 degree angle to bottom right corner
	IT_Wipe225,          // Wipe at 225 degree angle to bottom left corner
	IT_Wipe315,          // Wipe at 315 degree angle to top left corner
	IT_WipeToVCenter,    // Wipe from left/right to vert line going down center
	IT_WipeFromVCenter,  // Wipe to left/right from vert line going down center
	IT_WipeInRect,       // Wipe from outside corners inward in rect shape
	IT_WipeOutRect,      // Wipe to outside corners outward in rect shape
	IT_WipeInCorners,    // Wipe 4 rectangles from corners growing inward (4 growing rectangles in corners, shrinking + sign)
	IT_WipeOutCorners,   // Wipe 4 rectangles from center shrinking outward (creates a growing + sign in middle of screen)
	IT_WipeCW,           // CW wipe from 0 degrees top dead center
	IT_WipeCCW,          // CCW wipe from 0 degrees top dead center
	IT_WipeQuadCW,       // Quad CW wipe from 0 degrees top dead center
	IT_WipeQuadCCW,      // Quad CCW wipe from 0 degrees top dead center
	IT_WipeOctoCW,       // Octo CW wipe from 0 degrees top dead center
	IT_WipeOctoCCW,      // Octo CCW wipe from 0 degrees top dead center
	IT_VertBlindsOpen,   // Vertical blinds (8 bands?)
	IT_VertBlindsClose,  // Vertical blinds (8 bands?)
	IT_Fade,             // Fade from one image to the other (copy original and slowly fade each pixel)
	IT_END
} ImageTransitionType;


class FrameBuffer {
  public:
	FrameBuffer();
	~FrameBuffer();

	int  FBInit(Json::Value &config);
	void FBCopyData(unsigned char *buffer, int draw = 0);
	void FBStartDraw(ImageTransitionType transitionType = IT_Default);

	void Dump(void);
	void GetConfig(Json::Value &config);

	void DrawLoop(void);
	void PrepLoop(void);

	std::string    m_device;
	std::string    m_dataFormat;
	int            m_fbFd;
	int            m_ttyFd;
	int            m_fbWidth;
	int            m_fbHeight;
	int            m_bpp;
	char          *m_fbp;
	char          *m_outputBuffer;
	int            m_screenSize;
	int            m_useRGB;
	int            m_inverted;
	uint16_t    ***m_rgb565map;
	int            m_lastFrameSize;
	unsigned char *m_lastFrame;
	int            m_rOffset;
	int            m_gOffset;
	int            m_bOffset;

	ImageTransitionType m_transitionType;
	volatile ImageTransitionType m_nextTransitionType;
	int                 m_transitionTime;

	unsigned int   m_typeSeed;

	struct fb_var_screeninfo m_vInfo;
	struct fb_var_screeninfo m_vInfoOrig;
	struct fb_fix_screeninfo m_fInfo;

	volatile bool m_runLoop;
	volatile bool m_imageReady;

	std::thread *m_drawThread;

	std::mutex m_bufferLock;
	std::mutex m_drawLock;

	std::condition_variable m_drawSignal;

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

	inline void SyncDisplay(void);

	int  InitializeFrameBuffer(void);

#ifdef USE_X11
	int  InitializeX11Window(void);
	void DestroyX11Window(void);

	std::string    m_title;
	Display       *m_display;
	int            m_screen;
	Window         m_window;
	GC             m_gc;
	Pixmap         m_pixmap;
	XImage        *m_xImage;
#endif

};

inline void FrameBuffer::SyncDisplay(void)
{
#ifdef USE_X11
	XLockDisplay(m_display);
	XPutImage(m_display, m_window, m_gc, m_xImage, 0, 0, 0, 0, m_fbWidth, m_fbHeight);
	XSync(m_display, True);
	XFlush(m_display);
	XUnlockDisplay(m_display);
#endif
}

#endif
