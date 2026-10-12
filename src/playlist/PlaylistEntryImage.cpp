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

#include "fpp-json.h"

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds

#include "../common.h"
#include "../log.h"
#include "../settings.h"

#include "PlaylistEntryImage.h"
#include <sys/stat.h>

#include <cstdio>
#include <filesystem>
#include <functional>
using namespace std::filesystem;

#include <Magick++.h>
using namespace Magick;

#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayEffects.h" // ScaleOverlayImage - shared sizing

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
    {
        // Under the lock: PrepLoop() checks m_runLoop and then waits, and a
        // write between the two was a lost wakeup -- the prep thread slept on
        // and the join() below hung whichever thread was deleting the entry.
        std::lock_guard<std::mutex> lock(m_bufferLock);
        m_runLoop = false;
    }
    m_prepSignal.notify_all();
    if (m_prepThread) {
        m_prepThread->join();
        delete m_prepThread;
    }

    // Freed only after the prep thread is joined above, so no PrepImage() write
    // to m_buffer can race this.  m_buffer is NULL until Init() allocates it, so
    // delete[] is a safe no-op on an entry that failed (or never ran) Init.
    delete[] m_buffer;

    if (m_modelOrigState == PixelOverlayState::PixelState::Disabled) {
        m_model = PixelOverlayManager::INSTANCE.getModelShared(m_modelName);

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
        WarningHolder::AddWarningTimeout(60, 35, "Playlist image entry has an empty Pixel Overlay Model name");
        return 0;
    }

    m_model = PixelOverlayManager::INSTANCE.getModelShared(m_modelName);

    if (!m_model) {
        LogErr(VB_PLAYLIST, "Invalid Pixel Overlay Model: '%s'\n", m_modelName.c_str());
        WarningHolder::AddWarningTimeout(60, 35, "Invalid Pixel Overlay Model: '" + m_modelName + "'");
        return 0;
    }
    m_width = m_model->getWidth();
    m_height = m_model->getHeight();
    m_modelOrigState = m_model->getState().getState();

    if (m_modelOrigState == PixelOverlayState::PixelState::Disabled)
        m_model->setState(PixelOverlayState(PixelOverlayState::PixelState::Enabled));

    if (!config.isMember("imagePath")) {
        LogErr(VB_PLAYLIST, "Missing imagePath\n");
        WarningHolder::AddWarningTimeout(60, 34, "Playlist image entry is missing the image path");
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

    // Each pass draws afresh.  m_imageDrawn was only ever set, so on a repeat
    // whose image wasn't ready yet the entry "finished" without drawing.
    m_imageDrawn = false;

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
    // At most one refill per call.  This used to recurse once per missing
    // file and refill whenever the list ran dry, so a directory whose files
    // had all gone (broken links) recursed until the stack overflowed.
    bool refilled = false;
    while (true) {
        if (!m_files.size()) {
            if (refilled)
                break;
            SetFileList();
            refilled = true;
            if (!m_files.size())
                break;
        }

        int i = rand_r(&m_fileSeed) % m_files.size();
        std::string result = m_files[i];
        m_files.erase(m_files.begin() + i);

        if (FileExists(result)) {
            LogDebug(VB_PLAYLIST, "GetNextFile() = %s\n", result.c_str());
            return result;
        }
    }

    LogWarn(VB_PLAYLIST, "No files found in GetNextFile()\n");
    return "";
}

/*
 *
 */
void PlaylistEntryImage::PrepImage(void) {
    Image image;
    Blob blob;
    bool needToCache = true;
    std::string nextFile = GetNextFile();

    if (nextFile == "") {
        m_noImage = true;
        m_imagePrepped = true;
        return;
    }
    m_noImage = false;

    if (nextFile == m_nextFileName) {
        m_imagePrepped = true;
        return;
    }

    // The GraphicsMagick decode/resize/cache work below only touches locals
    // (image, blob) and the on-disk cache — m_bufferLock is needed solely for
    // the m_buffer/m_nextFileName updates at the end.  Decoding outside the
    // lock keeps Draw() (playlist thread) from blocking behind a slow disk
    // read or resize.
    bool decoded = false;
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

        // Scale when the image differs from the model in EITHER dimension: the
        // RGB blob must be exactly m_width*m_height*3 to fill m_buffer, and
        // ScaleOverlayImage() crops to precisely m_width x m_height.  An image
        // matching one axis but not the other still needs scaling; with && it
        // was left at the wrong size and the guarded memcpy below truncated it,
        // rendering garbled.  Matching both axes still skips (||-of-two-falses).
        if ((cols != m_width) || (rows != m_height)) {
            needToCache = true;

            // Shared with the Image overlay effect so a given image lands on a
            // model identically whether it arrives via a playlist entry or a
            // command.  See ScaleOverlayImage() in PixelOverlayEffects.cpp.
            ScaleOverlayImage(image, m_width, m_height, "Scale to Fit");
        }

        image.type(TrueColorType);

        if (needToCache) {
            CacheImage(nextFile, image);
        }

        image.magick("RGB");
        image.write(&blob);

        decoded = true;
    } catch (Exception& error_) {
        LogErr(VB_PLAYLIST, "GraphicsMagick exception reading %s: %s\n",
               nextFile.c_str(), error_.what());
        WarningHolder::AddWarningTimeout(60, 34, "Could not read playlist image file: " + nextFile);
    } catch (const std::exception& ex) {
        // Not everything that can throw here is a Magick::Exception
        // (std::bad_alloc, filesystem errors).  The old code held
        // m_bufferLock via raw lock()/unlock() with only the Magick catch,
        // so any other exception leaked the mutex and wedged both Draw()
        // and this prep thread permanently.
        LogErr(VB_PLAYLIST, "Exception preparing image %s: %s\n",
               nextFile.c_str(), ex.what());
        WarningHolder::AddWarningTimeout(60, 34, "Could not read playlist image file: " + nextFile);
    }

    {
        std::lock_guard<std::mutex> lock(m_bufferLock);
        m_nextFileName = nextFile;
        memset(m_buffer, 0, m_bufferSize);
        if (decoded) {
            // Guard the copy length — a short blob previously read past the
            // end of the blob's allocation.
            size_t len = blob.length() < (size_t)m_bufferSize ? blob.length() : (size_t)m_bufferSize;
            memcpy(m_buffer, blob.data(), len);
        }
    }
    m_imagePrepped = true;
}

