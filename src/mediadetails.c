#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mediadetails.h"
#include "settings.h"
#include "common.h"
#include "log.h"

#include <taglib/fileref.h>
#include <taglib/tag.h>

MediaDetails MediaDetails::INSTANCE;

MediaDetails::MediaDetails()
: year(0), track(0), length(0), seconds(0), minutes(0),
 bitrate(0), sampleRate(0), channels(0)
{
}
MediaDetails::~MediaDetails() {
    
}

void MediaDetails::clearPreviousMedia()
{
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

void MediaDetails::ParseMedia(const char *mediaFilename)
{
    char fullMediaPath[1024];
	int seconds;
	int minutes;

	if (!mediaFilename)
		return;

    LogDebug(VB_MEDIAOUT, "ParseMedia(%s)\n", mediaFilename);

    if (snprintf(fullMediaPath, 1024, "%s/%s", getMusicDirectory(), mediaFilename) >= 1024) {
        LogErr(VB_MEDIAOUT, "Unable to parse media details for %s, full path name too long\n",
            mediaFilename);
        return;
    }

    if (!FileExists(fullMediaPath)) {
		if (snprintf(fullMediaPath, 1024, "%s/%s", getVideoDirectory(), mediaFilename) >= 1024) {
			LogErr(VB_MEDIAOUT, "Unable to parse media details for %s, full path name too long\n",
				mediaFilename);
			return;
		}

		if (!FileExists(fullMediaPath)) {
			LogErr(VB_MEDIAOUT, "Unable to find %s media file to parse meta data\n", mediaFilename);
			return;
		}
    }

	clearPreviousMedia();

	TagLib::FileRef f(fullMediaPath);

	if (f.isNull() || !f.tag())
		return;

	TagLib::Tag *tag = f.tag();

	title   = tag->title().toCString();
	artist  = tag->artist().toCString();
	album   = tag->album().toCString();
	year    = tag->year();
	comment = tag->comment().toCString();
	track   = tag->track();
	genre   = tag->genre().toCString();

	if (f.audioProperties()) {
		TagLib::AudioProperties *properties = f.audioProperties();

		length     = properties->length();
		seconds    = properties->length() % 60;
		minutes    = (properties->length() - seconds) / 60;

		bitrate    = properties->bitrate();
		sampleRate = properties->sampleRate();
		channels   = properties->channels();
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
