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

#include "mediadetails.h"
#include "mediaoutput.h"
#include "settings.h"
#include "plugins.h"
#include "common.h"
#include "log.h"

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

//non-blocking
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

				char eventScript[1024];
				strcpy(eventScript, getFPPDirectory());
				strncat(eventScript, "/scripts/eventScript", sizeof(eventScript)-strlen(eventScript)-1);

				// build our data string here
				char data[64*1024]; // "Unreasonably" high to handle all data we're going to pass
				// TODO: parse JSON a little better like escaping special
				// characters if needed, should only be quote that requires
				// escaping, we shouldn't worry about things like tabs,
				// backspaces, form feeds, etc..

				PlaylistEntry *plEntry = &playlistDetails.playList[playlistDetails.currentPlaylistEntry];

				strncpy(&data[0], "{", sizeof(data));
				snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
				"\"type\":\"%s\",", type_to_string[plEntry->type]);
				if ( strlen(plEntry->seqName) )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"Sequence\":\"%s\",", plEntry->seqName);
				if ( strlen(plEntry->songName) )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"Media\":\"%s\",", plEntry->songName);
				if ( mediaDetails.title && strlen(mediaDetails.title) )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"title\":\"%s\",", mediaDetails.title);
				if ( mediaDetails.artist && strlen(mediaDetails.artist) )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"artist\":\"%s\",", mediaDetails.artist);
				if ( mediaDetails.album && strlen(mediaDetails.album) )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"album\":\"%s\",", mediaDetails.album);
				if ( mediaDetails.year )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"year\":\"%d\",", mediaDetails.year);
				if ( mediaDetails.comment && strlen(mediaDetails.comment) )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"comment\":\"%s\",", mediaDetails.comment);
				if ( mediaDetails.track )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"track\":\"%d\",", mediaDetails.track);
				if ( mediaDetails.genre && strlen(mediaDetails.genre) )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"genre\":\"%s\",", mediaDetails.genre);
				if ( mediaDetails.seconds )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"seconds\":\"%d\",", mediaDetails.seconds);
				if ( mediaDetails.minutes )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"minutes\":\"%d\",", mediaDetails.minutes);
				if ( mediaDetails.bitrate )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"bitrate\":\"%d\",", mediaDetails.bitrate);
				if ( mediaDetails.sampleRate )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"sampleRate\":\"%d\",", mediaDetails.sampleRate);
				if ( mediaDetails.channels )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"channels\":\"%d\",", mediaDetails.channels);

				data[strlen(data)-1] = '}'; //replace last comma with a closing brace

				LogWarn(VB_PLUGIN, "Media plugin data: %s\n", data);
				execl(eventScript, "eventScript", plugins[i].script, "--type", "media", "--data", data, NULL);

				LogErr(VB_PLUGIN, "We failed to exec our media callback!\n");
				exit(EXIT_FAILURE);
			}
			else
			{
				LogExcess(VB_PLUGIN, "Media parent process, resuming work.\n");
			}
		}
	}
}

//non-blocking
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

				char eventScript[1024];
				memcpy(eventScript, getFPPDirectory(), sizeof(eventScript));
				strncat(eventScript, "/scripts/eventScript", sizeof(eventScript)-strlen(eventScript)-1);

				// TODO: parse JSON a little better like escaping special
				// characters if needed, should only be quote that requires
				// escaping, we shouldn't worry about things like tabs,
				// backspaces, form feeds, etc..

				char data[4096];
				int j;
				strncpy(&data[0], "{", sizeof(data));
				for ( j = 0; j < playlistDetails->playListCount; ++j )
				{
					PlaylistEntry plEntry = playlistDetails->playList[j];

					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
						"\"sequence%d\" : {", j);

					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"type\":\"%s\",", type_to_string[plEntry.type]);

					switch(plEntry.type)
					{
						case PL_TYPE_BOTH:
							if ( strlen(plEntry.seqName) )
								snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
								"\"Sequence\":\"%s\",", plEntry.seqName);
							if ( strlen(plEntry.songName) )
								snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
								"\"Media\":\"%s\"", plEntry.songName);
							break;
						case PL_TYPE_MEDIA:
						case PL_TYPE_VIDEO:
							if ( strlen(plEntry.songName) )
								snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
								"\"Media\":\"%s\"", plEntry.songName);
							break;
						case PL_TYPE_SEQUENCE:
							if ( strlen(plEntry.seqName) )
								snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
								"\"Sequence\":\"%s\"", plEntry.seqName);
							break;
						case PL_TYPE_EVENT:
							if ( strlen(plEntry.seqName) )
								snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
								"\"EventID\":\"%s\"", plEntry.eventID);
							break;
						case PL_TYPE_PLUGIN_NEXT:
							snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
							"\"PluginEvent\":\"\"");
						default:
							LogWarn(VB_PLUGIN, "Invalid entry type!\n");
							break;
					}


					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
						"},");
				}

				snprintf(&data[strlen(data)],
						sizeof(data)-strlen(data),
						"\"Action\":\"%s\",",
						(starting == PLAYLIST_STARTING ? "start" : "stop"));

				data[strlen(data)-1] = '}'; //replace last comma with a closing brace

				LogWarn(VB_PLUGIN, "Playlist plugin data: %s\n", data);
				execl(eventScript, "eventScript", plugins[i].script, "--type", "playlist", "--data", data, NULL);

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
				char data[2048];
				int j;

				LogDebug(VB_PLUGIN, "Child process, calling %s callback for nextplaylist: %s\n", plugins[i].name, plugins[i].script);

				char eventScript[1024];
				memcpy(eventScript, getFPPDirectory(), sizeof(eventScript));
				strncat(eventScript, "/scripts/eventScript", sizeof(eventScript)-strlen(eventScript)-1);

				// TODO: parse JSON a little better like escaping special
				// characters if needed, should only be quote that requires
				// escaping, we shouldn't worry about things like tabs,
				// backspaces, form feeds, etc..

				strncpy(&data[0], "{", sizeof(data));
				snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"currentPlaylistEntry\":\"%d\",", currentPlaylistEntry);
				char *mode_string = modeToString(mode);
				if ( mode_string )
				{
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
						"\"mode\":\"%s\",", mode_string);
					free(mode_string); mode_string = NULL;
				}
				snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"repeat\":\"%s\",", (repeat == true ? "true" : "false" ));

				if ( strlen(plugin_data) )
				{
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"data\":\"%s\",", plugin_data );
				}

				data[strlen(data)-1] = '}'; //replace last comma with a closing brace

				LogWarn(VB_PLUGIN, "NextPlaylist plugin data: %s\n", data);

				dup2(output_pipe[1], STDOUT_FILENO);
				close(output_pipe[1]);
				execl(eventScript, "eventScript", plugins[i].script, "--type", "nextplaylist", "--data", data, NULL);

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
