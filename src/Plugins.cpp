#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/stat.h>
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

#include "Plugin.h"
#include "mediadetails.h"


PluginManager PluginManager::INSTANCE;

FPPPlugin::FPPPlugin(const std::string &n) : name(n) {
    reloadSettings();
}


void FPPPlugin::reloadSettings() {
    settings.clear();
    std::string dname = getMediaDirectory();
    dname += "/config/plugin.";
    dname += name;
    if (FileExists(dname)) {
        FILE *file = fopen(dname.c_str(), "r");
        
        if (file != NULL) {
            char * line = NULL;
            size_t len = 0;
            ssize_t read;
            int sIndex = 0;
            while ((read = getline(&line, &len, file)) != -1) {
                if (( ! line ) || ( ! read ) || ( read == 1 )) {
                    continue;
                }
                
                char *key = NULL, *value = NULL;    // These are values we're looking for and will
                // run through trimwhitespace which means they
                // must be freed before we are done.
                
                char *token = strtok(line, "=");
                if (!token) {
                    continue;
                }
                
                key = trimwhitespace(token);
                if (!strlen(key)) {
                    free(key);
                    continue;
                }
                
                token = strtok(NULL, "=");
                if (!token) {
                    fprintf(stderr, "Error tokenizing value for %s setting\n", key);
                    free(key);
                    continue;
                }
                value = trimwhitespace(token);
                
                if (key) {
                    if (value) {
                        settings[key] = value;
                    }
                    free(key);
                    key = NULL;
                }
                
                if (value) {
                    free(value);
                    value = NULL;
                }
            }
            
            if (line) {
                free(line);
            }
            fclose(file);
        }
    }
}



class MediaCallback
{
public:
    MediaCallback(const std::string &name, const std::string &filename) : mName(name), mFilename(filename) {
    }
    virtual ~MediaCallback() {}
    
    
    void run(const Json::Value &playlist, const MediaDetails &mediaDetails);
private:
    std::string mName;
    std::string mFilename;
};


class PlaylistCallback
{
    public:
    PlaylistCallback(const std::string &name, const std::string &filename) : mName(name), mFilename(filename)  {
    }
    virtual ~PlaylistCallback() {}
    
    
    void run(const Json::Value &playlist, const std::string &action, const std::string &section, int idx);
    private:
    std::string mName;
    std::string mFilename;
};


class EventCallback
{
public:
    EventCallback(const std::string &name, const std::string &filename) : mName(name), mFilename(filename)  {
    }
    virtual ~EventCallback() {}
    
    void run(const char *, const char *);
private:
    std::string mName;
    std::string mFilename;
    
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
        std::vector<std::string> types = split(lst, ',');
        for (int i = 0; i < types.size(); i++) {
            if (types[i] == "media") {
                LogDebug(VB_PLUGIN, "Plugin %s supports media callback.\n", name.c_str());
                m_mediaCallback = new MediaCallback(name, filename);
            } else if (types[i] == "event") {
                LogDebug(VB_PLUGIN, "Plugin %s supports event callback.\n", name.c_str());
                m_eventCallback = new EventCallback(name, filename);
            } else if (types[i] == "playlist") {
                LogDebug(VB_PLUGIN, "Plugin %s supports playlist callback.\n", name.c_str());
                m_playlistCallback = new PlaylistCallback(name, filename);
            } else {
                otherTypes.push_back(types[i]);
            }
        }
    }
    virtual ~ScriptFPPPlugin() {
        if (m_mediaCallback) delete m_mediaCallback;
        if (m_eventCallback) delete m_eventCallback;
        if (m_playlistCallback) delete m_playlistCallback;
    }
    
    bool hasCallback() const {
        return m_mediaCallback != nullptr || m_eventCallback != nullptr || m_playlistCallback != nullptr;
    }
    
    const std::list<std::string> &getOtherTypes() const {
        return otherTypes;
    }
    
    virtual void eventCallback(const char *id, const char *impetus) override {
        if (m_eventCallback) {
            m_eventCallback->run(id, impetus);
        }
    }
    virtual void mediaCallback(const Json::Value &playlist, const MediaDetails &mediaDetails) override {
        if (m_mediaCallback) {
            m_mediaCallback->run(playlist, mediaDetails);
        }
    }
    virtual void playlistCallback(const Json::Value &playlist, const std::string &action, const std::string &section, int item) override {
        if (m_playlistCallback) {
            m_playlistCallback->run(playlist, action, section, item);
        }
    }

