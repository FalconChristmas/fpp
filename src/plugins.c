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

			// We're one of ".", "..", or hidden, so let's skip
			// this entry in the directory
			if ( location == 0 )
				continue;

			// Let's grab the next plugin entry
			plugin = &plugins[plugin_count];
			plugin->name = strdup(ep->d_name);

			strcpy(long_filename, getPluginDirectory());
			strcat(long_filename, "/");
			strcat(long_filename, ep->d_name);
			strcat(long_filename, "/callbacks");

			LogDebug(VB_PLUGIN, "Found Plugin: (%s)\n", ep->d_name);

			if (!FileExists(long_filename))
			{
				LogExcess(VB_PLUGIN, "No callbacks supported by this plugin\n");
				continue;
			}

			LogDebug(VB_PLUGIN, "Processing Callbacks for %s plugin\n", ep->d_name);

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
				execl(long_filename, "callbacks", "--list", NULL);
			}
			else
			{
				close(output_pipe[1]);
				read(output_pipe[0], callbacks_list, sizeof(callbacks_list));

				while ( callbacks_list[strlen(callbacks_list)] == '\n' )
					callbacks_list[strlen(callbacks_list)] = '\0';	// hack to cut off
																	// trailing newlines

				LogExcess(VB_PLUGIN, "Callback output: (%s)\n", callbacks_list);
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

				// build our data string here
				char data[1024];
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
				if ( mediaDetails.title )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"title\":\"%s\",", mediaDetails.title);
				if ( mediaDetails.artist )
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data),
					"\"artist\":\"%s\",", mediaDetails.artist);

				data[strlen(data)-1] = '}'; //replace last comma with a closing brace

				LogWarn(VB_PLUGIN, "Media plugin data: %s\n", data);
				execl(plugins[i].script, "callbacks", "--type", "media", "--data", data, NULL);
			}
			else
			{
				LogExcess(VB_PLUGIN, "Parent process, resuming work\n");
				wait(NULL);
			}
		}
	}
}

//non-blocking
void PlaylistCallback(PlaylistDetails *playlistDetails)
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

				char data[2048];
				int j;
				strncpy(&data[0], "{", sizeof(data));
				for ( j = 0; j < playlistDetails->playListCount; ++j )
				{
					snprintf(&data[strlen(data)], sizeof(data)-strlen(data), "\"Sequence\":\"%s\",", playlistDetails->playList[j].seqName);
				}
				data[strlen(data)-1] = '}'; //replace last comma with a closing brace
				execl(plugins[i].script, "callbacks", "--type", "playlist", "--data", data, NULL);
			}
			else
			{
				LogExcess(VB_PLUGIN, "Parent process, resuming work\n");
				wait(NULL);
			}
		}
	}
}

//blocking
void NextPlaylistEntryCallback(void)
{
	/*
	foreach ( plugins )
	{
		if ( this.mask & NEXT_PLAYLIST_ENTRY_CALLBACK )
		{
			//make the call
		}
	}
	*/
}
