/*
 *   Playlist Entry Image Class for Falcon Player (FPP)
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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "boost/filesystem.hpp"

#include "common.h"
#include "log.h"
#include "PlaylistEntryImage.h"

using namespace boost::filesystem;

void StartPrepLoopThread(PlaylistEntryImage *fb);

/*
 *
 */
PlaylistEntryImage::PlaylistEntryImage(PlaylistEntryBase *parent)
#ifdef USE_X11VSFB
  : PlaylistEntryBase(parent),
#else
  : PlaylistEntryBase(parent), FrameBuffer(),
#endif
	m_width(800),
	m_height(600),
	m_buffer(NULL),
	m_bufferSize(0)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryImage::PlaylistEntryImage()\n");

	m_type = "image";
	m_device = "fb0";

	m_fileSeed = rand();
	m_cacheDir = "/home/fpp/media/cache";
	m_cacheEntries = 200;
	m_cacheSize = 1024; // MB
	m_freeSpace = 2048; // MB

#ifdef USE_X11VSFB
	m_display = NULL;
	m_screen = 0;
	m_xImage = NULL;
#endif
}

/*
 *
 */
PlaylistEntryImage::~PlaylistEntryImage()
{
	m_runLoop = false;
	m_prepSignal.notify_all();
	if (m_prepThread)
	{
		m_prepThread->join();
		delete m_prepThread;
	}

#ifdef USE_X11VSFB
	XDestroyWindow(m_display, m_window);
	XFreePixmap(m_display, m_pixmap);
	XFreeGC(m_display, m_gc);
	XCloseDisplay(m_display);
	delete [] m_imageData;
#endif
}

/*
 *
 */
