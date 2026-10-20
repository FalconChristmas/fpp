/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */
#include "fpp-json.h"

#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <ctype.h>
#include <fcntl.h>
#include <map>
#include <memory>
#include <mutex>
#include <pwd.h>
#include <shared_mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/file.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "common.h"
#include "log.h"
#include "settings.h"

#ifdef PLATFORM_OSX
#include <mach-o/dyld.h>
#endif
#include <sys/fcntl.h>

// Built on first use and never destroyed. Both are reached from other static
// objects: AES67Manager's constructor asks for the media directory during
// static initialization, and ~PluginManager -> CommandManager::Cleanup() asks
// again when fppd leaves through exit(). File-scope strings could be used
// there before they were constructed, or after they were destroyed.
static std::string& FPPBinDir() {
    static std::string* dir = new std::string();
    return *dir;
}
static std::string& FPPMediaDir() {
    static std::string* dir = new std::string();
    return *dir;
}

std::string getFPPDDir(const std::string& path) {
    std::string& FPP_BIN_DIR = FPPBinDir();
    if (FPP_BIN_DIR == "") {
        char exePath[2048];
        exePath[0] = 0;
#ifdef PLATFORM_OSX
        uint32_t len = sizeof(exePath);
        _NSGetExecutablePath(exePath, &len);
#else
        ssize_t len = ::readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
        if (len == -1) {
            LogWarn(VB_GENERAL, "readlink /proc/self/exe failed: %s\n", strerror(errno));
            exePath[0] = '\0';
        } else {
            exePath[len] = '\0';
        }
#endif
        FPP_BIN_DIR = exePath;
        FPP_BIN_DIR = FPP_BIN_DIR.substr(0, FPP_BIN_DIR.rfind("/"));
        FPP_BIN_DIR = FPP_BIN_DIR.substr(0, FPP_BIN_DIR.rfind("/"));
    }
    return FPP_BIN_DIR + path;
}
std::string getFPPMediaDir(const std::string& path) {
    std::string& FPP_MEDIA_DIR = FPPMediaDir();
    if (FPP_MEDIA_DIR == "") {
        std::string filename = getFPPDDir("/www/media_root.txt");
        std::string fc = "";
        if (FileExists(filename)) {
            fc = GetFileContents(filename);
            FPP_MEDIA_DIR = fc;
            TrimWhiteSpace(FPP_MEDIA_DIR);
        }
        if (FPP_MEDIA_DIR == "") {
#ifdef PLATFORM_OSX
            // getpwuid() is not thread-safe; use getpwuid_r().  Also guards
            // against a NULL deref when neither $HOME nor the passwd entry exist.
            const char* homedir = getenv("HOME");
            struct passwd pwd;
            struct passwd* pwres = nullptr;
            char pbuf[16384];
            if (homedir == NULL &&
                getpwuid_r(getuid(), &pwd, pbuf, sizeof(pbuf), &pwres) == 0 && pwres) {
                homedir = pwres->pw_dir;
            }
            if (homedir) {
                FPP_MEDIA_DIR = homedir;
                FPP_MEDIA_DIR += "/Documents/fpp";
            }
#else
            FPP_MEDIA_DIR = "/home/fpp/media";
#endif
        }
        if (FPP_MEDIA_DIR != fc) {
            PutFileContents(filename, FPP_MEDIA_DIR);
        }
    }
    return FPP_MEDIA_DIR + path;
}

class SettingListener {
public:
    std::string id;
    std::function<void(const std::string&)> callback;
};
class SettingsConfig {
public:
    SettingsConfig();
    ~SettingsConfig();

    void Init();

    // written by the settings-file reload, read unlocked by getFPPmode()
    std::atomic<FPPMode> fppMode;

    std::string getSetting(const std::string& setting, const std::string& defaultVal);
    int getSettingInt(const std::string& setting, int defaultVal);

    void setSetting(const std::string& key, const std::string& value, bool persist);

    int LoadSettings(const std::string& base, bool logChanges = false);

