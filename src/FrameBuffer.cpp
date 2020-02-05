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

#include <fcntl.h>
#include <linux/kd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "common.h"
#include "log.h"
#include "FrameBuffer.h"

void StartDrawLoopThread(FrameBuffer *fb);

/*
 *
 */
FrameBuffer::FrameBuffer()
  : m_device("fb0"),
	m_dataFormat("RGB"),
	m_fbFd(-1),
	m_ttyFd(-1),
	m_fbWidth(0),
	m_fbHeight(0),
	m_bpp(24),
	m_fbp(NULL),
	m_outputBuffer(NULL),
	m_screenSize(0),
	m_useRGB(0),
	m_inverted(0),
	m_rgb565map(NULL),
	m_lastFrameSize(0),
	m_lastFrame(NULL),
	m_rOffset(0),
	m_gOffset(1),
	m_bOffset(2),
	m_transitionType(IT_Normal),
	m_nextTransitionType(IT_Normal),
	m_transitionTime(800),
	m_runLoop(false),
	m_imageReady(false),
	m_drawThread(NULL)
{
	LogDebug(VB_PLAYLIST, "FrameBuffer::FrameBuffer()\n");

	m_typeSeed = (unsigned int)time(NULL);

#ifdef USE_X11
	m_display = NULL;
	m_screen = 0;
	m_xImage = NULL;
#endif
}

/*
 *
 */
FrameBuffer::~FrameBuffer()
{
	if (!m_runLoop)
		return; // never initialized/run so no need to tear down

	m_runLoop = false;
	m_drawSignal.notify_all();

	if (m_drawThread) {
		m_drawThread->join();
		delete m_drawThread;
	}

	if (m_fbp)
	{
		memset(m_fbp, 0, m_screenSize);
		munmap(m_fbp, m_screenSize);
	}

	if (m_outputBuffer)
		free(m_outputBuffer);

	if (m_device == "/dev/fb0")
	{
		if (ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig))
			LogErr(VB_PLAYLIST, "Error resetting variable info\n");
	}
#ifdef USE_X11
	else if (m_device == "x11")
	{
		DestroyX11Window();
	}
#endif

	if (m_fbFd > -1)
		close(m_fbFd);

	if (m_rgb565map)
	{
		for (int r = 0; r < 32; r++)
		{
			for (int g = 0; g < 64; g++)
			{
				delete[] m_rgb565map[r][g];
			}

			delete[] m_rgb565map[r];
		}

		delete[] m_rgb565map;
	}
}

/*
 *
 */
