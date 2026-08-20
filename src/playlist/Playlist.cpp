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

#include <sys/wait.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <functional>
#include <time.h>

#include "../Events.h"
#include "../Sequence.h"
#include "../Warnings.h"
#include "../common.h"
#include "../log.h"
#include "../settings.h"

#include "../fseq/FSEQFile.h"

#include "Playlist.h"
#include "Plugins.h"
#include "fpp.h"

#include "PlaylistEntryBoth.h"
#include "PlaylistEntryBranch.h"
#include "PlaylistEntryCommand.h"
#include "PlaylistEntryDynamic.h"
#include "PlaylistEntryImage.h"
#include "PlaylistEntryMedia.h"
#include "PlaylistEntryPause.h"
#include "PlaylistEntryPlaylist.h"
#include "PlaylistEntryRemap.h"
#include "PlaylistEntryScript.h"
#include "PlaylistEntrySequence.h"
#include "PlaylistEntryURL.h"
#include "../mediaoutput/StreamSlotManager.h"
#include "../util/RegExCache.h"

static std::list<Playlist*> PL_CLEANUPS;
// Entries deleted while one of their own methods may still be on the
// call stack (e.g. a "Start Playlist" command entry that reloads this
// playlist from inside StartPlaying) are parked here and freed at the
// top of Process(), after the stack has unwound.  Mirrors PL_CLEANUPS.
static std::list<PlaylistEntryBase*> PL_ENTRY_CLEANUPS;
Playlist* playlist = NULL;

// ──────────────────────────────────────────────────────────────────────────
// Crash snapshot
//
// What was playing, and where, at the moment fppd died.  The crash handler
// already records the sequence file and elapsed ms; the playlist half is the
// part that explains *why* a given entry was being started -- most usefully
// the three section sizes, since an empty section that a caller believed was
// non-empty is the shape of a whole class of bug here.
//
// Deliberately plain scalars in file statics, never the live Playlist: see
// PlaylistDumpCrashState in the header for why the handler cannot lock or
// walk containers.  Torn reads are accepted.
// ──────────────────────────────────────────────────────────────────────────
namespace {
struct CrashSnapshot {
    char playlistName[128];
    char section[24];
    int sectionPosition;
    int leadInSize;
    int mainSize;
    int leadOutSize;
    int status;
    bool everSet;
};
CrashSnapshot s_crashSnapshot{};

void copyFixed(char* dst, size_t dstSize, const std::string& src) {
    size_t n = src.size() < dstSize - 1 ? src.size() : dstSize - 1;
    memcpy(dst, src.data(), n);
    dst[n] = '\0';
}
} // namespace

void Playlist::UpdateCrashSnapshot(void) {
    CrashSnapshot& s = s_crashSnapshot;
    copyFixed(s.playlistName, sizeof(s.playlistName), m_name);
    copyFixed(s.section, sizeof(s.section), m_currentSectionStr);
    s.sectionPosition = m_sectionPosition;
    s.leadInSize = (int)m_leadIn.size();
    s.mainSize = (int)m_mainPlaylist.size();
    s.leadOutSize = (int)m_leadOut.size();
    s.status = (int)m_status;
    s.everSet = true;
}

void PlaylistDumpCrashState(int fd) {
    const CrashSnapshot& s = s_crashSnapshot;
    char buf[512];
    int n;
    if (!s.everSet) {
        n = snprintf(buf, sizeof(buf), "Playlist state: none (no playlist has run)\n");
    } else {
        n = snprintf(buf, sizeof(buf),
                     "Playlist state: name='%.127s' section=%.23s pos=%d status=%d "
                     "sizes(leadIn=%d main=%d leadOut=%d)\n",
                     s.playlistName, s.section, s.sectionPosition, s.status,
                     s.leadInSize, s.mainSize, s.leadOutSize);
    }
    if (n > 0) {
        (void)!write(fd, buf, (size_t)(n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1));
    }
}

// ──────────────────────────────────────────────────────────────────────────
// Deferred notifications and deferred re-entrant starts.
//
// TriggerPreset() and PluginManager::playlistCallback() run arbitrary code
// INLINE on the calling thread: preset commands can start/stop playlists,
// native plugins can call any FPP API, and script plugins fork and block in
// waitpid.  Historically these fired from inside Play()/Start()/StopNow()/
// SetIdle()/Process() while m_playlistMutex (recursive) was held and while
// this playlist's state was mid-transition.  The recursion "worked", but:
//   - PLAYLIST_STARTED presets that start another playlist re-entered
//     Play() on the same stack; chains (A starts B, B starts A) recursed
//     without bound until stack overflow.
//   - Presets/plugins observed and mutated half-updated playlist state
//     (the PL_CLEANUPS / PL_ENTRY_CLEANUPS / deferred-start machinery is
//     scar tissue from exactly this).
//   - The mutex stayed held across fork/waitpid script runs, stalling the
//     status API.
//
// The invariant now: a state transition COMPLETES before any preset or
// playlist callback runs.  Notification sites queue a snapshot-capturing
// closure via QueuePlaylistNotification(); every public transition method
// holds a PlaylistTransitionGuard, and when the OUTERMOST guard on this
// thread unwinds (mutex already released — the guard is declared before
// the lock), the queue is drained iteratively.  A Play() triggered by a
// drained notification runs inline at depth 1 and appends its own
// notifications to the same queue, so chains flatten into iteration
// instead of recursion; runaway chains hit MAX_NOTIFICATION_CASCADE and
// stop with a warning instead of a stack overflow.
//
// Re-entrant Play() (depth > 1, e.g. a "Start Playlist" COMMAND ENTRY
// executing inside its own StartPlaying() frame) is recorded in
// s_pendingStart* and replayed by the same drain.  This generalizes the
// old PlaylistEntryCommand/PlaylistEntryScript dynamic_cast special-case
// in Play() to every re-entrant caller.
// ──────────────────────────────────────────────────────────────────────────
static thread_local int tl_transitionDepth = 0;
static thread_local bool tl_drainingNotifications = false;
static thread_local std::vector<std::function<void()>> tl_pendingNotifications;
// One drain may legitimately fire a handful of chained events (playlist A's
// STOPPED preset starts B, etc.).  Each start/stop cycle queues ~4-5
// notifications, so 20 allows ~4 chained playlist switches — anything past
// that is a preset loop (PLAYLIST_STARTED → Start Playlist → ...).
static constexpr int MAX_NOTIFICATION_CASCADE = 20;

// Pending re-entrant start request.  File-scope (not per-Playlist members)
// because the active Playlist object can be swapped/queued for cleanup
// between the defer and the drain (inserted playlists, SetIdle-to-parent) —
// the request belongs to "the player", not to one instance.
static std::mutex s_pendingStartMutex;
static std::string s_pendingStartFilename;
static int s_pendingStartPosition = 0;
static int s_pendingStartRepeat = 0;
static int s_pendingStartScheduleEntry = 0;
static int s_pendingStartEndPosition = 0;

static void QueuePlaylistNotification(std::function<void()>&& fn) {
    if (tl_transitionDepth == 0) {
        // Not inside a transition (shouldn't happen — all queue sites are in
        // guarded methods) — fire directly rather than parking it on this
        // thread's queue where it would only drain on some later transition.
        fn();
        return;
    }
    tl_pendingNotifications.push_back(std::move(fn));
}

namespace {
class PlaylistTransitionGuard {
public:
    PlaylistTransitionGuard() { tl_transitionDepth++; }
    ~PlaylistTransitionGuard() {
        if (--tl_transitionDepth > 0 || tl_drainingNotifications) {
            // Inner frame, or a frame created by the drain itself — the
            // outermost/draining frame owns the queue.
            return;
        }
        tl_drainingNotifications = true;
        int fired = 0;
        while (true) {
            if (!tl_pendingNotifications.empty()) {
                if (++fired > MAX_NOTIFICATION_CASCADE) {
                    LogErr(VB_PLAYLIST, "Playlist notification cascade exceeded %d events — presets/plugins are starting playlists in a loop (e.g. PLAYLIST_STARTED -> Start Playlist -> PLAYLIST_STARTED). Dropping %d pending notifications.\n",
                           MAX_NOTIFICATION_CASCADE, (int)tl_pendingNotifications.size());
                    WarningHolder::AddWarningTimeout(300, 36, "Command preset loop detected: a playlist preset (e.g. PLAYLIST_STARTED) starts a playlist which re-triggers the preset. Fix the preset configuration.");
                    tl_pendingNotifications.clear();
                    std::lock_guard<std::mutex> l(s_pendingStartMutex);
                    s_pendingStartFilename.clear();
                    break;
                }
                std::function<void()> fn = std::move(tl_pendingNotifications.front());
                tl_pendingNotifications.erase(tl_pendingNotifications.begin());
                fn();
                continue;
            }
            // Replay a deferred re-entrant start, if one was recorded.
            std::string nm;
            int pos, rep, sched, endPos;
            {
                std::lock_guard<std::mutex> l(s_pendingStartMutex);
                if (s_pendingStartFilename.empty())
                    break;
                nm = s_pendingStartFilename;
                s_pendingStartFilename.clear();
                pos = s_pendingStartPosition;
                rep = s_pendingStartRepeat;
                sched = s_pendingStartScheduleEntry;
                endPos = s_pendingStartEndPosition;
            }
            if (playlist) {
                // Runs at depth 0 → executes inline; its notifications land
                // back on this queue and the loop continues.
                playlist->Play(nm, pos, rep, sched, endPos);
            }
        }
        tl_drainingNotifications = false;
    }
};
} // namespace
/*
 *
 */
Playlist::Playlist(Playlist* parent) :
    m_parent(parent),
    m_repeat(0),
    m_loop(0),
    m_loopCount(0),
    m_random(0),
    m_blankAtEnd(1),
    m_startTime(0),
    m_subPlaylistDepth(0),
    m_forceStop(0),
    m_stopAtPos(-1),
    m_loadStartPos(-1),
    m_loadEndPos(-1),
    m_fileTime(0),
    m_configTime(0),
    m_currentState("idle"),
    m_currentSection(nullptr),
    m_currentSectionStr("New"),
    m_sectionPosition(0),
    m_startPosition(0),
    m_globalPauseBetweenSequencesMS(0),
    m_isInGlobalPause(false),
    m_shouldStartGlobalPause(false),
    m_globalPauseStartTime(0),
    m_status(FPP_STATUS_IDLE) {
    SetIdle(false);

    SetRepeat(0);

    if (Events::HasEventHandlers()) {
        // Legacy callbacks
        std::function<void(const std::string& t, const std::string& payload)> f1 = [this](const std::string& t, const std::string& payload) {
            std::string emptyStr;
            LogDebug(VB_CONTROL, "Received deprecated MQTT Topic: '%s' \n", t.c_str());
            std::string topic = t;
            topic.replace(0, 10, emptyStr); // Replace until /#
            this->MQTTHandler(topic, payload);
        };
        Events::AddCallback("/playlist/name/set", f1);
        Events::AddCallback("/playlist/repeat/set", f1);
        Events::AddCallback("/playlist/sectionPosition/set", f1);

    } else {
        LogDebug(VB_CONTROL, "Not registered MQTT Callbacks for Playlist. MQTT Not configured. \n");
    }
}

/*
 *
 */
Playlist::~Playlist() {
    Cleanup();
}

