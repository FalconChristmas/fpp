/*
 *   X11 Pixel Strings Test Channel Output for Falcon Player (FPP)
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

#ifndef _X11PIXELSTRINGS_H
#define _X11PIXELSTRINGS_H

#include <X11/Xlib.h>

#include <vector>

#include "ThreadedChannelOutputBase.h"
#include "PixelString.h"

class X11PixelStringsOutput : public ThreadedChannelOutputBase {
  public:
	X11PixelStringsOutput(unsigned int startChannel, unsigned int channelCount);
	~X11PixelStringsOutput();

	virtual int Init(Json::Value config) override;
	virtual int Close(void) override;

	virtual void PrepData(unsigned char *channelData) override;
	virtual int  RawSendData(unsigned char *channelData) override;

	virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override;

  private:
    int  InitializeX11Window(void);
    void DestroyX11Window(void);

    inline void SyncDisplay(void);

    std::string    m_title;
    Display       *m_display;
    int            m_screen;
    Window         m_window;
    GC             m_gc;
    Pixmap         m_pixmap;
    XImage        *m_xImage;

    int            m_scale;
    int            m_scaledWidth;
    int            m_scaledHeight;
    char          *m_fbp;

	int            m_longestString;
	int            m_pixels;
	unsigned char *m_buffer;
	int            m_bufferSize;

	std::vector<PixelString*> m_strings;
};

inline void X11PixelStringsOutput::SyncDisplay(void)
{
    XLockDisplay(m_display);
    XPutImage(m_display, m_window, m_gc, m_xImage, 0, 0, 0, 0, m_scaledWidth, m_scaledHeight);
    XSync(m_display, True);
    XFlush(m_display);
    XUnlockDisplay(m_display);
}

#endif