int PlaylistEntryImage::Init(Json::Value &config)
{
	if (!config.isMember("imagePath"))
	{
		LogErr(VB_PLAYLIST, "Missing imagePath\n");
		return 0;
	}

	m_imagePath = config["imagePath"].asString();

	if (config.isMember("device"))
		m_device = config["device"].asString();

	if (m_device == "fb0")
		m_device = "/dev/fb0";
	else if (m_device == "fb1")
		m_device = "/dev/fb1";

	SetFileList();
	CleanupCache();

	if ((m_device == "/dev/fb0") || (m_device == "/dev/fb1"))
	{
#ifdef USE_X11VSFB
		// Initialize X11 Window here
		m_title = "PlaylistEntryImage Virtual Framebuffer";
		m_display = XOpenDisplay(getenv("DISPLAY"));
		if (!m_display)
		{
			LogErr(VB_PLAYLIST, "Unable to connect to X Server\n");
			return 0;
		}

		m_screen = DefaultScreen(m_display);

		m_imageData = new unsigned char[m_width * m_height * 4];

		m_xImage = XCreateImage(m_display, CopyFromParent, 24, ZPixmap, 0,
			(char *)m_imageData, m_width, m_height, 32, m_width * 4);

		int win_x = 100;
		int win_y = 100;
		XSetWindowAttributes attributes;

		attributes.background_pixel = BlackPixel(m_display, m_screen);

		XGCValues values;

		m_pixmap = XCreatePixmap(m_display, XDefaultRootWindow(m_display), m_width, m_height, 24);

		m_gc = XCreateGC(m_display, m_pixmap, 0, &values);
		if (m_gc < 0)
		{
			LogErr(VB_PLAYLIST, "Unable to create GC\n");
			return 0;
		}

		m_window = XCreateWindow(
			m_display, RootWindow(m_display, m_screen), win_x, win_y,
			m_width, m_height, 5, 24, InputOutput,
			DefaultVisual(m_display, m_screen), CWBackPixel, &attributes);

		XMapWindow(m_display, m_window);

		XStoreName(m_display, m_window, m_title.c_str());
		XSetIconName(m_display, m_window, m_title.c_str());
		
		XFlush(m_display);
		LogDebug(VB_PLAYLIST, "Created X11 Window instead of using %s\n", m_device.c_str());
#else
		config["dataFormat"] = "RGBA";
		FBInit(config);

		m_width = m_fbWidth;
		m_height = m_fbHeight;
#endif
	}
	else
	{
		// Pixel Overlay Model??
	}

	m_bufferSize = m_width * m_height * 4; // RGBA
	m_buffer = new unsigned char[m_bufferSize];

	m_runLoop = true;
	m_imagePrepped = false;
	m_imageDrawn = false;
	m_prepThread = new std::thread(StartPrepLoopThread, this);
	
	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryImage::StartPlaying(void)
{
	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	if (m_imagePrepped)
		Draw();
	else
		m_prepSignal.notify_all();

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryImage::Process(void)
{
	if (m_imagePrepped && !m_imageDrawn)
		Draw();

	if (m_imageDrawn)
		FinishPlay();

	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryImage::Stop(void)
{
	FinishPlay();

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryImage::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Image Path     : %s\n", m_imagePath.c_str());
	LogDebug(VB_PLAYLIST, "Output Device  : %s\n", m_device.c_str());

#ifndef USE_X11VSFB
	FrameBuffer::Dump();
#endif
}

/*
 *
 */
Json::Value PlaylistEntryImage::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["imagePath"] = m_imagePath;
	result["device"] = m_device;

#ifndef USE_X11VSFB
	FrameBuffer::GetConfig(result);
#endif

	return result;
}

/*
 *
 */
void PlaylistEntryImage::SetFileList(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryImage::SetFileList()\n");

	m_files.clear();

	if (is_regular_file(m_imagePath))
	{
		m_files.push_back(m_imagePath);
		return;
	}

	if (is_directory(m_imagePath))
	{
		namespace fs = boost::filesystem;

		fs::path apk_path(m_imagePath);
		fs::recursive_directory_iterator end;

		for (fs::recursive_directory_iterator i(apk_path); i != end; ++i)
		{
			const fs::path cp = (*i);
			std::string entry = cp.string();
			if ((boost::iends_with(entry, ".gif")) ||
				(boost::iends_with(entry, ".bmp")) ||
				(boost::iends_with(entry, ".jpeg")) ||
				(boost::iends_with(entry, ".jpg")) ||
				(boost::iends_with(entry, ".png")))
				m_files.push_back(entry);
		}

		LogDebug(VB_PLAYLIST, "%d images in directory\n", m_files.size());

		return;
	}

	// FIXME, handle file glob
}

/*
 *
 */
const std::string PlaylistEntryImage::GetNextFile(void)
{
	std::string result;

	if (!m_files.size())
		SetFileList();
	
	if (!m_files.size())
	{
		LogWarn(VB_PLAYLIST, "No files found in GetNextFile()\n");
		return result;
	}

	int i = rand_r(&m_fileSeed) % m_files.size();
	result = m_files[i];
	m_files.erase(m_files.begin() + i);

	LogDebug(VB_PLAYLIST, "GetNextFile() = %s\n", result.c_str());

	return result;
}

/*
 *
 */
void PlaylistEntryImage::PrepImage(void)
{
LogDebug(VB_PLAYLIST, "PlaylistEntryImage::PrepImage()\n");
	Image image;
	Blob blob;
	bool needToCache = true;
	std::string nextFile = GetNextFile();
LogDebug(VB_PLAYLIST, "nextFile = %s\n", nextFile.c_str());

	if (nextFile == "")
		return;

	if (nextFile == m_lastFileName)
	{
LogDebug(VB_PLAYLIST, "nextFile same as m_lastFileName: %s\n", nextFile.c_str());
		m_imagePrepped = true;
		return;
	}

LogDebug(VB_PLAYLIST, "locking m_bufferLock\n");
	m_bufferLock.lock();

	m_lastFileName = nextFile;

LogDebug(VB_PLAYLIST, "memset()\n");
	memset(m_buffer, 0, m_bufferSize);

	try {
LogDebug(VB_PLAYLIST, "image.quiet()\n");
		image.quiet(true); // Squelch warning exceptions

LogDebug(VB_PLAYLIST, "checking cache\n");
		if (GetImageFromCache(nextFile, image))
		{
LogDebug(VB_PLAYLIST, "loaded image from cache\n");
			needToCache = false;
		}
		else
		{
LogDebug(VB_PLAYLIST, "image.read(%s)\n", nextFile.c_str());
			image.read(nextFile.c_str());
		}

LogDebug(VB_PLAYLIST, "image.columns()\n");
		int cols = image.columns();
LogDebug(VB_PLAYLIST, "image.rows()\n");
		int rows = image.rows();

		if ((cols != m_width) && (rows != m_height))
		{
LogDebug(VB_PLAYLIST, "need to resize %dx%d image to %dx%d\n", cols, rows, m_width, m_height);
			needToCache = true;

#ifndef OLDGRAPHICSMAGICK
//			image.autoOrient();
#endif

LogDebug(VB_PLAYLIST, "image.modifyImage()\n");
			image.modifyImage();

			// Resize to slightly larger since trying to get exact can
			// leave us off by one pixel.  Going slightly larger will let
			// us crop to exact size later.
LogDebug(VB_PLAYLIST, "image.resize()\n");
			image.resize(Geometry(m_width + 2, m_height + 2, 0, 0));

LogDebug(VB_PLAYLIST, "image.columns()\n");
			cols = image.columns();
LogDebug(VB_PLAYLIST, "image.rows()\n");
			rows = image.rows();

			if (cols < m_width)       // center horizontally
			{
				int diff = m_width - cols;

LogDebug(VB_PLAYLIST, "image.borderColor()\n");
				image.borderColor(Color("black"));
LogDebug(VB_PLAYLIST, "image.border()\n");
				image.border(Geometry(diff / 2 + 1, 0, 0, 0));
			}
			else if (rows < m_height) // center vertically
			{
				int diff = m_height - rows;

LogDebug(VB_PLAYLIST, "image.borderColor()\n");
				image.borderColor(Color("black"));
LogDebug(VB_PLAYLIST, "image.border()\n");
				image.border(Geometry(0, diff / 2 + 1, 0, 0));
			}

LogDebug(VB_PLAYLIST, "image.crop()\n");
			image.crop(Geometry(m_width, m_height, 0, 0));
		}

LogDebug(VB_PLAYLIST, "image.type()\n");
		image.type(TrueColorType);

		if (needToCache)
		{
LogDebug(VB_PLAYLIST, "caching image\n");
			CacheImage(nextFile, image);
		}

LogDebug(VB_PLAYLIST, "image.magick()\n");
		image.magick("RGBA");
LogDebug(VB_PLAYLIST, "image.write()\n");
		image.write(&blob);

LogDebug(VB_PLAYLIST, "memcpy()\n");
		memcpy(m_buffer, blob.data(), m_bufferSize);
	}
	catch( Exception &error_ )
	{
		LogErr(VB_PLAYLIST, "GraphicsMagick exception reading %s: %s\n",
			nextFile.c_str(), error_.what());
	}

#ifdef USE_X11VSFB
LogDebug(VB_PLAYLIST, "converting RGBA to BGRA for X11\n");
	// RGBA -> BGRA
	unsigned char *R = m_buffer;
	unsigned char *B = R + 2;
	unsigned char T;
	int pixels = m_width * m_height;
	for (int i = 0; i < pixels; i++)
	{
		T = *R;
		*R = *B;
		*B = T;

		R += 4;
		B += 4;
	}
#endif

LogDebug(VB_PLAYLIST, "unlocking m_bufferLock\n");
	m_bufferLock.unlock();
LogDebug(VB_PLAYLIST, "setting m_imagePrepped = true\n");
	m_imagePrepped = true;
LogDebug(VB_PLAYLIST, "imagePrep() DONE\n");
}

/*
 *
 */
void PlaylistEntryImage::Draw(void)
{
	m_bufferLock.lock();

	if ((m_device == "/dev/fb0") || (m_device == "/dev/fb1"))
	{
#ifdef USE_X11VSFB
		memcpy(m_imageData, m_buffer, m_bufferSize);

		XLockDisplay(m_display);
		XPutImage(m_display, m_window, m_gc, m_xImage, 0, 0, 0, 0, m_width, m_height);
		XSync(m_display, True);
		XFlush(m_display);
		XUnlockDisplay(m_display);
#else
		FBCopyData(m_buffer);
		FBStartDraw(); // Actual draw runs in another thread
#endif
	}
	else
	{
		// Pixel Overlay Model?
	}

	m_bufferLock.unlock();

	m_imageDrawn = true;

	m_imagePrepped = false;
	m_prepSignal.notify_all();
}

/*
 *
 */
void StartPrepLoopThread(PlaylistEntryImage *pe)
{
	pe->PrepLoop();
}

void PlaylistEntryImage::PrepLoop(void)
{
	std::unique_lock<std::mutex> lock(m_bufferLock);

	while (m_runLoop)
	{
		if (!m_imagePrepped)
		{
			m_bufferLock.unlock();

#ifndef USE_X11VSFB
			// Wait until the FrameBuffer image transition is done before
			// using a bunch of CPU prepping the next image.
			while (m_imageReady)
				usleep(250000);
#endif

			PrepImage();
			m_bufferLock.lock();
		}

		m_prepSignal.wait(lock);
	}
}

/*
 *
 */
std::string PlaylistEntryImage::GetCacheFileName(std::string fileName)
{
    std::size_t found = fileName.find_last_of("/");
	std::string baseFile = fileName.substr(found + 1);
	std::string result = m_cacheDir;

	result += "/pei-";
	result += baseFile;
	result += ".png";

	return result;
}

/*
 *
 */
bool PlaylistEntryImage::GetImageFromCache(std::string fileName, Image &image)
{
	std::string cacheFile = GetCacheFileName(fileName);
	struct stat os;
	struct stat cs;

	if (FileExists(cacheFile))
	{
		stat(fileName.c_str(), &os);
		stat(cacheFile.c_str(), &cs);

		if (os.st_mtime > cs.st_mtime)
{
LogDebug(VB_PLAYLIST, "Source image is newer than cached file\n");
			return false;
}

		try {
LogDebug(VB_PLAYLIST, "Reading cached file\n");
			image.read(cacheFile.c_str());
		}
		catch( Exception &error_ )
		{
			LogErr(VB_PLAYLIST, "GraphicsMagick exception reading %s: %s\n",
				cacheFile.c_str(), error_.what());
		}

		return true;
	}
else
{
LogDebug(VB_PLAYLIST, "cache file %s does not exist\n", cacheFile.c_str());
}

	return false;
}

/*
 *
 */
void PlaylistEntryImage::CacheImage(std::string fileName, Image &image)
{
	std::string cacheFile = GetCacheFileName(fileName);

	image.write(cacheFile.c_str());

	CleanupCache();
}

/*
 *
 */
void PlaylistEntryImage::CleanupCache(void)
{
	// FIXME
}