PlaylistStatus Playlist::getPlaylistStatus() {
    return m_status;
}

/*
 *
 */
int Playlist::LoadJSONIntoPlaylist(std::vector<PlaylistEntryBase*>& playlistPart, const Json::Value& entries, int startPos, int& maxEntries) {
    PlaylistEntryBase* plEntry = NULL;

    for (int c = startPos; (c < entries.size()) && (maxEntries > 0); c++) {
        // Long-term handle sub-playlists on-demand instead of at load time
        if (entries[c]["type"].asString() == "playlist") {
            m_subPlaylistDepth++;
            if (m_subPlaylistDepth < 5) {
                std::string filename = FPP_DIR_PLAYLIST("/" + entries[c]["name"].asString() + ".json");

                Json::Value subPlaylist = LoadJSON(filename);
                int tmpMax = 999999;

                if (subPlaylist.isMember("leadIn"))
                    LoadJSONIntoPlaylist(playlistPart, subPlaylist["leadIn"], 0, tmpMax);

                if (subPlaylist.isMember("mainPlaylist"))
                    LoadJSONIntoPlaylist(playlistPart, subPlaylist["mainPlaylist"], 0, tmpMax);

                if (subPlaylist.isMember("leadOut"))
                    LoadJSONIntoPlaylist(playlistPart, subPlaylist["leadOut"], 0, tmpMax);
            } else {
                LogErr(VB_PLAYLIST, "Error, recursive playlist.  Sub-playlist depth exceeded 5 trying to include '%s'\n", entries[c]["name"].asString().c_str());
                WarningHolder::AddWarningTimeout(60, 24, "Recursive playlist: sub-playlist depth exceeded trying to include '" + entries[c]["name"].asString() + "'");
            }

            m_subPlaylistDepth--;
        } else {
            plEntry = LoadPlaylistEntry(entries[c]);
            if (plEntry) {
                playlistPart.push_back(plEntry);
                maxEntries--;
            }
        }
    }

    return 1;
}

/*
 *
 */
int Playlist::Load(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "Playlist::Load(JSON)\n");

    Cleanup();

    if (config.isMember("name"))
        m_name = config["name"].asString();

    if (config.isMember("desc"))
        m_desc = config["desc"].asString();

    m_repeat = config["repeat"].asInt();
    m_loopCount = config["loopCount"].asInt();
    m_subPlaylistDepth = 0;

    // Load global pause between sequences configuration
    if (config.isMember("globalPauseBetweenSequencesMS"))
        m_globalPauseBetweenSequencesMS = config["globalPauseBetweenSequencesMS"].asInt();
    else
        m_globalPauseBetweenSequencesMS = 0;

    m_playlistInfo = config["playlistInfo"];

    PlaylistEntryBase* plEntry = NULL;

    int startPos = 0;
    int maxEntries = 999999;
    int origEntryCount = 0;

    if (m_loadEndPos >= 0) {
        maxEntries = m_loadEndPos + 1 - ((m_loadStartPos >= 0) ? m_loadStartPos : 0);
    }

    if (config.isMember("leadIn") && config["leadIn"].size()) {
        LogDebug(VB_PLAYLIST, "Loading LeadIn:\n");
        const Json::Value leadIn = config["leadIn"];

        if (m_loadStartPos >= 0)
            startPos = m_loadStartPos;
        else
            startPos = 0;

        if (startPos < leadIn.size())
            LoadJSONIntoPlaylist(m_leadIn, leadIn, startPos, maxEntries);

        origEntryCount += leadIn.size();
    }

    if (config.isMember("mainPlaylist") && config["mainPlaylist"].size()) {
        LogDebug(VB_PLAYLIST, "Loading MainPlaylist:\n");
        const Json::Value playlist = config["mainPlaylist"];

        if ((m_loadStartPos >= 0) && (m_loadStartPos > origEntryCount)) {
            startPos = m_loadStartPos - origEntryCount;
        } else if (m_loadStartPos == -2 && m_loadEndPos == 0) {
            // random single item
            int l = playlist.size();
            if (l > 1) {
                startPos = FPPrand() % l;
                maxEntries = 1;
            }
        } else {
            startPos = 0;
        }

        if (startPos < playlist.size()) {
            LoadJSONIntoPlaylist(m_mainPlaylist, playlist, startPos, maxEntries);
        }

        origEntryCount += playlist.size();
    }

    if (config.isMember("leadOut") && config["leadOut"].size()) {
        LogDebug(VB_PLAYLIST, "Loading LeadOut:\n");
        const Json::Value leadOut = config["leadOut"];

        if ((m_loadStartPos >= 0) && (m_loadStartPos > origEntryCount))
            startPos = m_loadStartPos - origEntryCount;
        else
            startPos = 0;

        if (startPos < leadOut.size())
            LoadJSONIntoPlaylist(m_leadOut, leadOut, startPos, maxEntries);
    }

    // set the positions prior to any randomizations
    int curPos = 0;
    for (auto& a : m_leadIn) {
        a->SetPositionInPlaylist(curPos);
        ++curPos;
    }
    for (auto& a : m_mainPlaylist) {
        a->SetPositionInPlaylist(curPos);
        ++curPos;
    }
    for (auto& a : m_leadOut) {
        a->SetPositionInPlaylist(curPos);
        ++curPos;
    }

    if (config.isMember("random")) {
        if (config["random"].isNumeric()) {
            m_random = config["random"].asInt();
        } else if (config["random"].isBool() && config["random"].asBool()) {
            m_random = 1;
        } else if (config["random"].isString()) {
            m_random = std::atoi(config["random"].asString().c_str());
        }
    } else {
        m_random = 0;
    }

    m_sectionPosition = 0;
    m_currentSection = nullptr;

    if (WillLog(LOG_DEBUG, VB_PLAYLIST))
        Dump();

    return 1;
}

/*
 *
 */
Json::Value Playlist::LoadJSON(const std::string& filename) {
    LogDebug(VB_PLAYLIST, "Playlist::LoadJSON(%s)\n", filename.c_str());

    Json::Value root;

    if (filename.empty()) {
        LogErr(VB_PLAYLIST, "Playlist::LoadJSON() called with empty filename\n");
        return root;
    }

    if (!LoadJsonFromFile(filename, root, JsonRoot::Object)) {
        std::string warn = "Could not load playlist ";
        warn += filename;
        WarningHolder::AddWarningTimeout(30, 24, warn);
        LogErr(VB_PLAYLIST, "Error loading %s\n", filename.c_str());
        return root;
    }

    if (m_filename == filename) {
        struct stat attr;
        stat(filename.c_str(), &attr);

        char timeBuf[32];
        LogDebug(VB_PLAYLIST, "Playlist Last Modified: %s\n", ctime_r(&attr.st_mtime, timeBuf));

        m_fileTime = attr.st_mtime;
    }

    return root;
}

std::string sanitizeMediaName(std::string mediaName) {
    LogDebug(VB_PLAYLIST, "Searching for Media File (%s)\n", mediaName.c_str());
    // Same regex as PHP's sanitizeFilename
    RegExCache rec("([^\\w\\s\\d\\-_~,;\\[\\]\\(\\).])");
    // Check raw format for older Music uploads
    std::string tmpMedia = FPP_DIR_MUSIC("/" + mediaName);
    std::string tmpMedialClean = std::regex_replace(mediaName, *rec.regex, "");

    LogDebug(VB_PLAYLIST, "SanitizeMedia: Checking Raw Music (%s)\n", tmpMedia.c_str());
    if (FileExists(tmpMedia)) {
        return mediaName;
    }

    // Try Cleaned Music
    tmpMedia = FPP_DIR_MUSIC("/" + tmpMedialClean);
    LogDebug(VB_PLAYLIST, "SanitizeMedia: Checking Cleaned Music (%s)\n", tmpMedia.c_str());
    if (FileExists(tmpMedia)) {
        return tmpMedialClean;
    }

    // Try Older (orginal) Video upload
    tmpMedia = FPP_DIR_VIDEO("/" + mediaName);
    LogDebug(VB_PLAYLIST, "SanitizeMedia: Checking Raw Video (%s)\n", tmpMedia.c_str());
    if (FileExists(tmpMedia)) {
        return mediaName;
    }

    // Try cleaned video file
    tmpMedia = FPP_DIR_VIDEO("/" + tmpMedialClean);
    LogDebug(VB_PLAYLIST, "SanitizeMedia: Checking Clean Video (%s)\n", tmpMedia.c_str());
    if (FileExists(tmpMedia)) {
        return tmpMedialClean;
    }

    LogWarn(VB_PLAYLIST, "SanitizeMedia: Unable to find Media: (%s)\n", mediaName.c_str());
    tmpMedia = "";
    return tmpMedia;
}

/*
 *
 */
int Playlist::Load(const std::string& filename) {
    LogDebug(VB_PLAYLIST, "Playlist::Load(%s)\n", filename.c_str());

    if (filename.empty()) {
        LogErr(VB_PLAYLIST, "Playlist::Load() called with empty filename\n");
        return 0;
    }

    Events::Publish("playlist/name/status", filename);

    try {
        Json::Value root;
        std::string tmpFilename = filename;

        std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);

        if (endsWith(tmpFilename, ".fseq")) {
            m_filename = FPP_DIR_SEQUENCE("/" + tmpFilename);

            root["name"] = tmpFilename;
            root["repeat"] = 0;
            root["loopCount"] = 0;

            std::string mediaName;
            FSEQFile* src = FSEQFile::openFSEQFile(m_filename);
            if (src && !src->getVariableHeaders().empty()) {
                for (auto& head : src->getVariableHeaders()) {
                    if ((head.code[0] == 'm') && (head.code[1] == 'f')) {
                        if (strchr((char*)&head.getData()[0], '/')) {
                            mediaName = (char*)(strrchr((char*)&head.getData()[0], '/') + 1);
                        } else if (strchr((char*)&head.getData()[0], '\\')) {
                            mediaName = (char*)(strrchr((char*)&head.getData()[0], '\\') + 1);
                        } else {
                            mediaName = (const char*)&head.getData()[0];
                        }
                        std::string tmpMedia = sanitizeMediaName(mediaName);
                        if (tmpMedia == "") {
                            std::string warn = "fseq \"" + tmpFilename + "\" lists a media file of \"" + mediaName + "\" but it can not be found";

                            WarningHolder::AddWarningTimeout(60, 24, warn);
                            LogDebug(VB_PLAYLIST, "%s\n", warn.c_str());
                            mediaName = "";
                        }
                        // Set the Media to the correct name
                        mediaName = tmpMedia;
                    }
                }
            }

            if (src)
                delete src;

            Json::Value mp(Json::arrayValue);
            Json::Value pe;
            if (mediaName.empty()) {
                pe["type"] = "sequence";
                LogDebug(VB_PLAYLIST, "Generated an on-the-fly playlist for %s\n", tmpFilename.c_str());
            } else {
                pe["type"] = "both";
                pe["mediaName"] = mediaName;
                LogDebug(VB_PLAYLIST, "Generated an on-the-fly playlist for %s/%s\n", tmpFilename.c_str(), mediaName.c_str());
            }

            pe["enabled"] = 1;
            pe["playOnce"] = 0;
            pe["sequenceName"] = tmpFilename;
            pe["videoOut"] = "--Default--";

            mp.append(pe);
            root["mainPlaylist"] = mp;

        } else {
            if (IsExtensionAudio(GetFileExtension(tmpFilename)) || IsExtensionVideo(GetFileExtension(tmpFilename))) {
                if (IsExtensionAudio(GetFileExtension(tmpFilename)))
                    m_filename = FPP_DIR_MUSIC("/" + filename);
                else
                    m_filename = FPP_DIR_VIDEO("/" + filename);

                root["name"] = tmpFilename;
                root["repeat"] = 0;
                root["loopCount"] = 0;

                Json::Value mp(Json::arrayValue);
                Json::Value pe;

                pe["type"] = "media";
                pe["mediaName"] = tmpFilename;

                LogDebug(VB_PLAYLIST, "Generated an on-the-fly playlist for %s\n", tmpFilename.c_str());

                pe["enabled"] = 1;
                pe["playOnce"] = 0;
                pe["videoOut"] = "--Default--";
                mp.append(pe);
                root["mainPlaylist"] = mp;

            } else {
                m_filename = FPP_DIR_PLAYLIST("/" + filename + ".json");
                root = LoadJSON(m_filename);
            }
        }
        return Load(root);
    } catch (std::exception& er) {
        std::string warn = "Playlist " + GetPlaylistName() + " is invalid: " + er.what();
        LogWarn(VB_PLAYLIST, "%s\n", warn.c_str());
        WarningHolder::AddWarningTimeout(60, 24, warn);
        return 0;
    }
}