/*
 *
 */
void PlaylistEntryImage::Draw(void) {
    {
        // RAII guard: the old raw lock()/unlock() code returned from the
        // !m_model branch with m_bufferLock still held; m_imageDrawn stayed
        // false, so the next Process() tick called Draw() again and the
        // playlist thread re-locked a mutex it already owned — a permanent
        // wedge whenever the overlay model disappeared mid-playlist.
        std::lock_guard<std::mutex> lock(m_bufferLock);

        // We need to look this up again since the model may have disappeared
        m_model = PixelOverlayManager::INSTANCE.getModelShared(m_modelName);

        if (!m_model) {
            // The model went away mid-playlist.  Finish the entry rather than
            // retrying (and re-raising the warning) on every tick forever.
            LogErr(VB_PLAYLIST, "Invalid Pixel Overlay Model: '%s'\n", m_modelName.c_str());
            WarningHolder::AddWarningTimeout(60, 35, "Invalid Pixel Overlay Model: '" + m_modelName + "'");
            m_imageDrawn = true;
            return;
        }

        if (m_noImage) {
            // Nothing to show; count it as drawn so the entry finishes.
            LogWarn(VB_PLAYLIST, "No image found for '%s', skipping\n", m_imagePath.c_str());
        } else {
            // m_buffer is m_width x m_height RGB.  setData() reads the model's
            // own pixel size -- 4 bytes on an RGBW model, a third past the end
            // of this buffer -- and assumes the model is still the size it was
            // at Init().  blitData() converts to the model's format and clips.
            m_model->blitData(m_buffer, m_width, m_height, 3, 0, 0,
                              PixelOverlayState(PixelOverlayState::Enabled));
            // Reported as imageFilename in status; it was never set.
            m_curFileName = m_nextFileName;
        }

        // Under the lock for the same lost-wakeup reason as the destructor.
        m_imageDrawn = true;
        m_imagePrepped = false;
    }

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

        // The predicate guards against a lost wakeup: the destructor sets
        // m_runLoop=false and notifies while this thread is inside
        // PrepImage() (lock released above); a bare wait() would then sleep
        // forever and the destructor's join() would hang whichever thread
        // is deleting this entry.
        m_prepSignal.wait(lock, [this] { return !m_runLoop || !m_imagePrepped; });
    }
}

/*
 *
 */
std::string PlaylistEntryImage::GetCacheFileName(std::string fileName) {
    std::size_t found = fileName.find_last_of("/");
    std::string baseFile = fileName.substr(found + 1);
    std::string result = m_cacheDir;

    // Keyed by the full path and the model size, not just the file name: two
    // images with the same name in different folders used to share a cache
    // file and show each other.  The base name stays in it for readability;
    // the "pei-" prefix and ".png" suffix are what CleanupCache() matches.
    char key[64];
    snprintf(key, sizeof(key), "-%016zx-%dx%d", std::hash<std::string>{}(fileName), m_width, m_height);

    result += "/pei-";
    result += baseFile;
    result += key;
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
        // Either stat failing leaves its st_mtime uninitialized; treat that as
        // a cache miss rather than comparing garbage.
        if ((stat(fileName.c_str(), &os) != 0) || (stat(cacheFile.c_str(), &cs) != 0))
            return false;

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
    try {
        if (!exists(m_cacheDir)) {
            return;
        }

        // Collect all cache files with their metadata
        std::vector<std::pair<std::string, std::filesystem::file_time_type>> cacheFiles;
        uintmax_t totalSize = 0;

        for (const auto& entry : directory_iterator(m_cacheDir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                // Only process our cache files
                if (filename.find("pei-") == 0 && filename.find(".png") != std::string::npos) {
                    cacheFiles.push_back({entry.path().string(), last_write_time(entry)});
                    totalSize += file_size(entry);
                }
            }
        }

        // Sort by modification time (oldest first)
        std::sort(cacheFiles.begin(), cacheFiles.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });

        // Remove files if we exceed entry count or size limit
        size_t maxEntries = m_cacheEntries;
        uintmax_t maxSize = static_cast<uintmax_t>(m_cacheSize) * 1024 * 1024; // Convert MB to bytes

        while ((!cacheFiles.empty()) &&
               ((cacheFiles.size() > maxEntries) || (totalSize > maxSize))) {
            // Remove the oldest file
            const auto& oldest = cacheFiles.front();
            uintmax_t fileSize = file_size(oldest.first);
            
            remove(oldest.first);
            LogDebug(VB_PLAYLIST, "Removed cache file: %s (size: %lu bytes)\n",
                     oldest.first.c_str(), fileSize);
            
            totalSize -= fileSize;
            cacheFiles.erase(cacheFiles.begin());
        }
    } catch (std::filesystem::filesystem_error& e) {
        LogErr(VB_PLAYLIST, "Error cleaning up cache: %s\n", e.what());
    }
}