private:
    const std::string fileName;
    
    std::list<std::string> otherTypes;
    MediaCallback *m_mediaCallback = nullptr;
    EventCallback *m_eventCallback = nullptr;
    PlaylistCallback *m_playlistCallback = nullptr;
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
            struct stat statbuf;
            std::string dname = getPluginDirectory();
            dname += "/";
            dname += ep->d_name;
            lstat(dname.c_str(), &statbuf);
            if (!S_ISDIR(statbuf.st_mode)) {
                dname += "/.linkOK"; // Allow developers to use symlinks if desired
                if (!FileExists(dname))
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

				LogErr(VB_PLUGIN, "We failed to exec our callbacks query!  %s  %s %s --list\n", eventScript.c_str(), "eventScript", filename.c_str());
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

				TrimWhiteSpace(callback_list);

				LogExcess(VB_PLUGIN, "Callback output: (%s)\n", callback_list.c_str());
				wait(NULL);
			}

            ScriptFPPPlugin *spl = new ScriptFPPPlugin(ep->d_name, filename, callback_list);
            if (spl->hasCallback()) {
                mPlugins.push_back(spl);
            } else {
                for (auto &a : spl->getOtherTypes()) {
                    if (startsWith(a, "c++")) {
                        std::string shlibName = std::string(getPluginDirectory()) + "/" + ep->d_name + "/lib" + ep->d_name + ".so";
                        if (a[3] == ':') {
                            shlibName = std::string(getPluginDirectory()) + "/" + ep->d_name + "/" + a.substr(4);
                        }
                        void *handle = dlopen(shlibName.c_str(), RTLD_NOW);
                        if (handle == nullptr) {
                            LogErr(VB_PLUGIN, "Failed to load plugin shlib %s: %s\n", shlibName.c_str(), dlerror());
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

void PluginManager::eventCallback(const char *id, const char *impetus)
{
    for (auto a : mPlugins) {
        a->eventCallback(id, impetus);
    }
}
void PluginManager::mediaCallback(const Json::Value &playlist, const MediaDetails &mediaDetails)
{
    for (auto a : mPlugins) {
        a->mediaCallback(playlist, mediaDetails);
    }
}
void PluginManager::registerApis(httpserver::webserver *m_ws) {
    for (auto a : mPlugins) {
        a->registerApis(m_ws);
    }
}
void PluginManager::unregisterApis(httpserver::webserver *m_ws)  {
    for (auto a : mPlugins) {
        a->unregisterApis(m_ws);
    }
}
void PluginManager::modifyChannelData(int ms, uint8_t *seqData) {
    for (auto a : mPlugins) {
        a->modifyChannelData(ms, seqData);
    }
}
void PluginManager::addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks) {
    for (auto a : mPlugins) {
        a->addControlCallbacks(callbacks);
    }
}
void PluginManager::multiSyncData(const std::string &pn, uint8_t *data, int len) {
    for (auto a : mPlugins) {
        if (a->getName() == pn) {
            a->multiSyncData(data, len);
        }
    }
}
void PluginManager::playlistCallback(const Json::Value &playlist, const std::string &action, const std::string &section, int item) {
    for (auto a : mPlugins) {
        a->playlistCallback(playlist, action, section, item);
    }
}


//blocking
void MediaCallback::run(const Json::Value &playlist, const MediaDetails &mediaDetails)
{
	int pid;

	if ((pid = fork()) == -1 ) {
		LogErr(VB_PLUGIN, "Failed to fork\n");
		exit(EXIT_FAILURE);
	}

	if ( pid == 0 ) {
		LogDebug(VB_PLUGIN, "Child process, calling %s callback for media : %s\n", mName.c_str(), mFilename.c_str());

		std::string eventScript = std::string(getFPPDirectory()) + "/scripts/eventScript";
		Json::Value root;

		root["type"] = playlist["currentEntry"]["type"];

        root["Sequence"] = playlist["currentEntry"]["type"].asString() == "both" ? playlist["currentEntry"]["sequence"]["sequenceName"].asString().c_str() : "";
        root["Media"] = playlist["currentEntry"]["type"].asString() == "both"
                             ? playlist["currentEntry"]["media"]["mediaFilename"].asString().c_str()
                             : playlist["currentEntry"]["mediaFilename"].asString().c_str();
		if (!mediaDetails.title.empty()) {
			root["title"] = mediaDetails.title;
		}
		if (!mediaDetails.artist.empty()) {
			root["artist"] = mediaDetails.artist;
		}
		if (!mediaDetails.album.empty()) {
			root["album"] = mediaDetails.album;
		}
		if (mediaDetails.year) {
			root["year"] = std::to_string(mediaDetails.year);
		}
		if (!mediaDetails.comment.empty()) {
			root["comment"] = mediaDetails.comment;
		}
		if (mediaDetails.track) {
			root["track"] = std::to_string(mediaDetails.track);
		}
		if (!mediaDetails.genre.empty()) {
			root["genre"] = mediaDetails.genre;
		}
		if (mediaDetails.length) {
			root["length"] = std::to_string(mediaDetails.length);
		}
		if (mediaDetails.seconds) {
			root["seconds"] = std::to_string(mediaDetails.seconds);
		}
		if (mediaDetails.minutes) {
			root["minutes"] = std::to_string(mediaDetails.minutes);
		}
		if (mediaDetails.bitrate)  {
			root["bitrate"] = std::to_string(mediaDetails.bitrate);
		}
		if (mediaDetails.sampleRate) {
			root["sampleRate"] = std::to_string(mediaDetails.sampleRate);
		}
		if (mediaDetails.channels) {
			root["channels"] = std::to_string(mediaDetails.channels);
		}

		std::string pluginData = SaveJsonToString(root);
		LogDebug(VB_PLUGIN, "Media plugin data: %s\n", pluginData.c_str());
		execl(eventScript.c_str(), "eventScript", mFilename.c_str(), "--type", "media", "--data", pluginData.c_str(), NULL);

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
		LogDebug(VB_PLUGIN, "Child process, calling %s callback for event: %s\n", mName.c_str(), mFilename.c_str());

		char *data = NULL;
		std::string eventScript = std::string(getFPPDirectory()) + "/scripts/eventScript";
		FPPEvent *event = LoadEvent(id);
		Json::Value root = event->toJsonValue();

		root["caller"] = std::string(impetus);

		std::string pluginData = SaveJsonToString(root);
		LogDebug(VB_PLUGIN, "Media plugin data: %s\n", pluginData.c_str());
		execl(eventScript.c_str(), "eventScript", mFilename.c_str(), "--type", "media", "--data", pluginData.c_str(), NULL);

		LogErr(VB_PLUGIN, "We failed to exec our media callback!\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		LogExcess(VB_PLUGIN, "Media parent process, resuming work.\n");
		wait(NULL);
	}
}


void PlaylistCallback::run(const Json::Value &playlist, const std::string &action, const std::string &section, int idx)
{
    int pid;
    if ((pid = fork()) == -1 ) {
        LogErr(VB_PLUGIN, "Failed to fork\n");
        exit(EXIT_FAILURE);
    }
    
    if ( pid == 0 ) {
        LogDebug(VB_PLUGIN, "Child process, calling %s callback for media : %s\n", mName.c_str(), mFilename.c_str());
        
        std::string eventScript = std::string(getFPPDirectory()) + "/scripts/eventScript";
        Json::Value root;
        
        root = playlist;
        root["Action"] = action;
        root["Section"] = section;
        root["Item"] = idx;
        
        std::string pluginData = SaveJsonToString(root);
        LogDebug(VB_PLUGIN, "Playlist plugin data: %s\n", pluginData.c_str());
        execl(eventScript.c_str(), "eventScript", mFilename.c_str(), "--type", "playlist", "--data", pluginData.c_str(), NULL);
        
        LogErr(VB_PLUGIN, "We failed to exec our playlist callback!\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        LogExcess(VB_PLUGIN, "Playlist parent process, resuming work.\n");
        wait(NULL);
    }
}