    void registerSettingsListener(const std::string& id, const std::string& setting, std::function<void(const std::string&)>&& cb);
    void unregisterSettingsListener(const std::string& id, const std::string& setting);

private:
    bool setSettingLocked(const std::string& key, const std::string& value, bool logInfo);
    bool persistSetting(const std::string& key, const std::string& value);
    void notifyListeners(const std::map<std::string, std::string>& changed);
    void LoadSettingsInfo();

    Json::Value settingsInfo;
    Json::Value settings;
    std::shared_mutex settingsMutex;
    // Serializes setSetting(persist) so the file is written in the same order
    // the in-memory values changed.  Taken before settingsMutex, never after.
    std::mutex persistMutex;

    std::map<std::string, std::list<SettingListener>> settingsListeners;
    std::shared_mutex settingsListenersMutex;
    // Listener calls copied out by notifyListeners() and not yet returned,
    // per (setting, id): unregisterSettingsListener() waits for its own.
    // listenerCallsLock is a leaf lock; no callback runs under it.
    std::map<std::pair<std::string, std::string>, int> listenerCallsInFlight;
    std::mutex listenerCallsLock;
    std::condition_variable listenerCallFinished;
    void retireListenerCalls(const std::vector<std::pair<SettingListener, const std::string*>>& calls,
                             size_t from, size_t to);
};
// Nonzero while this thread is inside a settings listener: a listener that
// unregisters (itself or another) must not wait on a call that may be its
// own, or one waiting on it.
static thread_local int t_inSettingsListener = 0;
static SettingsConfig settings;

SettingsConfig::SettingsConfig() {
}

SettingsConfig::~SettingsConfig() {
}

void SettingsConfig::Init() {
    if (settingsInfo.empty()) {
        LoadSettingsInfo();
        fppMode = PLAYER_MODE;

        // load UUID File
        if (access(getFPPDDir("/scripts/get_uuid").c_str(), F_OK) == 0) {
            FILE* uuid_fp;
            char temp_uuid[500];
            uuid_fp = popen(getFPPDDir("/scripts/get_uuid").c_str(), "r");
            if (uuid_fp == NULL) {
                LogWarn(VB_SETTING, "Couldn't execute get_uuid\n");
            } else {
                if (fgets(temp_uuid, sizeof(temp_uuid), uuid_fp) != NULL) {
                    int pos = strlen(temp_uuid) - 1;
                    // Strip newline
                    if (pos >= 0 && '\n' == temp_uuid[pos]) {
                        temp_uuid[pos] = '\0';
                    }
                    settings["SystemUUID"] = std::string(temp_uuid);
                }
                pclose(uuid_fp);
            }
        }

        // Set to unknown if not found
        if (!settings.isMember("SystemUUID")) {
            settings["SystemUUID"] = "Unknown";
            LogWarn(VB_SETTING, "Couldn't find UUID\n");
        }
    }
}
void SettingsConfig::registerSettingsListener(const std::string& id, const std::string& setting, std::function<void(const std::string&)>&& cb) {
    std::unique_lock<std::shared_mutex> lock(settingsListenersMutex);
    SettingListener listener;
    listener.id = id;
    listener.callback = std::move(cb);

    if (settingsListeners.find(setting) == settingsListeners.end()) {
        settingsListeners[setting] = std::list<SettingListener>();
    }
    settingsListeners[setting].push_back(listener);
    LogDebug(VB_SETTING, "Registered settings listener for %s setting with id %s\n", setting.c_str(), id.c_str());
}
void SettingsConfig::unregisterSettingsListener(const std::string& id, const std::string& setting) {
    {
        std::unique_lock<std::shared_mutex> lock(settingsListenersMutex);
        if (settingsListeners.find(setting) != settingsListeners.end()) {
            auto& listeners = settingsListeners[setting];
            for (auto it = listeners.begin(); it != listeners.end();) {
                if (it->id == id) {
                    it = listeners.erase(it);
                } else {
                    ++it;
                }
            }
            if (listeners.empty()) {
                settingsListeners.erase(setting);
            }
        }
    }
    // Listeners are called from a copy with no lock held, so one copied out
    // before the erase above can still be running, or about to run. Its
    // owner frees what it captured once this returns (an output deleted by a
    // reload), so wait for those calls to finish. Not under
    // settingsListenersMutex: a running listener may register or unregister.
    if (t_inSettingsListener == 0) {
        std::unique_lock<std::mutex> lock(listenerCallsLock);
        auto key = std::make_pair(setting, id);
        listenerCallFinished.wait(lock, [this, &key]() { return listenerCallsInFlight.count(key) == 0; });
    }
}
void SettingsConfig::retireListenerCalls(const std::vector<std::pair<SettingListener, const std::string*>>& calls,
                                         size_t from, size_t to) {
    if (from >= to) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(listenerCallsLock);
        for (size_t i = from; i < to; i++) {
            auto f = listenerCallsInFlight.find(std::make_pair(*calls[i].second, calls[i].first.id));
            if (f != listenerCallsInFlight.end() && --f->second <= 0) {
                listenerCallsInFlight.erase(f);
            }
        }
    }
    listenerCallFinished.notify_all();
}

