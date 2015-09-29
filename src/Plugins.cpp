#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>

#include <iostream>

#include "events.h"
#include "mediadetails.h"
#include "mediaoutput.h"
#include "settings.h"
#include "Plugins.h"
#include "common.h"
#include "log.h"
#include <jsoncpp/json/json.h>

//Boost because... why not?
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

typedef struct plugin_s
{
	char *name;
	char *script;
	int mask;
} plugin_t;

PluginCallbackManager pluginCallbackManager;

// TODO: Maybe make this a linked list so we
// can support more than a hard-coded value.
#define MAX_PLUGINS 10

plugin_t plugins[MAX_PLUGINS];
int plugin_count = 0;

extern MediaDetails	 mediaDetails;

const char *type_to_string[] = {
	"both",
	"media",
	"sequence",
	"pause",
	"video",
	"event",
};

PluginCallbackManager::PluginCallbackManager()
{
}

void PluginCallbackManager::init()
{
	DIR *dp;
	struct dirent *ep;

	dp = opendir(getPluginDirectory());
	if (dp != NULL)
	{
		while (ep = readdir(dp))
		{
			int location = strstr(ep->d_name,".") - ep->d_name;
			// We're one of ".", "..", or hidden, so let's skip
			if ( location == 0 )
				continue;

			LogDebug(VB_PLUGIN, "Found Plugin: (%s)\n", ep->d_name);

			std::string filename = std::string(getPluginDirectory()) + "/" + ep->d_name + "/callbacks";
			bool found = false;

			if ( FileExists(filename.c_str()) )
			{
				printf("Found callback with no extension");
				found = true;
			}
			else
			{
				std::vector<std::string> extensions;
				extensions.push_back(std::string(".sh"));
				extensions.push_back(std::string(".pl"));
				extensions.push_back(std::string(".php"));
				extensions.push_back(std::string(".py"));

				for ( std::vector<std::string>::iterator i = extensions.begin(); i != extensions.end(); ++i)
				{
					std::string tmpFilename = filename + *i;
					if ( FileExists( tmpFilename.c_str() ) )
					{
						filename += *i;
						found = true;
					}
				}
			}
			std::string eventScript = std::string(getFPPDirectory()) + "/scripts/eventScript";

			if ( !found )
			{
				LogExcess(VB_PLUGIN, "No callbacks supported by plugin: '%s'\n", ep->d_name);
				continue;
			}

			LogDebug(VB_PLUGIN, "Processing Callbacks (%s) for plugin: '%s'\n", filename.c_str(), ep->d_name);

			int output_pipe[2], pid, bytes_read;
			char readbuffer[128];
			std::string callback_list = "";

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
				execl(eventScript.c_str(), "eventScript", filename.c_str(), "--list", NULL);

				LogErr(VB_PLUGIN, "We failed to exec our callbacks query!\n");
				exit(EXIT_FAILURE);
			}
			else
			{
				close(output_pipe[1]);

				while (true)
				{
					bytes_read = read(output_pipe[0], readbuffer, sizeof(readbuffer)-1);

					if (bytes_read <= 0)
						break;

					readbuffer[bytes_read] = '\0';
					callback_list += readbuffer;
				}

				boost::trim(callback_list);

				LogExcess(VB_PLUGIN, "Callback output: (%s)\n", callback_list.c_str());

				wait(NULL);
			}

			boost::char_separator<char> sep(",");
			boost::tokenizer< boost::char_separator<char> > tokens(callback_list, sep);
		    BOOST_FOREACH (const std::string& type, tokens)
			{
				if (type == "media")
				{
					LogDebug(VB_PLUGIN, "Plugin %s supports media callback.\n", ep->d_name);
					MediaCallback *media = new MediaCallback(std::string(ep->d_name), filename);
					mCallbacks.push_back(media);
				}
				else if (type == "playlist")
				{
					LogDebug(VB_PLUGIN, "Plugin %s supports playlist callback.\n", ep->d_name);
					PlaylistCallback *playlist = new PlaylistCallback(std::string(ep->d_name), filename);
					mCallbacks.push_back(playlist);
				}
				else if (type == "nextplaylist")
				{
					LogDebug(VB_PLUGIN, "Plugin %s supports nextplaylist callback.\n", ep->d_name);
					NextPlaylistEntryCallback *nextplaylistentry = new NextPlaylistEntryCallback(std::string(ep->d_name), filename);
					mCallbacks.push_back(nextplaylistentry);
				}
				else if (type == "event")
				{
					LogDebug(VB_PLUGIN, "Plugin %s supports nextplaylist callback.\n", ep->d_name);
					EventCallback *eventcallback = new EventCallback(std::string(ep->d_name), filename);
					mCallbacks.push_back(eventcallback);
				}
			}

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

PluginCallbackManager::~PluginCallbackManager()
{
	while (!mCallbacks.empty())
	{
		delete mCallbacks.back();
		mCallbacks.pop_back();
	}
}

int PluginCallbackManager::nextPlaylistEntryCallback(const char *plugin_data, int currentPlaylistEntry, int mode, bool repeat, PlaylistEntry *pe)
{
	BOOST_FOREACH (Callback *callback, mCallbacks)
	{
		if ( dynamic_cast<NextPlaylistEntryCallback*>(callback) != NULL )
		{
			NextPlaylistEntryCallback *cb = dynamic_cast<NextPlaylistEntryCallback*>(callback);
			cb->run(plugin_data, currentPlaylistEntry, mode, repeat, pe);
		}
	}
}
void PluginCallbackManager::playlistCallback(PlaylistDetails *playlistDetails, bool starting)
{
	BOOST_FOREACH (Callback *callback, mCallbacks)
	{
		if ( dynamic_cast<PlaylistCallback*>(callback) != NULL )
		{
			PlaylistCallback *cb = dynamic_cast<PlaylistCallback*>(callback);
			cb->run(playlistDetails, starting);
		}
	}
}
void PluginCallbackManager::eventCallback(char *id, char *impetus)
{
	BOOST_FOREACH (Callback *callback, mCallbacks)
	{
		if ( dynamic_cast<EventCallback*>(callback) != NULL )
		{
			EventCallback *cb = dynamic_cast<EventCallback*>(callback);
			cb->run(id, impetus);
		}
	}
}
void PluginCallbackManager::mediaCallback()
{
	BOOST_FOREACH (Callback *callback, mCallbacks)
	{
		if ( dynamic_cast<MediaCallback*>(callback) != NULL )
		{
			MediaCallback *cb = dynamic_cast<MediaCallback*>(callback);
			cb->run();
		}
	}
}

Callback::Callback(std::string name, std::string filename)
{
	mName = name;
	mFilename = filename;
}
Callback::~Callback()
{
}

MediaCallback::~MediaCallback()
{
}

//blocking
void MediaCallback::run(void)
{
	int pid;

	if ((pid = fork()) == -1 )
	{
		LogErr(VB_PLUGIN, "Failed to fork\n");
		exit(EXIT_FAILURE);
	}

	if ( pid == 0 )
	{
		LogDebug(VB_PLUGIN, "Child process, calling %s callback for media : %s\n", this->getName().c_str(), this->getFilename().c_str());

		std::string eventScript = std::string(getFPPDirectory()) + "/scripts/eventScript";
		PlaylistEntry *plEntry = &playlist->m_playlistDetails.playList[playlist->m_playlistDetails.currentPlaylistEntry];
		Json::Value root;
		Json::FastWriter writer;

		root["type"] = std::string(type_to_string[plEntry->type]);

		if (strlen(plEntry->seqName))
			root["Sequence"] = std::string(plEntry->seqName);
		if (strlen(plEntry->songName))
			root["Media"] = std::string(plEntry->songName);
		if (mediaDetails.title && strlen(mediaDetails.title))
			root["title"] = std::string(mediaDetails.title);
		if (mediaDetails.artist && strlen(mediaDetails.artist))
			root["artist"] = std::string(mediaDetails.artist);
		if (mediaDetails.album && strlen(mediaDetails.album))
			root["album"] = std::string(mediaDetails.album);
		if (mediaDetails.year)
			root["year"] = mediaDetails.year;
		if (mediaDetails.comment && strlen(mediaDetails.comment))
			root["comment"] = std::string(mediaDetails.comment);
		if (mediaDetails.track)
			root["track"] = mediaDetails.track;
		if (mediaDetails.genre && strlen(mediaDetails.genre))
			root["genre"] = std::string(mediaDetails.genre);
		if (mediaDetails.length)
			root["length"] = mediaDetails.length;
		if (mediaDetails.seconds)
			root["seconds"] = mediaDetails.seconds;
		if (mediaDetails.minutes)
			root["minutes"] = mediaDetails.minutes;
		if (mediaDetails.bitrate)
			root["bitrate"] = mediaDetails.bitrate;
		if (mediaDetails.sampleRate)
			root["sampleRate"] = mediaDetails.sampleRate;
		if (mediaDetails.channels)
			root["channels"] = mediaDetails.channels;

		LogDebug(VB_PLUGIN, "Media plugin data: %s\n", writer.write(root).c_str());
		execl(eventScript.c_str(), "eventScript", this->getFilename().c_str(), "--type", "media", "--data", writer.write(root).c_str(), NULL);

		LogErr(VB_PLUGIN, "We failed to exec our media callback!\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		LogExcess(VB_PLUGIN, "Media parent process, resuming work.\n");
		wait(NULL);
	}
}

PlaylistCallback::~PlaylistCallback()
{
}

//blocking
void PlaylistCallback::run(PlaylistDetails *playlistDetails, bool starting)
{
	int pid;

	if ((pid = fork()) == -1 )
	{
		LogErr(VB_PLUGIN, "Failed to fork\n");
		exit(EXIT_FAILURE);
	}

	if ( pid == 0 )
	{
		LogDebug(VB_PLUGIN, "Child process, calling %s callback for playlist: %s\n", this->getName().c_str(), this->getFilename().c_str());

		int j;
		std::string eventScript = std::string(getFPPDirectory()) + "/scripts/eventScript";
		Json::Value root;
		Json::FastWriter writer;

		for ( j = 0; j < playlistDetails->playListCount; ++j )
		{
			PlaylistEntry plEntry = playlistDetails->playList[j];
			Json::Value node;

			node["type"] = std::string(type_to_string[plEntry.type]);

			switch(plEntry.type)
			{
				case PL_TYPE_BOTH:
					if ( strlen(plEntry.seqName) )
						node["Sequence"] = std::string(plEntry.seqName);
					if ( strlen(plEntry.songName) )
						node["Media"] = std::string(plEntry.songName);
					break;
				case PL_TYPE_MEDIA:
				case PL_TYPE_VIDEO:
					if ( strlen(plEntry.songName) )
						node["Media"] = std::string(plEntry.songName);
					break;
				case PL_TYPE_SEQUENCE:
					if ( strlen(plEntry.seqName) )
						node["Sequence"] = std::string(plEntry.seqName);
					break;
				case PL_TYPE_EVENT:
					if ( strlen(plEntry.seqName) )
						node["EventID"] = std::string(plEntry.eventID);
					break;
				case PL_TYPE_PLUGIN_NEXT:
					node["PluginEvent"] = std::string("");
					break;
				default:
					LogWarn(VB_PLUGIN, "Invalid entry type!\n");
					break;
			}

			char sequenceNumber[20] = {0};
			snprintf(&sequenceNumber[strlen(sequenceNumber)],
					sizeof(sequenceNumber)-strlen(sequenceNumber),
					"sequence%d", j);

			root[sequenceNumber] = node;
		}

		root["Action"] = std::string((starting == PLAYLIST_STARTING ? "start" : "stop"));

		LogDebug(VB_PLUGIN, "Playlist plugin data: %s\n", writer.write(root).c_str());
		execl(eventScript.c_str(), "eventScript", this->getFilename().c_str(), "--type", "playlist", "--data", writer.write(root).c_str(), NULL);

		LogErr(VB_PLUGIN, "We failed to exec our playlist callback!\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		LogExcess(VB_PLUGIN, "Playlist parent process, waiting to resume work.\n");
		wait(NULL);
	}
}

NextPlaylistEntryCallback::~NextPlaylistEntryCallback()
{
}

//blocking
int NextPlaylistEntryCallback::run(const char *plugin_data, int currentPlaylistEntry, int mode, bool repeat, PlaylistEntry *pe)
{
	int output_pipe[2], pid, ret_val;
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
		LogDebug(VB_PLUGIN, "Child process, calling %s callback for nextplaylist: %s\n", this->getName().c_str(), this->getFilename().c_str());

		std::string eventScript = std::string(getFPPDirectory()) + "/scripts/eventScript";
		Json::Value root;
		Json::FastWriter writer;

		root["currentPlaylistEntry"] = currentPlaylistEntry;

		char *mode_string = modeToString(mode);
		if (mode_string)
		{
			root["mode"] = std::string(mode_string);
			free(mode_string); mode_string = NULL;
		}

		root["repeat"] = std::string((repeat == true ? "true" : "false" ));

		if (strlen(plugin_data))
			root["data"] = std::string( plugin_data);

		LogDebug(VB_PLUGIN, "NextPlaylist plugin data: %s\n", writer.write(root).c_str());

		dup2(output_pipe[1], STDOUT_FILENO);
		close(output_pipe[1]);
		execl(eventScript.c_str(), "eventScript", this->getFilename().c_str(), "--type", "nextplaylist", "--data", writer.write(root).c_str(), NULL);

		LogErr(VB_PLUGIN, "We failed to exec our nextplaylist callback!\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		close(output_pipe[1]);
		read(output_pipe[0], &playlist_entry, sizeof(playlist_entry));

		LogExcess(VB_PLUGIN, "Parsed playlist entry: %s\n", playlist_entry);
		ret_val = playlist->ParsePlaylistEntry(playlist_entry, pe);
		//Set our type back to 'P' so we re-parse it next time we pass it in the playlist
		pe->cType = 'P';

		LogExcess(VB_PLUGIN, "NextPlaylist parent process, waiting to resume work.\n");
		wait(NULL);
	}

	return ret_val;
}

EventCallback::~EventCallback()
{
}

//blocking
void EventCallback::run(char *id, char *impetus)
{
	int pid;

	if ((pid = fork()) == -1 )
	{
		LogErr(VB_PLUGIN, "Failed to fork\n");
		exit(EXIT_FAILURE);
	}

	if ( pid == 0 )
	{
		LogDebug(VB_PLUGIN, "Child process, calling %s callback for event: %s\n", this->getName().c_str(), this->getFilename().c_str());

		char *data = NULL;
		std::string eventScript = std::string(getFPPDirectory()) + "/scripts/eventScript";
		FPPevent *event = LoadEvent(id);
		Json::Value root;
		Json::FastWriter writer;

		root["caller"] = std::string(impetus);
		root["major"] = event->majorID;
		root["minor"] = event->minorID;
		if ( event->name && strlen(event->name) )
			root["name"] = std::string(event->name);
		if ( event->effect && strlen(event->effect) )
			root["effect"] = std::string(event->effect);
		root["startChannel"] = event->startChannel;
		if ( event->script && strlen(event->script) )
			root["script"] = std::string(event->script);

		LogDebug(VB_PLUGIN, "Media plugin data: %s\n", writer.write(root).c_str());
		execl(eventScript.c_str(), "eventScript", this->getFilename().c_str(), "--type", "media", "--data", writer.write(root).c_str(), NULL);

		LogErr(VB_PLUGIN, "We failed to exec our media callback!\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		LogExcess(VB_PLUGIN, "Media parent process, resuming work.\n");
		wait(NULL);
	}
}
