#pragma once
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

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <string>
#include <thread>

#include "overlays/PixelOverlayModel.h"
#include "PlaylistEntryBase.h"

namespace Magick
{
    class Image;
};

class PlaylistEntryImage : public PlaylistEntryBase {
public:
    PlaylistEntryImage(Playlist* playlist, PlaylistEntryBase* parent = NULL);
    virtual ~PlaylistEntryImage();

    virtual int Init(Json::Value& config) override;

    virtual int StartPlaying(void) override;
    virtual int Process(void) override;
    virtual int Stop(void) override;

    virtual void Dump(void) override;

    virtual Json::Value GetConfig(void) override;

    void PrepLoop(void);

private:
    void SetFileList(void);

    const std::string GetNextFile(void);
    void PrepImage(void);

    void Draw(void);

    std::string GetCacheFileName(std::string fileName);
    bool GetImageFromCache(std::string fileName, Magick::Image& image);
    void CacheImage(std::string fileName, Magick::Image& image);
    void CleanupCache(void);

    std::string m_imagePath;
    std::string m_imageFullPath;
    std::string m_curFileName;
    std::string m_nextFileName;

    std::string m_cacheDir;
    int m_cacheEntries; // # of items
    int m_cacheSize;    // MB used by cached files
    int m_freeSpace;    // MB free on filesystem

    std::vector<std::string> m_files;

    int m_width;
    int m_height;

    std::string m_modelName;
    PixelOverlayModel *m_model = nullptr;
    PixelOverlayState::PixelState m_modelOrigState = PixelOverlayState::PixelState::Disabled;

    unsigned char* m_buffer;
    int m_bufferSize;

    unsigned int m_fileSeed;

    volatile bool m_runLoop;
    volatile bool m_imagePrepped;
    volatile bool m_imageDrawn;

    std::thread* m_prepThread;
    std::mutex m_prepLock;
    std::mutex m_bufferLock;

    std::condition_variable m_prepSignal;
};
