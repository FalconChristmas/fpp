#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mediadetails.h"
#include "settings.h"
#include "common.h"
#include "log.h"

#include <taglib/fileref.h>
#include <taglib/tag.h>

MediaDetails	 mediaDetails;

void initMediaDetails()
{
	bzero(&mediaDetails, sizeof(MediaDetails));
}

void clearPreviousMedia()
{
	if (mediaDetails.title)
		free(mediaDetails.title);
	mediaDetails.title = NULL;

	if (mediaDetails.artist)
		free(mediaDetails.artist);
	mediaDetails.artist = NULL;

	if (mediaDetails.album)
		free(mediaDetails.album);
	mediaDetails.album = NULL;

	mediaDetails.year = 0;

	if (mediaDetails.comment)
		free(mediaDetails.comment);
	mediaDetails.comment = NULL;

	mediaDetails.track = 0;

	if (mediaDetails.genre)
		free(mediaDetails.genre);
	mediaDetails.genre = NULL;

	mediaDetails.length = 0;
	mediaDetails.seconds = 0;
	mediaDetails.minutes = 0;

	mediaDetails.bitrate = 0;
	mediaDetails.sampleRate = 0;
	mediaDetails.channels = 0;
}

void ParseMedia(const char *mediaFilename)
{
    char fullMediaPath[1024];
	int seconds;
	int minutes;

	if (!mediaFilename)
		return;

    LogDebug(VB_MEDIAOUT, "ParseMedia(%s)\n", mediaFilename);

    if (snprintf(fullMediaPath, 1024, "%s/%s", getMusicDirectory(), mediaFilename)
        >= 1024)
    {
        LogErr(VB_MEDIAOUT, "Unable to parse media details for %s, full path name too long\n",
            mediaFilename);
        return;
    }

    if (!FileExists(fullMediaPath))
    {
		if (snprintf(fullMediaPath, 1024, "%s/%s", getVideoDirectory(), mediaFilename)
	        >= 1024)
		{
			LogErr(VB_MEDIAOUT, "Unable to parse media details for %s, full path name too long\n",
				mediaFilename);
			return;
		}

		if (!FileExists(fullMediaPath))
		{
			LogErr(VB_MEDIAOUT, "Unable to find %s media file to parse meta data\n", mediaFilename);
			return;
		}
    }

	clearPreviousMedia();

	TagLib::FileRef f(fullMediaPath);

	if (f.isNull() || !f.tag())
		return;

	TagLib::Tag *tag = f.tag();

	mediaDetails.title   = strdup(tag->title().toCString());
	mediaDetails.artist  = strdup(tag->artist().toCString());
	mediaDetails.album   = strdup(tag->album().toCString());
	mediaDetails.year    = tag->year();
	mediaDetails.comment = strdup(tag->comment().toCString());
	mediaDetails.track   = tag->track();
	mediaDetails.genre   = strdup(tag->genre().toCString());

	if (f.audioProperties())
	{
		TagLib::AudioProperties *properties = f.audioProperties();

		mediaDetails.length     = properties->length();
		mediaDetails.seconds    = properties->length() % 60;
		mediaDetails.minutes    = (properties->length() - mediaDetails.seconds) / 60;

		mediaDetails.bitrate    = properties->bitrate();
		mediaDetails.sampleRate = properties->sampleRate();
		mediaDetails.channels   = properties->channels();
	}

	LogDebug(VB_MEDIAOUT, "  Title        : %s\n", mediaDetails.title);
	LogDebug(VB_MEDIAOUT, "  Artist       : %s\n", mediaDetails.artist);
	LogDebug(VB_MEDIAOUT, "  Album        : %s\n", mediaDetails.album);
	LogDebug(VB_MEDIAOUT, "  Year         : %d\n", mediaDetails.year);
	LogDebug(VB_MEDIAOUT, "  Comment      : %s\n", mediaDetails.comment);
	LogDebug(VB_MEDIAOUT, "  Track        : %d\n", mediaDetails.track);
	LogDebug(VB_MEDIAOUT, "  Genre        : %s\n", mediaDetails.genre);
	LogDebug(VB_MEDIAOUT, "  Properties:\n");
	LogDebug(VB_MEDIAOUT, "    Length     : %d\n", mediaDetails.length);
	LogDebug(VB_MEDIAOUT, "    Seconds    : %d\n", mediaDetails.seconds);
	LogDebug(VB_MEDIAOUT, "    Minutes    : %d\n", mediaDetails.minutes);
	LogDebug(VB_MEDIAOUT, "    Bitrate    : %d\n", mediaDetails.bitrate);
	LogDebug(VB_MEDIAOUT, "    Sample Rate: %d\n", mediaDetails.sampleRate);
	LogDebug(VB_MEDIAOUT, "    Channels   : %d\n", mediaDetails.channels);

	return;
}
