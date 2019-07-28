#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <dlfcn.h>

#include <iostream>

#include "events.h"
#include "mediadetails.h"
#include "settings.h"
#include "Plugins.h"
#include "common.h"
#include "log.h"
#include "mediaoutput/mediaoutput.h"
#include <jsoncpp/json/json.h>

#include "playlist/Playlist.h"

//Boost because... why not?
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

#include "Plugin.h"



typedef struct plugin_s
{
	char *name;
	char *script;
	int mask;
} plugin_t;

PluginManager pluginManager;

class Callback
{
public:
    Callback(const std::string name, const std::string &filename) {
        mName = name;
        mFilename = filename;
    }
    virtual ~Callback() {}
    
    std::string getName() { return mName; }
    std::string getFilename() { return mFilename; }
    
private:
    std::string mName;
    std::string mFilename;
};

class MediaCallback : public Callback
{
public:
    MediaCallback(const std::string &a, const std::string &b) : Callback(a, b) {}
    virtual ~MediaCallback() {}
    
    void run();
private:
};

class PlaylistCallback : public Callback
{
public:
    PlaylistCallback(const std::string &a, const std::string &b) : Callback(a, b) {}
    virtual ~PlaylistCallback() {}
    
    void run(OldPlaylistDetails *, bool);
private:
};

class NextPlaylistEntryCallback : public Callback
{
public:
    NextPlaylistEntryCallback(const std::string &a, const std::string &b) : Callback(a, b) {}
    virtual ~NextPlaylistEntryCallback() {}
    
    int run(const char *, int, int, bool, OldPlaylistEntry *);
private:
};

class EventCallback : public Callback
{
public:
    EventCallback(std::string a, std::string b) : Callback(a, b) {}
    virtual ~EventCallback() {}
    
    void run(const char *, const char *);
private:
};



extern MediaDetails	 mediaDetails;

const char *type_to_string[] = {
	"both",
	"media",
	"sequence",
	"pause",
	"video",
	"event",
};


class ScriptFPPPlugin : public FPPPlugin {
public:
    ScriptFPPPlugin(const std::string &n, const std::string &filename, const std::string &lst) : FPPPlugin(n), fileName(filename) {
        boost::char_separator<char> sep(",");
        boost::tokenizer< boost::char_separator<char> > tokens(lst, sep);
        BOOST_FOREACH (const std::string& type, tokens) {
            if (type == "media") {
                LogDebug(VB_PLUGIN, "Plugin %s supports media callback.\n", name.c_str());
                m_mediaCallback = new MediaCallback(name, filename);
            } else if (type == "playlist") {
                LogDebug(VB_PLUGIN, "Plugin %s supports playlist callback.\n", name.c_str());
                m_playlistCallback = new PlaylistCallback(name, filename);
            } else if (type == "nextplaylist") {
                LogDebug(VB_PLUGIN, "Plugin %s supports nextplaylist callback.\n", name.c_str());
                m_nextPlaylistCallback = new NextPlaylistEntryCallback(name, filename);
            } else if (type == "event") {
                LogDebug(VB_PLUGIN, "Plugin %s supports event callback.\n", name.c_str());
                m_eventCallback = new EventCallback(name, filename);
            } else {
                otherTypes.push_back(type);
            }
        }
    }
    virtual ~ScriptFPPPlugin() {
        if (m_mediaCallback) delete m_mediaCallback;
        if (m_playlistCallback) delete m_playlistCallback;
        if (m_nextPlaylistCallback) delete m_nextPlaylistCallback;
        if (m_eventCallback) delete m_eventCallback;
    }
    
    bool hasCallback() const {
        return m_mediaCallback != nullptr || m_playlistCallback != nullptr
        || m_nextPlaylistCallback != nullptr || m_eventCallback != nullptr;
    }
    
    const std::list<std::string> &getOtherTypes() const {
        return otherTypes;
    }
    
    virtual int nextPlaylistEntryCallback(const char *plugin_data, int currentPlaylistEntry, int mode, bool repeat, OldPlaylistEntry *pe) {
        if (m_nextPlaylistCallback) {
            return m_nextPlaylistCallback->run(plugin_data, currentPlaylistEntry, mode, repeat, pe);
        }
        return 0;
    }
    virtual void playlistCallback(OldPlaylistDetails *oldPlaylistDetails, bool starting) {
        if (m_playlistCallback) {
            m_playlistCallback->run(oldPlaylistDetails, starting);
        }
    }
    virtual void eventCallback(const char *id, const char *impetus) {
        if (m_eventCallback) {
            m_eventCallback->run(id, impetus);
        }
    }
    virtual void mediaCallback() {
        if (m_mediaCallback) {
            m_mediaCallback->run();
        }
    }
    
private:
    const std::string fileName;
    