int FrameBuffer::FBInit(Json::Value &config)
{
	if (config.isMember("width"))
		m_fbWidth = config["width"].asInt();

	if (config.isMember("height"))
		m_fbHeight = config["height"].asInt();

	if (config.isMember("inverted"))
		m_inverted = config["inverted"].asInt();

	if (config.isMember("device"))
		m_device = config["device"].asString();

	if (config.isMember("transitionType"))
		m_transitionType = (ImageTransitionType)(atoi(config["transitionType"].asString().c_str()));

	if (config.isMember("dataFormat"))
		m_dataFormat = config["dataFormat"].asString();

	if (m_device == "fb0")
		m_device = "/dev/fb0";
	else if (m_device == "fb1")
		m_device = "/dev/fb1";

	if (m_dataFormat == "RGB")
		m_useRGB = 1;

	int result = 0;

#ifdef USE_X11
	if (m_device == "x11")
		result = InitializeX11Window();
	else
#endif
		result = InitializeFrameBuffer();

	if (!result)
		return 0;

	m_screenSize = m_fbWidth * m_fbHeight * m_bpp / 8;

	m_lastFrameSize = m_fbWidth * m_fbHeight * m_dataFormat.size();

	m_outputBuffer = (char *)malloc(m_screenSize);
	if (!m_outputBuffer)
	{
		LogErr(VB_PLAYLIST, "Error, unable to allocate outputBuffer buffer\n");
		ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
		close(m_fbFd);
		return 0;
	}

	m_lastFrame = (unsigned char*)malloc(m_lastFrameSize);
	if (!m_lastFrame)
	{
		LogErr(VB_PLAYLIST, "Error, unable to allocate lastFrame buffer\n");
		ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
		close(m_fbFd);
		return 0;
	}

	memset(m_outputBuffer, 0, m_screenSize);
	memset(m_lastFrame, 0, m_lastFrameSize);

	std::string colorOrder = m_dataFormat.substr(0,3);

	if (colorOrder == "RGB")
	{
		m_rOffset = 0;
		m_gOffset = 1;
		m_bOffset = 2;
	}
	else if (colorOrder == "RBG")
	{
		m_rOffset = 0;
		m_gOffset = 2;
		m_bOffset = 1;
	}
	else if (colorOrder == "GRB")
	{
		m_rOffset = 1;
		m_gOffset = 0;
		m_bOffset = 2;
	}
	else if (colorOrder == "GBR")
	{
		m_rOffset = 2;
		m_gOffset = 0;
		m_bOffset = 1;
	}
	else if (colorOrder == "BRG")
	{
		m_rOffset = 1;
		m_gOffset = 2;
		m_bOffset = 0;
	}
	else if (colorOrder == "BGR")
	{
		m_rOffset = 2;
		m_gOffset = 1;
		m_bOffset = 0;
	}

	m_runLoop = true;
	m_imageReady = false;

	m_drawThread = new std::thread(StartDrawLoopThread, this);

	return 1;
}

/*
 *
 */