/*
 *
 */
PlaylistEntryBase* Playlist::LoadPlaylistEntry(Json::Value entry) {
    PlaylistEntryBase* result = NULL;

    if (entry["type"].asString() == "both")
        result = new PlaylistEntryBoth(this);
    else if (entry["type"].asString() == "branch")
        result = new PlaylistEntryBranch(this);
    else if (entry["type"].asString() == "dynamic")
        result = new PlaylistEntryDynamic(this);
    else if (entry["type"].asString() == "image")
        result = new PlaylistEntryImage(this);
    else if (entry["type"].asString() == "media")
        result = new PlaylistEntryMedia(this);
    else if (entry["type"].asString() == "pause")
        result = new PlaylistEntryPause(this);
    else if (entry["type"].asString() == "playlist")
        result = new PlaylistEntryPlaylist(this);
    else if (entry["type"].asString() == "remap")
        result = new PlaylistEntryRemap(this);
    else if (entry["type"].asString() == "script")
        result = new PlaylistEntryScript(this);
    else if (entry["type"].asString() == "sequence")
        result = new PlaylistEntrySequence(this);
    else if (entry["type"].asString() == "url")
        result = new PlaylistEntryURL(this);
    else if (entry["type"].asString() == "command")
        result = new PlaylistEntryCommand(this);
    else {
        LogErr(VB_PLAYLIST, "Unknown Playlist Entry Type: %s\n", entry["type"].asString().c_str());
        WarningHolder::AddWarningTimeout(60, 24, "Unknown playlist entry type: " + entry["type"].asString());
        return NULL;
    }

    if (result->Init(entry))
        return result;

    return NULL;
}

/*
 *
 */
int Playlist::ReloadPlaylist(void) {
    LogDebug(VB_PLAYLIST, "Playlist::ReloadPlaylist()\n");

    if (m_filename == "") {
        LogErr(VB_PLAYLIST, "Playlist::ReloadPlaylist() called but m_filename is empty\n");
        return 0;
    }

    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    std::string currentSectionStr = m_currentSectionStr;
    int repeat = m_repeat;
    int loopCount = m_loopCount;
    long long startTime = m_startTime;

    Json::Value root = LoadJSON(m_filename);

    if (!Load(root))
        return 0;

    if (m_random > 0) {
        RandomizeMainPlaylist();
    }

    m_repeat = repeat;
    m_loopCount = loopCount;
    m_startTime = startTime;
    m_sectionPosition = 0;
    m_currentSectionStr = "MainPlaylist";
    m_currentSection = &m_mainPlaylist;

    return 1;
}

/*
 *
 */
void Playlist::ReloadIfNeeded(void) {
    if (FileHasBeenModified()) {
        LogDebug(VB_PLAYLIST, "Playlist .json file has been modified, reloading playlist\n");

        if (!ReloadPlaylist())
            LogErr(VB_PLAYLIST, "Error reloading playlist, continuing with existing copy.");
    }
}

/*
 *
 */
void Playlist::SwitchToMainPlaylist(void) {
    LogDebug(VB_PLAYLIST, "Switching to MainPlaylist\n");

    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    UpdateCrashSnapshot();

    // A caller's "does this section have entries" test can be invalidated
    // before we get here: ReloadIfNeeded() runs Cleanup(), which pops every
    // entry into PL_ENTRY_CLEANUPS and then reloads from the file, so a
    // playlist edited mid-play can come back without this section.  The
    // vector is empty but keeps its capacity, so [0] hands back a stale
    // pointer to an entry that Process() is about to delete -- the vtable is
    // still mapped, the virtual call dispatches, and the fault lands inside
    // StartPlaying().  Check here, at the one place the section is indexed,
    // rather than at each call site.
    if (m_mainPlaylist.empty()) {
        LogDebug(VB_PLAYLIST, "MainPlaylist is empty, switching to idle.\n");
        SetIdle();
        return;
    }

    m_currentSectionStr = "MainPlaylist";
    m_currentSection = &m_mainPlaylist;
    m_sectionPosition = 0;
    StartPlayingWithGlobalPause(m_mainPlaylist[0]);
}

/*
 *
 */
void Playlist::SwitchToLeadOut(void) {
    LogDebug(VB_PLAYLIST, "Switching to LeadOut\n");

    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    UpdateCrashSnapshot();

    // See SwitchToMainPlaylist() -- a reload between the caller's size check
    // and here can leave this section empty.
    if (m_leadOut.empty()) {
        LogDebug(VB_PLAYLIST, "LeadOut is empty, switching to idle.\n");
        SetIdle();
        return;
    }

    m_currentSectionStr = "LeadOut";
    m_currentSection = &m_leadOut;
    m_sectionPosition = 0;
    StartPlayingWithGlobalPause(m_leadOut[0]);
}

/*
 *
 */
int Playlist::Start(void) {
    LogDebug(VB_PLAYLIST, "Playlist::Start()\n");

    PlaylistTransitionGuard guard;
    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);

    if ((!m_leadIn.size()) &&
        (!m_mainPlaylist.size()) &&
        (!m_leadOut.size())) {
        std::string warn = "Playlist " + GetPlaylistName() + " is empty. Nothing to play.";
        WarningHolder::AddWarningTimeout(30, 24, warn);

        SetIdle();
        return 0;
    }

    m_status = FPP_STATUS_PLAYLIST_PLAYING;

    std::string origCurState = m_currentState;
    m_currentState = "playing";

    m_startTime = GetTime();
    m_loop = 0;
    m_forceStop = 0;

    LogDebug(VB_PLAYLIST, "============================================================================\n");

    if (m_startPosition > 0) {
        if (m_startPosition >= (m_leadIn.size() + m_mainPlaylist.size() + m_leadOut.size()))
            m_startPosition = 0;

        if (m_startPosition >= (m_leadIn.size() + m_mainPlaylist.size())) {
            m_sectionPosition = m_startPosition - (m_leadIn.size() + m_mainPlaylist.size());
            m_currentSectionStr = "LeadOut";
            m_currentSection = &m_leadOut;
        } else if (m_startPosition >= m_leadIn.size()) {
            m_sectionPosition = m_startPosition - m_leadIn.size();
            m_currentSectionStr = "MainPlaylist";
            m_currentSection = &m_mainPlaylist;
        } else {
            m_sectionPosition = m_startPosition;
            m_currentSectionStr = "LeadIn";
            m_currentSection = &m_leadIn;
        }
    } else {
        if (m_leadIn.size()) {
            m_currentSectionStr = "LeadIn";
            m_currentSection = &m_leadIn;
        } else if (m_mainPlaylist.size()) {
            m_currentSectionStr = "MainPlaylist";
            m_currentSection = &m_mainPlaylist;
        } else { // must be only lead Out
            m_currentSectionStr = "LeadOut";
            m_currentSection = &m_leadOut;
        }

        m_sectionPosition = 0;
    }

    {
        // Deferred — plugin callbacks run arbitrary code inline; snapshot the
        // state now, deliver after the transition completes (see
        // PlaylistTransitionGuard).
        Json::Value info = GetInfo();
        std::string action = (origCurState == "playing") ? "playing" : "start";
        std::string sec = m_currentSectionStr;
        int pos = m_sectionPosition;
        QueuePlaylistNotification([info, action, sec, pos]() {
            PluginManager::INSTANCE.playlistCallback(info, action, sec, pos);
        });
    }
    Events::Publish("status", m_currentState);
    Events::Publish("playlist/section/status", m_currentSectionStr);
    Events::Publish("playlist/sectionPosition/status", m_sectionPosition);

    m_currentSection->at(m_sectionPosition)->StartPlaying();

    LogDebug(VB_PLAYLIST, "Exiting Playlist::Start()\n");
    return 1;
}

/*
 *
 */
int Playlist::StopNow(int forceStop) {
    if (m_status == FPP_STATUS_IDLE) {
        return 1;
    }

    PlaylistTransitionGuard guard;
    if (tl_transitionDepth > 1) {
        // Re-entrant stop — e.g. a "Stop Now" (or "Start Playlist At Item",
        // which pre-stops) command executing from inside a playlist entry's
        // own StartPlaying frame.  Running inline would tear the playlist
        // stack out from under the frames above us: with inserted (nested)
        // playlists, SetIdle() switches the global `playlist` to the parent
        // and condemns levels to PL_CLEANUPS while SwitchToInsertedPlaylist/
        // Start frames still reference them.  Queue it; the outermost
        // transition drains it (before any deferred start — queue order is
        // preserved), targeting whatever playlist is current at that point.
        LogDebug(VB_PLAYLIST, "Playlist::StopNow(%d) re-entrant (depth %d) — deferring until current transition completes\n",
                 forceStop, tl_transitionDepth);
        QueuePlaylistNotification([forceStop]() {
            if (playlist) {
                playlist->StopNowImpl(forceStop);
            }
        });
        return 1;
    }
    return StopNowImpl(forceStop);
}

// The body of StopNow().  Called from StopNow() (non-re-entrant), from the
// deferred-stop drain closure, and directly by internal orchestration
// (PlayImpl's stop-before-load, Process()'s invalid-section bail-out) that
// must stop inline as part of its own transition.
int Playlist::StopNowImpl(int forceStop) {
    LogDebug(VB_PLAYLIST, "Playlist::StopNow(%d)\n", forceStop);

    if (m_status == FPP_STATUS_IDLE) {
        return 1;
    }

    PlaylistTransitionGuard guard;

    // Stop background stream slots (2-5).  Slot 1 is stopped by the current
    // entry's Stop() below, which sends the MultiSync media stop packet
    // BEFORE the (slow) pipeline teardown — stopping slot 1 here first would
    // delay that packet by ~0.5s and make remotes stop late (issue #2676).
    StreamSlotManager::Instance().StopBackgroundSlots();

    std::map<std::string, std::string> keywords;
    keywords["PLAYLIST_NAME"] = m_name;
    if (CommandManager::INSTANCE.HasPreset("PLAYLIST_STOPPING_NOW")) {
        // Deferred — preset commands run inline and can re-enter the
        // playlist; fired after this stop completes (PlaylistTransitionGuard).
        QueuePlaylistNotification([keywords]() mutable {
            CommandManager::INSTANCE.TriggerPreset("PLAYLIST_STOPPING_NOW", keywords);
        });
    }

    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    m_status = FPP_STATUS_STOPPING_NOW;

    if (m_currentSection && m_sectionPosition < m_currentSection->size() && m_currentSection->at(m_sectionPosition)->IsPlaying())
        m_currentSection->at(m_sectionPosition)->Stop();

    m_forceStop = forceStop;
    SetIdle();

    return 1;
}

