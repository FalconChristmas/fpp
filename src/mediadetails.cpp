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

void MediaDetails::ParseMedia(const char* mediaFilename) {
    char fullMediaPath[2048];
    int seconds;
    int minutes;

    if (!mediaFilename)
        return;

    LogDebug(VB_MEDIAOUT, "ParseMedia(%s)\n", mediaFilename);

    if ((mediaFilename[0] == '/') && FileExists(mediaFilename)) {
        if (strlen(mediaFilename) > 2047) {
            LogErr(VB_MEDIAOUT, "Unable to parse media details for %s, path name too long\n",
                   mediaFilename);
            return;
        }
        strcpy(fullMediaPath, mediaFilename);
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

    if (f.isNull() || !f.tag())
        return;

    TagLib::Tag* tag = f.tag();

    title = tag->title().toCString();
    artist = tag->artist().toCString();
    album = tag->album().toCString();
    year = tag->year();
    comment = tag->comment().toCString();
    track = tag->track();
    genre = tag->genre().toCString();

    if (f.audioProperties()) {
        TagLib::AudioProperties* properties = f.audioProperties();

        lengthMS = properties->lengthInMilliseconds();
        length = properties->length();
        seconds = properties->length() % 60;
        minutes = (properties->length() - seconds) / 60;

        bitrate = properties->bitrate();
        sampleRate = properties->sampleRate();
        channels = properties->channels();
    }

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