int FrameBuffer::InitializeFrameBuffer(void)
{
	LogDebug(VB_PLAYLIST, "Using FrameBuffer device %s\n", m_device.c_str());

	m_fbFd = open(m_device.c_str(), O_RDWR);
	if (!m_fbFd)
	{
		LogErr(VB_PLAYLIST, "Error opening FrameBuffer device: %s\n", m_device.c_str());
		return 0;
	}

	if (ioctl(m_fbFd, FBIOGET_VSCREENINFO, &m_vInfo))
	{
		LogErr(VB_PLAYLIST, "Error getting FrameBuffer info\n");
		close(m_fbFd);
		return 0;
	}

	memcpy(&m_vInfoOrig, &m_vInfo, sizeof(struct fb_var_screeninfo));

	if (m_vInfo.bits_per_pixel == 32)
		m_vInfo.bits_per_pixel = 24;

	m_bpp = m_vInfo.bits_per_pixel;
	LogDebug(VB_PLAYLIST, "FrameBuffer is using %d BPP\n", m_bpp);

	if ((m_bpp != 24) && (m_bpp != 16))
	{
		LogErr(VB_PLAYLIST, "Do not know how to handle %d BPP\n", m_bpp);
		close(m_fbFd);
		return 0;
	}

	if (m_bpp == 16)
	{
		LogExcess(VB_PLAYLIST, "Current Bitfield offset info:\n");
		LogExcess(VB_PLAYLIST, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
		LogExcess(VB_PLAYLIST, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
		LogExcess(VB_PLAYLIST, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);

		// RGB565
		m_vInfo.red.offset    = 11;
		m_vInfo.red.length    = 5;
		m_vInfo.green.offset  = 5;
		m_vInfo.green.length  = 6;
		m_vInfo.blue.offset   = 0;
		m_vInfo.blue.length   = 5;
		m_vInfo.transp.offset = 0;
		m_vInfo.transp.length = 0;

		LogExcess(VB_PLAYLIST, "New Bitfield offset info should be:\n");
		LogExcess(VB_PLAYLIST, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
		LogExcess(VB_PLAYLIST, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
		LogExcess(VB_PLAYLIST, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);
	}

	if (m_fbWidth && m_fbHeight)
	{
		m_vInfo.xres = m_vInfo.xres_virtual = m_fbWidth;
		m_vInfo.yres = m_vInfo.yres_virtual = m_fbHeight;
	}
	else
	{
		m_fbWidth = m_vInfo.xres;
		m_fbHeight = m_vInfo.yres;
	}

	if (ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfo))
	{
		LogErr(VB_PLAYLIST, "Error setting FrameBuffer info\n");
		close(m_fbFd);
		return 0;
	}

	if (ioctl(m_fbFd, FBIOGET_FSCREENINFO, &m_fInfo))
	{
		LogErr(VB_PLAYLIST, "Error getting fixed FrameBuffer info\n");
		close(m_fbFd);
		return 0;
	}

	m_screenSize = m_vInfo.xres * m_vInfo.yres * m_vInfo.bits_per_pixel / 8;

	if (m_screenSize != (m_fbWidth * m_fbHeight * m_vInfo.bits_per_pixel / 8))
	{
		LogErr(VB_PLAYLIST, "Error, screensize incorrect\n");
		ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
		close(m_fbFd);
		return 0;
	}

	m_lastFrameSize = m_fbWidth * m_fbHeight * m_dataFormat.size();

	if (m_device == "/dev/fb0")
	{
		m_ttyFd = open("/dev/console", O_RDWR);
		if (!m_ttyFd)
		{
			LogErr(VB_PLAYLIST, "Error, unable to open /dev/console\n");
			ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
			close(m_fbFd);
			return 0;
		}

		// Hide the text console
		ioctl(m_ttyFd, KDSETMODE, KD_GRAPHICS);

		// Shouldn't need this anymore.
		close(m_ttyFd);
		m_ttyFd = -1;
	}

	m_fbp = (char*)mmap(0, m_screenSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fbFd, 0);

	if ((char *)m_fbp == (char *)-1)
	{
		LogErr(VB_PLAYLIST, "Error, unable to map /dev/fb0\n");
		ioctl(m_fbFd, FBIOPUT_VSCREENINFO, &m_vInfoOrig);
		close(m_fbFd);
		return 0;
	}

	memset(m_fbp, 0, m_screenSize);

	if (m_bpp == 16)
	{
		LogExcess(VB_PLAYLIST, "Generating RGB565Map for Bitfield offset info:\n");
		LogExcess(VB_PLAYLIST, " R: %d (%d bits)\n", m_vInfo.red.offset, m_vInfo.red.length);
		LogExcess(VB_PLAYLIST, " G: %d (%d bits)\n", m_vInfo.green.offset, m_vInfo.green.length);
		LogExcess(VB_PLAYLIST, " B: %d (%d bits)\n", m_vInfo.blue.offset, m_vInfo.blue.length);

		unsigned char rMask = (0xFF ^ (0xFF >> m_vInfo.red.length));
		unsigned char gMask = (0xFF ^ (0xFF >> m_vInfo.green.length));
		unsigned char bMask = (0xFF ^ (0xFF >> m_vInfo.blue.length));
		int rShift = m_vInfo.red.offset - (8 + (8-m_vInfo.red.length));
		int gShift = m_vInfo.green.offset - (8 + (8 - m_vInfo.green.length));
		int bShift = m_vInfo.blue.offset - (8 + (8 - m_vInfo.blue.length));;

		//LogDebug(VB_PLAYLIST, "rM/rS: 0x%02x/%d, gM/gS: 0x%02x/%d, bM/bS: 0x%02x/%d\n", rMask, rShift, gMask, gShift, bMask, bShift);

		uint16_t o;
		m_rgb565map = new uint16_t**[32];

		for (uint16_t b = 0; b < 32; b++)
		{
			m_rgb565map[b] = new uint16_t*[64];
			for (uint16_t g = 0; g < 64; g++)
			{
				m_rgb565map[b][g] = new uint16_t[32];
				for (uint16_t r = 0; r < 32; r++)
				{
					o = 0;

					if (rShift >= 0)
						o |= r >> rShift;
					else
						o |= r << abs(rShift);

					if (gShift >= 0)
						o |= g >> gShift;
					else
						o |= g << abs(gShift);

					if (bShift >= 0)
						o |= b >> bShift;
					else
						o |= b << abs(bShift);

					m_rgb565map[b][g][r] = o;
				}
			}
		}
	}

	return 1;
}

#ifdef USE_X11
int FrameBuffer::InitializeX11Window(void)
{
	if ((m_fbWidth == 0) || (m_fbHeight == 0))
	{
		m_fbWidth = 720;
		m_fbHeight = 400;
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

	m_fbp = new char[m_fbWidth * m_fbHeight * 4];

	m_xImage = XCreateImage(m_display, CopyFromParent, 24, ZPixmap, 0,
		(char *)m_fbp, m_fbWidth, m_fbHeight, 32, m_fbWidth * 4);

	m_bpp = 32;

	XSetWindowAttributes attributes;

	attributes.background_pixel = BlackPixel(m_display, m_screen);

	XGCValues values;

	m_pixmap = XCreatePixmap(m_display, XDefaultRootWindow(m_display), m_fbWidth, m_fbHeight, 24);

	m_gc = XCreateGC(m_display, m_pixmap, 0, &values);
	if (m_gc < 0)
	{
		LogErr(VB_PLAYLIST, "Unable to create GC\n");
		return 0;
	}

	m_window = XCreateWindow(
		m_display, RootWindow(m_display, m_screen), m_fbWidth, m_fbHeight,
		m_fbWidth, m_fbHeight, 5, 24, InputOutput,
		DefaultVisual(m_display, m_screen), CWBackPixel, &attributes);

	XMapWindow(m_display, m_window);

	XStoreName(m_display, m_window, m_title.c_str());
	XSetIconName(m_display, m_window, m_title.c_str());

	XFlush(m_display);

	return 1;
}

void FrameBuffer::DestroyX11Window(void)
{
	XDestroyWindow(m_display, m_window);
	XFreePixmap(m_display, m_pixmap);
	XFreeGC(m_display, m_gc);
	XCloseDisplay(m_display);
	delete [] m_fbp;
}
#endif

/*
 *
 */
void FrameBuffer::FBCopyData(unsigned char *buffer, int draw)
{
	int ostride = m_fbWidth * m_bpp / 8;
	int srow = 0;
	int drow = m_inverted ? m_fbHeight - 1 : 0;
	unsigned char *s = buffer;
	unsigned char *d;
	unsigned char *l = m_lastFrame;
	unsigned char *sR = buffer + m_rOffset;
	unsigned char *sG = buffer + m_gOffset;
	unsigned char *sB = buffer + m_bOffset;
	int sBpp = m_dataFormat.size();
	int skipped = 0;
	unsigned char *ob = (unsigned char *)m_outputBuffer;

	m_bufferLock.lock();

	if (draw)
		ob = (unsigned char *)m_fbp;

	if (m_bpp == 16)
	{
		for (int y = 0; y < m_fbHeight; y++)
		{
			d = ob + (drow * ostride);
			for (int x = 0; x < m_fbWidth; x++)
			{
				if (memcmp(l, sR, sBpp))
				{
					if (skipped)
					{
						sG += skipped * sBpp;
						sB += skipped * sBpp;
						d  += skipped * 2;
						skipped = 0;
					}

//					if (m_useRGB) // RGB data to BGR framebuffer
						//*((uint16_t*)d) = m_rgb565map[*sR >> 3][*sG >> 2][*sB >> 3];
						int ri = *sR >> 3;
						int gi = *sG >> 2;
						int bi = *sB >> 3;
//LogDebug(VB_PLAYLIST, "x,y: %d,%d   RGB: %02X (%d), %02X (%d),  %02X (%d)\n", x, y, *sR, *sR >> 3, *sG, *sG >> 2, *sB, *sB >> 3);
//LogDebug(VB_PLAYLIST, "x,y: %d,%d   D: %04X\n", x, y, m_rgb565map[ri][gi][bi]);
						uint16_t td = m_rgb565map[*sR >> 3][*sG >> 2][*sB >> 3];
						*((uint16_t*)d) = td;
//					else // BGR data to BGR framebuffer
//						*((uint16_t*)d) = m_rgb565map[*sB >> 3][*sG >> 2][*sR >> 3];

					sG += sBpp;
					sB += sBpp;
					d += 2;
				}
				else
				{
					skipped++;
				}

				sR += sBpp;
				l += sBpp;
			}

			srow++;
			drow += m_inverted ? -1 : 1;
		}
	}
	else if (m_bpp == 32) // X11 BGRA
	{
		unsigned char *dR;
		unsigned char *dG;
		unsigned char *dB;

		for (int y = 0; y < m_fbHeight; y++)
		{
			// Data to BGR framebuffer
			dR = ob + (drow * ostride) + 2;
			dG = ob + (drow * ostride) + 1;
			dB = ob + (drow * ostride) + 0;

			for (int x = 0; x < m_fbWidth; x++)
			{
//				if (memcmp(l, sB, 3))
//				{
					*dR = *sR;
					*dG = *sG;
					*dB = *sB;
//				}

				sR += sBpp;
				sG += sBpp;
				sB += sBpp;
				dR += 4;
				dG += 4;
				dB += 4;
			}

			srow++;
			drow += m_inverted ? -1 : 1;
		}
	}
	else if (m_dataFormat != "BGR") // non-BGR to 24bpp BGR
	{
		unsigned char *dR;
		unsigned char *dG;
		unsigned char *dB;

		for (int y = 0; y < m_fbHeight; y++)
		{
			// Data to BGR framebuffer
			dR = ob + (drow * ostride) + 2;
			dG = ob + (drow * ostride) + 1;
			dB = ob + (drow * ostride) + 0;

			for (int x = 0; x < m_fbWidth; x++)
			{
//				if (memcmp(l, sB, 3))
//				{
					*dR = *sR;
					*dG = *sG;
					*dB = *sB;
//				}

				sR += sBpp;
				sG += sBpp;
				sB += sBpp;
				dR += 3;
				dG += 3;
				dB += 3;
			}

			srow++;
			drow += m_inverted ? -1 : 1;
		}
	}
	else // 24bpp BGR input data to match 24bpp BGR framebuffer
	{
		if (m_inverted)
		{
			int istride = m_fbWidth * sBpp;
			unsigned char *src = buffer;
			unsigned char *dst = ob + (ostride * (m_fbHeight-1));

			for (int y = 0; y < m_fbHeight; y++)
			{
				memcpy(dst, src, istride);
				src += istride;
				dst -= ostride;
			}
		}
		else
		{
			memcpy(ob, buffer, m_screenSize);
		}
	}

	memcpy(m_lastFrame, buffer, m_lastFrameSize);

	m_bufferLock.unlock();

	if (!draw)
		m_imageReady = true;
}

/*
 *
 */
void FrameBuffer::Dump(void)
{
	LogDebug(VB_PLAYLIST, "Transition Type: %d\n", (int)m_transitionType);
	LogDebug(VB_PLAYLIST, "Width          : %d\n", m_fbWidth);
	LogDebug(VB_PLAYLIST, "Height         : %d\n", m_fbHeight);
	LogDebug(VB_PLAYLIST, "Device         : %s\n", m_device.c_str());
	LogDebug(VB_PLAYLIST, "DataFormat     : %s\n", m_dataFormat.c_str());
	LogDebug(VB_PLAYLIST, "Inverted       : %d\n", m_inverted);
	LogDebug(VB_PLAYLIST, "bpp            : %d\n", m_bpp);
}

/*
 *
 */
void FrameBuffer::GetConfig(Json::Value &config)
{
	config["transitionType"] = m_transitionType;
	config["width"] = m_fbWidth;
	config["height"] = m_fbHeight;
	config["device"] = m_device;
	config["dataFormat"] = m_dataFormat;
	config["inverted"] = m_inverted;
}


/*
 *
 */
void FrameBuffer::FBStartDraw(ImageTransitionType transitionType)
{
	if (transitionType == IT_Default)
		transitionType = m_transitionType;

	if (transitionType == IT_Random)
		transitionType = (ImageTransitionType)(rand_r(&m_typeSeed) % ((int)IT_MAX - 1) + 1);

	m_nextTransitionType = transitionType;

	m_drawSignal.notify_all();
}

/*
 *
 */
void StartDrawLoopThread(FrameBuffer *fb)
{
	fb->DrawLoop();
}

void FrameBuffer::DrawLoop(void)
{
	std::unique_lock<std::mutex> lock(m_bufferLock);

	while (m_runLoop)
	{
		if (m_imageReady)
		{
			lock.unlock();
			//m_nextTransitionType = IT_WipeFromHCenter;
			switch (m_nextTransitionType)
			{
				case IT_Random:
					m_nextTransitionType = (ImageTransitionType)(rand_r(&m_typeSeed) % ((int)IT_MAX - 1) + 1);
					lock.lock();
					continue;
					break;

				case IT_SlideUp:
					FBDrawSlideUp();
					break;

				case IT_SlideDown:
					FBDrawSlideDown();
					break;

				case IT_SlideLeft:
					FBDrawSlideLeft();
					break;

				case IT_SlideRight:
					FBDrawSlideRight();
					break;

				case IT_WipeUp:
					FBDrawWipeUp();
					break;

				case IT_WipeDown:
					FBDrawWipeDown();
					break;

				case IT_WipeLeft:
					FBDrawWipeLeft();
					break;

				case IT_WipeRight:
					FBDrawWipeRight();
					break;

				case IT_WipeToHCenter:
					FBDrawWipeToHCenter();
					break;

				case IT_WipeFromHCenter:
					FBDrawWipeFromHCenter();
					break;

				case IT_HorzBlindsOpen:
					FBDrawHorzBlindsOpen();
					break;

				case IT_HorzBlindsClose:
					FBDrawHorzBlindsClose();
					break;

				case IT_Mosaic:
					FBDrawMosaic();
					break;

				default:
					FBDrawNormal();
					break;
			}

			lock.lock();

			m_imageReady = false;
		}

		m_drawSignal.wait(lock);
	}
}

/*
 *
 */
void FrameBuffer::FBDrawNormal(void)
{
	m_bufferLock.lock();
	memcpy(m_fbp, m_outputBuffer, m_screenSize);
	SyncDisplay();
	m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawSlideUp(void)
{
	int stride = m_fbWidth * m_bpp / 8;
	int rEach = 2;
	int sleepTime = 800 * 1000 * rEach / m_fbHeight;

	if ((m_fbHeight % 4) == 0)
		rEach = 4;

	m_bufferLock.lock();
	for (int i = m_fbHeight - rEach; i>= 0; )
	{
		memcpy(m_fbp + (stride * i), m_outputBuffer, stride * (m_fbHeight - i));
		SyncDisplay();

		usleep(sleepTime);

		i -= rEach;
	}
	m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawSlideDown(void)
{
	int stride = m_fbWidth * m_bpp / 8;
	int rEach = 2;
	int sleepTime = 800 * 1000 * rEach / m_fbHeight;

	if ((m_fbHeight % 4) == 0)
		rEach = 4;

	m_bufferLock.lock();
	for (int i = rEach; i <= m_fbHeight;)
	{
		memcpy(m_fbp, m_outputBuffer + (stride * (m_fbHeight - i)), stride * i);
		SyncDisplay();

		usleep(sleepTime);

		i += rEach;
	}
	m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawSlideLeft(void)
{
	int colsEach = 2;
	int sleepTime = 800 * 1000 * colsEach / m_fbWidth;

	for (int x = m_fbWidth - colsEach; x >= 0; x -= colsEach)
	{
		DrawSquare(x, 0, m_fbWidth - x, m_fbHeight, 0, 0);
		SyncDisplay();

		usleep(sleepTime);
	}
}

/*
 *
 */
void FrameBuffer::FBDrawSlideRight(void)
{
	int colsEach = 2;
	int sleepTime = 800 * 1000 * colsEach / m_fbWidth;

	for (int x = 2; x <= m_fbWidth; x += colsEach)
	{
		DrawSquare(0, 0, x, m_fbHeight, m_fbWidth - x, 0);
		SyncDisplay();

		usleep(sleepTime);
	}
}

/*
 *
 */
void FrameBuffer::FBDrawWipeUp(void)
{
	int stride = m_fbWidth * m_bpp / 8;
	int rEach = 2;

	if ((m_fbHeight % 4) == 0)
		rEach = 4;

	int sleepTime = 800 * 1000 * rEach / m_fbHeight;

	m_bufferLock.lock();
	for (int i = m_fbHeight - rEach; i>= 0; )
	{
		memcpy(m_fbp + (stride * i), m_outputBuffer + (stride * i), stride * rEach);
		SyncDisplay();

		usleep(sleepTime);

		i -= rEach;
	}
	m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawWipeDown(void)
{
	int stride = m_fbWidth * m_bpp / 8;
	int rEach = 2;

	if ((m_fbHeight % 4) == 0)
		rEach = 4;

	int sleepTime = 800 * 1000 * rEach / m_fbHeight;

	m_bufferLock.lock();
	for (int i = 0; i < m_fbHeight;)
	{
		memcpy(m_fbp + (stride * i), m_outputBuffer + (stride * i), stride * rEach);
		SyncDisplay();

		usleep(sleepTime);

		i += rEach;
	}
	m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawWipeLeft(void)
{
	int stride = m_fbWidth * m_bpp / 8;
	int BPP = m_bpp / 8;

	m_bufferLock.lock();

	for (int x = m_fbWidth - 1; x >= 0; x--)
	{
		for (int y = 0; y < m_fbHeight; y++)
		{
			memcpy(m_fbp + (stride * y) + (x * BPP),
				m_outputBuffer + (stride * y) + (x * BPP), BPP);
		}
		SyncDisplay();
	}

	m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawWipeRight(void)
{
	int stride = m_fbWidth * m_bpp / 8;
	int BPP = m_bpp / 8;

	m_bufferLock.lock();

	for (int x = 0; x < m_fbWidth; x++)
	{
		for (int y = 0; y < m_fbHeight; y++)
		{
			memcpy(m_fbp + (stride * y) + (x * BPP),
				m_outputBuffer + (stride * y) + (x * BPP), BPP);
		}
		SyncDisplay();
	}

	m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawWipeToHCenter(void)
{
	int stride = m_fbWidth * m_bpp / 8;
	int rEach = 2;
	int mid = m_fbHeight / 2;

	if ((m_fbHeight % 4) == 0)
		rEach = 4;

	int sleepTime = 800 * 1000 * rEach / m_fbHeight * 2;

	m_bufferLock.lock();
	for (int i = 0; i < mid; i += rEach)
	{
		memcpy(m_fbp + (stride * i), m_outputBuffer + (stride * i), stride * rEach);
		memcpy(m_fbp + (stride * (m_fbHeight - i - rEach)),
				m_outputBuffer + (stride * (m_fbHeight - i - rEach)),
				stride * rEach);
		SyncDisplay();

		usleep(sleepTime);
	}
	m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawWipeFromHCenter(void)
{
	int stride = m_fbWidth * m_bpp / 8;
	int mid = m_fbHeight / 2;
	int rowsEachUpdate = 2;
	int sleepTime = 800 * 1000 * rowsEachUpdate / m_fbHeight * 2;

	m_bufferLock.lock();

	int t = mid - 2;
	for (int b = mid; b < m_fbHeight; b += rowsEachUpdate, t -= rowsEachUpdate)
	{
		memcpy(m_fbp + (stride * b), m_outputBuffer + (stride * b),
			stride * rowsEachUpdate);

		if (t >= 0)
			memcpy(m_fbp + (stride * t), m_outputBuffer + (stride * t),
				stride * rowsEachUpdate);

		SyncDisplay();

		usleep(sleepTime);
	}

	m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawHorzBlindsOpen(void)
{
	int stride = m_fbWidth * m_bpp / 8;
	int rowsEachUpdate = 2;
	int blindSize = 32;
	int sleepTime = 800 * 1000 * rowsEachUpdate / blindSize;

	m_bufferLock.lock();
	for (int i = (blindSize - 1); i >= 0; i -= rowsEachUpdate)
	{
		for (int y = i; y < m_fbHeight; y += blindSize)
		{
			memcpy(m_fbp + (stride * y), m_outputBuffer + (stride * y),
				stride * rowsEachUpdate);
		}

		SyncDisplay();

		usleep(sleepTime);
	}
	m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawHorzBlindsClose(void)
{
	int stride = m_fbWidth * m_bpp / 8;
	int rowsEachUpdate = 2;
	int blindSize = 32;
	int sleepTime = 800 * 1000 * rowsEachUpdate / blindSize;

	m_bufferLock.lock();
	for (int i = 0; i < blindSize; i += rowsEachUpdate)
	{
		for (int y = i; y < m_fbHeight; y += blindSize)
		{
			memcpy(m_fbp + (stride * y), m_outputBuffer + (stride * y),
				stride * rowsEachUpdate);
		}

		SyncDisplay();

		usleep(sleepTime);
	}
	m_bufferLock.unlock();
}

/*
 *
 */
void FrameBuffer::FBDrawMosaic(void)
{
	int squareSize = 32;
	int xSquares = m_fbWidth / squareSize + 1;
	int ySquares = m_fbHeight / squareSize + 1;
	int squares[ySquares][xSquares];
	int count = xSquares * ySquares;
	int sleepTime = 800 * 1000 / count;

	memset(squares, 0, sizeof(squares));

	m_bufferLock.lock();

	for (int i = 0; i < (count * 10); i++)
	{
		int square = rand_r(&m_typeSeed) % (count - 1);
		int x = square % xSquares;
		int y = square / xSquares;

		if (!squares[y][x])
		{
			DrawSquare(x * squareSize, y * squareSize, squareSize, squareSize);
			squares[y][x] = 1;

			SyncDisplay();

			usleep(sleepTime);
		}
	}

	for (int y = 0; y < ySquares; y++)
	{
		for (int x = 0; x < xSquares; x++)
		{
			if (!squares[y][x])
			{
				DrawSquare(x * squareSize, y * squareSize, squareSize,
					squareSize);
				squares[y][x] = 1;

				SyncDisplay();

				usleep(sleepTime);
			}
		}
	}

	m_bufferLock.unlock();
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void FrameBuffer::DrawSquare(int dx, int dy, int w, int h, int sx, int sy)
{
	int stride = m_fbWidth * m_bpp / 8;
	int dRowOffset = dx * m_bpp / 8;
	int sRowOffset = ((sx == -1) ? dx : sx) * m_bpp / 8;
	int bytesWide = w * m_bpp / 8;

	if (sy == -1)
		sy = dy;

	for (int i = 0; i < h; i++)
	{
		memcpy(m_fbp + (stride * (dy + i)) + dRowOffset,
			m_outputBuffer + (stride * (sy + i)) + sRowOffset, bytesWide);
	}
}

