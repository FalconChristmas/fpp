#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "playList.h"
#include "mediadetails.h"
#include "settings.h"
#include "common.h"
#include "log.h"

#include <taglib/tag_c.h>

MediaDetails	 mediaDetails;
extern PlaylistDetails playlistDetails;

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

	mediaDetails.seconds = 0;
	mediaDetails.minutes = 0;

	mediaDetails.bitrate = 0;
	mediaDetails.sampleRate = 0;
	mediaDetails.channels = 0;
}

void ParseMedia()
{
    char fullMediaPath[1024];
	int seconds;
	int minutes;
	TagLib_File *file;
	TagLib_Tag *tag;
	const TagLib_AudioProperties *properties;
	PlaylistEntry *plEntry = &playlistDetails.playList[playlistDetails.currentPlaylistEntry];

	if (!plEntry->songName)
		return;

    LogDebug(VB_MEDIAOUT, "ParseMedia(%s)\n", plEntry->songName);

    if (snprintf(fullMediaPath, 1024, "%s/%s", getMusicDirectory(), plEntry->songName)
        >= 1024)
    {
        LogErr(VB_MEDIAOUT, "Unable to parse media details for %s, full path name too long\n",
            plEntry->songName);
        return;
    }

    if (!FileExists(fullMediaPath))
    {
    	if (snprintf(fullMediaPath, 1024, "%s/%s", getVideoDirectory(), plEntry->songName)
	        >= 1024)
		{
			LogErr(VB_MEDIAOUT, "Unable to parse media details for %s, full path name too long\n",
				plEntry->songName);
			return;
		}

		if (!FileExists(fullMediaPath))
		{
			LogErr(VB_MEDIAOUT, "Unable to find %s media file to parse meta data\n", plEntry->songName);
			return;
		}
    }

	clearPreviousMedia();

	taglib_set_strings_unicode(false);
	file = taglib_file_new(fullMediaPath);

	if (file == NULL)
		return;

	tag = taglib_file_tag(file);
	properties = taglib_file_audioproperties(file);

	if (tag != NULL)
	{
		mediaDetails.title = strdup(taglib_tag_title(tag));
		mediaDetails.artist = strdup(taglib_tag_artist(tag));
		mediaDetails.album = strdup(taglib_tag_album(tag));
		mediaDetails.year = taglib_tag_year(tag);
		mediaDetails.comment = strdup(taglib_tag_comment(tag));
		mediaDetails.track = taglib_tag_track(tag);
		mediaDetails.genre = strdup(taglib_tag_genre(tag));
	}

	if (properties != NULL)
	{
		mediaDetails.seconds = taglib_audioproperties_length(properties) % 60;
		mediaDetails.minutes = (taglib_audioproperties_length(properties) - seconds) / 60;

		mediaDetails.bitrate = taglib_audioproperties_bitrate(properties);
		mediaDetails.sampleRate = taglib_audioproperties_samplerate(properties);
		mediaDetails.channels = taglib_audioproperties_channels(properties);
	}

	taglib_tag_free_strings();
	taglib_file_free(file);

	return;
}