/*
 *
 */
int Playlist::StopGracefully(int forceStop, int afterCurrentLoop) {
    LogDebug(VB_PLAYLIST, "Playlist::StopGracefully(%d, %d)\n", forceStop, afterCurrentLoop);

    PlaylistTransitionGuard guard;
    if (m_status == FPP_STATUS_PLAYLIST_PAUSED && this == playlist) {
        Resume();
    }
    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);

    std::map<std::string, std::string> keywords;
    keywords["PLAYLIST_NAME"] = m_name;

    if (afterCurrentLoop) {
        if (CommandManager::INSTANCE.HasPreset("PLAYLIST_STOPPING_AFTER_LOOP")) {
            QueuePlaylistNotification([keywords]() mutable {
                CommandManager::INSTANCE.TriggerPreset("PLAYLIST_STOPPING_AFTER_LOOP", keywords);
            });
        }
        m_status = FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP;
        m_currentState = "stoppingAfterLoop";
    } else {
        if (CommandManager::INSTANCE.HasPreset("PLAYLIST_STOPPING_GRACEFULLY")) {
            QueuePlaylistNotification([keywords]() mutable {
                CommandManager::INSTANCE.TriggerPreset("PLAYLIST_STOPPING_GRACEFULLY", keywords);
            });
        }
        m_status = FPP_STATUS_STOPPING_GRACEFULLY;
        m_currentState = "stoppingGracefully";
    }
    m_forceStop = forceStop;
    if (m_parent) {
        m_parent->StopGracefully(forceStop, afterCurrentLoop);
    }

    return 1;
}

void Playlist::Pause() {
    LogDebug(VB_PLAYLIST, "Playlist::Pause called on %s\n", m_filename.c_str());
    PlaylistTransitionGuard guard;
    if (IsPlaying()) {
        std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
        if (m_currentSection && m_sectionPosition < m_currentSection->size() && m_currentSection->at(m_sectionPosition)->IsPlaying()) {
            m_currentSection->at(m_sectionPosition)->Pause();
            if (m_currentSection->at(m_sectionPosition)->IsPaused()) {
                m_status = FPP_STATUS_PLAYLIST_PAUSED;
            }
        }
    }
}
void Playlist::Resume() {
    LogDebug(VB_PLAYLIST, "Playlist::Resume called on %s\n", m_filename.c_str());
    PlaylistTransitionGuard guard;
    if (IsPlaying()) {
        std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
        if (m_status == FPP_STATUS_PLAYLIST_PAUSED) {
            m_currentSection->at(m_sectionPosition)->Resume();
            m_status = FPP_STATUS_PLAYLIST_PLAYING;

            // Notify of current playlists because was likely changed when Paused.
            Events::Publish("playlist/name/status", m_name);
            Events::Publish("status", m_currentState);
            Events::Publish("playlist/section/status", m_currentSectionStr);
            Events::Publish("playlist/sectionPosition/status", m_sectionPosition);
            {
                Json::Value info = GetInfo();
                std::string sec = m_currentSectionStr;
                int pos = m_sectionPosition;
                QueuePlaylistNotification([info, sec, pos]() {
                    PluginManager::INSTANCE.playlistCallback(info, "playing", sec, pos);
                });
            }
        }
    }
}

/*
 *
 */
int Playlist::IsPlaying(void) {
    if ((m_status == FPP_STATUS_PLAYLIST_PLAYING) ||
        (m_status == FPP_STATUS_STOPPING_GRACEFULLY) ||
        (m_status == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP) ||
        (m_status == FPP_STATUS_PLAYLIST_PAUSED)) // paused is technically still "running"
        return 1;

    return 0;
}

/*
 *
 */
int Playlist::FileHasBeenModified(void) {
    if ((endsWith(m_filename, ".fseq")) ||
        (IsExtensionAudio(GetFileExtension(m_filename))) ||
        (IsExtensionVideo(GetFileExtension(m_filename)))) {
        return 0;
    }

    struct stat attr;
    stat(m_filename.c_str(), &attr);

    char timeBuf[32];
    LogDebug(VB_PLAYLIST, "Playlist Last Modified: %s\n", ctime_r(&attr.st_mtime, timeBuf));

    if (attr.st_mtime > m_fileTime)
        return 1;

    return 0;
}

// Process() re-enters itself on this thread: after switching to an inserted
// playlist it tail-calls `return pl->Process()` while the outer frame still
// holds its own m_playlistMutex -- and that outer instance may already be on
// PL_CLEANUPS.  Draining only in the outermost frame is what makes the
// PL_CLEANUPS/PL_ENTRY_CLEANUPS promise ("freed after the stack has unwound")
// true; without it the nested drain deletes the object whose mutex an outer
// frame is about to unlock.
static thread_local int tl_processDepth = 0;
namespace {
class ProcessDepthGuard {
public:
    ProcessDepthGuard() { tl_processDepth++; }
    ~ProcessDepthGuard() { tl_processDepth--; }
};
} // namespace

/*
 *
 */
int Playlist::Process(void) {
    static time_t lastCheckTime = time(nullptr);
    time_t procTime = time(nullptr);

    PlaylistTransitionGuard guard;
    ProcessDepthGuard depthGuard;

    // Process any background stream slots (2-5) — fire-and-forget media on secondary streams
    StreamSlotManager::Instance().ProcessBackgroundSlots();

    // Pull sync-enabled slots toward the primary media's position so extra
    // streams (a second language on another audio output, a second display)
    // stay locked to the show rather than free-running.  Hooked here rather
    // than in ProcessMedia(), which the main loop only calls in REMOTE_MODE.
    if (mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING &&
        mediaOutputStatus.mediaSeconds > 0.0f) {
        StreamSlotManager::Instance().SyncSlotsToMaster(mediaOutputStatus.mediaSeconds);
    }

    // LogExcess(VB_PLAYLIST, "Playlist::Process: %s, section %s, position: %d\n", m_name.c_str(), m_currentSectionStr.c_str(), m_sectionPosition);

    if (tl_processDepth == 1) {
        if (!PL_CLEANUPS.empty()) {
            PL_CLEANUPS.sort();
            PL_CLEANUPS.unique();
            while (!PL_CLEANUPS.empty()) {
                Playlist* p = PL_CLEANUPS.front();
                delete p;
                PL_CLEANUPS.pop_front();
            }
        }
        while (!PL_ENTRY_CLEANUPS.empty()) {
            delete PL_ENTRY_CLEANUPS.front();
            PL_ENTRY_CLEANUPS.pop_front();
        }
    }
    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    if (m_currentSectionStr == "New") {
        return 0;
    }

    UpdateCrashSnapshot();

    if (m_currentSection == nullptr || m_sectionPosition >= m_currentSection->size()) {
        LogErr(VB_PLAYLIST, "Section position %d is outside of section %s\n",
               m_sectionPosition, m_currentSectionStr.c_str());
        StopNowImpl();
        return 0;
    }

    // Handle global pause between sequences
    if (m_isInGlobalPause) {
        long long currentTime = GetTimeMS();
        if ((currentTime - m_globalPauseStartTime) >= m_globalPauseBetweenSequencesMS) {
            LogDebug(VB_PLAYLIST, "Global pause between sequences completed, starting next item\n");
            m_isInGlobalPause = false;
            // Start the current item (which was delayed by the pause)
            if (m_currentSection && m_sectionPosition < m_currentSection->size()) {
                m_currentSection->at(m_sectionPosition)->StartPlaying();
            }
        } else {
            // Still in pause period, don't process anything
            return 1;
        }
    }

    if (!m_currentSection->at(m_sectionPosition)->IsPaused() && m_currentSection->at(m_sectionPosition)->IsPlaying()) {
        m_currentSection->at(m_sectionPosition)->Process();
    }

    Playlist* pl = nullptr;
    if (m_currentSection->at(m_sectionPosition)->IsPaused() && ((pl = SwitchToInsertedPlaylist()) != nullptr)) {
        std::unique_lock<std::recursive_mutex> lck(pl->m_playlistMutex);
        // SwitchToInsertedPlaylist() already started pl (via PlayImpl) --
        // starting it again would replay its first entry.
        return pl->Process();
    }

    if (m_currentSection->at(m_sectionPosition)->IsFinished()) {
        LogDebug(VB_PLAYLIST, "Playlist entry finished\n");
        if (WillLog(LOG_DEBUG, VB_PLAYLIST))
            m_currentSection->at(m_sectionPosition)->Dump();

        LogDebug(VB_PLAYLIST, "============================================================================\n");

        // Check if we should start a global pause before the next item
        if (m_globalPauseBetweenSequencesMS > 0) {
            bool isStoppingGracefully = (m_status == FPP_STATUS_STOPPING_GRACEFULLY || 
                                       m_status == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP);
            
            if (!isStoppingGracefully) {
                LogDebug(VB_PLAYLIST, "Marking for global pause between sequences for %d ms\n", m_globalPauseBetweenSequencesMS);
                m_shouldStartGlobalPause = true;
            }
        }

        // "query_next" stays SYNCHRONOUS by design: it is the decision-point
        // hook plugins use to insert the next item (InsertPlaylistAsNext)
        // before the SwitchToInsertedPlaylist checks just below — deferring
        // it would make the insertion miss this cycle.  Re-entrant Play()
        // from the callback is safe now (Play() defers it), which was the
        // only hazard of calling it inline.
        PluginManager::INSTANCE.playlistCallback(GetInfo(), "query_next", m_currentSectionStr, m_sectionPosition);

        if (m_status == FPP_STATUS_STOPPING_GRACEFULLY) {
            if ((m_currentSectionStr == "LeadIn") ||
                (m_currentSectionStr == "MainPlaylist")) {
                ReloadIfNeeded();

                if (m_leadOut.size()) {
                    LogDebug(VB_PLAYLIST, "Stopping Gracefully\n");
                    SwitchToLeadOut();
                } else {
                    SetIdle();
                }
                return 1;
            }
        }
        if (m_stopAtPos != -1 && m_stopAtPos <= (GetPosition() - 1)) {
            if ((pl = SwitchToInsertedPlaylist(true)) != nullptr) {
                return pl->Process();
            }
            LogDebug(VB_PLAYLIST, "Stopping after end position\n");
            SetIdle();
            return 1;
        }
        if ((pl = SwitchToInsertedPlaylist(WillStopAfterCurrent())) != nullptr) {
            return pl->Process();
        }

        auto currentEntry = m_currentSection->at(m_sectionPosition);
        if (currentEntry->GetNextBranchType() == PlaylistEntryBase::PlaylistBranchType::Index) {
            if (currentEntry->GetNextSection() != "") {
                LogDebug(VB_PLAYLIST, "Attempting Switch to %s section.\n",
                         currentEntry->GetNextSection().c_str());

                if (currentEntry->GetNextSection() == "leadIn") {
                    m_currentSectionStr = "LeadIn";
                    m_currentSection = &m_leadIn;
                } else if (currentEntry->GetNextSection() == "leadOut") {
                    m_currentSectionStr = "LeadOut";
                    m_currentSection = &m_leadOut;
                } else {
                    m_currentSectionStr = "MainPlaylist";
                    m_currentSection = &m_mainPlaylist;
                }

                if (currentEntry->GetNextItem() == -1) {
                    m_sectionPosition = 0;
                } else if (currentEntry->GetNextItem() < m_currentSection->size()) {
                    m_sectionPosition = currentEntry->GetNextItem();
                } else {
                    m_sectionPosition = 0;
                }
            } else if (currentEntry->GetNextItem() != -1) {
                if (currentEntry->GetNextItem() < m_currentSection->size()) {
                    m_sectionPosition = currentEntry->GetNextItem();
                } else {
                    m_sectionPosition = m_currentSection->size();
                }
            } else {
                m_sectionPosition++;
            }
        } else if (currentEntry->GetNextBranchType() == PlaylistEntryBase::PlaylistBranchType::Offset) {
            int nextPosition = m_sectionPosition + currentEntry->GetNextItem();
            LogDebug(VB_PLAYLIST, "Attempting Offset Branch to section position %d\n", nextPosition);
            if (nextPosition < 0) {
                LogDebug(VB_PLAYLIST, "New position negative, branching to first position in section\n");
                m_sectionPosition = 0;
            } else if (nextPosition > m_currentSection->size()) {
                m_sectionPosition = m_currentSection->size();
                LogDebug(VB_PLAYLIST, "Offset outside current section, ending section\n");
            } else {
                m_sectionPosition = nextPosition;
            }
        } else if (currentEntry->GetNextBranchType() == PlaylistEntryBase::PlaylistBranchType::Playlist) {
            // handled internally to PlaylistEntryBranch
            m_sectionPosition++;
        } else {
            m_sectionPosition++;
        }

        if (m_sectionPosition >= m_currentSection->size()) {
            if (m_currentSectionStr == "LeadIn") {
                LogDebug(VB_PLAYLIST, "At end of leadIn.\n");

                ReloadIfNeeded();

                if (m_mainPlaylist.size()) {
                    SwitchToMainPlaylist();
                } else if (m_leadOut.size()) {
                    SwitchToLeadOut();
                } else {
                    LogDebug(VB_PLAYLIST, "No more playlist entries, switching to idle.\n");
                    SetIdle();
                }
            } else if (m_currentSectionStr == "MainPlaylist") {
                m_loop++;
                LogDebug(VB_PLAYLIST, "mainPlaylist loop now: %d\n", m_loop);
                if ((m_repeat) && (!m_loopCount || (m_loop < m_loopCount))) {
                    ReloadIfNeeded();

                    if (m_status == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP) {
                        if (m_leadOut.size()) {
                            LogDebug(VB_PLAYLIST, "Stopping Gracefully after loop\n");
                            SwitchToLeadOut();
                        } else {
                            LogDebug(VB_PLAYLIST, "Stopping Gracefully after loop. Empty leadOut, setting to Idle state\n");
                            SetIdle();
                        }

                        return 1;
                    }

                    if (!m_loopCount)
                        LogDebug(VB_PLAYLIST, "mainPlaylist repeating for another loop, loopCount == 0\n");
                    else
                        LogDebug(VB_PLAYLIST, "mainPlaylist repeating for another loop, %d <= %d\n", m_loop, m_loopCount);

                    if (m_random == 2) {
                        RandomizeMainPlaylist();
                    }

                    // Goes through SwitchToMainPlaylist() for its empty-section
                    // guard; we are already in the MainPlaylist section, so the
                    // section/position it sets are the values this branch wants.
                    // ReloadIfNeeded() above may have emptied the section.
                    SwitchToMainPlaylist();
                } else if (m_leadOut.size()) {
                    ReloadIfNeeded();

                    SwitchToLeadOut();
                } else {
                    LogDebug(VB_PLAYLIST, "No more playlist entries, switching to idle.\n");
                    SetIdle();
                }
            } else {
                LogDebug(VB_PLAYLIST, "No more playlist entries, switching to idle.\n");
                SetIdle();
            }
        } else {
            // Start the next item in the current section
            StartPlayingWithGlobalPause(m_currentSection->at(m_sectionPosition));
        }

        // NOTE: deferred "Start Playlist" requests recorded by re-entrant
        // Play() calls are replayed by the outermost PlaylistTransitionGuard
        // when this Process() frame unwinds — the drain loop that used to
        // live here moved there so it also covers requests deferred during
        // idle transitions (Process() stops running once the player goes
        // idle, so a drain only here could strand them).

        {
            Json::Value info = GetInfo();
            std::string sec = m_currentSectionStr;
            int pos = m_sectionPosition;
            QueuePlaylistNotification([info, sec, pos]() {
                PluginManager::INSTANCE.playlistCallback(info, "playing", sec, pos);
            });
        }
        Events::Publish("playlist/section/status", m_currentSectionStr);
        Events::Publish("playlist/sectionPosition/status", m_sectionPosition);
    }

    return 1;
}

