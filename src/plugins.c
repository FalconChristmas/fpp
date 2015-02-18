#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>

#include "events.h"
#include "mediadetails.h"
#include "mediaoutput.h"
#include "settings.h"
#include "plugins.h"
#include "common.h"
#include "log.h"
#include "jansson.h"

typedef struct plugin_s
{
	char *name;
	char *script;
	int mask;
} plugin_t;


// TODO: Maybe make this a linked list so we
// can support more than a hard-coded value.
#define MAX_PLUGINS 10

plugin_t plugins[MAX_PLUGINS];
int plugin_count = 0;

extern MediaDetails	 mediaDetails;
extern PlaylistDetails playlistDetails;

const char *type_to_string[] = {
	"both",
	"media",
	"sequence",
	"pause",
	"video",
	"event",
};

void InitPluginCallbacks(void)
{
	plugin_t *plugin;
	DIR *dp;
	struct dirent *ep;

	dp = opendir(getPluginDirectory());
	if (dp != NULL)
	{
		while (ep = readdir(dp))
		{
			char long_filename[1024];
			int output_pipe[2], pid;
			int location = strstr(ep->d_name,".") - ep->d_name;
			char callbacks_list[1024];
			char *token, *callback_type;

			bzero(&callbacks_list[0], sizeof(callbacks_list));

			// We're one of ".", "..", or hidden, so let's skip
			// this entry in the directory
			if ( location == 0 )
				continue;

			// Let's grab the next plugin entry
			plugin = &plugins[plugin_count];
			plugin->name = strdup(ep->d_name);

			strncpy(long_filename, getPluginDirectory(), sizeof(long_filename));
			strncat(long_filename, "/", sizeof(long_filename) - strlen(long_filename)-1);
			strncat(long_filename, ep->d_name, sizeof(long_filename) - strlen(long_filename)-1);
			strncat(long_filename, "/callbacks", sizeof(long_filename) - strlen(long_filename)-1);

			LogDebug(VB_PLUGIN, "Found Plugin: (%s)\n", ep->d_name);

			int i;
			bool found = false;

			// Supported extensions for now
			char *extensions[] = { ".sh", ".pl", ".php", ".py" };
			char extension_count = 4;

			for (i = 0; i < extension_count; i++)
			{
				char _filename[1024];
				memcpy(_filename, long_filename, sizeof(long_filename));

				strncat(_filename, extensions[i], sizeof(long_filename) - strlen(long_filename)-1);
				if ( FileExists(_filename) )
				{
					found = true;
					break;
				}
			}

			if ( !found )
			{
				LogExcess(VB_PLUGIN, "No callbacks supported by plugin: %s\n", ep->d_name);
				continue;
			}

			char eventScript[1024];
			strcpy(eventScript, getFPPDirectory());
			strncat(eventScript, "/scripts/eventScript", sizeof(eventScript)-strlen(eventScript)-1);

			strncat(long_filename, extensions[i], sizeof(long_filename) - strlen(long_filename)-1);
			LogDebug(VB_PLUGIN, "Processing Callbacks (%s) for %s plugin\n", long_filename, ep->d_name);

			if (pipe(output_pipe) == -1)
			{
				LogErr(VB_PLUGIN, "Failed to make pipe\n");
				exit(EXIT_FAILURE);
			}

			if ((pid = fork()) == -1 )
			{
				LogErr(VB_PLUGIN, "Failed to fork\n");
				exit(EXIT_FAILURE);
			}

			if ( pid == 0 )
			{
				dup2(output_pipe[1], STDOUT_FILENO);
				close(output_pipe[1]);
				execl(eventScript, "eventScript", long_filename, "--list", NULL);

				LogErr(VB_PLUGIN, "We failed to exec our callbacks query!\n");
				exit(EXIT_FAILURE);
			}
			else
			{
				close(output_pipe[1]);
				read(output_pipe[0], callbacks_list, sizeof(callbacks_list));

				char *trimmed = NULL;
				trimmed = trimwhitespace(callbacks_list);

				if ( trimmed )
				{
					LogExcess(VB_PLUGIN, "Callback output: (%s)\n", trimmed);
					free(trimmed);
					trimmed = NULL;
				}

				wait(NULL);
			}

			// Let's parse the comma delimited list of supported callbacks for this plugin.
			token = strtok(callbacks_list, ",");
			while ( token )
			{
				callback_type = trimwhitespace(token);
				if ( !strlen(callback_type) )
				{
					free(callback_type);
					continue;
				}

				if (strcmp(callback_type, "media") == 0)
				{
					LogDebug(VB_PLUGIN, "Plugin %s supports media callback.\n", plugin->name);
					plugin->mask |= MEDIA_CALLBACK;
					if ( plugin->script == NULL )
						plugin->script = strdup(long_filename);
				}
				else if (strcmp(callback_type, "playlist") == 0)
				{
					LogDebug(VB_PLUGIN, "Plugin %s supports playlist callback.\n", plugin->name);
					plugin->mask |= PLAYLIST_CALLBACK;
					if ( plugin->script == NULL )
						plugin->script = strdup(long_filename);
				}
				else if (strcmp(callback_type, "nextplaylist") == 0)
				{
					LogDebug(VB_PLUGIN, "Plugin %s supports nextplaylist callback.\n", plugin->name);
					plugin->mask |= NEXT_PLAYLIST_ENTRY_CALLBACK;
					if ( plugin->script == NULL )
						plugin->script = strdup(long_filename);
				}
				else if (strcmp(callback_type, "event") == 0)
				{
					LogDebug(VB_PLUGIN, "Plugin %s supports nextplaylist callback.\n", plugin->name);
					plugin->mask |= EVENT_ENTRY_CALLBACK;
					if ( plugin->script == NULL )
						plugin->script = strdup(long_filename);
				}

				free(callback_type); callback_type = NULL;
				token = strtok(NULL, ",");
			}

			free(token); token = NULL;
			plugin_count += 1;
		}
		closedir(dp);
	}
	else
	{
		LogWarn(VB_PLUGIN, "Couldn't open the directory %s: (%d): %s\n", getPluginDirectory(), errno, strerror(errno));
	}

	return;
}