void SettingsConfig::LoadSettingsInfo() {
    std::string settingsJson = getFPPDDir("/www/settings.json");
    if (FileExists(settingsJson)) {
        settingsInfo = LoadJsonFromFile(settingsJson, JsonRoot::Object);

        Json::Value s = settingsInfo["settings"];
        Json::Value::Members memberNames = s.getMemberNames();

        for (int i = 0; i < memberNames.size(); i++) {
            std::string type = s[memberNames[i]]["type"].asString();
            std::string def = "";

            if (s[memberNames[i]].isMember("default")) {
                def = s[memberNames[i]]["default"].asString();
            } else {
                if ((type == "checkbox") ||
                    (type == "number")) {
                    def = std::string("0");
                } else {
                    def = std::string("");
                }
            }

            settings[memberNames[i]] = def;
            // The following will never be logged due to startup order
            LogExcess(VB_SETTING, "Setting default for '%s' setting to '%s'\n",
                      memberNames[i].c_str(), def.c_str());
        }
    }
#ifdef PLATFORM_OSX
    char hostname[256];
    gethostname(hostname, 256);
    std::string hn = hostname;
    int idx = hn.find(".");
    if (idx != std::string::npos) {
        hn = hn.substr(0, idx);
    }
    settings["HostName"] = hn;
#endif
}
void SettingsConfig::setSetting(const std::string& key, const std::string& value, bool persist) {
    std::unique_lock<std::mutex> persistLock(persistMutex, std::defer_lock);
    if (persist) {
        // The file is line based and nothing escapes a value, so a line break
        // would start a new setting.  Same rule as WriteSettingToFile().
        if (value.find_first_of(std::string("\r\n\0", 3)) != std::string::npos) {
            LogWarn(VB_SETTING, "Not saving setting %s: value contains a line break or NUL\n", key.c_str());
            return;
        }
        persistLock.lock();
    }
    std::unique_lock<std::shared_mutex> lock(settingsMutex);
    if (setSettingLocked(key, value, false)) {
        lock.unlock();
        if (persist) {
            persistSetting(key, value);
            persistLock.unlock();
        }
        notifyListeners({ { key, value } });
    }
}
// Rewrites one key in the settings file under a single LOCK_EX held across
// the read and the write, so a concurrent writer (the PHP side holds the
// same lock) can't slip in between and have its change lost.  Called without
// settingsMutex: LoadSettings() takes settingsMutex and then the file lock.
bool SettingsConfig::persistSetting(const std::string& key, const std::string& value) {
    int fd = open(FPP_FILE_SETTINGS.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
    if (fd == -1) {
        LogWarn(VB_SETTING, "Could not open %s to save %s: %s\n", FPP_FILE_SETTINGS.c_str(), key.c_str(), strerror(errno));
        return false;
    }
    while (flock(fd, LOCK_EX) == -1 && errno == EINTR) {
    }
    std::string contents;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) != 0) {
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            LogWarn(VB_SETTING, "Could not read %s to save %s: %s\n", FPP_FILE_SETTINGS.c_str(), key.c_str(), strerror(errno));
            flock(fd, LOCK_UN);
            close(fd);
            return false;
        }
        contents.append(buf, n);
    }

    // Match the key the way LoadSettings() reads it (the text before the
    // first '=', trimmed) rather than searching for "key = " anywhere, which
    // also hit a key that ends with this one or a value containing that text.
    // The first line for the key is replaced and any later duplicates are
    // dropped, since the last one would win on the next load.
    const std::string newLine = key + " = \"" + value + "\"\n";
    std::string out;
    out.reserve(contents.size() + newLine.size());
    bool found = false;
    size_t pos = 0;
    while (pos < contents.size()) {
        size_t end = contents.find('\n', pos);
        end = (end == std::string::npos) ? contents.size() : end + 1;
        std::string line = contents.substr(pos, end - pos);
        pos = end;
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            char* k = trimwhitespace(line.substr(0, eq).c_str());
            bool match = (key == k);
            free(k);
            if (match) {
                if (!found) {
                    out += newLine;
                    found = true;
                }
                continue;
            }
        }
        out += line;
        if (out.back() != '\n') {
            out += '\n';
        }
    }
    if (!found) {
        out += newLine;
    }

    bool ok = ftruncate(fd, 0) == 0;
    const char* p = out.data();
    size_t left = out.size();
    off_t off = 0;
    while (ok && left > 0) {
        ssize_t w = pwrite(fd, p, left, off);
        if (w < 0 && errno == EINTR) {
            continue;
        }
        if (w <= 0) {
            ok = false;
            break;
        }
        p += w;
        left -= w;
        off += w;
    }
    if (!ok) {
        LogWarn(VB_SETTING, "Could not write %s to save %s: %s\n", FPP_FILE_SETTINGS.c_str(), key.c_str(), strerror(errno));
    }
    flock(fd, LOCK_UN);
    close(fd);
    SetFilePerms(FPP_FILE_SETTINGS);
    return ok;
}
// Listeners run with no settings lock held.  They take their own locks and
// read settings back (MultiSync holds its lock and calls getSetting, which
// takes settingsMutex), and settingsMutex is held while settingsListenersMutex
// is taken, so calling them under either lock closes a cycle.  It also lets a
// listener register or unregister listeners.  Each call is counted until it
// returns, for unregisterSettingsListener() to wait on.
void SettingsConfig::notifyListeners(const std::map<std::string, std::string>& changed) {
    std::vector<std::pair<SettingListener, const std::string*>> toCall;
    {
        std::shared_lock<std::shared_mutex> listenersLock(settingsListenersMutex);
        std::lock_guard<std::mutex> callsLock(listenerCallsLock);
        for (const auto& change : changed) {
            auto it = settingsListeners.find(change.first);
            if (it != settingsListeners.end()) {
                for (const auto& listener : it->second) {
                    toCall.emplace_back(listener, &change.first);
                    ++listenerCallsInFlight[std::make_pair(change.first, listener.id)];
                }
            }
        }
    }
    if (toCall.empty()) {
        return;
    }
    // Each call is retired as it returns; a throw retires the rest.
    size_t next = 0;
    struct InFlight {
        SettingsConfig* self;
        const std::vector<std::pair<SettingListener, const std::string*>>& calls;
        const size_t& next;
        ~InFlight() {
            --t_inSettingsListener;
            self->retireListenerCalls(calls, next, calls.size());
        }
    };
    ++t_inSettingsListener;
    InFlight inFlight{ this, toCall, next };
    while (next < toCall.size()) {
        const auto& [listener, key] = toCall[next];
        const std::string& value = changed.at(*key);
        LogDebug(VB_SETTING, "Notifying listener %s for setting %s with value '%s'\n",
                 listener.id.c_str(), key->c_str(), value.c_str());
        listener.callback(value);
        retireListenerCalls(toCall, next, next + 1);
        ++next;
    }
}
bool SettingsConfig::setSettingLocked(const std::string& key, const std::string& value, bool logInfo) {
    std::string val = settings[key].asString();
    if (val != value) {
        if (logInfo) {
            std::shared_lock<std::shared_mutex> listenersLock(settingsListenersMutex);
            if (settingsListeners.find(key) != settingsListeners.end()) {
                LogInfo(VB_SETTING, "Setting %s changed from %s to %s\n", key.c_str(), val.c_str(), value.c_str());
            } else {
                LogDebug(VB_SETTING, "Setting %s:  from %s to %s\n", key.c_str(), val.c_str(), value.c_str());
            }
        } else {
            LogDebug(VB_SETTING, "Setting %s:  from %s to %s\n", key.c_str(), val.c_str(), value.c_str());
        }
        settings[key] = value;
        if (key == "fppMode") {
            if (value == "player" || value.empty()) {
                fppMode = PLAYER_MODE;
            } else if (value == "bridge") {
                // legacy, remap to PLAYER
                fppMode = PLAYER_MODE;
            } else if (value == "master") {
                // legacy, remap to player, but turn on MultiSync setting
                fppMode = PLAYER_MODE;
                settings[key] = "player";
                settings["MultiSyncEnabled"] = "1";
            } else if (value == "remote") {
                fppMode = REMOTE_MODE;
            } else {
                // default to playmode if string is invalid
                fppMode = PLAYER_MODE;
                settings[key] = "player";
            }
        } else if (startsWith(key, "LogLevel_")) {
            // Starts with LogLevel_
            if (value != "") {
                FPPLogger::INSTANCE.SetLevel(key.c_str(), value.c_str());
            } else {
                FPPLogger::INSTANCE.SetLevel(key.c_str(), "warn");
            }
        }
        return true;
    }
    return false;
}
std::string SettingsConfig::getSetting(const std::string& setting, const std::string& defaultVal) {
    std::shared_lock<std::shared_mutex> lock(settingsMutex);
    std::string result = defaultVal;
    if (settings.isMember(setting)) {
        result = settings[setting].asString();
    }
    LogExcess(VB_SETTING, "getSetting(%s) returning '%s'\n", setting.c_str(), result.c_str());
    return result;
}
int SettingsConfig::getSettingInt(const std::string& setting, int defaultVal) {
    std::shared_lock<std::shared_mutex> lock(settingsMutex);
    int result = defaultVal;
    if (settings.isMember(setting)) {
        result = atoi(settings[setting].asString().c_str());
    }
    LogExcess(VB_SETTING, "getSettingInt(%s) returning %d\n", setting.c_str(), result);
    return result;
}

