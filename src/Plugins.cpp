#include "fpp-pch.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <iostream>
#include <libgen.h>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Plugins.h"
#include "mediadetails.h"
#include "mediaoutput/mediaoutput.h"
#include <jsoncpp/json/json.h>

#include "commands/Commands.h"
#include "playlist/Playlist.h"

#include "Plugin.h"
#include "mediadetails.h"

PluginManager PluginManager::INSTANCE;

FPPPlugin::FPPPlugin(const std::string& n) :
    name(n) {
    reloadSettings();
}

void FPPPlugin::reloadSettings() {
    settings.clear();
    std::string dname = std::string(FPP_DIR_CONFIG) + "/plugin." + name;
    if (FileExists(dname)) {
        FILE* file = fopen(dname.c_str(), "r");

        if (file != NULL) {
            char* line = NULL;
            size_t len = 0;
            ssize_t read;
            int sIndex = 0;
            while ((read = getline(&line, &len, file)) != -1) {
                if ((!line) || (!read) || (read == 1)) {
                    continue;
                }

                char *key = NULL, *value = NULL; // These are values we're looking for and will
                // run through trimwhitespace which means they
                // must be freed before we are done.

                char* token = strtok(line, "=");
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

class MediaCallback {
public:
    MediaCallback(const std::string& name, const std::string& filename) :
        mName(name),
        mFilename(filename) {
    }
    virtual ~MediaCallback() {}

    void run(const Json::Value& playlist, const MediaDetails& mediaDetails);

private:
    std::string mName;
    std::string mFilename;
};

class PlaylistCallback {
public:
    PlaylistCallback(const std::string& name, const std::string& filename) :
        mName(name),
        mFilename(filename) {
    }
    virtual ~PlaylistCallback() {}

    void run(const Json::Value& playlist, const std::string& action, const std::string& section, int idx);

private:
    std::string mName;
    std::string mFilename;
};

extern MediaDetails mediaDetails;

const char* type_to_string[] = {
    "both",
    "media",
    "sequence",
    "pause",
    "video",
};

class ScriptFPPPlugin : public FPPPlugin {
public:
    ScriptFPPPlugin(const std::string& n, const std::string& filename, const std::string& lst) :
        FPPPlugin(n),
        fileName(filename) {
        std::vector<std::string> types = split(lst, ',');
        for (int i = 0; i < types.size(); i++) {
            if (types[i] == "media") {
                LogDebug(VB_PLUGIN, "Plugin %s supports media callback.\n", name.c_str());
                m_mediaCallback = new MediaCallback(name, filename);
            } else if (types[i] == "playlist") {
                LogDebug(VB_PLUGIN, "Plugin %s supports playlist callback.\n", name.c_str());
                m_playlistCallback = new PlaylistCallback(name, filename);
            } else {
                otherTypes.push_back(types[i]);
            }
        }
    }
    virtual ~ScriptFPPPlugin() {
        if (m_mediaCallback)
            delete m_mediaCallback;
        if (m_playlistCallback)
            delete m_playlistCallback;
    }

    bool hasCallback() const {
        return m_mediaCallback != nullptr || m_playlistCallback != nullptr;
    }

    const std::list<std::string>& getOtherTypes() const {
        return otherTypes;
    }

    virtual void mediaCallback(const Json::Value& playlist, const MediaDetails& mediaDetails) override {
        if (m_mediaCallback) {
            m_mediaCallback->run(playlist, mediaDetails);
        }
    }
    virtual void playlistCallback(const Json::Value& playlist, const std::string& action, const std::string& section, int item) override {
        if (m_playlistCallback) {
            m_playlistCallback->run(playlist, action, section, item);
        }
    }

private:
    const std::string fileName;

    std::list<std::string> otherTypes;
    MediaCallback* m_mediaCallback = nullptr;
    PlaylistCallback* m_playlistCallback = nullptr;
};

PluginManager::PluginManager() :
    mPluginsLoaded(false) {
}

class ScriptCommand : public Command {
public:
    ScriptCommand(const std::string& dir, Json::Value& json) :
        Command(json["name"].asString()),
        directory(dir),
        description(json) {
        script = json["script"].asString();
        description.removeMember("script");
    }
    bool IsOk() {
        return FileExists(directory + "/" + script);
    }
    virtual Json::Value getDescription() override {
        return description;
    }

    virtual std::unique_ptr<Result> run(const std::vector<std::string>& args) override {
        int pid = fork();
        if (pid == -1) {
            LogErr(VB_PLUGIN, "Failed to fork\n");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            LogDebug(VB_PLUGIN, "Child process, calling %s for command: %s\n", script.c_str(), name.c_str());

            std::string eventScript = directory + "/" + script;

            std::vector<const char*> sargs;
            sargs.push_back(eventScript.c_str());
            for (auto& a : args) {
                sargs.push_back(a.c_str());
            }
            sargs.push_back(nullptr);

            execv(eventScript.c_str(), (char* const*)&sargs[0]);

            LogErr(VB_PLUGIN, "We failed to exec our command callback:  %s\n", strerror(errno));
            for (auto a : sargs) {
                LogErr(VB_PLUGIN, "  %s\n", a);
            }
            exit(EXIT_FAILURE);
        } else {
            LogExcess(VB_PLUGIN, "Command parent process, resuming work.\n");
            waitpid(pid, nullptr, 0);
        }
        return std::make_unique<Command::Result>(name + " complete");
    }

    std::string directory;
    std::string script;
    Json::Value description;
};

static void LoadPluginCommands(const std::string& dir) {
    std::string commandDir = std::string(FPP_DIR_PLUGIN) + "/" + dir + "/commands/";
    std::string descriptions = commandDir + "/descriptions.json";
    if (FileExists(descriptions)) {
        Json::Value json = LoadJsonFromFile(descriptions);
        for (int x = 0; x < json.size(); x++) {
            Json::Value jscmd = json[x];
            ScriptCommand* cmd = new ScriptCommand(commandDir, jscmd);
            if (cmd->IsOk()) {
                CommandManager::INSTANCE.addCommand(cmd);
            } else {
                delete cmd;
            }
        }
    }
}

void PluginManager::init() {
    DIR* dp;
    struct dirent* ep;

    dp = opendir(FPP_DIR_PLUGIN);
    if (dp != NULL) {
        while ((ep = readdir(dp))) {
            int location = strstr(ep->d_name, ".") - ep->d_name;
            // We're one of ".", "..", or hidden, so let's skip
            if (location == 0) {
                continue;
            }
            struct stat statbuf;
            std::string dname = FPP_DIR_PLUGIN;
            dname += "/";
            dname += ep->d_name;
            lstat(dname.c_str(), &statbuf);
            if (!S_ISDIR(statbuf.st_mode)) {
                dname += "/.linkOK"; // Allow developers to use symlinks if desired
                if (!FileExists(dname))
                    continue;
            }

            LogDebug(VB_PLUGIN, "Found Plugin: (%s)\n", ep->d_name);

            std::string filename = std::string(FPP_DIR_PLUGIN) + "/" + ep->d_name + "/callbacks";
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

                for (std::vector<std::string>::iterator i = extensions.begin(); i != extensions.end(); ++i) {
                    std::string tmpFilename = filename + *i;
                    if (FileExists(tmpFilename.c_str())) {
                        filename += *i;
                        found = true;
                    }
                }
            }
            LoadPluginCommands(ep->d_name);

            std::string eventScript(FPP_DIR "/scripts/eventScript");

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

            if ((pid = fork()) == -1) {
                LogErr(VB_PLUGIN, "Failed to fork\n");
                exit(EXIT_FAILURE);
            }

            if (pid == 0) {
                dup2(output_pipe[1], STDOUT_FILENO);
                close(output_pipe[1]);
                execl(eventScript.c_str(), "eventScript", filename.c_str(), "--list", NULL);

                LogErr(VB_PLUGIN, "We failed to exec our callbacks query!  %s  %s %s --list\n", eventScript.c_str(), "eventScript", filename.c_str());
                exit(EXIT_FAILURE);
            } else {
                close(output_pipe[1]);

                while (true) {
                    bytes_read = read(output_pipe[0], readbuffer, sizeof(readbuffer) - 1);

                    if (bytes_read <= 0)
                        break;

                    readbuffer[bytes_read] = '\0';
                    callback_list += readbuffer;
                }

                TrimWhiteSpace(callback_list);

                LogExcess(VB_PLUGIN, "Callback output: (%s)\n", callback_list.c_str());
                wait(NULL);
            }

            ScriptFPPPlugin* spl = new ScriptFPPPlugin(ep->d_name, filename, callback_list);
            if (spl->hasCallback()) {
                mPlugins.push_back(spl);
            } else {
                for (auto& a : spl->getOtherTypes()) {
                    if (startsWith(a, "c++")) {
                        std::string shlibName = std::string(FPP_DIR_PLUGIN) + "/" + ep->d_name + "/lib" + ep->d_name + ".so";
                        if (a[3] == ':') {
                            shlibName = std::string(FPP_DIR_PLUGIN) + "/" + ep->d_name + "/" + a.substr(4);
                        }
                        void* handle = dlopen(shlibName.c_str(), RTLD_NOW);
                        if (handle == nullptr) {
                            LogErr(VB_PLUGIN, "Failed to load plugin shlib: %s\n", dlerror());
                            continue;
                        }
                        FPPPlugin* (*fptr)();
                        *(void**)(&fptr) = dlsym(handle, "createPlugin");
                        if (fptr == nullptr) {
                            LogErr(VB_PLUGIN, "Failed to find  createPlugin() function in shlib %s\n", shlibName.c_str());
                            dlclose(handle);
                            continue;
                        }
                        FPPPlugin* p = fptr();
                        if (p == nullptr) {
                            LogErr(VB_PLUGIN, "Failed to create plugin from shlib %s\n", shlibName.c_str());
                            dlclose(handle);
                            continue;
                        }
                        mShlibHandles.push_back(handle);
                        mPlugins.push_back(p);
                    }
                }
                delete spl;
            }
        }
        closedir(dp);
    } else {
        LogWarn(VB_PLUGIN, "Couldn't open the directory %s: (%d): %s\n", FPP_DIR_PLUGIN, errno, strerror(errno));
    }
    mPluginsLoaded = true;

    return;
}

PluginManager::~PluginManager() {
    Cleanup();
}
void PluginManager::Cleanup() {
    while (!mPlugins.empty()) {
        delete mPlugins.back();
        mPlugins.pop_back();
    }
    for (auto& a : mShlibHandles) {
        dlclose(a);
    }
    mShlibHandles.clear();
}

bool PluginManager::hasPlugins() {
    return mPluginsLoaded && !mPlugins.empty();
}

void PluginManager::mediaCallback(const Json::Value& playlist, const MediaDetails& mediaDetails) {
    if (mPluginsLoaded) {
        for (auto a : mPlugins) {
            a->mediaCallback(playlist, mediaDetails);
        }
    }
}
void PluginManager::registerApis(httpserver::webserver* m_ws) {
    if (mPluginsLoaded) {
        for (auto a : mPlugins) {
            a->registerApis(m_ws);
        }
    }
}
void PluginManager::unregisterApis(httpserver::webserver* m_ws) {
    if (mPluginsLoaded) {
        for (auto a : mPlugins) {
            a->unregisterApis(m_ws);
        }
    }
}
void PluginManager::modifySequenceData(int ms, uint8_t* seqData) {
    if (mPluginsLoaded) {
        for (auto a : mPlugins) {
            a->modifySequenceData(ms, seqData);
        }
    }
}
void PluginManager::modifyChannelData(int ms, uint8_t* seqData) {
    if (mPluginsLoaded) {
        for (auto a : mPlugins) {
            a->modifyChannelData(ms, seqData);
        }
    }
}
void PluginManager::addControlCallbacks(std::map<int, std::function<bool(int)>>& callbacks) {
    if (mPluginsLoaded) {
        for (auto a : mPlugins) {
            a->addControlCallbacks(callbacks);
        }
    }
}
void PluginManager::multiSyncData(const std::string& pn, uint8_t* data, int len) {
    if (mPluginsLoaded) {
        for (auto a : mPlugins) {
            if (a->getName() == pn) {
                a->multiSyncData(data, len);
            }
        }
    }
}
void PluginManager::playlistCallback(const Json::Value& playlist, const std::string& action, const std::string& section, int item) {
    if (mPluginsLoaded) {
        for (auto a : mPlugins) {
            a->playlistCallback(playlist, action, section, item);
        }
    }
}

//blocking
void MediaCallback::run(const Json::Value& playlist, const MediaDetails& mediaDetails) {
    int pid;

    if ((pid = fork()) == -1) {
        LogErr(VB_PLUGIN, "Failed to fork\n");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        LogDebug(VB_PLUGIN, "Child process, calling %s callback for media : %s\n", mName.c_str(), mFilename.c_str());

        std::string eventScript(FPP_DIR "/scripts/eventScript");
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
        if (mediaDetails.bitrate) {
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
    } else {
        LogExcess(VB_PLUGIN, "Media parent process, resuming work.\n");
        wait(NULL);
    }
}

void PlaylistCallback::run(const Json::Value& playlist, const std::string& action, const std::string& section, int idx) {
    int pid;
    if ((pid = fork()) == -1) {
        LogErr(VB_PLUGIN, "Failed to fork\n");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        LogDebug(VB_PLUGIN, "Child process, calling %s callback for media : %s\n", mName.c_str(), mFilename.c_str());

        std::string eventScript(FPP_DIR "/scripts/eventScript");
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
    } else {
        LogExcess(VB_PLUGIN, "Playlist parent process, resuming work.\n");
        wait(NULL);
    }
}