//blocking
void MediaCallback(void)
{
	int i;
	for ( i = 0; i < plugin_count; ++i )
	{
		if ( plugins[i].mask & MEDIA_CALLBACK )
		{
			int pid;

			if ((pid = fork()) == -1 )
			{
				LogErr(VB_PLUGIN, "Failed to fork\n");
				exit(EXIT_FAILURE);
			}

			if ( pid == 0 )
			{
				LogDebug(VB_PLUGIN, "Child process, calling %s callback for media : %s\n", plugins[i].name, plugins[i].script);

				char *data = NULL;
				char eventScript[1024];
				memcpy(eventScript, getFPPDirectory(), sizeof(eventScript));
				strncat(eventScript, "/scripts/eventScript", sizeof(eventScript)-strlen(eventScript)-1);

				PlaylistEntry *plEntry = &playlistDetails.playList[playlistDetails.currentPlaylistEntry];
				json_t *root = json_object();

				json_object_set_new(root, "type", json_string(type_to_string[plEntry->type]));

				if (strlen(plEntry->seqName))
					json_object_set_new(root, "Sequence", json_string( plEntry->seqName));
				if (strlen(plEntry->songName))
					json_object_set_new(root, "Media", json_string(plEntry->songName));
				if (mediaDetails.title && strlen(mediaDetails.title))
					json_object_set_new(root, "title", json_string(mediaDetails.title));
				if (mediaDetails.artist && strlen(mediaDetails.artist))
					json_object_set_new(root, "artist", json_string(mediaDetails.artist));
				if (mediaDetails.album && strlen(mediaDetails.album))
					json_object_set_new(root, "album", json_string(mediaDetails.album));
				if (mediaDetails.year)
					json_object_set_new(root, "year", json_integer(mediaDetails.year));
				if (mediaDetails.comment && strlen(mediaDetails.comment))
					json_object_set_new(root, "comment", json_string(mediaDetails.comment));
				if (mediaDetails.track)
					json_object_set_new(root, "track", json_integer(mediaDetails.track));
				if (mediaDetails.genre && strlen(mediaDetails.genre))
					json_object_set_new(root, "genre", json_string(mediaDetails.genre));
				if (mediaDetails.seconds)
					json_object_set_new(root, "seconds", json_integer(mediaDetails.seconds));
				if (mediaDetails.minutes)
					json_object_set_new(root, "minutes", json_integer(mediaDetails.minutes));
				if (mediaDetails.bitrate)
					json_object_set_new(root, "bitrate", json_integer(mediaDetails.bitrate));
				if (mediaDetails.sampleRate)
					json_object_set_new(root, "sampleRate", json_integer(mediaDetails.sampleRate));
				if (mediaDetails.channels)
					json_object_set_new(root, "channels", json_integer(mediaDetails.channels));

				data = json_dumps(root, 0);
				json_decref(root);

				LogWarn(VB_PLUGIN, "Media plugin data: %s\n", data);
				execl(eventScript, "eventScript", plugins[i].script, "--type", "media", "--data", data, NULL);

				free(data); data = NULL;

				LogErr(VB_PLUGIN, "We failed to exec our media callback!\n");
				exit(EXIT_FAILURE);
			}
			else
			{
				LogExcess(VB_PLUGIN, "Media parent process, resuming work.\n");
				wait(NULL);
			}
		}
	}
}

