/*
 *   X11 Matrix handler for Falcon Player (FPP)
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
#include "X11Matrix.h"


extern "C" {
    X11MatrixOutput *createX11MatrixOutput(unsigned int startChannel,
                                           unsigned int channelCount) {
        return new X11MatrixOutput(startChannel, channelCount);
    }
}
/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
X11MatrixOutput::X11MatrixOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
	m_width(0),
	m_height(0),
	m_scale(10),
	m_scaleWidth(0),
	m_scaleHeight(0),
	m_imageData(NULL)
{
	LogDebug(VB_CHANNELOUT, "X11MatrixOutput::X11MatrixOutput(%u, %u)\n",
		startChannel, channelCount);

	m_useDoubleBuffer = 1;
}

/*
 *
 */
X11MatrixOutput::~X11MatrixOutput()
{
	LogDebug(VB_CHANNELOUT, "X11MatrixOutput::~X11MatrixOutput()\n");
}

/*
 *
 */
int X11MatrixOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "X11MatrixOutput::Init(JSON)\n");

	m_width  = config["width"].asInt();
	m_height = config["height"].asInt();
	m_scale = config["scale"].asInt();

	if (config.isMember("title"))
		m_title = config["title"].asString();
	else
		m_title = "X11Matrix";

	if (!m_scale)
		m_scale = 10;

	m_scaleWidth = m_scale * m_width;
	m_scaleHeight = m_scale * m_height;
	m_imageData = new char[m_scaleWidth * m_scaleHeight * 4];

	// Initialize X11 Window here
	m_display = XOpenDisplay(getenv("DISPLAY"));
	if (!m_display)
	{
		LogDebug(VB_CHANNELOUT, "Unable to connect to X Server\n");
		return 0;
	}

	m_screen = DefaultScreen(m_display);

	m_image = XCreateImage(m_display, CopyFromParent, 24, ZPixmap, 0,
		m_imageData, m_scaleWidth, m_scaleHeight, 32, m_scaleWidth * 4);

	int win_x = 100;
	int win_y = 100;
	XSetWindowAttributes attributes;

	attributes.background_pixel = BlackPixel(m_display, m_screen);

	XGCValues values;

	m_pixmap = XCreatePixmap(m_display, XDefaultRootWindow(m_display),
		m_scaleWidth, m_scaleHeight, 24);

	m_gc = XCreateGC(m_display, m_pixmap, 0, &values);
	if (m_gc < 0)
	{
		LogDebug(VB_CHANNELOUT, "Unable to create GC\n");
		return 0;
	}

	m_window = XCreateWindow(
		m_display, RootWindow(m_display, m_screen), win_x, win_y,
		m_scaleWidth, m_scaleHeight, 5, 24, InputOutput,
		DefaultVisual(m_display, m_screen), CWBackPixel, &attributes);

	XMapWindow(m_display, m_window);

	XStoreName(m_display, m_window, m_title.c_str());
	XSetIconName(m_display, m_window, m_title.c_str());
	
	XFlush(m_display);

	return ThreadedChannelOutputBase::Init(config);
}

/*
 *
 */
int X11MatrixOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "X11MatrixOutput::Close()\n");

	// Close X11 Window here

	XLockDisplay(m_display);
	XDestroyWindow(m_display, m_window);
	XFreePixmap(m_display, m_pixmap);
	XFreeGC(m_display, m_gc);
	XCloseDisplay(m_display);
	XUnlockDisplay(m_display);
	delete [] m_imageData;

	return ThreadedChannelOutputBase::Close();
}

/*
 *
 */
int X11MatrixOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "X11MatrixOutput::RawSendData(%p)\n", channelData);

	// Draw on X11 Window here
	char *c = (char *)channelData;
	char *d = m_imageData;
	char r = 0;
	char g = 0;
	char b = 0;
	char *row = NULL;
	int stride = m_scaleWidth * 4;
	for (int y = 0; y < m_height; y++)
	{
		row = d;

		for (int x = 0; x < m_width; x++)
		{
			r = *(c++);
			g = *(c++);
			b = *(c++);

			for (int s = 0; s < m_scale; s++)
			{
				*(d++) = b;
				*(d++) = g;
				*(d++) = r;
				*(d++) = 0;
			}
		}

		for (int s = 1; s < m_scale; s++)
		{
			memcpy(row + stride, row, stride);
			row += stride;
			d += stride;
		}
	}

	XLockDisplay(m_display);

	XPutImage(m_display, m_window, m_gc, m_image, 0, 0, 0, 0, m_scaleWidth, m_scaleHeight);

	XSync(m_display, True);
	XFlush(m_display);

	XUnlockDisplay(m_display);

	return m_channelCount;
}

/*
 *
 */
void X11MatrixOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

/*
 *
 */
void X11MatrixOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "X11MatrixOutput::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    Width         : %d\n", m_width);
	LogDebug(VB_CHANNELOUT, "    Height        : %d\n", m_height);
	LogDebug(VB_CHANNELOUT, "    Scale         : %d\n", m_scale);
	LogDebug(VB_CHANNELOUT, "    Scaled Width  : %d\n", m_scaleWidth);
	LogDebug(VB_CHANNELOUT, "    Scaled Height : %d\n", m_scaleHeight);

	ThreadedChannelOutputBase::DumpConfig();
}

