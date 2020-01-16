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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "common.h"
#include "log.h"
#include "X11PixelStrings.h"
#include "Sequence.h" // for FPPD_MAX_CHANNELS
#include "settings.h"


/////////////////////////////////////////////////////////////////////////////

extern "C" {
    X11PixelStringsOutput *createX11PixelStringsOutput(unsigned int startChannel,
                                           unsigned int channelCount) {
        return new X11PixelStringsOutput(startChannel, channelCount);
    }
}


/*
 *
 */
X11PixelStringsOutput::X11PixelStringsOutput(unsigned int startChannel, unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
    m_display(NULL),
    m_screen(0),
    m_xImage(NULL),
    m_scale(10),
    m_scaledWidth(0),
    m_scaledHeight(0),
    m_fbp(NULL),
	m_longestString(0),
	m_pixels(0)
{
	LogDebug(VB_CHANNELOUT, "X11PixelStringsOutput::X11PixelStringsOutput(%u, %u)\n",
		startChannel, channelCount);

	XInitThreads();
}

/*
 *
 */
X11PixelStringsOutput::~X11PixelStringsOutput()
{
	LogDebug(VB_CHANNELOUT, "X11PixelStringsOutput::~X11PixelStringsOutput()\n");

	for (int s = 0; s < m_strings.size(); s++)
		delete m_strings[s];

    DestroyX11Window();
}


/*
 *
 */
int X11PixelStringsOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "X11PixelStringsOutput::Init(JSON)\n");

	int pixels = 0;
	for (int i = 0; i < config["outputs"].size(); i++)
	{
		Json::Value s = config["outputs"][i];
		PixelString *newString = new PixelString();

		if (!newString->Init(s))
			return 0;

		pixels = newString->m_outputChannels / 3;
		m_pixels += pixels;

		if (pixels > m_longestString)
			m_longestString = pixels;

		m_strings.push_back(newString);
	}

	LogDebug(VB_CHANNELOUT, "   Fount %d strings of pixels\n", m_strings.size());

    // Window size
    m_scaledWidth = m_longestString * m_scale;
    m_scaledHeight = m_strings.size() * m_scale;

	InitializeX11Window();

	return ThreadedChannelOutputBase::Init(config);
}

/*
 *
 */
int X11PixelStringsOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "X11PixelStringsOutput::Close()\n");

	return ThreadedChannelOutputBase::Close();
}

int X11PixelStringsOutput::InitializeX11Window(void)
{
	if ((m_scaledWidth == 0) || (m_scaledHeight == 0))
	{
		m_scaledWidth = 50 * m_scale;
		m_scaledHeight = 16 * m_scale;
	}

	// Initialize X11 Window here
	m_title = "X11 FrameBuffer";
	m_display = XOpenDisplay(getenv("DISPLAY"));
	if (!m_display)
	{
		LogErr(VB_PLAYLIST, "Unable to connect to X Server\n");
		return 0;
	}

	m_screen = DefaultScreen(m_display);

	m_fbp = new char[m_scaledWidth * m_scaledHeight * 4];

	m_xImage = XCreateImage(m_display, CopyFromParent, 24, ZPixmap, 0,
		(char *)m_fbp, m_scaledWidth, m_scaledHeight, 32, m_scaledWidth * 4);

	XSetWindowAttributes attributes;

	attributes.background_pixel = BlackPixel(m_display, m_screen);

	XGCValues values;

	m_pixmap = XCreatePixmap(m_display, XDefaultRootWindow(m_display), m_scaledWidth, m_scaledHeight, 24);

	m_gc = XCreateGC(m_display, m_pixmap, 0, &values);
	if (m_gc < 0)
	{
		LogErr(VB_PLAYLIST, "Unable to create GC\n");
		return 0;
	}

	m_window = XCreateWindow(
		m_display, RootWindow(m_display, m_screen), m_scaledWidth, m_scaledHeight,
		m_scaledWidth, m_scaledHeight, 5, 24, InputOutput,
		DefaultVisual(m_display, m_screen), CWBackPixel, &attributes);

	XMapWindow(m_display, m_window);

	XStoreName(m_display, m_window, m_title.c_str());
	XSetIconName(m_display, m_window, m_title.c_str());

	XFlush(m_display);

	return 1;
}

void X11PixelStringsOutput::DestroyX11Window(void)
{
	XDestroyWindow(m_display, m_window);
	XFreePixmap(m_display, m_pixmap);
	XFreeGC(m_display, m_gc);
	XCloseDisplay(m_display);
	delete [] m_fbp;
}

void X11PixelStringsOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    int min = FPPD_MAX_CHANNELS;
    int max = 0;
    
    PixelString *ps = NULL;
    for (int s = 0; s < m_strings.size(); s++) {
        ps = m_strings[s];
        int inCh = 0;
        for (int p = 0; p < ps->m_outputChannels; p++) {
            int ch = ps->m_outputMap[inCh++];
            if (ch < (FPPD_MAX_CHANNELS - 3)) {
                min = std::min(min, ch);
                max = std::max(max, ch);
            }
        }
    }

    if (min < max)
        addRange(min, max);
}

/*
 *
 */
void X11PixelStringsOutput::PrepData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "X11PixelStringsOutput::RawSendData(%p)\n", channelData);

	unsigned char *c = channelData;
	unsigned char r = 0;
	unsigned char g = 0;
	unsigned char b = 0;
	int stride = m_scaledWidth * 4; // RGBA
	unsigned char *d = (unsigned char*)m_fbp;
    unsigned char *l = d;

	PixelString *ps = NULL;
	int inCh = 0;

	for (int s = 0; s < m_strings.size(); s++)
	{
		ps = m_strings[s];

		d = (unsigned char *)m_fbp + (s * stride * m_scale);
        l = d;
		inCh = 0;

		for (int p = 0, pix = 0; p < ps->m_outputChannels; pix++)
		{
			r = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
			g = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
			b = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];

            // scale horizontally by duplicating pixels
            for (int i = 0; i < m_scale; i++)
            {
                *(d++) = r;
                *(d++) = g;
                *(d++) = b;
                d++;
            }
		}

        // scale vertically by duplicating rows
        for (int i = 0; i < (m_scale - 1); i++)
        {
            memcpy(d, l, stride);
            d += stride;
        }
	}
}

int X11PixelStringsOutput::RawSendData(unsigned char *channelData)
{
    SyncDisplay();

	return m_channelCount;
}

/*
 *
 */
void X11PixelStringsOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "X11PixelStringsOutput::DumpConfig()\n");

	for (int i = 0; i < m_strings.size(); i++)
	{
		LogDebug(VB_CHANNELOUT, "    String #%d\n", i);
		m_strings[i]->DumpConfig();
	}

	ThreadedChannelOutputBase::DumpConfig();
}