// Returns a string that's the white-space trimmed version
// of the input string.  Also trim double quotes now that the
// settings file will have double quotes in it.
char* trimwhitespace(const char* str, int quotesAlso) {
    const char* end;
    size_t out_size;
    char* out;

    // Trim leading space
    while ((isspace(*str)) || (quotesAlso && (*str == '"')))
        str++;

    if (*str == 0) // All spaces?
        return strdup("");

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && ((isspace(*end)) || (quotesAlso && (*end == '"'))))
        end--;
    end++;

    // Set output size to minimum of trimmed string length
    out_size = end - str;

    // All whitespace
    if (out_size == 0)
        return strdup("");

    out = (char*)malloc(out_size + 1);
    // If we didn't allocate anything, die!
    if (!out) {
        fprintf(stderr, "Failed to allocate memory for string trimming\n");
        exit(EXIT_FAILURE);
    }

    // Copy trimmed string and add null terminator
    memcpy(out, str, out_size);
    out[out_size] = 0;

    return out;
}

// Caller must free the char array we give them
char* modeToString(int mode) {
    if (mode == PLAYER_MODE)
        return strdup("player");
    else if (mode == REMOTE_MODE)
        return strdup("remote");
    else if (mode == BRIDGE_MODE)
        return strdup("bridge");
    else if (mode == MASTER_MODE)
        return strdup("master");

    return strdup("unknown");
}

