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

#include "../common.h"
#include "../log.h"
#include "../settings.h"

#include "PlaylistEntryImage.h"
#include <sys/stat.h>

#include <filesystem>
using namespace std::filesystem;

#include <Magick++.h>
using namespace Magick;

#include "overlays/PixelOverlay.h"

void StartPrepLoopThread(PlaylistEntryImage* fb);

/*
 *
 */
PlaylistEntryImage::PlaylistEntryImage(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_width(800),
    m_height(600),
    m_buffer(NULL),
    m_bufferSize(0) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryImage::PlaylistEntryImage()\n");

    m_type = "image";

    m_fileSeed = (unsigned int)(time(NULL) + ((long long)this & 0xFFFFFFFF));
    m_cacheDir = FPP_DIR_MEDIA("/cache");
    m_cacheEntries = 200;
    m_cacheSize = 1024; // MB
    m_freeSpace = 2048; // MB
}

/*
 *
 */
PlaylistEntryImage::~PlaylistEntryImage() {
    m_runLoop = false;
    m_prepSignal.notify_all();
    if (m_prepThread) {
        m_prepThread->join();
        delete m_prepThread;
    }

    if (m_modelOrigState == PixelOverlayState::PixelState::Disabled) {
        m_model = PixelOverlayManager::INSTANCE.getModel(m_modelName);

        if (m_model) {
            m_model->clear();
            m_model->setState(PixelOverlayState(PixelOverlayState::PixelState::Disabled));
        }
    }
}

/*
 *
 */
