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
#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

#include <cstring>
#include <ctype.h>
#include <map>
#include <memory>
#include <mutex>
#include <pwd.h>
#include <shared_mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <utility>

#include "common.h"
#include "log.h"
#include "settings.h"

#ifdef PLATFORM_OSX
#include <mach-o/dyld.h>
#endif
#include <sys/fcntl.h>

static std::string FPP_BIN_DIR("");
static std::string FPP_MEDIA_DIR("");

std::string getFPPDDir(const std::string& path) {
    if (FPP_BIN_DIR == "") {
        char exePath[2048];
        exePath[0] = 0;
#ifdef PLATFORM_OSX
        uint32_t len = sizeof(exePath);
        _NSGetExecutablePath(exePath, &len);
#else
        ssize_t len = ::readlink("/proc/self/exe", exePath, sizeof(exePath));
        exePath[len] = '\0';
#endif
        FPP_BIN_DIR = exePath;
        FPP_BIN_DIR = FPP_BIN_DIR.substr(0, FPP_BIN_DIR.rfind("/"));
        FPP_BIN_DIR = FPP_BIN_DIR.substr(0, FPP_BIN_DIR.rfind("/"));
    }
    return FPP_BIN_DIR + path;
}
std::string getFPPMediaDir(const std::string& path) {
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
            const char* homedir;
            if ((homedir = getenv("HOME")) == NULL) {
                homedir = getpwuid(getuid())->pw_dir;
            }
            FPP_MEDIA_DIR = homedir;
            FPP_MEDIA_DIR += "/Documents/fpp";
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

    FPPMode fppMode;

    std::string getSetting(const std::string& setting, const std::string& defaultVal);
    int getSettingInt(const std::string& setting, int defaultVal);

    void setSetting(const std::string& key, const std::string& value, bool persist);

    int LoadSettings(const std::string& base);

    void registerSettingsListener(const std::string& id, const std::string& setting, std::function<void(const std::string&)>&& cb);
    void unregisterSettingsListener(const std::string& id, const std::string& setting);

private:
    bool setSettingLocked(const std::string& key, const std::string& value);
    void LoadSettingsInfo();

    Json::Value settingsInfo;
    Json::Value settings;
    std::shared_mutex settingsMutex;

    std::map<std::string, std::list<SettingListener>> settingsListeners;
    std::shared_mutex settingsListenersMutex;
};
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
            }
            if (fgets(temp_uuid, sizeof(temp_uuid), uuid_fp) != NULL) {
                int pos = strlen(temp_uuid) - 1;
                // Strip newline
                if ('\n' == temp_uuid[pos]) {
                    temp_uuid[pos] = '\0';
                }
                settings["SystemUUID"] = std::string(temp_uuid);
            }
            if (uuid_fp != NULL) {
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

void SettingsConfig::LoadSettingsInfo() {
    std::string settingsJson = getFPPDDir("/www/settings.json");
    if (FileExists(settingsJson)) {
        settingsInfo = LoadJsonFromFile(settingsJson);

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
    std::unique_lock<std::shared_mutex> lock(settingsMutex);
    if (setSettingLocked(key, value)) {
        if (persist) {
            std::string contents = GetFileContents(FPP_FILE_SETTINGS);
            auto pos = contents.find(key + " = ");
            if (pos != std::string::npos) {
                // Found the key, replace its value
                size_t endPos = contents.find('\n', pos);
                if (endPos == std::string::npos) {
                    endPos = contents.size();
                }
                std::string oldLine = contents.substr(pos, endPos - pos);
                std::string newLine = key + " = \"" + value + "\"\n";
                contents.replace(pos, oldLine.size(), newLine);
            } else {
                contents += key + " = \"" + value + "\"\n";
            }
            PutFileContents(FPP_FILE_SETTINGS, contents);
        }
        lock.unlock();

        // Notify listeners about the setting change
        std::shared_lock<std::shared_mutex> listenersLock(settingsListenersMutex);
        if (settingsListeners.find(key) != settingsListeners.end()) {
            for (const auto& listener : settingsListeners[key]) {
                LogDebug(VB_SETTING, "Notifying listener %s for setting %s with value '%s'\n",
                         listener.id.c_str(), key.c_str(), value.c_str());
                listener.callback(value);
            }
        }
    }
}
bool SettingsConfig::setSettingLocked(const std::string& key, const std::string& value) {
    std::string val = settings[key].asString();
    if (val != value) {
        LogDebug(VB_SETTING, "Setting %s:  from %s to %s\n", key.c_str(), val.c_str(), value.c_str());
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
    LogExcess(VB_SETTING, "getSetting(%s) returning '%s'\n", setting, result.c_str());
    return result;
}
int SettingsConfig::getSettingInt(const std::string& setting, int defaultVal) {
    std::shared_lock<std::shared_mutex> lock(settingsMutex);
    int result = defaultVal;
    if (settings.isMember(setting)) {
        result = atoi(settings[setting].asString().c_str());
    }
    LogExcess(VB_SETTING, "getSettingInt(%s) returning %d\n", setting, result);
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

int SettingsConfig::LoadSettings(const std::string& base) {
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

            char *key = NULL, *value = NULL; // These are values we're looking for and will
                                             // run through trimwhitespace which means they
                                             // must be freed before we are done.

            char* token = strtok(line, "=");
            if (!token) {
                if (line) {
                    free(line);
                }
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
            if (setSettingLocked(key, value)) {
                changed[key] = value; // Store the changed setting
            }

            if (key) {
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
        flock(fileno(file), LOCK_UN); // Lock the file for exclusive access
        fclose(file);
    } else {
        LogWarn(VB_SETTING, "Warning: couldn't open settings file: '%s'!\n", FPP_FILE_SETTINGS.c_str());
        return -1;
    }
    lock.unlock();
    if (!changed.empty()) {
        std::shared_lock<std::shared_mutex> listenersLock(settingsListenersMutex);
        for (const auto& change : changed) {
            if (settingsListeners.find(change.first) != settingsListeners.end()) {
                for (const auto& listener : settingsListeners[change.first]) {
                    listener.callback(change.second);
                }
            }
        }
    }
    return 0;
}

int LoadSettings(const char* base) {
    return settings.LoadSettings(base);
}

int setSetting(const std::string& key, const int value) {
    return setSetting(key, std::to_string(value));
}
int setSetting(const std::string& key, const std::string& value, bool persist) {
    settings.setSetting(key, value, persist);
    return 1;
}
int SetSetting(const std::string& key, const int value) {
    return SetSetting(key, value);
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
