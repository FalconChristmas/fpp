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

#if __GNUC__ >= 8
#  include <filesystem>
using namespace std::filesystem;
#else
#  include <experimental/filesystem>
using namespace std::experimental::filesystem;
#endif

#include "common.h"
#include "log.h"
#include "PlaylistEntryImage.h"

void StartPrepLoopThread(PlaylistEntryImage *fb);

/*
 *
 */
PlaylistEntryImage::PlaylistEntryImage(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent), FrameBuffer(),
	m_width(800),
	m_height(600),
	m_buffer(NULL),
	m_bufferSize(0)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryImage::PlaylistEntryImage()\n");

	m_type = "image";
	m_device = "fb0";

	m_fileSeed = (unsigned int)time(NULL);
	m_cacheDir = "/home/fpp/media/cache";
	m_cacheEntries = 200;
	m_cacheSize = 1024; // MB
	m_freeSpace = 2048; // MB
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

	if ((m_device == "/dev/fb0") || (m_device == "/dev/fb1") || (m_device == "x11"))
	{
		config["dataFormat"] = "RGBA";
		FBInit(config);

		m_width = m_fbWidth;
		m_height = m_fbHeight;
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

	FrameBuffer::Dump();
}

/*
 *
 */
Json::Value PlaylistEntryImage::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["imagePath"] = m_imagePath;
	result["device"] = m_device;

	std::size_t found = m_curFileName.find_last_of("/");
	if (found > 0)
		result["imageFilename"] = m_curFileName.substr(found + 1);
	else
		result["imageFilename"] = m_curFileName;

	FrameBuffer::GetConfig(result);

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
        for (auto &cp : recursive_directory_iterator(m_imagePath)) {
            std::string entry = cp.path().string();
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
	Image image;
	Blob blob;
	bool needToCache = true;
	std::string nextFile = GetNextFile();

	if (nextFile == "")
		return;

	if (nextFile == m_nextFileName)
	{
		m_imagePrepped = true;
		return;
	}

	m_bufferLock.lock();

	m_nextFileName = nextFile;

	memset(m_buffer, 0, m_bufferSize);

	try {
		image.quiet(true); // Squelch warning exceptions

		if (GetImageFromCache(nextFile, image))
		{
			needToCache = false;
		}
		else
		{
			image.read(nextFile.c_str());
		}

		int cols = image.columns();
		int rows = image.rows();

		if ((cols != m_width) && (rows != m_height))
		{
			needToCache = true;

#ifndef OLDGRAPHICSMAGICK
//			image.autoOrient();
#endif

			image.modifyImage();

			// Resize to slightly larger since trying to get exact can
			// leave us off by one pixel.  Going slightly larger will let
			// us crop to exact size later.
			image.resize(Geometry(m_width + 2, m_height + 2, 0, 0));

			cols = image.columns();
			rows = image.rows();

			if (cols < m_width)       // center horizontally
			{
				int diff = m_width - cols;

				image.borderColor(Color("black"));
				image.border(Geometry(diff / 2 + 1, 0, 0, 0));
			}
			else if (rows < m_height) // center vertically
			{
				int diff = m_height - rows;

				image.borderColor(Color("black"));
				image.border(Geometry(0, diff / 2 + 1, 0, 0));
			}

			image.crop(Geometry(m_width, m_height, 0, 0));
		}

		image.type(TrueColorType);

		if (needToCache)
		{
			CacheImage(nextFile, image);
		}

		image.magick("RGBA");
		image.write(&blob);

		memcpy(m_buffer, blob.data(), m_bufferSize);
	}
	catch( Exception &error_ )
	{
		LogErr(VB_PLAYLIST, "GraphicsMagick exception reading %s: %s\n",
			nextFile.c_str(), error_.what());
	}

	m_bufferLock.unlock();
	m_imagePrepped = true;
}

/*
 *
 */
void PlaylistEntryImage::Draw(void)
{
	m_bufferLock.lock();

	if ((m_device == "/dev/fb0") || (m_device == "/dev/fb1") || (m_device == "x11"))
	{
		FBCopyData(m_buffer);
		FBStartDraw(); // Actual draw runs in another thread
		m_curFileName = m_nextFileName;
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
			lock.unlock();

			PrepImage();
			lock.lock();
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
			return false;

		try {
			image.read(cacheFile.c_str());
		}
		catch( Exception &error_ )
		{
			LogErr(VB_PLAYLIST, "GraphicsMagick exception reading %s: %s\n",
				cacheFile.c_str(), error_.what());
		}

		return true;
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

