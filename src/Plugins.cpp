/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include "fpp-json.h"

#include <sys/stat.h>
#include <sys/wait.h>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "FileMonitor.h"
#include "Plugin.h"
#include "Warnings.h"
#include "common.h"
#include "config.h"
#include "log.h"
#include "mediadetails.h"
#include "settings.h"
#include "commands/Commands.h"

#include "CurlManager.h"
#include "EPollManager.h"
#include "Timers.h"
#include "Plugins.h"

PluginManager PluginManager::INSTANCE;

namespace FPPPlugins
{
    static std::map<std::string, std::string> parseSettingsFile(const std::string& dname) {
        std::map<std::string, std::string> settings;
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

                    char* saveptr = nullptr;
                    char* token = strtok_r(line, "=", &saveptr);
                    if (!token) {
                        continue;
                    }

                    key = trimwhitespace(token);
                    if (!strlen(key)) {
                        free(key);
                        continue;
                    }

                    token = strtok_r(NULL, "=", &saveptr);
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
        return settings;
    }

    Plugin::Plugin(const std::string& n) :
        name(n) {
        std::string dname = FPP_DIR_CONFIG("/plugin." + name);
        settings = parseSettingsFile(dname);
    }
    Plugin::Plugin(const std::string& n, bool monitorSettings) :
        name(n) {
        if (monitorSettings) {
            std::string dname = FPP_DIR_CONFIG("/plugin." + name);
            settings = parseSettingsFile(dname);
            FileMonitor::INSTANCE.AddFile(name, dname, [this, dname]() {
                std::map<std::string, std::string> newSettings = parseSettingsFile(dname);
                for (auto& s : newSettings) {
                    if (settings.find(s.first) == settings.end() || settings[s.first] != s.second) {
                        LogInfo(VB_PLUGIN, "Plugin %s setting %s changed to %s\n", name.c_str(), s.first.c_str(), s.second.c_str());
                        settings[s.first] = s.second;
                        settingChanged(s.first, s.second);
                    }
                }
            });
        }
    }

    Plugin::~Plugin() {
        FileMonitor::INSTANCE.RemoveFile(name, FPP_DIR_CONFIG("/plugin." + name));
    }

    void Plugin::reloadSettings() {
        std::string dname = FPP_DIR_CONFIG("/plugin." + name);
        settings = parseSettingsFile(dname);
    }
}
class LifecycleCallback {
public:
    LifecycleCallback(const std::string& name, const std::string& filename) :
        mName(name),
        mFilename(filename) {
    }
    virtual ~LifecycleCallback() {}

    void run(const std::string& lifecycle);

private:
    std::string mName;
    std::string mFilename;
};

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

class ScriptFPPPlugin : public FPPPlugins::Plugin, public FPPPlugins::PlaylistEventPlugin {
public:
    ScriptFPPPlugin(const std::string& n, const std::string& filename, const std::string& lst) :
        FPPPlugins::Plugin(n), FPPPlugins::PlaylistEventPlugin(), fileName(filename) {
        std::vector<std::string> types = split(lst, ',');
        for (int i = 0; i < types.size(); i++) {
            if (types[i] == "media") {
                LogDebug(VB_PLUGIN, "Plugin %s supports media callback.\n", name.c_str());
                m_mediaCallback = new MediaCallback(name, filename);
            } else if (types[i] == "playlist") {
                LogDebug(VB_PLUGIN, "Plugin %s supports playlist callback.\n", name.c_str());
                m_playlistCallback = new PlaylistCallback(name, filename);
            } else if (types[i] == "lifecycle") {
                m_lifecycleCallback = new LifecycleCallback(name, filename);
            } else {
                otherTypes.push_back(types[i]);
            }
        }
        lifecycleCallback("startup");
    }
    virtual ~ScriptFPPPlugin() {
        lifecycleCallback("shutdown");
        if (m_mediaCallback)
            delete m_mediaCallback;
        if (m_playlistCallback)
            delete m_playlistCallback;
        if (m_lifecycleCallback)
            delete m_lifecycleCallback;
    }