    std::list<std::string> otherTypes;
    MediaCallback *m_mediaCallback = nullptr;
    PlaylistCallback *m_playlistCallback = nullptr;
    NextPlaylistEntryCallback *m_nextPlaylistCallback = nullptr;
    EventCallback *m_eventCallback = nullptr;
};



PluginManager::PluginManager()
{
}

void PluginManager::init()
{
	DIR *dp;
	struct dirent *ep;

	dp = opendir(getPluginDirectory());
	if (dp != NULL) {
		while (ep = readdir(dp)) {
			int location = strstr(ep->d_name, ".") - ep->d_name;
			// We're one of ".", "..", or hidden, so let's skip
            if (location == 0) {
				continue;
            }

			LogDebug(VB_PLUGIN, "Found Plugin: (%s)\n", ep->d_name);

			std::string filename = std::string(getPluginDirectory()) + "/" + ep->d_name + "/callbacks";
			bool found = false;

			if (FileExists(filename)) {
				printf("Found callback with no extension");
				found = true;
			} else {
				std::vector<std::string> extensions;
				extensions.push_back(std::string(".sh"));
				extensions.push_back(std::string(".pl"));
				extensions.push_back(std::string(".php"));
				extensions.push_back(std::string(".py"));

				for ( std::vector<std::string>::iterator i = extensions.begin(); i != extensions.end(); ++i) {
					std::string tmpFilename = filename + *i;
					if ( FileExists( tmpFilename.c_str() ) ) {
						filename += *i;
						found = true;
					}
				}
			}
            
			std::string eventScript = std::string(getFPPDirectory()) + "/scripts/eventScript";

			if (!found) {
				LogExcess(VB_PLUGIN, "No callbacks supported by plugin: '%s'\n", ep->d_name);
				continue;
			}

			LogDebug(VB_PLUGIN, "Processing Callbacks (%s) for plugin: '%s'\n", filename.c_str(), ep->d_name);

			int output_pipe[2], pid, bytes_read;
			char readbuffer[128];
			std::string callback_list = "";

			if (pipe(output_pipe) == -1) {
				LogErr(VB_PLUGIN, "Failed to make pipe\n");
				exit(EXIT_FAILURE);
			}

			if ((pid = fork()) == -1 ) {
				LogErr(VB_PLUGIN, "Failed to fork\n");
				exit(EXIT_FAILURE);
			}

			if ( pid == 0 ) {
				dup2(output_pipe[1], STDOUT_FILENO);
				close(output_pipe[1]);
				execl(eventScript.c_str(), "eventScript", filename.c_str(), "--list", NULL);

				LogErr(VB_PLUGIN, "We failed to exec our callbacks query!\n");
				exit(EXIT_FAILURE);
			} else {
				close(output_pipe[1]);

				while (true) {
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

            ScriptFPPPlugin *spl = new ScriptFPPPlugin(ep->d_name, filename, callback_list);
            if (spl->hasCallback()) {
                mPlugins.push_back(spl);
            } else {
                for (auto &a : spl->getOtherTypes()) {
                    if (boost::starts_with(a, "c++")) {
                        std::string shlibName = std::string(getPluginDirectory()) + "/" + ep->d_name + "/lib" + ep->d_name + ".so";
                        if (a[3] == ':') {
                            shlibName = std::string(getPluginDirectory()) + "/" + ep->d_name + "/" + a.substr(4);
                        }
                        void *handle = dlopen(shlibName.c_str(), RTLD_NOW);
                        if (handle == nullptr) {
                            LogErr(VB_PLUGIN, "Failed to load plugin shlib %s\n", shlibName.c_str());
                            continue;
                        }
                        FPPPlugin* (*fptr)();
                        *(void **)(&fptr) = dlsym(handle, "createPlugin");
                        if (fptr == nullptr) {
                            LogErr(VB_PLUGIN, "Failed to find  createPlugin() function in shlib %s\n", shlibName.c_str());
                            dlclose(handle);
                            continue;
                        }
                        FPPPlugin *p = fptr();
                        if (p == nullptr) {
                            LogErr(VB_PLUGIN, "Failed to create plugin from shlib %s\n", shlibName.c_str());
                            dlclose(handle);
                            continue;
                        }
                        mPlugins.push_back(p);
                    }
                }
                delete spl;
            }
		}
		closedir(dp);
	} else {
		LogWarn(VB_PLUGIN, "Couldn't open the directory %s: (%d): %s\n", getPluginDirectory(), errno, strerror(errno));
	}

	return;
}

PluginManager::~PluginManager()
{
	while (!mPlugins.empty()) {
		delete mPlugins.back();
		mPlugins.pop_back();
	}
}

int PluginManager::nextPlaylistEntryCallback(const char *plugin_data, int currentPlaylistEntry, int mode, bool repeat, OldPlaylistEntry *pe)
{
    int i = 0;
    for (auto a : mPlugins) {
        i |= a->nextPlaylistEntryCallback(plugin_data, currentPlaylistEntry, mode, repeat, pe);
    }
    return i;
}
void PluginManager::playlistCallback(OldPlaylistDetails *oldPlaylistDetails, bool starting)
{
    for (auto a : mPlugins) {
        a->playlistCallback(oldPlaylistDetails, starting);
    }
}
void PluginManager::eventCallback(const char *id, const char *impetus)
{
    for (auto a : mPlugins) {
        a->eventCallback(id, impetus);
    }
}
void PluginManager::mediaCallback()
{
    for (auto a : mPlugins) {
        a->mediaCallback();
    }
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
		OldPlaylistEntry *plEntry = &oldPlaylist->m_playlistDetails.playList[oldPlaylist->m_playlistDetails.currentPlaylistEntry];
		Json::Value root;
		Json::FastWriter writer;

		Json::Value pl = playlist->GetInfo();
		//root["type"] = std::string(type_to_string[plEntry->type]);
		root["type"] = pl["currentEntry"]["type"];

		//if (strlen(plEntry->seqName))
		//{
			//root["Sequence"] = std::string(plEntry->seqName);
			root["Sequence"] = pl["currentEntry"]["type"].asString() == "both" ? pl["currentEntry"]["sequence"]["sequenceName"].asString().c_str() : "";
		//}
		//if (strlen(plEntry->songName))
		//{
			//root["Media"] = std::string(plEntry->songName);
			root["Media"] = pl["currentEntry"]["type"].asString() == "both"
                                 ? pl["currentEntry"]["media"]["mediaFilename"].asString().c_str()
                                 : pl["currentEntry"]["mediaFilename"].asString().c_str();
		//}
		if (mediaDetails.title && strlen(mediaDetails.title))
		{
			root["title"] = std::string(mediaDetails.title);
		}
		if (mediaDetails.artist && strlen(mediaDetails.artist))
		{
			root["artist"] = std::string(mediaDetails.artist);
		}
		if (mediaDetails.album && strlen(mediaDetails.album))
		{
			root["album"] = std::string(mediaDetails.album);
		}
		if (mediaDetails.year)
		{
			root["year"] = std::to_string(mediaDetails.year);
		}
		if (mediaDetails.comment && strlen(mediaDetails.comment))
		{
			root["comment"] = std::string(mediaDetails.comment);
		}
		if (mediaDetails.track)
		{
			root["track"] = std::to_string(mediaDetails.track);
		}
		if (mediaDetails.genre && strlen(mediaDetails.genre))
		{
			root["genre"] = std::string(mediaDetails.genre);
		}
		if (mediaDetails.length)
		{
			root["length"] = std::to_string(mediaDetails.length);
		}
		if (mediaDetails.seconds)
		{
			root["seconds"] = std::to_string(mediaDetails.seconds);
		}
		if (mediaDetails.minutes)
		{
			root["minutes"] = std::to_string(mediaDetails.minutes);
		}
		if (mediaDetails.bitrate)
		{
			root["bitrate"] = std::to_string(mediaDetails.bitrate);
		}
		if (mediaDetails.sampleRate)
		{
			root["sampleRate"] = std::to_string(mediaDetails.sampleRate);
		}
		if (mediaDetails.channels)
		{
			root["channels"] = std::to_string(mediaDetails.channels);
		}

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


//blocking
void PlaylistCallback::run(OldPlaylistDetails *oldPlaylistDetails, bool starting)
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

		for ( j = 0; j < oldPlaylistDetails->playListCount; ++j )
		{
			OldPlaylistEntry plEntry = oldPlaylistDetails->playList[j];
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


//blocking
int NextPlaylistEntryCallback::run(const char *plugin_data, int currentPlaylistEntry, int mode, bool repeat, OldPlaylistEntry *pe)
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
		ret_val = oldPlaylist->ParsePlaylistEntry(playlist_entry, pe);
		//Set our type back to 'P' so we re-parse it next time we pass it in the playlist
		pe->cType = 'P';

		LogExcess(VB_PLUGIN, "NextPlaylist parent process, waiting to resume work.\n");
		wait(NULL);
	}

	return ret_val;
}


//blocking
void EventCallback::run(const char *id, const char *impetus)
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