bool Playlist::WillStopAfterCurrent() {
    if ((m_sectionPosition + 1) >= m_currentSection->size()) {
        if (m_currentSectionStr == "LeadIn") {
            return !m_mainPlaylist.empty() || !m_leadOut.empty();
        } else if (m_currentSectionStr == "MainPlaylist") {
            if (m_repeat) {
                return false;
            }
            return !m_leadOut.empty();
        } else {
            return true;
        }
    }
    return false;
}

Playlist* Playlist::SwitchToInsertedPlaylist(bool isStopping) {
    if (m_insertedPlaylist != "") {
        // We are exiting, so there is no point wasting our memory on the stack
        // of playlists: the new playlist takes our place by inheriting our
        // parent, and we clean ourselves up once it is known to be running.
        bool replaceSelf = isStopping && m_parent;
        Playlist* pl = new Playlist(replaceSelf ? m_parent : this);
        std::string plname = m_insertedPlaylist;
        // PlayImpl, not Play: this runs inside the parent's transition
        // (depth > 1) and the child MUST start inline as part of the switch —
        // the public Play() would defer it.
        pl->PlayImpl(m_insertedPlaylist.c_str(), m_insertedPlaylistPosition, 0, m_scheduleEntry, m_insertedPlaylistEndPosition);
        m_insertedPlaylist = "";
        // An instance is condemned to PL_CLEANUPS only once it is no longer
        // the global `playlist` — the drain deletes unconditionally, so
        // condemning the instance the player is still running frees it out
        // from under the next Process() tick.  That is why the cleanup of
        // `this` waits until the replacement is known to be playing.
        if (pl->IsPlaying()) {
            LogDebug(VB_PLAYLIST, "Switching to inserted playlist '%s'\n", plname.c_str());
            playlist = pl;
            if (replaceSelf) {
                m_parent = nullptr;
                PL_CLEANUPS.push_back(this);
            }
            return playlist;
        }
        PL_CLEANUPS.push_back(pl);
        if (replaceSelf && playlist != this) {
            // pl failed to start and its own SetIdle() already handed the
            // global on to our parent, so nothing will process us again.
            m_parent = nullptr;
            PL_CLEANUPS.push_back(this);
        }
    }
    return nullptr;
}

/*
 *
 */
void Playlist::ProcessMedia(void) {
    std::unique_lock<std::mutex> lock(mediaOutputLock);
    if (mediaOutput) {
        mediaOutput->Process();
    }
}

/*
 *
 */
void Playlist::SetIdle(bool exit) {
    PlaylistTransitionGuard guard;
    m_status = FPP_STATUS_IDLE;
    m_currentState = "idle";

    std::map<std::string, std::string> keywords;
    if (m_name != "") {
        keywords["PLAYLIST_NAME"] = m_name;
    }
    Cleanup();
    // Both deferred (see PlaylistTransitionGuard): a PLAYLIST_STOPPED preset
    // that starts another playlist used to run here, mid-SetIdle — the rest
    // of this function then continued tearing down state the preset's Play()
    // had just replaced, and the trailing "idle" publishes lied about the
    // actual (playing) state.  The "stop" callback snapshot keeps the OLD
    // playlist's info, which is what the plugin is being told stopped.
    if (!keywords.empty()) {
        if (CommandManager::INSTANCE.HasPreset("PLAYLIST_STOPPED")) {
            QueuePlaylistNotification([keywords]() mutable {
                CommandManager::INSTANCE.TriggerPreset("PLAYLIST_STOPPED", keywords);
            });
        }
    }

    {
        Json::Value info = GetInfo();
        std::string sec = m_currentSectionStr;
        int pos = m_sectionPosition;
        QueuePlaylistNotification([info, sec, pos]() {
            PluginManager::INSTANCE.playlistCallback(info, "stop", sec, pos);
        });
    }

    bool publishIdle = true;
    // Walk up past parents that are already condemned to PL_CLEANUPS — handing
    // the global `playlist` pointer to one would resurrect an object the next
    // Process() tick deletes (crash), or double-play it.  A parent can be
    // condemned while we're exiting when a re-entrant stop tore down an
    // intermediate level of an inserted-playlist stack.
    Playlist* par = m_parent;
    while (par && std::find(PL_CLEANUPS.begin(), PL_CLEANUPS.end(), par) != PL_CLEANUPS.end()) {
        par = par->m_parent;
    }
    if (par && exit) {
        playlist = par;
        if (par->getPlaylistStatus() == FPP_STATUS_PLAYLIST_PAUSED) {
            par->Resume();
        }
        PL_CLEANUPS.push_back(this);

        if (par->getPlaylistStatus() != FPP_STATUS_IDLE)
            publishIdle = false;
    } else if (exit) {
        if (m_parent) {
            // Whole remaining chain was condemned — the global `playlist`
            // still points at us, so we must NOT be condemned too (the
            // PL_CLEANUPS drain deletes unconditionally).  We become the
            // resident idle playlist, like any top-level SetIdle; just drop
            // the dangling pointer into the condemned chain.
            m_parent = nullptr;
        }
        sequence->SendBlankingData();
    }

    if (publishIdle) {
        Events::Publish("status", "idle");
        Events::Publish("playlist/name/status", "");
        Events::Publish("playlist/section/status", "");
        Events::Publish("playlist/sectionPosition/status", 0);
        Events::Publish("playlist/repeat/status", 0);
    }
}

/*
 *
 */
int Playlist::Cleanup(void) {
    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);

    m_name = "";
    m_desc = "";
    m_currentSectionStr = "New";
    m_currentSection = nullptr;
    m_startPosition = 0;
    m_sectionPosition = 0;
    m_repeat = 0;
    m_loopCount = 0;
    m_startTime = 0;

    while (m_leadIn.size()) {
        PlaylistEntryBase* entry = m_leadIn.back();
        m_leadIn.pop_back();
        PL_ENTRY_CLEANUPS.push_back(entry);
    }

    while (m_mainPlaylist.size()) {
        PlaylistEntryBase* entry = m_mainPlaylist.back();
        m_mainPlaylist.pop_back();
        PL_ENTRY_CLEANUPS.push_back(entry);
    }

    while (m_leadOut.size()) {
        PlaylistEntryBase* entry = m_leadOut.back();
        m_leadOut.pop_back();
        PL_ENTRY_CLEANUPS.push_back(entry);
    }
    return 1;
}

