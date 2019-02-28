/*
 *   X11VirtualDisplay Channel Output for Falcon Player (FPP)
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "log.h"
#include "serialutil.h"
#include "Sequence.h"
#include "X11VirtualDisplay.h"

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
X11VirtualDisplayOutput::X11VirtualDisplayOutput(unsigned int startChannel,
	unsigned int channelCount)
  : VirtualDisplayOutput(startChannel, channelCount)
{
	LogDebug(VB_CHANNELOUT, "X11VirtualDisplayOutput::X11VirtualDisplayOutput(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = FPPD_MAX_CHANNELS;
	m_bytesPerPixel = 4;
	m_bpp = 32;
}

/*
 *
 */
X11VirtualDisplayOutput::~X11VirtualDisplayOutput()
{
	LogDebug(VB_CHANNELOUT, "X11VirtualDisplayOutput::~X11VirtualDisplayOutput()\n");

	Close();
}

/*
 *
 */
int X11VirtualDisplayOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "X11VirtualDisplayOutput::Init(JSON)\n");

	if (!VirtualDisplayOutput::Init(config))
		return 0;

	m_virtualDisplay = new unsigned char[m_width * m_height * m_bytesPerPixel];
	if (!m_virtualDisplay)
	{
		LogDebug(VB_CHANNELOUT, "Unable to allocate image data\n");
		return 0;
	}
	bzero(m_virtualDisplay, m_width * m_height * m_bytesPerPixel);

	// Initialize X11 Window here
	m_display = XOpenDisplay(getenv("DISPLAY"));
	if (!m_display)
	{
		LogDebug(VB_CHANNELOUT, "Unable to connect to X Server\n");
		return 0;
	}

	XInitThreads();

	m_screen = DefaultScreen(m_display);

	m_image = XCreateImage(m_display, CopyFromParent, 24, ZPixmap, 0,
		(char *)m_virtualDisplay, m_width, m_height, 32, m_width * m_bytesPerPixel);

	int win_x = 100;
	int win_y = 100;
	XSetWindowAttributes attributes;

	attributes.background_pixel = BlackPixel(m_display, m_screen);

	XGCValues values;

	m_pixmap = XCreatePixmap(m_display, XDefaultRootWindow(m_display),
		m_width, m_height, 24);

	m_gc = XCreateGC(m_display, m_pixmap, 0, &values);
	if (m_gc < 0)
	{
		LogDebug(VB_CHANNELOUT, "Unable to create GC\n");
		return 0;
	}

	m_window = XCreateWindow(
		m_display, RootWindow(m_display, m_screen), win_x, win_y,
		m_width, m_height, 5, 24, InputOutput,
		DefaultVisual(m_display, m_screen), CWBackPixel, &attributes);

	XMapWindow(m_display, m_window);

	XStoreName(m_display, m_window, "Virtual Display");
	XSetIconName(m_display, m_window, "Virtual Display");
	
	XFlush(m_display);

	return InitializePixelMap();
}

/*
 *
 */
int X11VirtualDisplayOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "X11VirtualDisplayOutput::Close()\n");

	// Close X11 Window here

	XLockDisplay(m_display);
	XDestroyWindow(m_display, m_window);
	XFreePixmap(m_display, m_pixmap);
	XFreeGC(m_display, m_gc);
	XCloseDisplay(m_display);
	XUnlockDisplay(m_display);
	delete [] m_virtualDisplay;

	return ChannelOutputBase::Close();
}

/*
 *
 */
int X11VirtualDisplayOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "X11VirtualDisplayOutput::RawSendData(%p)\n", channelData);

	DrawPixels(channelData);

	XLockDisplay(m_display);

	XPutImage(m_display, m_window, m_gc, m_image, 0, 0, 0, 0, m_width, m_height);

	XSync(m_display, True);
	XFlush(m_display);

	XUnlockDisplay(m_display);

	return m_channelCount;
}

