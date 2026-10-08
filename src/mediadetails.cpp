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

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tstring.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "common.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"

#include "mediadetails.h"

MediaDetails MediaDetails::INSTANCE;

MediaDetails::MediaDetails() :
    year(0), track(0), length(0), seconds(0), minutes(0), bitrate(0), sampleRate(0), channels(0) {
}
MediaDetails::~MediaDetails() {
}

void MediaDetails::Clear() {
    title.clear();
    artist.clear();
    album.clear();
    year = 0;
    comment.clear();
    track = 0;
    genre.clear();

    length = 0;
    seconds = 0;
    minutes = 0;

    bitrate = 0;
    sampleRate = 0;
    channels = 0;
}

// TagLib only reads a file's header, and some files carry no length there: a
// fragmented MP4 (an empty moov followed by moof/mdat fragments) declares a
// duration of zero, and finding the real one means walking every fragment --
// seconds of work on a single-core board, far too slow to do as a track
// starts.  The web UI has already done it: it runs ffprobe on every media file
// and caches the result in config/media_durations.cache, keyed by the name
// relative to the music or video directory and checked against the file size.
// Take that whenever it is longer than what TagLib found.
static void UseCachedProbeDuration(MediaDetails& details, const std::string& fullPath) {
    std::string name;
    for (const std::string& dir : { FPP_DIR_MUSIC("/"), FPP_DIR_VIDEO("/") }) {
        if (startsWith(fullPath, dir)) {
            name = fullPath.substr(dir.size());
            break;
        }
    }
    std::string cacheFile = FPP_DIR_CONFIG("/media_durations.cache");
    struct stat st;
    if (name.empty() || !FileExists(cacheFile) || stat(fullPath.c_str(), &st) != 0) {
        return;
    }
    Json::Value cache;
    if (!LoadJsonFromFile(cacheFile, cache) || !cache.isObject() || !cache.isMember(name)) {
        return;
    }
    // PHP writes the size as a number and ffprobe's duration as a string, but
    // accept either for both.
    auto number = [](const Json::Value& v) {
        return v.isString() ? atof(v.asString().c_str()) : (v.isNumeric() ? v.asDouble() : 0.0);
    };
    const Json::Value& entry = cache[name];
    if ((off_t)number(entry["filesize"]) != st.st_size) {
        return;
    }
    int ms = (int)(number(entry["duration"]) * 1000.0);
    if (ms <= details.lengthMS) {
        return;
    }
    LogDebug(VB_MEDIAOUT, "  Length %d ms from the media duration cache (TagLib found %d ms)\n", ms, details.lengthMS);
    details.lengthMS = ms;
    details.length = ms / 1000;
    details.seconds = details.length % 60;
    details.minutes = details.length / 60;
}

void MediaDetails::ParseMedia(const char* mediaFilename) {
    char fullMediaPath[2048];
    int seconds;
    int minutes;

    if (!mediaFilename)
        return;

    LogDebug(VB_MEDIAOUT, "ParseMedia(%s)\n", mediaFilename);

    if ((mediaFilename[0] == '/') && FileExists(mediaFilename)) {
        if (strlen(mediaFilename) >= sizeof(fullMediaPath)) {
            LogErr(VB_MEDIAOUT, "Unable to parse media details for %s, path name too long\n",
                   mediaFilename);
            return;
        }
        snprintf(fullMediaPath, sizeof(fullMediaPath), "%s", mediaFilename);
    } else {
        if (snprintf(fullMediaPath, 2048, "%s", FPP_DIR_MUSIC("/" + mediaFilename).c_str()) >= 2048) {
            LogErr(VB_MEDIAOUT, "Unable to parse media details for %s, full path name too long\n",
                   mediaFilename);
            return;
        }

        if (!FileExists(fullMediaPath)) {
            if (snprintf(fullMediaPath, 2048, "%s", FPP_DIR_VIDEO("/" + mediaFilename).c_str()) >= 2048) {
                LogErr(VB_MEDIAOUT, "Unable to parse media details for %s, full path name too long\n",
                       mediaFilename);
                return;
            }

            if (!FileExists(fullMediaPath)) {
                LogErr(VB_MEDIAOUT, "Unable to find %s media file to parse meta data\n", mediaFilename);
                return;
            }
        }
    }

    Clear();

    TagLib::FileRef f(fullMediaPath);

    if (f.isNull() || !f.tag()) {
        UseCachedProbeDuration(*this, fullMediaPath);
        return;
    }

    TagLib::Tag* tag = f.tag();

    title = tag->title().toCString(true);
    artist = tag->artist().toCString(true);
    album = tag->album().toCString(true);
    year = tag->year();
    comment = tag->comment().toCString(true);
    track = tag->track();
    genre = tag->genre().toCString(true);

    if (f.audioProperties()) {
        TagLib::AudioProperties* properties = f.audioProperties();

        lengthMS = properties->lengthInMilliseconds();
        length = lengthMS / 1000;
        seconds = length % 60;
        minutes = (length - seconds) / 60;

        bitrate = properties->bitrate();
        sampleRate = properties->sampleRate();
        channels = properties->channels();
    }
    UseCachedProbeDuration(*this, fullMediaPath);
    length = lengthMS / 1000;
    seconds = length % 60;
    minutes = (length - seconds) / 60;

    LogDebug(VB_MEDIAOUT, "  Title        : %s\n", title.c_str());
    LogDebug(VB_MEDIAOUT, "  Artist       : %s\n", artist.c_str());
    LogDebug(VB_MEDIAOUT, "  Album        : %s\n", album.c_str());
    LogDebug(VB_MEDIAOUT, "  Year         : %d\n", year);
    LogDebug(VB_MEDIAOUT, "  Comment      : %s\n", comment.c_str());
    LogDebug(VB_MEDIAOUT, "  Track        : %d\n", track);
    LogDebug(VB_MEDIAOUT, "  Genre        : %s\n", genre.c_str());
    LogDebug(VB_MEDIAOUT, "  Properties:\n");
    LogDebug(VB_MEDIAOUT, "    Length     : %d\n", length);
    LogDebug(VB_MEDIAOUT, "    Seconds    : %d\n", seconds);
    LogDebug(VB_MEDIAOUT, "    Minutes    : %d\n", minutes);
    LogDebug(VB_MEDIAOUT, "    Bitrate    : %d\n", bitrate);
    LogDebug(VB_MEDIAOUT, "    Sample Rate: %d\n", sampleRate);
    LogDebug(VB_MEDIAOUT, "    Channels   : %d\n", channels);

    return;
}