    bool hasCallback() const {
        return m_mediaCallback != nullptr || m_playlistCallback != nullptr || m_lifecycleCallback != nullptr;
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
    virtual void lifecycleCallback(const std::string& lifecycle) {
        if (m_lifecycleCallback) {
            m_lifecycleCallback->run(lifecycle);
        }
    }

private:
    const std::string fileName;

    std::list<std::string> otherTypes;
    MediaCallback* m_mediaCallback = nullptr;
    PlaylistCallback* m_playlistCallback = nullptr;
    LifecycleCallback* m_lifecycleCallback = nullptr;
};

PluginManager::PluginManager() {
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
            std::vector<const char*> envs;

            char mediaDir[1024];
            char fppDir[1024];
            char pluginDir[1024];
            snprintf(mediaDir, sizeof(mediaDir), "MEDIADIR=%s", getFPPMediaDir().c_str());
            snprintf(fppDir, sizeof(fppDir), "FPPDIR=%s", getFPPDDir().c_str());
            snprintf(pluginDir, sizeof(pluginDir), "SCRIPTDIR=%s", directory.c_str());
            envs.push_back(mediaDir);
            envs.push_back(fppDir);
            envs.push_back(pluginDir);
            envs.push_back(nullptr);

            sargs.push_back(eventScript.c_str());
            for (auto& a : args) {
                sargs.push_back(a.c_str());
            }
            sargs.push_back(nullptr);

            execve(eventScript.c_str(), (char* const*)&sargs[0], (char* const*)&envs[0]);

            LogErr(VB_PLUGIN, "We failed to exec our command callback:  %s\n", FPPstrerror(errno));
            for (auto a : sargs) {
                LogErr(VB_PLUGIN, "  %s\n", a);
            }
            _exit(EXIT_FAILURE);
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

// Returns the commands registered, so unloading the plugin can take them back
// out again - they are FPP-owned objects describing scripts in the plugin's
// directory, and an uninstalled plugin should not leave its commands listed.
static std::vector<Command*> LoadPluginCommands(const std::string& dir) {
    std::vector<Command*> added;
    std::string commandDir = FPP_DIR_PLUGIN("/" + dir + "/commands/");
    std::string descriptions = commandDir + "/descriptions.json";
    if (FileExists(descriptions)) {
        Json::Value json = LoadJsonFromFile(descriptions, JsonRoot::Array);
        for (int x = 0; x < json.size(); x++) {
            Json::Value jscmd = json[x];
            ScriptCommand* cmd = new ScriptCommand(commandDir, jscmd);
            if (cmd->IsOk()) {
                CommandManager::INSTANCE.addCommand(cmd);
                added.push_back(cmd);
            } else {
                delete cmd;
            }
        }
    }
    return added;
}

void PluginManager::init() {
}
void PluginManager::loadUserPlugins() {
    DIR* dp;
    struct dirent* ep;

    dp = opendir(FPP_DIR_PLUGIN("").c_str());
    if (dp != NULL) {
        while ((ep = readdir(dp))) {
            int location = strstr(ep->d_name, ".") - ep->d_name;
            // We're one of ".", "..", or hidden, so let's skip
            if (location == 0) {
                continue;
            }
            struct stat statbuf;
            std::string dname = FPP_DIR_PLUGIN("/" + ep->d_name);
            lstat(dname.c_str(), &statbuf);
            if (!S_ISDIR(statbuf.st_mode)) {
                dname += "/.linkOK"; // Allow developers to use symlinks if desired
                if (!FileExists(dname))
                    continue;
            }
            if (mLoadedUserPlugins.find(ep->d_name) == mLoadedUserPlugins.end()) {
                loadUserPlugin(ep->d_name);
            }
        }
        closedir(dp);
    } else {
        LogWarn(VB_PLUGIN, "Couldn't open the directory %s: (%d): %s\n", FPP_DIR_PLUGIN("").c_str(), errno, FPPstrerror(errno));
    }

    // After an FPPOS reflash the boot code sets pluginReinstallNeededAfterOS if
    // plugins were present; surface the reinstall prompt, and keep it in sync if
    // the flag is cleared (by a successful Reinstall All) while fppd is running.
    static bool reinstallListenerRegistered = false;
    if (!reinstallListenerRegistered) {
        reinstallListenerRegistered = true;
        registerSettingsListener("PluginManager", "pluginReinstallNeededAfterOS",
                                 [this](const std::string&) { checkPluginReinstallWarning(); });
    }
    checkPluginReinstallWarning();

    return;
}

// Warning id shared with the www/warnings-definitions.json "Plugin_Warnings"
// entry (drives the icon/grouping); the Fix button itself is rendered from the
// fixUrl/fixText carried in the warning data.
static constexpr int PLUGIN_REINSTALL_WARNING_ID = 57;
static const std::string PLUGIN_REINSTALL_WARNING_MSG = "FPPOS upgraded, plugins must be reinstalled";

void PluginManager::checkPluginReinstallWarning() {
    if (getSetting("pluginReinstallNeededAfterOS") != "") {
        WarningHolder::AddWarning(PLUGIN_REINSTALL_WARNING_ID, PLUGIN_REINSTALL_WARNING_MSG,
                                  "plugins.php?action=reinstallAll", "Reinstall All Plugins");
    } else {
        WarningHolder::RemoveWarning(PLUGIN_REINSTALL_WARNING_ID, PLUGIN_REINSTALL_WARNING_MSG);
    }
}

PluginManager::~PluginManager() {
    Cleanup();
}
void PluginManager::Cleanup() {
    // Give API-providing plugins their unregister callback before destroying
    // them: that is where they remove (and may delete) the Command objects
    // they added to CommandManager. Without this, those commands are still
    // registered when CommandManager::Cleanup() bulk-deletes everything it
    // holds, and a plugin destructor that also deletes them double-frees.
    unregisterApis();
    mAPIProviderPlugins.clear();
    // Anything still settling from an unload will never get another timer tick
    // now, so finish it here rather than leaking the object (and, for an opted-in
    // plugin, the mapping) on the way out.
    while (!mPendingUnloads.empty()) {
        PendingUnload p = std::move(mPendingUnloads.back());
        mPendingUnloads.pop_back();
        finishUnload(p);
    }
    // Inbound HTTP is disarmed above and the routes have drained, so no request
    // is inside plugin code. Now let each plugin stop its own activity - threads,
    // timers, connections - while it is still a whole object. Destructors are too
    // late for that: a thread calling in during destruction reads a plugin that
    // is half gone.
    for (auto& a : mPlugins) {
        a->shutdown();
    }
    while (!mPlugins.empty()) {
        delete mPlugins.back();
        mPlugins.pop_back();
    }
    // Delete any commands that plugins registered but did not remove in
    // unregisterApis(). Those commands have vtables in the plugin library.
    // CommandManager::Cleanup() must run before dlclose() so that the virtual
    // destructor dispatch doesn't hit unmapped memory. The call is idempotent
    // (guarded by an atomic flag), so the subsequent call in main() is a no-op.
    CommandManager::INSTANCE.Cleanup();
    for (auto& a : mShlibHandles) {
        dlclose(a);
    }
    mShlibHandles.clear();
}

FPPPlugins::Plugin* PluginManager::findPlugin(const std::string& name, const std::string& shlibName) {
    for (auto& a : mPlugins) {
        if (a->getName() == name) {
            return a;
        }
    }
    std::string libName = "lib" + (shlibName == "" ? name : shlibName) + SHLIB_EXT;
    FPPPlugins::Plugin* p = loadSHLIBPlugin(libName);
    if (!p) {
        if (DirectoryExists(FPP_DIR_PLUGIN("/" + name)) && (mLoadedUserPlugins.find(name) == mLoadedUserPlugins.end())) {
            loadUserPlugin(name);
        }
        for (auto& a : mPlugins) {
            if (a->getName() == name) {
                return a;
            }
        }
    }
    if (!p) {
        LogErr(VB_PLUGIN, "Failed to load plugin: %s     Error: %s\n", libName.c_str(), dlerror());
    }
    return p;
}
FPPPlugins::Plugin* PluginManager::loadUserPlugin(const std::string& name) {
    LogDebug(VB_PLUGIN, "Found Plugin: (%s)\n", name.c_str());
    std::string filename = FPP_DIR_PLUGIN("/" + name + "/callbacks");
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
    {
        std::vector<Command*> cmds = LoadPluginCommands(name);
        if (!cmds.empty()) {
            auto& slot = mPluginCommands[name];
            slot.insert(slot.end(), cmds.begin(), cmds.end());
        }
    }

    std::string eventScript = FPP_DIR + "/scripts/eventScript";
    if (!found) {
        LogExcess(VB_PLUGIN, "No callbacks supported by plugin: '%s'\n", name.c_str());
        return nullptr;
    }
    LogDebug(VB_PLUGIN, "Processing Callbacks (%s) for plugin: '%s'\n", filename.c_str(), name.c_str());

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
        _exit(EXIT_FAILURE);
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
        waitpid(pid, NULL, 0);
    }

    ScriptFPPPlugin* spl = new ScriptFPPPlugin(name, filename, callback_list);
    if (spl->hasCallback()) {
        mLoadedUserPlugins.emplace(name);
        addPlugin(spl);
        return spl;
    } else {
        for (auto& a : spl->getOtherTypes()) {
            if (startsWith(a, "c++")) {
                std::string shlibName = FPP_DIR_PLUGIN("/" + name + "/lib" + name + SHLIB_EXT);
                if (a[3] == ':') {
                    shlibName = FPP_DIR_PLUGIN("/" + name + "/" + a.substr(4));
                }
                delete spl;
                mLoadedUserPlugins.emplace(name);
                auto* p = loadSHLIBPlugin(shlibName);
                if (p == nullptr) {
                    WarningHolder::AddWarning(5, "Could not load plugin " + name);
                }
                return p;
            }
        }
        delete spl;
    }
    return nullptr;
}

FPPPlugins::Plugin* PluginManager::loadSHLIBPlugin(const std::string& shlibName) {
    // If this path was loaded before and the file behind it has since been
    // replaced - an upgrade between an unloadPlugin() and a loadPlugin() - it
    // cannot simply be reopened. dlopen() matches an already-loaded object by
    // NAME before it ever compares the file, so the same path hands back the
    // copy still mapped and the new build is ignored: the upgrade appears to
    // succeed while the old code keeps running. Give the new file a name of its
    // own to open, then unlink that name - the mapping keeps the inode alive, so
    // nothing is left behind on disk.
    std::string openPath = shlibName;
    struct stat sb;
    bool haveStat = (stat(shlibName.c_str(), &sb) == 0);
    auto known = mLoadedShlibInodes.find(shlibName);
    if (haveStat && known != mLoadedShlibInodes.end() && known->second != sb.st_ino) {
        static int generation = 0;
        std::string genPath = shlibName + ".gen" + std::to_string(++generation);
        unlink(genPath.c_str());
        if (link(shlibName.c_str(), genPath.c_str()) == 0) {
            openPath = genPath;
        } else {
            LogWarn(VB_PLUGIN, "%s was replaced on disk but a private name could not be made (%s); "
                               "the previously loaded build stays in use until fppd restarts\n",
                    shlibName.c_str(), FPPstrerror(errno));
        }
    }

    void* handle = dlopen(openPath.c_str(), RTLD_NOW);
    if (openPath != shlibName) {
        unlink(openPath.c_str());
    }
    if (handle != nullptr && haveStat) {
        mLoadedShlibInodes[shlibName] = sb.st_ino;
    }
    if (handle == nullptr) {
        if (!FileExists(shlibName) && !FileExists(getFPPDDir("/" + shlibName))) {
            LogErr(VB_PLUGIN, "Failed to find shlib %s\n", shlibName.c_str());
        }
        char* er = dlerror();
        LogErr(VB_PLUGIN, "Failed to load shlib: %s\n", er);
        return nullptr;
    }
    FPPPlugins::Plugin* (*fptr)();
    *(void**)(&fptr) = dlsym(handle, "createPlugin");
    if (fptr == nullptr) {
        LogErr(VB_PLUGIN, "Failed to find  createPlugin() function in shlib %s\n", shlibName.c_str());
        WarningHolder::AddWarning(5, "Could not load plugin " + shlibName + " (missing createPlugin entry point)");
        dlclose(handle);
        return nullptr;
    }

    // Check plugin API version to prevent crashes from ABI-incompatible plugins.
    //
    // dlsym() on a handle searches the object AND its DT_NEEDED dependencies, and
    // every plugin links -lfpp. libfpp.so also defines this weak symbol (Plugin.h
    // emits it into every TU that includes it), so a plugin predating the version
    // mechanism resolves straight through to FPP's own copy and reports a version
    // that trivially matches. Verified on Debian 13/aarch64: without the
    // provenance check below, a .so defining no version symbol is accepted.
    // So confirm the symbol actually lives in the plugin by comparing the shared
    // object it was found in against the one createPlugin() came from - that entry
    // point is never defined by libfpp, so it identifies the plugin itself.
    int (*vfptr)();
    *(void**)(&vfptr) = dlsym(handle, "fpp_plugin_api_version");
    Dl_info versionInfo;
    Dl_info pluginInfo;
    bool ownVersionSymbol = vfptr != nullptr &&
                            dladdr((void*)vfptr, &versionInfo) != 0 &&
                            dladdr((void*)fptr, &pluginInfo) != 0 &&
                            versionInfo.dli_fbase == pluginInfo.dli_fbase;
    if (!ownVersionSymbol) {
        LogErr(VB_PLUGIN, "Plugin %s was compiled against an older FPP API and is not compatible. Please update and rebuild the plugin.\n", shlibName.c_str());
        WarningHolder::AddWarning(5, "Could not load plugin " + shlibName + " (built against an older FPP API - rebuild required)");
        dlclose(handle);
        return nullptr;
    }
    int pluginVersion = vfptr();
    if (pluginVersion != FPP_PLUGIN_API_VERSION) {
        LogErr(VB_PLUGIN, "Plugin %s API version %d does not match FPP API version %d. Please update and rebuild the plugin.\n", shlibName.c_str(), pluginVersion, FPP_PLUGIN_API_VERSION);
        WarningHolder::AddWarning(5, "Could not load plugin " + shlibName + " (API version mismatch - rebuild required)");
        dlclose(handle);
        return nullptr;
    }

    // Second, independent gate: compare the plugin's compiled-in sizeof() for
    // the two types it shares layout with against ours. The version number
    // above only helps if somebody remembers to bump it, and twice now a member
    // was added to CommandArg without one - a plugin then push_backs onto
    // Command::args with its own smaller node size while Command::~Command
    // destroys those nodes with the larger one, running ~string past the end of
    // the plugin's allocation. This catches that even when the versions agree.
    // Both types are pimpl'd as of version 5 so these should now never move,
    // which makes this cheap to keep and loud if the freeze ever slips.
    //
    // Same provenance trick as above, but inverted: here a symbol resolving to
    // libfpp's own copy is the expected case for a plugin that never includes
    // Commands.h. It registers no commands, so there is no layout to disagree
    // about and nothing to check.
    auto abiSizeMatches = [&](const char* symbol, unsigned int coreSize, const char* what) {
        unsigned int (*sfptr)();
        *(void**)(&sfptr) = dlsym(handle, symbol);
        Dl_info sizeInfo;
        if (sfptr == nullptr || dladdr((void*)sfptr, &sizeInfo) == 0 ||
            sizeInfo.dli_fbase != pluginInfo.dli_fbase) {
            return true; // not the plugin's own symbol - nothing to compare
        }
        unsigned int pluginSize = sfptr();
        if (pluginSize == coreSize) {
            return true;
        }
        LogErr(VB_PLUGIN, "Plugin %s was built with a %u byte %s but FPP uses %u bytes. Please update and rebuild the plugin.\n",
               shlibName.c_str(), pluginSize, what, coreSize);
        WarningHolder::AddWarning(5, "Could not load plugin " + shlibName + " (" + what + " ABI mismatch - rebuild required)");
        return false;
    };
    if (!abiSizeMatches("fpp_command_arg_abi_size", (unsigned int)sizeof(Command::CommandArg), "Command::CommandArg") ||
        !abiSizeMatches("fpp_command_abi_size", (unsigned int)sizeof(Command), "Command") ||
        // The VB_* macros resolve to a member offset within FPPLogger::INSTANCE,
        // so a plugin that disagrees about this layout passes _LogWrite() a
        // reference into the wrong facility - see the LAYOUT RULE in log.h.
        !abiSizeMatches("fpp_logger_instance_abi_size", (unsigned int)sizeof(FPPLoggerInstance), "FPPLoggerInstance") ||
        !abiSizeMatches("fpp_logger_abi_span", fpp_logger_abi_span(), "FPPLogger facility layout")) {
        dlclose(handle);
        return nullptr;
    }

    FPPPlugins::Plugin* p = fptr();
    if (p == nullptr) {
        LogErr(VB_PLUGIN, "Failed to create plugin from shlib %s\n", shlibName.c_str());
        WarningHolder::AddWarning(5, "Could not load plugin " + shlibName + " (createPlugin returned no plugin)");
        dlclose(handle);
        return nullptr;
    }
    mShlibHandles.push_back(handle);
    addPlugin(p);

    // Opt-in: only a plugin that declares itself safe gets dlclose()d on unload.
    // Same provenance check as the ABI gates above - the symbol has to be the
    // plugin's own, not one picked up from a dependency.
    bool supportsUnload = false;
    int (*unloadFptr)();
    *(void**)(&unloadFptr) = dlsym(handle, "fpp_plugin_supports_unload");
    Dl_info unloadInfo;
    if (unloadFptr != nullptr && dladdr((void*)unloadFptr, &unloadInfo) != 0 &&
        unloadInfo.dli_fbase == pluginInfo.dli_fbase) {
        supportsUnload = (unloadFptr() == 1);
    }
    mPluginLibraries[p->getName()] = { handle, supportsUnload };
    LogDebug(VB_PLUGIN, "Plugin %s %s unloading\n", p->getName().c_str(),
             supportsUnload ? "supports" : "does not support");
    return p;
}
bool PluginManager::hasPlugins() {
    return !mPlugins.empty();
}
void PluginManager::addPlugin(FPPPlugins::Plugin* p) {
    mPlugins.push_back(p);

    FPPPlugin::PlaylistEventPlugin* pep = dynamic_cast<FPPPlugin::PlaylistEventPlugin*>(p);
    if (pep) {
        mPlaylistPlugins.push_back(pep);
    }
    FPPPlugin::ChannelOutputPlugin* coep = dynamic_cast<FPPPlugin::ChannelOutputPlugin*>(p);
    if (coep) {
        mChannelOutputPlugins.push_back(coep);
    }
    FPPPlugin::ChannelDataPlugin* cdp = dynamic_cast<FPPPlugin::ChannelDataPlugin*>(p);
    if (cdp) {
        mChannelDataPlugins.push_back(cdp);
    }
    FPPPlugin::APIProviderPlugin* app = dynamic_cast<FPPPlugin::APIProviderPlugin*>(p);
    if (app) {
        mAPIProviderPlugins.push_back(app);
    }
}
void PluginManager::mediaCallback(const Json::Value& playlist, const MediaDetails& mediaDetails) {
    for (auto a : mPlaylistPlugins) {
        a->mediaCallback(playlist, mediaDetails);
    }
}
void PluginManager::registerApis() {
    for (auto a : mAPIProviderPlugins) {
        a->registerApis();
    }
}
void PluginManager::unregisterApis() {
    for (auto a : mAPIProviderPlugins) {
        a->unregisterApis();
    }
}
void PluginManager::modifySequenceData(int ms, uint8_t* seqData) {
    for (auto a : mChannelDataPlugins) {
        a->modifySequenceData(ms, seqData);
    }
}
void PluginManager::modifyChannelData(int ms, uint8_t* seqData) {
    for (auto a : mChannelDataPlugins) {
        a->modifyChannelData(ms, seqData);
    }
}
void PluginManager::addControlCallbacks(std::map<int, std::function<bool(int)>>& callbacks) {
    for (auto a : mAPIProviderPlugins) {
        // Record which descriptors this plugin added, so unloadPlugin() can take
        // them back out of the epoll loop. The callbacks it installs are
        // std::functions whose code lives in the plugin, so leaving one
        // registered after the plugin is gone is a call into nothing.
        std::set<int> before;
        for (const auto& c : callbacks) {
            before.insert(c.first);
        }
        a->addControlCallbacks(callbacks);
        auto* plugin = dynamic_cast<FPPPlugins::Plugin*>(a);
        if (!plugin) {
            continue;
        }
        for (const auto& c : callbacks) {
            if (before.find(c.first) == before.end()) {
                mPluginControlFds[plugin->getName()].push_back(c.first);
            }
        }
    }
}

// ---- Runtime load/unload ---------------------------------------------------

void PluginManager::noteChannelOutputCreated(FPPPlugins::ChannelOutputPlugin* plugin) {
    if (auto* p = dynamic_cast<FPPPlugins::Plugin*>(plugin)) {
        mPluginsWithOutputs.insert(p->getName());
    }
}

bool PluginManager::isPluginLoaded(const std::string& name) {
    for (auto& a : mPlugins) {
        if (a->getName() == name) {
            return true;
        }
    }
    return false;
}

void PluginManager::startPlugin(FPPPlugins::Plugin* plugin) {
    auto* api = dynamic_cast<FPPPlugins::APIProviderPlugin*>(plugin);
    if (!api) {
        return;
    }
    api->registerApis();

    // At boot these land in a map fppd hands to EPollManager once everything has
    // registered. That map is long gone by the time a plugin loads at runtime,
    // so collect into a local one and register the descriptors directly.
    std::map<int, std::function<bool(int)>> callbacks;
    api->addControlCallbacks(callbacks);
    for (auto& c : callbacks) {
        EPollManager::INSTANCE.addFileDescriptor(c.first, c.second);
        mPluginControlFds[plugin->getName()].push_back(c.first);
    }
}

void PluginManager::detachPlugin(FPPPlugins::Plugin* plugin) {
    auto drop = [plugin](auto& vec) {
        for (auto it = vec.begin(); it != vec.end();) {
            if (dynamic_cast<FPPPlugins::Plugin*>(*it) == plugin) {
                it = vec.erase(it);
            } else {
                ++it;
            }
        }
    };
    drop(mPlaylistPlugins);
    drop(mChannelOutputPlugins);
    drop(mChannelDataPlugins);
    drop(mAPIProviderPlugins);
    for (auto it = mPlugins.begin(); it != mPlugins.end(); ++it) {
        if (*it == plugin) {
            mPlugins.erase(it);
            break;
        }
    }
}

bool PluginManager::loadPlugin(const std::string& name, std::string& error) {
    if (isPluginLoaded(name)) {
        return true; // already running; nothing to do
    }
    if (!DirectoryExists(FPP_DIR_PLUGIN("/" + name))) {
        error = "no plugin directory for " + name;
        return false;
    }
    // Plenty of plugins are web-UI only - no callbacks script, nothing for fppd
    // to run. That is a successful no-op, not a failure: reporting it as one
    // told the user to restart FPPD to enable something that needs neither.
    std::string callbacks = FPP_DIR_PLUGIN("/" + name + "/callbacks");
    bool haveCallbacks = FileExists(callbacks);
    for (const char* ext : { ".sh", ".pl", ".php", ".py" }) {
        haveCallbacks = haveCallbacks || FileExists(callbacks + ext);
    }
    if (!haveCallbacks) {
        LogDebug(VB_PLUGIN, "Plugin %s has no callbacks; nothing for fppd to load\n", name.c_str());
        return true;
    }

    // A previous unloadPlugin() left the name in this set (the library stays
    // mapped), so clear it or loadUserPlugin() will not look at the plugin again.
    mLoadedUserPlugins.erase(name);

    FPPPlugins::Plugin* p = loadUserPlugin(name);
    if (!p) {
        // loadUserPlugin() logs the specific reason - a missing callbacks script,
        // an ABI-version refusal from loadSHLIBPlugin(), a createPlugin() that
        // returned nothing.
        error = "plugin " + name + " did not load - see the log";
        return false;
    }
    startPlugin(p);
    LogInfo(VB_PLUGIN, "Loaded plugin %s\n", name.c_str());
    return true;
}

bool PluginManager::unloadPlugin(const std::string& name, std::string& error) {
    FPPPlugins::Plugin* plugin = nullptr;
    for (auto& a : mPlugins) {
        if (a->getName() == name) {
            plugin = a;
            break;
        }
    }
    if (!plugin) {
        return true; // not loaded; the caller's goal already holds
    }
    // A channel output the plugin created is owned by the output system, not by
    // the plugin, and may be mid-show. Nothing here can take it back, so refuse
    // rather than leave an output running against a destroyed plugin. Keyed on
    // having actually produced one - merely implementing ChannelOutputPlugin
    // says nothing, since the FPPPlugin convenience base inherits all four
    // plugin interfaces.
    if (mPluginsWithOutputs.find(name) != mPluginsWithOutputs.end()) {
        error = name + " provides a channel output in use and can only be removed by restarting";
        return false;
    }

    // Order matters: stop inbound HTTP first (unregisterApis does not return
    // until no request is inside the plugin's handlers and they are destroyed),
    // then let the plugin stop its own threads and timers while it is still a
    // whole object, then stop calling it, then destroy it.
    if (auto* api = dynamic_cast<FPPPlugins::APIProviderPlugin*>(plugin)) {
        api->unregisterApis();
    }
    // Withdraw the plugin's epoll descriptors BEFORE shutdown(), while they are
    // still open: a plugin that closes them itself in shutdown() would make the
    // EPOLL_CTL_DEL fail, and the callback - which lives in the plugin - has to
    // come out of the map either way.
    auto fds = mPluginControlFds.find(name);
    if (fds != mPluginControlFds.end()) {
        for (int fd : fds->second) {
            EPollManager::INSTANCE.removeFileDescriptor(fd);
        }
        mPluginControlFds.erase(fds);
    }

    std::function<bool()> ready = plugin->shutdown();

    // Anything the plugin still has in flight holds a callback that lives in its
    // library. shutdown() cannot cancel those - they are owned by CurlManager -
    // so drop them here. Requests the plugin did not tag with its name are
    // invisible to this, which is exactly why unmapping is opt-in below.
    CurlManager::INSTANCE.cancelRequests(name);

    // Commands FPP registered from the plugin's descriptions.json. (A plugin
    // that added its own Command objects in registerApis() removes them in
    // unregisterApis(); these are the ones FPP owns.)
    auto cmds = mPluginCommands.find(name);
    if (cmds != mPluginCommands.end()) {
        for (Command* c : cmds->second) {
            CommandManager::INSTANCE.removeCommand(c); // unregisters only
            delete c;
        }
        mPluginCommands.erase(cmds);
    }

    // Detach now: from here nothing calls into the plugin, so as far as the rest
    // of FPP is concerned it is unloaded, whatever the settling time says.
    detachPlugin(plugin);
    mLoadedUserPlugins.erase(name);

    // Unmapping is opt-in. A plugin that declared FPP_PLUGIN_SUPPORTS_UNLOAD has
    // asserted it leaves nothing behind pointing into its library; without that
    // the mapping stays, which costs address space and nothing else.
    void* handle = nullptr;
    bool unmap = false;
    auto lib = mPluginLibraries.find(name);
    bool hadLibrary = (lib != mPluginLibraries.end());
    if (lib != mPluginLibraries.end()) {
        if (lib->second.supportsUnload) {
            handle = lib->second.handle;
            unmap = true;
            for (auto it = mShlibHandles.begin(); it != mShlibHandles.end(); ++it) {
                if (*it == handle) {
                    mShlibHandles.erase(it); // or Cleanup() would dlclose it a second time
                    break;
                }
            }
            mPluginLibraries.erase(lib);
            // The library leaves the link map when it is closed, so a later load
            // of the same path opens whatever is on disk - no generation link
            // needed for it.
            for (auto it = mLoadedShlibInodes.begin(); it != mLoadedShlibInodes.end();) {
                it = (it->first.find("/" + name + "/") != std::string::npos) ? mLoadedShlibInodes.erase(it) : ++it;
            }
        }
    }

    PendingUnload pending;
    pending.plugin = plugin;
    pending.handle = handle;
    pending.unmap = unmap;
    pending.hadLibrary = hadLibrary;
    pending.name = name;
    pending.ready = std::move(ready);
    // Cap the wait so a predicate that never returns true delays teardown
    // rather than holding the plugin forever.
    pending.deadlineMS = GetTimeMS() + 60000;

    if (!pending.ready) {
        finishUnload(pending);
        return true;
    }
    // Still finishing. A load arriving in the meantime is fine: it constructs a
    // fresh object, and the dlopen it does raises the library's reference count
    // above what this later drops.
    mPendingUnloads.push_back(std::move(pending));
    LogInfo(VB_PLUGIN, "Unloaded plugin %s (waiting for it to finish)\n", name.c_str());
    pollPendingUnload(name);
    return true;
}

// Re-arms itself once a second until the plugin says it is done. Safe to
// reschedule from inside the callback: a one-shot timer is removed from the list
// before it fires, so addTimer() here adds a fresh entry rather than mutating
// the one being run.
void PluginManager::pollPendingUnload(const std::string& name) {
    Timers::INSTANCE.addTimer("PluginUnload-" + name, GetTimeMS() + 1000, [this, name]() {
        for (auto it = mPendingUnloads.begin(); it != mPendingUnloads.end(); ++it) {
            if (it->name != name) {
                continue;
            }
            bool done = false;
            long long now = GetTimeMS();
            if (now >= it->deadlineMS) {
                LogWarn(VB_PLUGIN, "Plugin %s never reported finished; tearing down anyway\n", name.c_str());
                done = true;
            } else {
                // The predicate is the plugin's code, which is why this runs
                // while the library is still mapped.
                try {
                    done = it->ready();
                } catch (...) {
                    LogWarn(VB_PLUGIN, "Plugin %s threw while reporting readiness; tearing down\n", name.c_str());
                    done = true;
                }
            }
            if (done) {
                PendingUnload p = std::move(*it);
                mPendingUnloads.erase(it);
                finishUnload(p);
            } else {
                pollPendingUnload(name);
            }
            return;
        }
    });
}

void PluginManager::finishUnload(PendingUnload& p) {
    // Drop the predicate before the library goes: it is the plugin's code too.
    p.ready = nullptr;
    delete p.plugin; // the plugin's own full teardown
    if (p.unmap && p.handle) {
        dlclose(p.handle);
        LogInfo(VB_PLUGIN, "Unloaded plugin %s (library unmapped)\n", p.name.c_str());
    } else if (p.hadLibrary) {
        LogInfo(VB_PLUGIN, "Unloaded plugin %s (library kept mapped)\n", p.name.c_str());
    } else {
        // A script plugin has no library of its own to keep or drop.
        LogInfo(VB_PLUGIN, "Unloaded plugin %s\n", p.name.c_str());
    }
}
void PluginManager::multiSyncData(const std::string& pn, uint8_t* data, int len) {
    for (auto a : mPlugins) {
        if (a->getName() == pn) {
            a->multiSyncData(data, len);
        }
    }
}
void PluginManager::playlistCallback(const Json::Value& playlist, const std::string& action, const std::string& section, int item) {
    for (auto a : mPlaylistPlugins) {
        a->playlistCallback(playlist, action, section, item);
    }
}
void PluginManager::playlistInserted(const std::string& playlist, const int position, int endPosition, bool immediate) {
    for (auto a : mPlaylistPlugins) {
        a->playlistInserted(playlist, position, endPosition, immediate);
    }
}

// blocking
void MediaCallback::run(const Json::Value& playlist, const MediaDetails& mediaDetails) {
    int pid;

    if ((pid = fork()) == -1) {
        LogErr(VB_PLUGIN, "Failed to fork\n");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        LogDebug(VB_PLUGIN, "Child process, calling %s callback for media : %s\n", mName.c_str(), mFilename.c_str());

        std::string eventScript = FPP_DIR + "/scripts/eventScript";
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
        _exit(EXIT_FAILURE);
    } else {
        LogExcess(VB_PLUGIN, "Media parent process, resuming work.\n");
        waitpid(pid, NULL, 0);
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

        std::string eventScript = FPP_DIR + "/scripts/eventScript";
        Json::Value root;

        root = playlist;
        root["Action"] = action;
        root["Section"] = section;
        root["Item"] = idx;

        std::string pluginData = SaveJsonToString(root);
        LogDebug(VB_PLUGIN, "Playlist plugin data: %s\n", pluginData.c_str());
        execl(eventScript.c_str(), "eventScript", mFilename.c_str(), "--type", "playlist", "--data", pluginData.c_str(), NULL);

        LogErr(VB_PLUGIN, "We failed to exec our playlist callback!\n");
        _exit(EXIT_FAILURE);
    } else {
        LogExcess(VB_PLUGIN, "Playlist parent process, resuming work.\n");
        waitpid(pid, NULL, 0);
    }
}
void LifecycleCallback::run(const std::string& lifecycle) {
    int pid;
    if ((pid = fork()) == -1) {
        LogErr(VB_PLUGIN, "Failed to fork\n");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        LogDebug(VB_PLUGIN, "Child process, calling %s callback for lifecycle : %s\n", mName.c_str(), mFilename.c_str());

        std::string eventScript = FPP_DIR + "/scripts/eventScript";
        LogDebug(VB_PLUGIN, "Lifecycle plugin data: %s\n", lifecycle.c_str());
        execl(eventScript.c_str(), "eventScript", mFilename.c_str(), "--type", "lifecycle", lifecycle.c_str(), NULL);

        LogErr(VB_PLUGIN, "We failed to exec our lifecycle callback!\n");
        _exit(EXIT_FAILURE);
    } else {
        LogExcess(VB_PLUGIN, "Lifecycle parent process, resuming work.\n");
        waitpid(pid, NULL, 0);
    }
}