int SettingsConfig::LoadSettings(const std::string& base, bool logChanges) {
    std::unique_lock<std::shared_mutex> lock(settingsMutex);
    Init();

    if (!FileExists(FPP_FILE_SETTINGS)) {
        LogWarn(VB_SETTING,
                "Attempted to load settings file %s which does not exist!\n", FPP_FILE_SETTINGS.c_str());
        return -1;
    }

    FILE* file = fopen(FPP_FILE_SETTINGS.c_str(), "r");
    std::map<std::string, std::string> changed;
    if (file != NULL) {
        flock(fileno(file), LOCK_EX); // Lock the file for exclusive access
        char* line = (char*)calloc(256, 1);
        size_t len = 256;
        ssize_t read;
        int sIndex = 0;

        while ((read = getline(&line, &len, file)) != -1) {
            if ((!line) || (!read) || (read == 1))
                continue;

            // Split on the first '=' only and drop exactly one pair of
            // surrounding quotes, the way the PHP side and getRawSetting()
            // read this file.  Splitting on every '=' cut base64 passwords
            // and JSON values short, and trimming every quote ate quotes
            // that belong to the value.
            std::string l(line, read);
            size_t eq = l.find('=');
            char* key = trimwhitespace(l.substr(0, eq).c_str());
            if (!strlen(key)) {
                free(key);
                continue;
            }
            if (eq == std::string::npos) {
                fprintf(stderr, "Error tokenizing value for %s setting\n", key);
                free(key);
                continue;
            }
            std::string value = l.substr(eq + 1);
            TrimWhiteSpace(value);
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            if (setSettingLocked(key, value, logChanges)) {
                changed[key] = value; // Store the changed setting
            }
            free(key);
        }

        if (line) {
            free(line);
        }
        flock(fileno(file), LOCK_UN); // Lock the file for exclusive access
        fclose(file);
    } else {
        LogWarn(VB_SETTING, "Warning: couldn't open settings file: '%s'!\n", FPP_FILE_SETTINGS.c_str());
        return -1;
    }
    lock.unlock();
    notifyListeners(changed);
    return 0;
}