//blocking
void PlaylistCallback(PlaylistDetails *playlistDetails, bool starting)
{
	int i;
	for ( i = 0; i < plugin_count; ++i )
	{
		if ( plugins[i].mask & PLAYLIST_CALLBACK )
		{
			int pid;

			if ((pid = fork()) == -1 )
			{
				LogErr(VB_PLUGIN, "Failed to fork\n");
				exit(EXIT_FAILURE);
			}

			if ( pid == 0 )
			{
				LogDebug(VB_PLUGIN, "Child process, calling %s callback for playlist: %s\n", plugins[i].name, plugins[i].script);

				int j;
				char *data = NULL;
				char eventScript[1024];
				memcpy(eventScript, getFPPDirectory(), sizeof(eventScript));
				strncat(eventScript, "/scripts/eventScript", sizeof(eventScript)-strlen(eventScript)-1);

				json_t *root = json_object();

				for ( j = 0; j < playlistDetails->playListCount; ++j )
				{
					PlaylistEntry plEntry = playlistDetails->playList[j];
					json_t *node = json_object();

					json_object_set_new(node, "type", json_string(type_to_string[plEntry.type]));

					switch(plEntry.type)
					{
						case PL_TYPE_BOTH:
							if ( strlen(plEntry.seqName) )
								json_object_set_new(node, "Sequence", json_string(plEntry.seqName));
							if ( strlen(plEntry.songName) )
								json_object_set_new(node, "Media", json_string(plEntry.songName));
							break;
						case PL_TYPE_MEDIA:
						case PL_TYPE_VIDEO:
							if ( strlen(plEntry.songName) )
								json_object_set_new(node, "Media", json_string(plEntry.songName));
							break;
						case PL_TYPE_SEQUENCE:
							if ( strlen(plEntry.seqName) )
								json_object_set_new(node, "Sequence", json_string(plEntry.seqName));
							break;
						case PL_TYPE_EVENT:
							if ( strlen(plEntry.seqName) )
								json_object_set_new(node, "EventID", json_string(plEntry.eventID));
							break;
						case PL_TYPE_PLUGIN_NEXT:
							json_object_set_new(node, "PluginEvent", json_string(""));
							break;
						default:
							LogWarn(VB_PLUGIN, "Invalid entry type!\n");
							break;
					}

					char sequenceNumber[20] = {0};
					snprintf(&sequenceNumber[strlen(sequenceNumber)],
							sizeof(sequenceNumber)-strlen(sequenceNumber),
							"sequence%d", j);

					json_object_set_new(root, sequenceNumber, node );
				}

				json_object_set_new(root, "Action", json_string((starting == PLAYLIST_STARTING ? "start" : "stop")));

				data = json_dumps(root, 0);
				json_decref(root);

				LogWarn(VB_PLUGIN, "Playlist plugin data: %s\n", data);
				execl(eventScript, "eventScript", plugins[i].script, "--type", "playlist", "--data", data, NULL);

				free(data); data = NULL;

				LogErr(VB_PLUGIN, "We failed to exec our playlist callback!\n");
				exit(EXIT_FAILURE);
			}
			else
			{
				LogExcess(VB_PLUGIN, "Playlist parent process, waiting to resume work.\n");
				wait(NULL);
			}
		}
	}
}