void Playlist::InsertPlaylistAsNext(const std::string& filename, const int position, int endPosition) {
    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    PluginManager::INSTANCE.playlistInserted(filename, position, endPosition, false);
    if (m_status == FPP_STATUS_IDLE) {
        Play(filename, position, 0, m_scheduleEntry, endPosition);
    } else {
        m_insertedPlaylist = filename;
        m_insertedPlaylistPosition = position;
        m_insertedPlaylistEndPosition = endPosition;
    }
}
void Playlist::InsertPlaylistImmediate(const std::string& filename, const int position, int endPosition) {
    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    PluginManager::INSTANCE.playlistInserted(filename, position, endPosition, true);
    if (m_status == FPP_STATUS_IDLE) {
        Play(filename, position, 0, m_scheduleEntry, endPosition);
    } else {
        Pause();
        m_insertedPlaylist = filename;
        m_insertedPlaylistPosition = position;
        m_insertedPlaylistEndPosition = endPosition;
    }
}

int Playlist::Play(const std::string& filename, const int position, const int repeat, const int scheduleEntry, const int endPosition) {
    if (filename.empty())
        return 0;

    PlaylistTransitionGuard guard;
    if (tl_transitionDepth > 1) {
        // Re-entrant call: another transition on THIS thread is still on the
        // stack — e.g. a "Start Playlist" command entry executing inside its
        // own StartPlaying() frame, or a native plugin callback.  Executing
        // inline would reload/clobber playlist state that the outer frames
        // still reference (m_currentSection, positions), so record the
        // request; the outermost PlaylistTransitionGuard replays it after
        // the stack unwinds.  This replaces (and generalizes) the old
        // PlaylistEntryCommand/PlaylistEntryScript dynamic_cast check that
        // deferred via the startNewPlaylistFilename members.
        LogDebug(VB_PLAYLIST, "Playlist::Play('%s') re-entrant (depth %d) — deferring until current transition completes\n",
                 filename.c_str(), tl_transitionDepth);
        std::lock_guard<std::mutex> l(s_pendingStartMutex);
        s_pendingStartFilename = filename;
        s_pendingStartPosition = position;
        s_pendingStartRepeat = repeat;
        s_pendingStartScheduleEntry = scheduleEntry;
        s_pendingStartEndPosition = endPosition;
        return 1;
    }
    return PlayImpl(filename, position, repeat, scheduleEntry, endPosition);
}

// The body of Play().  Called only from Play() (guarded, non-re-entrant) and
// from SwitchToInsertedPlaylist(), which must start the inserted child
// playlist inline as part of the parent's own transition.
int Playlist::PlayImpl(const std::string& filename, const int position, const int repeat, const int scheduleEntry, const int endPosition) {
    LogDebug(VB_PLAYLIST, "Playlist::Play('%s', %d, %d, %d, %d)\n",
             filename.c_str(), position, repeat, scheduleEntry, endPosition);

    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);

    if ((m_status == FPP_STATUS_PLAYLIST_PLAYING) ||
        (m_status == FPP_STATUS_STOPPING_GRACEFULLY) ||
        (m_status == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP)) {
        std::string fullfilename = FPP_DIR_PLAYLIST("/" + filename + ".json");

        if ((m_filename == fullfilename) && (repeat == m_repeat) && m_currentSection && position >= 0) {
            // the requested playlist is already running and loaded, we can jump right to the index
            if (m_currentSection->at(m_sectionPosition)->IsPlaying()) {
                m_currentSection->at(m_sectionPosition)->Stop();
            }

            m_sectionPosition = 0;
            SetPosition(position);
            m_status = FPP_STATUS_PLAYLIST_PLAYING;
            Start();
            return 1;
        } else if (m_currentSection) {
            // Re-entrant callers (command/script entries, plugin callbacks)
            // never reach here — Play() deferred them — so it is safe to
            // stop the current playlist and load the new one directly.
            StopNowImpl(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (playlist != this) {
                // This was an inserted (nested) playlist: StopNow's SetIdle
                // unwound ONE level — the global `playlist` now points at
                // our parent (resumed), and `this` is already queued on
                // PL_CLEANUPS.  Loading the new playlist into `this` would
                // start it on a condemned object the player never processes
                // and the next Process() tick deletes (the historical
                // "started a playlist from inside nested playlists and it
                // blipped and vanished while the outer playlist resumed"
                // bug).  Delegate to the now-active level instead; this
                // repeats per level until the stack is flat and the root
                // object (which IS the global) performs the load.
                return playlist->PlayImpl(filename, position, repeat, scheduleEntry, endPosition);
            }
        }
    }
    m_scheduleEntry = scheduleEntry;
    m_forceStop = 0;

    int tmpStartPos = position;
    int tmpEndPos = endPosition;

    if ((tmpEndPos >= 0) && (tmpEndPos < tmpStartPos)) {
        LogWarn(VB_PLAYLIST, "Playlist::Play() called with end position less than start position: Play('%s', %d, %d, %d, %d)\n",
                filename.c_str(), position, repeat, scheduleEntry, endPosition);
        tmpEndPos = -1;
    }

    m_loadStartPos = tmpStartPos;
    m_loadEndPos = tmpEndPos;

    Load(filename);

    if (tmpStartPos >= 0) {
        // Load() would have trimmed our internal copy of the playlist, so adjust our values
        if (tmpEndPos >= 0)
            tmpEndPos -= tmpStartPos;

        tmpStartPos = 0;
    }

    if ((tmpStartPos == 0 || tmpStartPos == -1) && (m_random > 0)) {
        RandomizeMainPlaylist();
    }

    int p = tmpStartPos;
    if (p == -2) {
        // random
        int l = m_mainPlaylist.size();
        if (l > 1) {
            p = FPPrand() % l;
            p = p + m_leadIn.size();
        }
    }
    if (p >= 0)
        SetPosition(p);

    if (repeat >= 0)
        SetRepeat(repeat);

    m_stopAtPos = tmpEndPos;

    m_status = FPP_STATUS_PLAYLIST_PLAYING;
    int result = Start();

    if (result == 1) {
        std::map<std::string, std::string> keywords;
        keywords["PLAYLIST_NAME"] = m_name;
        if (CommandManager::INSTANCE.HasPreset("PLAYLIST_STARTED")) {
            // Deferred: preset commands run arbitrary code inline (including
            // "Start Playlist", which used to recurse into Play() under the
            // lock).  Fires after this transition completes and the mutex is
            // released — see PlaylistTransitionGuard.
            QueuePlaylistNotification([keywords]() mutable {
                CommandManager::INSTANCE.TriggerPreset("PLAYLIST_STARTED", keywords);
            });
        }
    }

    return result;
}

/*
 *
 */
void Playlist::SetPosition(int position) {
    m_startPosition = position;

    Events::Publish("playlist/position/status", position);
}

/*
 *
 */
void Playlist::SetRepeat(int repeat) {
    m_repeat = repeat;

    Events::Publish("playlist/repeat/status", repeat);
}

void Playlist::RandomizeMainPlaylist() {
    if (m_random == 0) {
        return;
    }

    if (m_mainPlaylist.empty()) {
        return;
    }

    std::vector<PlaylistEntryBase*> tmpPlaylist = m_mainPlaylist;
    m_mainPlaylist.clear();
    int origSize = tmpPlaylist.size();

    while (tmpPlaylist.size()) {
        int l = tmpPlaylist.size();
        if (l > 1) {
            int p = FPPrand() % l;

            // If this is the first item found and it is the last
            // item in the previous list then try again unless our playlist is only 2 items long
            if ((!m_mainPlaylist.size()) && (p == (origSize - 1)) && origSize > 2)
                continue;

            m_mainPlaylist.push_back(tmpPlaylist[p]);
            tmpPlaylist.erase(tmpPlaylist.begin() + p);
        } else {
            m_mainPlaylist.push_back(tmpPlaylist.back());
            tmpPlaylist.pop_back();
        }
    }
}

/*
 *
 */
void Playlist::Dump(void) {
    LogDebug(VB_PLAYLIST, "Playlist: %s\n", m_name.c_str());

    LogDebug(VB_PLAYLIST, "  Description      : %s\n", m_desc.c_str());
    LogDebug(VB_PLAYLIST, "  Repeat           : %d\n", m_repeat);
    LogDebug(VB_PLAYLIST, "  Loop Count       : %d\n", m_loopCount);
    LogDebug(VB_PLAYLIST, "  Current Section  : %s\n", m_currentSectionStr.c_str());
    LogDebug(VB_PLAYLIST, "  Section Position : %d\n", m_sectionPosition);

    if (m_leadIn.size()) {
        LogDebug(VB_PLAYLIST, "  Lead In:\n");
        for (int c = 0; c < m_leadIn.size(); c++)
            m_leadIn[c]->Dump();
    } else {
        LogDebug(VB_PLAYLIST, "  Lead In          : (No Lead In)\n");
    }

    if (m_mainPlaylist.size()) {
        LogDebug(VB_PLAYLIST, "  Main Playlist:\n");
        for (int c = 0; c < m_mainPlaylist.size(); c++)
            m_mainPlaylist[c]->Dump();
    } else {
        LogDebug(VB_PLAYLIST, "  Main Playlist    : (No Main Playlist)\n");
    }

    if (m_leadOut.size()) {
        LogDebug(VB_PLAYLIST, "  Lead Out:\n");
        for (int c = 0; c < m_leadOut.size(); c++)
            m_leadOut[c]->Dump();
    } else {
        LogDebug(VB_PLAYLIST, "  Lead Out         : (No Lead Out)\n");
    }
}

void Playlist::RestartItem(void) {
    LogDebug(VB_PLAYLIST, "RestartItem called for '%s'\n", m_name.c_str());
    PlaylistTransitionGuard guard;
    if (m_currentState == "idle") {
        return;
    }
    if (m_status != FPP_STATUS_PLAYLIST_PLAYING) {
        return;
    }

    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    
    // Clear global pause state when manually navigating
    m_shouldStartGlobalPause = false;
    m_isInGlobalPause = false;
    
    int pos = GetPosition() - 1;
    if (m_currentSection->at(m_sectionPosition)->IsPlaying())
        m_currentSection->at(m_sectionPosition)->Stop();

    m_sectionPosition = 0;
    m_startPosition = pos;
    Start();
}
void Playlist::NextItem(void) {
    LogDebug(VB_PLAYLIST, "NextItem called for '%s'\n", m_name.c_str());
    PlaylistTransitionGuard guard;
    if (m_currentState == "idle") {
        return;
    }
    if (m_status != FPP_STATUS_PLAYLIST_PLAYING) {
        return;
    }

    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    
    // Clear global pause state when manually navigating
    m_shouldStartGlobalPause = false;
    m_isInGlobalPause = false;
    bool somewhereToGo = true;
    int pos = GetPosition();
    if (m_currentSectionStr == "LeadIn") {
        if ((pos < m_leadIn.size()) ||
            (m_mainPlaylist.size()) ||
            (m_leadOut.size())) {
            // as long as we have somewhere to go then go to next
            pos++;
        } else {
            // nowhere to go, stop playlist
            somewhereToGo = false;
        }
    } else if (m_currentSectionStr == "MainPlaylist") {
        if (pos < (m_leadIn.size() + m_mainPlaylist.size())) {
            // if not at end of main go to next item
            pos++;
        } else if (m_repeat) {
            // if at end of main and repeating, go to first item in main
            pos = m_leadIn.size() + 1;
        } else if (m_leadOut.size()) {
            // if at end of main and non-repeating, go to first in LeadOut
            pos++;
        } else {
            // nowhere to go, stop playlist
            somewhereToGo = false;
        }
    } else if (m_currentSectionStr == "LeadOut") {
        if (pos < (m_leadIn.size() + m_mainPlaylist.size() + m_leadOut.size())) {
            // more in leadOut so go to next
            pos++;
        } else {
            // nowhere to go, stop playlist
            somewhereToGo = false;
        }
    } else {
        return;
    }

    if (m_currentSection && m_sectionPosition < m_currentSection->size() && m_currentSection->at(m_sectionPosition)->IsPlaying())
        m_currentSection->at(m_sectionPosition)->Stop();

    if (somewhereToGo) {
        pos--;
        m_sectionPosition = 0;
        m_startPosition = pos;
        Start();
    }
}