int PlaylistEntryImage::Init(Json::Value& config) {
    if (config.isMember("modelName"))
        m_modelName = config["modelName"].asString();

    if (m_modelName == "") {
        LogErr(VB_PLAYLIST, "Empty Pixel Overlay Model name\n");
        return 0;
    }

    m_model = PixelOverlayManager::INSTANCE.getModel(m_modelName);

    if (!m_model) {
        LogErr(VB_PLAYLIST, "Invalid Pixel Overlay Model: '%s'\n", m_modelName.c_str());
        return 0;
    }
    m_width = m_model->getWidth();
    m_height = m_model->getHeight();
    m_modelOrigState = m_model->getState().getState();

    if (m_modelOrigState == PixelOverlayState::PixelState::Disabled)
        m_model->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));

    if (!config.isMember("imagePath")) {
        LogErr(VB_PLAYLIST, "Missing imagePath\n");
        return 0;
    }

    m_imagePath = config["imagePath"].asString();
    if (startsWith(m_imagePath, "/")) {
        m_imageFullPath = m_imagePath;
    } else {
        m_imageFullPath = FPP_DIR_IMAGE("/" + m_imagePath);
    }

    SetFileList();
    CleanupCache();

    m_bufferSize = m_width * m_height * 3; // RGB
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
int PlaylistEntryImage::StartPlaying(void) {
    if (!CanPlay()) {
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
int PlaylistEntryImage::Process(void) {
    if (m_imagePrepped && !m_imageDrawn)
        Draw();

    if (m_imageDrawn)
        FinishPlay();

    return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryImage::Stop(void) {
    FinishPlay();

    return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryImage::Dump(void) {
    PlaylistEntryBase::Dump();

    LogDebug(VB_PLAYLIST, "Image Path     : %s\n", m_imagePath.c_str());
    LogDebug(VB_PLAYLIST, "Image Full Path: %s\n", m_imageFullPath.c_str());
    LogDebug(VB_PLAYLIST, "Model Name     : %s\n", m_modelName.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryImage::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();

    result["imagePath"] = m_imagePath;
    result["modelName"] = m_modelName;

    std::size_t found = m_curFileName.find_last_of("/");
    if (found > 0)
        result["imageFilename"] = m_curFileName.substr(found + 1);
    else
        result["imageFilename"] = m_curFileName;

    return result;
}

/*
 *
 */
void PlaylistEntryImage::SetFileList(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryImage::SetFileList()\n");

    m_files.clear();

    if (is_regular_file(m_imageFullPath)) {
        m_files.push_back(m_imageFullPath);
        return;
    }

    if (is_directory(m_imageFullPath)) {
        for (auto& cp : recursive_directory_iterator(m_imageFullPath)) {
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
const std::string PlaylistEntryImage::GetNextFile(void) {
    std::string result;

    if (!m_files.size())
        SetFileList();

    if (!m_files.size()) {
        LogWarn(VB_PLAYLIST, "No files found in GetNextFile()\n");
        return result;
    }

    int i = rand_r(&m_fileSeed) % m_files.size();
    result = m_files[i];
    m_files.erase(m_files.begin() + i);

    if (!FileExists(result))
        return GetNextFile(); // Recurse to handle refilling empty list

    LogDebug(VB_PLAYLIST, "GetNextFile() = %s\n", result.c_str());

    return result;
}

/*
 *
 */
void PlaylistEntryImage::PrepImage(void) {
    Image image;
    Blob blob;
    bool needToCache = true;
    std::string nextFile = GetNextFile();

    if (nextFile == "")
        return;

    if (nextFile == m_nextFileName) {
        m_imagePrepped = true;
        return;
    }

    m_bufferLock.lock();

    m_nextFileName = nextFile;

    memset(m_buffer, 0, m_bufferSize);

    try {
        int cols = 0;
        int rows = 0;

        needToCache = false;
        image.quiet(true); // Squelch warning exceptions

        if (GetImageFromCache(nextFile, image)) {
            cols = image.columns();
            rows = image.rows();
        }

        if ((cols != m_width) || (rows != m_height)) {
            image.read(nextFile.c_str());
            image.autoOrient();
            cols = image.columns();
            rows = image.rows();
        }

        if ((cols != m_width) && (rows != m_height)) {
            needToCache = true;

            image.modifyImage();

            // Resize to slightly larger since trying to get exact can
            // leave us off by one pixel.  Going slightly larger will let
            // us crop to exact size later.
            image.resize(Geometry(m_width + 4, m_height + 4, 0, 0));

            cols = image.columns();
            rows = image.rows();

            if (cols < m_width) // center horizontally
            {
                int diff = m_width - cols;

                image.borderColor(Color("black"));
                image.border(Geometry(diff / 2 + 1, 1, 0, 0));
            } else if (rows < m_height) // center vertically
            {
                int diff = m_height - rows;

                image.borderColor(Color("black"));
                image.border(Geometry(1, diff / 2 + 1, 0, 0));
            }

            image.crop(Geometry(m_width, m_height, 1, 1));
        }

        image.type(TrueColorType);

        if (needToCache) {
            CacheImage(nextFile, image);
        }

        image.magick("RGB");
        image.write(&blob);

        memcpy(m_buffer, blob.data(), m_bufferSize);
    } catch (Exception& error_) {
        LogErr(VB_PLAYLIST, "GraphicsMagick exception reading %s: %s\n",
               nextFile.c_str(), error_.what());
    }

    m_bufferLock.unlock();
    m_imagePrepped = true;
}

/*
 *
 */
void PlaylistEntryImage::Draw(void) {
    m_bufferLock.lock();

    // We need to look this up again since the model may have disappeared
    m_model = PixelOverlayManager::INSTANCE.getModel(m_modelName);

    if (!m_model) {
        LogErr(VB_PLAYLIST, "Invalid Pixel Overlay Model: '%s'\n", m_modelName.c_str());
        return;
    }

    m_model->setData(m_buffer);

    m_bufferLock.unlock();

    m_imageDrawn = true;

    m_imagePrepped = false;
    m_prepSignal.notify_all();
}

/*
 *
 */
void StartPrepLoopThread(PlaylistEntryImage* pe) {
    pe->PrepLoop();
}

void PlaylistEntryImage::PrepLoop(void) {
    SetThreadName("FPP-ImageLoop");
    std::unique_lock<std::mutex> lock(m_bufferLock);
    while (m_runLoop) {
        if (!m_imagePrepped) {
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
std::string PlaylistEntryImage::GetCacheFileName(std::string fileName) {
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
bool PlaylistEntryImage::GetImageFromCache(std::string fileName, Image& image) {
    std::string cacheFile = GetCacheFileName(fileName);
    struct stat os;
    struct stat cs;

    if (FileExists(cacheFile)) {
        stat(fileName.c_str(), &os);
        stat(cacheFile.c_str(), &cs);

        if (os.st_mtime > cs.st_mtime)
            return false;

        try {
            image.read(cacheFile.c_str());
        } catch (Exception& error_) {
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
void PlaylistEntryImage::CacheImage(std::string fileName, Image& image) {
    std::string cacheFile = GetCacheFileName(fileName);

    image.write(cacheFile.c_str());

    CleanupCache();
}

/*
 *
 */
void PlaylistEntryImage::CleanupCache(void) {
    // FIXME
}