int LoadSettings(const char* base, bool logChanges) {
    return settings.LoadSettings(base, logChanges);
}

int setSetting(const std::string& key, const int value) {
    return setSetting(key, std::to_string(value));
}
int setSetting(const std::string& key, const std::string& value, bool persist) {
    settings.setSetting(key, value, persist);
    return 1;
}
int SetSetting(const std::string& key, const int value) {
    return setSetting(key, value);
}
int SetSetting(const std::string& key, const std::string& value) {
    return setSetting(key, value);
}
std::string getSetting(const char* setting, const char* defaultVal) {
    if ((!setting) || (setting[0] == 0x00)) {
        LogErr(VB_SETTING, "getSetting() called with NULL or empty value\n");
        std::string result = defaultVal;
        return result;
    }
    return settings.getSetting(setting, defaultVal);
}

int getSettingInt(const char* setting, int defaultVal) {
    if ((!setting) || (setting[0] == 0x00)) {
        LogErr(VB_SETTING, "getSetting() called with NULL or empty value\n");
        return defaultVal;
    }
    return settings.getSettingInt(setting, defaultVal);
}

bool isPipeWireBackend() {
#ifdef PLATFORM_OSX
    // There is no PipeWire on macOS, whatever MediaBackend says (its default is
    // a PipeWire mode). Media plays through GStreamer's non-PipeWire path there -
    // autoaudiosink, i.e. osxaudiosink - so every PipeWire-only feature has to
    // see "no" from here rather than build pipewiresink/pipewiresrc pipelines
    // that cannot be created.
    return false;
#endif
    // Default to the settings.json default rather than "".  ALSA is retired --
    // the UI offers only the two PipeWire modes and FPPINIT migrates a stored
    // "alsa" away -- so a box that simply has no MediaBackend key must not fall
    // through to the retired path.
    std::string mb = settings.getSetting("MediaBackend", "pipewire-simple");
    // Lowercase compare without pulling in toLowerCopy here
    for (char& c : mb) {
        if (c >= 'A' && c <= 'Z')
            c = c + ('a' - 'A');
    }
    return (mb == "pipewire" || mb == "pipewire-simple");
}

FPPMode getFPPmode(void) {
    return settings.fppMode;
}

const std::string getFPPmodeStr(FPPMode mode) {
    if (mode == UNKNOWN_MODE)
        mode = settings.fppMode;

    std::string result;
    char* modePtr = modeToString((int)mode);
    if (modePtr) {
        result = modePtr;
        free(modePtr);
    } else {
        result = "unknown";
    }

    return result;
}

void registerSettingsListener(const std::string& id, const std::string& key, std::function<void(const std::string&)>&& cb) {
    settings.registerSettingsListener(id, key, std::move(cb));
}
void unregisterSettingsListener(const std::string& id, const std::string& key) {
    settings.unregisterSettingsListener(id, key);
}