/*
 *
 */
void Playlist::PrevItem(void) {
    LogDebug(VB_PLAYLIST, "PrevItem called for '%s'\n", m_name.c_str());
    PlaylistTransitionGuard guard;
    if (m_currentState == "idle") {
        return;
    }
    if (m_status != FPP_STATUS_PLAYLIST_PLAYING) {
        return;
    }

    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    
    // Clear global pause state when manually navigating
    m_shouldStartGlobalPause = false;
    m_isInGlobalPause = false;
    
    int pos = GetPosition();
    if ((m_currentSectionStr == "LeadIn") ||
        (m_currentSectionStr == "LeadOut")) {
        // No repeat on LeadIn/Out so just go to previous item
        if (pos > 1)
            pos--;
    } else if (m_currentSectionStr == "MainPlaylist") {
        if (pos > (m_leadIn.size() + 1)) {
            // If not first item in Main, go to prev item
            pos--;
        } else if (m_repeat) {
            // if first item in main and repeating, go to end of main
            pos = m_leadIn.size() + m_mainPlaylist.size();
        } else if (m_leadIn.size()) {
            // if first item in main and non-repeating go to end of LeadIn
            pos = m_leadIn.size();
        }
    }

    if (m_currentSection->at(m_sectionPosition)->IsPlaying())
        m_currentSection->at(m_sectionPosition)->Stop();

    m_sectionPosition = 0;
    m_startPosition = pos - 1;
    Start();
}

/*
 *
 */
int Playlist::GetPosition(void) {
    int result = 0;

    if (m_currentState == "idle")
        return result;

    if (m_currentSectionStr == "LeadIn")
        return m_sectionPosition + 1;

    if (m_currentSectionStr == "MainPlaylist")
        return m_leadIn.size() + m_sectionPosition + 1;

    if (m_currentSectionStr == "LeadOut")
        return m_leadIn.size() + m_mainPlaylist.size() + m_sectionPosition + 1;

    return result;
}

/*
 *
 */
int Playlist::GetSize(void) {
    if (m_currentState == "idle")
        return 0;

    return m_leadIn.size() + m_mainPlaylist.size() + m_leadOut.size();
}

/*
 *
 */
Json::Value Playlist::GetCurrentEntry(void) {
    Json::Value result;

    if (m_currentState == "idle" || m_currentSection == nullptr)
        return result;

    if (m_sectionPosition < m_currentSection->size())
        result = m_currentSection->at(m_sectionPosition)->GetConfig();

    return result;
}
static void GetFilenames(PlaylistEntryBase* entry, std::string& seq, std::string& med) {
    PlaylistEntrySequence* se = dynamic_cast<PlaylistEntrySequence*>(entry);
    PlaylistEntryMedia* me = dynamic_cast<PlaylistEntryMedia*>(entry);
    PlaylistEntryBoth* be = dynamic_cast<PlaylistEntryBoth*>(entry);
    if (se) {
        seq = se->GetSequenceName();
    }
    if (me) {
        med = me->GetMediaName();
    }
    if (be) {
        seq = be->GetSequenceName();
        med = be->GetMediaName();
    }
}

void Playlist::GetFilenamesForPos(int pos, std::string& seq, std::string& med) {
    for (auto& a : m_leadIn) {
        if (a->GetPositionInPlaylist() == pos) {
            GetFilenames(a, seq, med);
            return;
        }
    }
    for (auto& a : m_mainPlaylist) {
        if (a->GetPositionInPlaylist() == pos) {
            GetFilenames(a, seq, med);
            return;
        }
    }
    for (auto& a : m_leadOut) {
        if (a->GetPositionInPlaylist() == pos) {
            GetFilenames(a, seq, med);
            return;
        }
    }
}

int Playlist::FindPosForMS(uint64_t& t, bool itemDefinedOnly) {
    if (itemDefinedOnly) {
        PlaylistEntryBase* bestOption = nullptr;
        uint64_t diff = 0xFFFFFFFFFFL;
        for (auto& a : m_leadIn) {
            if (a->GetTimeCode() < t) {
                uint64_t d2 = t - a->GetTimeCode();
                if (d2 < diff) {
                    diff = d2;
                    bestOption = a;
                }
            }
        }
        for (auto& a : m_mainPlaylist) {
            if (a->GetTimeCode() < t) {
                uint64_t d2 = t - a->GetTimeCode();
                if (d2 < diff) {
                    diff = d2;
                    bestOption = a;
                }
            }
        }
        for (auto& a : m_leadOut) {
            if (a->GetTimeCode() < t) {
                uint64_t d2 = t - a->GetTimeCode();
                if (d2 < diff) {
                    diff = d2;
                    bestOption = a;
                }
            }
        }
        if (bestOption) {
            t -= bestOption->GetTimeCode();
            return bestOption->GetPositionInPlaylist();
        }
    } else {
        for (auto& a : m_leadIn) {
            uint64_t i = a->GetLengthInMS();
            if (t < i) {
                return a->GetPositionInPlaylist();
            }
            t -= i;
        }
        for (auto& a : m_mainPlaylist) {
            uint64_t i = a->GetLengthInMS();
            if (t < i) {
                return a->GetPositionInPlaylist();
            }
            t -= i;
        }
        for (auto& a : m_leadOut) {
            uint64_t i = a->GetLengthInMS();
            if (t < i) {
                return a->GetPositionInPlaylist();
            }
            t -= i;
        }
    }
    t = 0;
    return -1;
}

uint64_t Playlist::GetCurrentPosInMS() {
    int pos = 0;
    uint64_t posms;
    return GetCurrentPosInMS(pos, posms, false);
}
uint64_t Playlist::GetCurrentPosInMS(int& position, uint64_t& posms, bool itemDefinedOnly) {
    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);

    position = -1;
    posms = 0;
    if (m_currentState == "idle" || m_currentSection == nullptr) {
        return 0;
    }
    position = m_currentSection->at(m_sectionPosition)->GetPositionInPlaylist();
    posms = m_currentSection->at(m_sectionPosition)->GetElapsedMS();
    if (itemDefinedOnly) {
        int pos = m_currentSection->at(m_sectionPosition)->GetTimeCode();
        if (pos >= 0) {
            return pos + posms;
        } else {
            posms = 0;
            return 0;
        }
    }

    uint64_t pos = 0;
    for (int x = 0; x < m_sectionPosition; x++) {
        pos += m_currentSection->at(x)->GetLengthInMS();
    }
    pos += posms;
    // if we aren't in the LeadIn, add the time of the LeadIn
    if (m_currentSectionStr != "LeadIn") {
        for (auto& a : m_leadIn) {
            pos += a->GetLengthInMS();
        }
    } else {
        return pos;
    }
    if (m_currentSectionStr != "MainPlaylist") {
        // must be in the leadOut, add the main list length
        for (auto& a : m_mainPlaylist) {
            pos += a->GetLengthInMS();
        }
    }
    return pos;
}
uint64_t Playlist::GetPosStartInMS(int pos) {
    uint64_t ms = 0;

    for (auto& a : m_leadIn) {
        if (a->GetPositionInPlaylist() == pos) {
            return ms;
        }
        ms += a->GetLengthInMS();
    }
    for (auto& a : m_mainPlaylist) {
        if (a->GetPositionInPlaylist() == pos) {
            return ms;
        }
        ms += a->GetLengthInMS();
    }
    for (auto& a : m_leadOut) {
        if (a->GetPositionInPlaylist() == pos) {
            return ms;
        }
        ms += a->GetLengthInMS();
    }
    return ms;
}

Json::Value Playlist::GetMqttStatusJSON(void) {
    // this is called on background thread, need to lock
    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);

    Json::Value result;
    result["status"] = m_currentState; // Works because single playlist
    Json::Value playlistArray = Json::Value(Json::arrayValue);

    if (m_currentState != "idle" && m_currentSection != nullptr && m_sectionPosition < m_currentSection->size()) {
        Json::Value entryArray = Json::Value(Json::arrayValue);
        Json::Value playlist;
        // Only one entry right now.
        Json::Value playlistEntry = m_currentSection->at(m_sectionPosition)->GetMqttStatus();
        entryArray.append(playlistEntry);

        playlist["name"] = m_name;
        playlist["repeat"] = m_repeat;
        playlist["description"] = m_desc;
        playlist["currentItems"] = entryArray;
        playlistArray.append(playlist);
    }

    result["activePlaylists"] = playlistArray;
    return result;
}