//blocking
int NextPlaylistEntryCallback(const char *plugin_data, int currentPlaylistEntry, int mode, bool repeat, PlaylistEntry *pe)
{
	int i, ret_val;
	for ( i = 0; i < plugin_count; ++i )
	{
		if ( plugins[i].mask & NEXT_PLAYLIST_ENTRY_CALLBACK )
		{
			int output_pipe[2], pid;
			char playlist_entry[512];

			bzero(&playlist_entry[0], sizeof(playlist_entry));

			if (pipe(output_pipe) == -1)
			{
				LogErr(VB_PLUGIN, "Failed to make pipe\n");
				exit(EXIT_FAILURE);
			}

			if ((pid = fork()) == -1 )
			{
				LogErr(VB_PLUGIN, "Failed to fork\n");
				exit(EXIT_FAILURE);
			}

			if ( pid == 0 )
			{
				LogDebug(VB_PLUGIN, "Child process, calling %s callback for nextplaylist: %s\n", plugins[i].name, plugins[i].script);

				char *data = NULL;
				char eventScript[1024];
				memcpy(eventScript, getFPPDirectory(), sizeof(eventScript));
				strncat(eventScript, "/scripts/eventScript", sizeof(eventScript)-strlen(eventScript)-1);

				json_t *root = json_object();

				json_object_set_new(root, "currentPlaylistEntry", json_integer(currentPlaylistEntry));

				char *mode_string = modeToString(mode);
				if (mode_string)
				{
					json_object_set_new( root, "mode", json_string(mode_string));
					free(mode_string); mode_string = NULL;
				}

				json_object_set_new(root, "repeat", json_string((repeat == true ? "true" : "false" )));

				if (strlen(plugin_data))
					json_object_set_new(root, "data", json_string( plugin_data));

				data = json_dumps(root, 0);
				json_decref(root);

				LogWarn(VB_PLUGIN, "NextPlaylist plugin data: %s\n", data);

				dup2(output_pipe[1], STDOUT_FILENO);
				close(output_pipe[1]);
				execl(eventScript, "eventScript", plugins[i].script, "--type", "nextplaylist", "--data", data, NULL);

				free(data); data = NULL;

				LogErr(VB_PLUGIN, "We failed to exec our nextplaylist callback!\n");
				exit(EXIT_FAILURE);
			}
			else
			{
				close(output_pipe[1]);
				read(output_pipe[0], &playlist_entry, sizeof(playlist_entry));

				LogExcess(VB_PLUGIN, "Parsed playlist entry: %s\n", playlist_entry);
				ret_val = ParsePlaylistEntry(playlist_entry, pe);
				//Set our type back to 'P' so we re-parse it next time we pass it in the playlist
				pe->cType = 'P';

				LogExcess(VB_PLUGIN, "NextPlaylist parent process, waiting to resume work.\n");
				wait(NULL);
			}
		}
	}
	return ret_val;
}

//blocking
void EventCallback(char *id, char *impetus)
{
	int i, ret_val;
	for ( i = 0; i < plugin_count; ++i )
	{
		if ( plugins[i].mask & EVENT_ENTRY_CALLBACK )
		{
			int pid;

			if ((pid = fork()) == -1 )
			{
				LogErr(VB_PLUGIN, "Failed to fork\n");
				exit(EXIT_FAILURE);
			}

			if ( pid == 0 )
			{
				LogDebug(VB_PLUGIN, "Child process, calling %s callback for event: %s\n", plugins[i].name, plugins[i].script);

				char *data = NULL;
				char eventScript[1024];
				memcpy(eventScript, getFPPDirectory(), sizeof(eventScript));
				strncat(eventScript, "/scripts/eventScript", sizeof(eventScript)-strlen(eventScript)-1);

				FPPevent *event = LoadEvent(id);

				json_t *root = json_object();

				json_object_set_new(root, "caller", json_string(impetus));
				json_object_set_new(root, "major", json_integer(event->majorID));
				json_object_set_new(root, "minor", json_integer(event->minorID));
				if ( event->name && strlen(event->name) )
					json_object_set_new(root, "name", json_string(event->name));
				if ( event->effect && strlen(event->effect) )
					json_object_set_new(root, "effect", json_string(event->effect));
				json_object_set_new(root, "startChannel", json_integer(event->startChannel));
				if ( event->script && strlen(event->script) )
					json_object_set_new(root, "script", json_string(event->script));

				data = json_dumps(root, 0);
				json_decref(root);

				LogWarn(VB_PLUGIN, "Media plugin data: %s\n", data);
				execl(eventScript, "eventScript", plugins[i].script, "--type", "media", "--data", data, NULL);

				free(data); data = NULL;

				LogErr(VB_PLUGIN, "We failed to exec our media callback!\n");
				exit(EXIT_FAILURE);
			}
			else
			{
				LogExcess(VB_PLUGIN, "Media parent process, resuming work.\n");
				wait(NULL);
			}
		}
	}
}