void Playlist::GetCurrentStatus(Json::Value& result) {
    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    if (m_currentState == "idle" || m_currentSection == nullptr) {
        result["repeat_mode"] = "0";
        result["current_playlist"]["description"] = "";
        result["current_playlist"]["playlist"] = "";
        result["current_playlist"]["count"] = "0";
        result["current_playlist"]["index"] = "0";
        result["current_playlist"]["type"] = "";

        result["current_sequence"] = "";
        result["current_song"] = "";
        result["seconds_played"] = "0";
        result["seconds_remaining"] = "0";
        result["time_elapsed"] = "00:00";
        result["time_remaining"] = "00:00";
        
        // Still provide global pause and random info even when idle
        result["global_pause"]["configured"] = m_globalPauseBetweenSequencesMS > 0;
        result["global_pause"]["duration_ms"] = m_globalPauseBetweenSequencesMS;
        result["global_pause"]["active"] = false; // Always false when idle
        result["random"] = m_random;
        return;
    }

    std::string plname = m_name;
    result["repeat_mode"] = m_repeat;
    result["current_playlist"]["description"] = m_desc;
    result["current_playlist"]["count"] = std::to_string(GetSize());
    result["current_playlist"]["index"] = std::to_string(GetPosition());

    result["random"] = m_random;

    // Global pause between sequences status
    result["global_pause"]["configured"] = m_globalPauseBetweenSequencesMS > 0;
    result["global_pause"]["duration_ms"] = m_globalPauseBetweenSequencesMS;
    result["global_pause"]["active"] = m_isInGlobalPause;
    if (m_isInGlobalPause && m_globalPauseBetweenSequencesMS > 0) {
        long long elapsed = GetTimeMS() - m_globalPauseStartTime;
        long long remaining = m_globalPauseBetweenSequencesMS - elapsed;
        if (remaining < 0) remaining = 0;
        result["global_pause"]["elapsed_ms"] = static_cast<Json::Int64>(elapsed);
        result["global_pause"]["remaining_ms"] = static_cast<Json::Int64>(remaining);
        result["global_pause"]["remaining_seconds"] = static_cast<Json::Int64>((remaining + 999) / 1000); // Round up
    }

    // m_sectionPosition is allowed to sit at exactly m_currentSection->size()
    // while the playlist is between items -- Process() sets it to size() when
    // advancing past the end of a section, and checks for that state rather
    // than treating it as invalid.  Using at() unguarded here therefore throws
    // std::out_of_range out of the /api/fppd/status handler on a drogon I/O
    // thread, which aborts fppd.  Every other at() on this vector is guarded
    // the same way; treat "no current entry" as the null case the code below
    // already handles.
    PlaylistEntryBase* ple = nullptr;
    std::string type;
    if (m_sectionPosition < m_currentSection->size()) {
        ple = m_currentSection->at(m_sectionPosition);
        type = ple->GetType();
    }

    while (type == "dynamic") {
        PlaylistEntryDynamic* dyn = dynamic_cast<PlaylistEntryDynamic*>(ple);
        ple = dyn->GetCurrentEntry();
        if (ple) {
            type = ple->GetType();
        } else {
            type = "unknown";
        }
    }

    plname = plname.substr(plname.find_last_of("\\/") + 1);
    if (endsWith(plname, ".json")) {
        plname = plname.substr(0, plname.find_last_of("."));
    }
    result["current_playlist"]["playlist"] = plname;
    result["current_playlist"]["type"] = type;

    if (ple) {
        // Check if we're currently in a global pause - if so, show pause progress instead of item progress
        if (m_isInGlobalPause && m_globalPauseBetweenSequencesMS > 0) {
            // Show global pause progress
            long long elapsed = GetTimeMS() - m_globalPauseStartTime;
            long long remaining = m_globalPauseBetweenSequencesMS - elapsed;
            if (remaining < 0) remaining = 0;
            
            int secsElapsed = (int)(elapsed / 1000);
            int secsRemaining = (int)(remaining / 1000);
            
            result["current_sequence"] = "Global Pause";
            result["current_song"] = "";
            result["seconds_played"] = std::to_string(secsElapsed);
            result["seconds_elapsed"] = std::to_string(secsElapsed);
            result["milliseconds_elapsed"] = static_cast<Json::Int64>(elapsed);
            result["seconds_remaining"] = std::to_string(secsRemaining);
            result["time_elapsed"] = secondsToTime(secsElapsed);
            result["time_remaining"] = secondsToTime(secsRemaining);
        } else {
            // Normal item progress
            int msecs = ple->GetElapsedMS();
            int secsElapsed = (int)(msecs / 1000);
            int secsRemaining = (int)((ple->GetLengthInMS() - ple->GetElapsedMS()) / 1000);
            
            std::string currentSeq;
            std::string currentSong;
            if (type == "media") {
                PlaylistEntryMedia* med = dynamic_cast<PlaylistEntryMedia*>(ple);
                currentSong = med->GetMediaName();
            } else if (type == "both") {
                PlaylistEntryBoth* both = dynamic_cast<PlaylistEntryBoth*>(ple);
                currentSeq = both->GetSequenceName();
                currentSong = both->GetMediaName();
            } else if (type == "sequence") {
                PlaylistEntrySequence* seq = dynamic_cast<PlaylistEntrySequence*>(ple);
                currentSeq = seq->GetSequenceName();
                secsElapsed = sequence->m_seqMSElapsed / 1000;
                secsRemaining = sequence->m_seqMSRemaining / 1000;
            } else if (type == "script") {
                PlaylistEntryScript* scr = dynamic_cast<PlaylistEntryScript*>(ple);
                currentSeq = scr->GetScriptName();
            }
            result["current_sequence"] = currentSeq;
            result["current_song"] = currentSong;
            result["seconds_played"] = std::to_string(secsElapsed);
            result["seconds_elapsed"] = std::to_string(secsElapsed);
            result["milliseconds_elapsed"] = msecs;
            result["seconds_remaining"] = std::to_string(secsRemaining);
            result["time_elapsed"] = secondsToTime(secsElapsed);
            result["time_remaining"] = secondsToTime(secsRemaining);
        }
    } else {
        // No current entry (between items, or a dynamic entry with nothing
        // resolved).  Emit the same placeholders the idle branch above uses so
        // the status JSON keeps a stable shape for API consumers.
        result["current_sequence"] = "";
        result["current_song"] = "";
        result["seconds_played"] = "0";
        result["seconds_remaining"] = "0";
        result["time_elapsed"] = "00:00";
        result["time_remaining"] = "00:00";
    }

    std::list<std::string> parents;
    GetParentPlaylistNames(parents);
    if (!parents.empty()) {
        for (auto& n : parents) {
            result["breadcrumbs"].append(n);
        }
    }
}
void Playlist::GetParentPlaylistNames(std::list<std::string>& names) {
    if (m_parent) {
        m_parent->GetParentPlaylistNames(names);
        names.push_back(m_parent->GetPlaylistName());
    }
}

/*
 *
 */
Json::Value Playlist::GetInfo(void) {
    Json::Value result;
    GetInfo(result);
    return result;
}

void Playlist::GetInfo(Json::Value& result) {
    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    result["currentState"] = m_currentState;
    if (m_currentState == "idle") {
        result["name"] = "";
        result["desc"] = "";
        result["repeat"] = 0;
        result["loop"] = 0;
        result["loopCount"] = 0;
        result["random"] = 0;
        result["blankAtEnd"] = 0;
        result["size"] = 0;
    } else {
        result["name"] = m_name;
        result["desc"] = m_desc;
        result["repeat"] = m_repeat;
        result["loop"] = m_loop;
        result["loopCount"] = m_loopCount;
        result["random"] = m_random;
        result["blankAtEnd"] = m_blankAtEnd;
        result["size"] = GetSize();
    }
    result["currentEntry"] = GetCurrentEntry();
}

/*
 *
 */
std::string Playlist::GetConfigStr(void) {
    return SaveJsonToString(GetConfig());
}

/*
 *
 */
Json::Value Playlist::GetConfig(void) {
    Json::Value result;
    std::unique_lock<std::recursive_mutex> lck(m_playlistMutex);
    GetInfo(result);

    if (m_leadIn.size()) {
        result["leadIn"] = Json::Value(Json::arrayValue);
        Json::Value& jsonArray = result["leadIn"];
        for (int c = 0; c < m_leadIn.size(); c++) {
            jsonArray.append(m_leadIn[c]->GetConfig());
        }
    }

    if (m_mainPlaylist.size()) {
        result["mainPlaylist"] = Json::Value(Json::arrayValue);
        Json::Value& jsonArray = result["mainPlaylist"];
        for (int c = 0; c < m_mainPlaylist.size(); c++) {
            jsonArray.append(m_mainPlaylist[c]->GetConfig());
        }
    }

    if (m_leadOut.size()) {
        result["leadOut"] = Json::Value(Json::arrayValue);
        Json::Value& jsonArray = result["leadOut"];
        for (int c = 0; c < m_leadOut.size(); c++) {
            jsonArray.append(m_leadOut[c]->GetConfig());
        }
    }

    m_configTime = time(NULL);
    result["configTime"] = (Json::UInt64)m_configTime;

    result["random"] = m_random;
    result["globalPauseBetweenSequencesMS"] = m_globalPauseBetweenSequencesMS;

    result["playlistInfo"] = m_playlistInfo;
    m_config = result;

    return result;
}

/*
 *
 */
int Playlist::MQTTHandler(std::string topic, std::string msg) {
    LogDebug(VB_PLAYLIST, "Playlist::MQTTHandler('%s', '%s') while playing '%s'\n",
             topic.c_str(), msg.c_str(), m_name.c_str());

    // note the leading /set/playlist will be removed from topic by now

    int pos = topic.find("/");
    if (pos == std::string::npos) {
        LogWarn(VB_PLAYLIST, "Ignoring Invalid playlist topic: playlist/%s\n",
                topic.c_str());
        return 0;
    }
    std::string newPlaylistName = topic.substr(0, pos);
    std::string topicEnd = topic.substr(pos);

    /*
     * NOTE: This because multiple playlist are not supported, the newPlaylistname value
     * is only considered when starting a playlist.  All other actions will
     * apply to the current running playlist even if the names don't match
     */

    // ALLPLAYLIST should be checked first to avoid name colision.
    if (topic == "ALLPLAYLISTS/stop/now") {
        StopNow(1);

    } else if (topic == "ALLPLAYLISTS/stop/graceful") {
        StopGracefully(1);

    } else if (topic == "ALLPLAYLISTS/stop/afterloop") {
        StopGracefully(1, 1);

    } else if (topicEnd == "/next") {
        NextItem();

    } else if (topicEnd == "/prev") {
        PrevItem();

    } else if (topicEnd == "/repeat") {
        SetRepeat(atoi(msg.c_str()));

    } else if (topicEnd == "/startPosition") {
        SetPosition(atoi(msg.c_str()));

    } else if (topicEnd == "/stop/now") {
        StopNow(1);

    } else if (topicEnd == "/stop/graceful") {
        StopGracefully(1);

    } else if (topicEnd == "/stop/afterloop") {
        StopGracefully(1, 1);

        // These three are depgrecated and should be removed
    } else if (topic == "name/set") {
        LogInfo(VB_CONTROL, "playlist/%s is deprecated and will be removed in a future release\n",
                topic.c_str());
        Play(msg, m_sectionPosition, m_repeat);

    } else if (topic == "repeat/set") {
        LogInfo(VB_PLAYLIST, "playlist/%s is deprecated and will be removed in a future release\n",
                topic.c_str());
        SetRepeat(atoi(msg.c_str()));

    } else if (topic == "sectionPosition/set") {
        LogInfo(VB_PLAYLIST, "playlist/%s is deprecated and will be removed in a future release\n",
                topic.c_str());
        SetPosition(atoi(msg.c_str()));

    } else {
        LogWarn(VB_PLAYLIST, "Ignoring Invalid playlist topic: playlist/%s\n", topic.c_str());
        return 0;
    }

    return 1;
}

/*
 *
 */
std::string Playlist::ReplaceMatches(std::string in) {
    std::string out = in;

    LogDebug(VB_PLAYLIST, "In: '%s'\n", in.c_str());

    // FIXME, Playlist

    LogDebug(VB_PLAYLIST, "Out: '%s'\n", out.c_str());

    return out;
}

/*
 * Helper method to start playing an entry, handling global pause between sequences
 */
void Playlist::StartPlayingWithGlobalPause(PlaylistEntryBase* entry) {
    if (m_shouldStartGlobalPause && m_globalPauseBetweenSequencesMS > 0) {
        LogDebug(VB_PLAYLIST, "Starting global pause between sequences for %d ms before next item\n", m_globalPauseBetweenSequencesMS);
        m_shouldStartGlobalPause = false;
        m_isInGlobalPause = true;
        m_globalPauseStartTime = GetTimeMS();
        // Don't start the entry yet, it will be started when the pause completes
    } else {
        // No global pause needed, start immediately
        entry->StartPlaying();
    }
}
