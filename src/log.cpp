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
#include <cstring>

#include <atomic>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <sys/types.h>
#include <chrono>
#include <mutex>
#include <sstream>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include "fppversion.h"
#include "log.h"

// int logLevel = LOG_INFO;
// int logMask  = VB_MOST;

// Create Global Instance
FPPLogger FPPLogger::INSTANCE = FPPLogger();

char logFileName[1024] = "";
bool logToStdOut = true;

// ---------------------------------------------------------------------------
// Crash-time log ring
//
// The most useful line for diagnosing a crash is routinely one nobody kept: the
// trigger is logged at LogDebug, production runs at info, so it is filtered out
// before it is ever formatted.  This ring formats and retains those lines in
// memory without writing them anywhere, and the crash handler dumps it into the
// report -- debug-level detail at info-level cost on disk.
//
// Constraints that shaped it:
//   * No locking.  The normal log path takes logFileLock, and a crash handler
//     that blocks on a mutex the faulting thread already holds hangs the
//     process -- exactly the case (heap corruption inside free()) that most
//     needs the trace.  A slot may be torn between a writer and the dump; a
//     smeared line in a crash report is a fine price for never deadlocking.
//   * No allocation, for the same reason: a fixed array of fixed slots, so the
//     dump is a bounded series of write() calls and is async-signal-safe.
//   * LOG_EXCESSIVE is excluded.  It fires per frame in the output path, and
//     formatting it unconditionally would be a real cost on a single-core BBB;
//     LOG_DEBUG and coarser covers the state changes worth reconstructing.
// ---------------------------------------------------------------------------
static constexpr size_t kCrashRingSlots = 256;
static constexpr size_t kCrashRingSlotSize = 240;
static char s_crashRing[kCrashRingSlots][kCrashRingSlotSize];
static std::atomic<uint64_t> s_crashRingSeq{ 0 };

// Set once the crash handler starts, so its own output -- the gdb stack, the
// "Crash handler called" line -- stops evicting the pre-crash history that is
// the entire reason the ring exists.  That output is already captured in the
// stack file; duplicating it here would only cost us the lines we want.
static std::atomic<bool> s_crashRingFrozen{ false };

void CrashLogRingFreeze() {
    s_crashRingFrozen.store(true, std::memory_order_relaxed);
}

// Zero-init statics, so this is safe from any point in startup with no
// dependency on static initialization order.
static void CrashLogRingPush(const char* line, size_t len) {
    if (s_crashRingFrozen.load(std::memory_order_relaxed)) {
        return;
    }
    uint64_t seq = s_crashRingSeq.fetch_add(1, std::memory_order_relaxed);
    char* slot = s_crashRing[seq % kCrashRingSlots];
    if (len >= kCrashRingSlotSize) {
        len = kCrashRingSlotSize - 1;
    }
    memcpy(slot, line, len);
    slot[len] = '\0';
}

// Two independent filters, both needed:
//   * facility opt-out excludes the per-frame data facilities outright (see
//     FPPLoggerInstance::crashRingCapture -- HexDump emits at LogInfo, so a
//     level test alone would not have caught them);
//   * LOG_EXCESSIVE is dropped everywhere, since it is the per-frame level by
//     convention in every facility that has one.
bool CrashLogRingWillCapture(int level, FPPLoggerInstance& facility) {
    return facility.crashRingCapture && level <= LOG_DEBUG;
}

// Async-signal-safe: only write(), no allocation, no locks.
void CrashLogRingDump(int fd) {
    uint64_t seq = s_crashRingSeq.load(std::memory_order_relaxed);
    uint64_t count = seq < kCrashRingSlots ? seq : kCrashRingSlots;
    static const char hdr[] = "=== recent log lines (ring buffer, oldest first) ===\n";
    (void)!write(fd, hdr, sizeof(hdr) - 1);
    if (count == 0) {
        static const char none[] = "(empty)\n";
        (void)!write(fd, none, sizeof(none) - 1);
        return;
    }
    for (uint64_t i = seq - count; i < seq; i++) {
        const char* slot = s_crashRing[i % kCrashRingSlots];
        size_t len = strnlen(slot, kCrashRingSlotSize);
        if (len) {
            (void)!write(fd, slot, len);
        }
    }
}

// True when stdout is an interactive terminal (a person ran `fppd -f` in a
// shell) rather than a pipe to journald (how systemd runs `fppd -f`). Used to
// decide whether the stdout copy of a line already written to the log file is
// a useful on-screen echo or a pure journald duplicate. Cached: fppd's stdout
// is not reassigned after startup, and isatty() is a syscall per log line.
static bool stdoutIsInteractive() {
    static const bool interactive = isatty(fileno(stdout)) != 0;
    return interactive;
}

// Program name for the syslog-style program field. fppd.log is a merged,
// sequential record -- fppd itself, the fppd_start/stop/restart scripts, and
// operation breadcrumbs all append to it -- so every line has to say who wrote
// it. The [facility] field cannot carry this: it is a subsystem *within* fppd
// (General/ChannelOut/Playlist/...), not a program.
static const char* logProgramName() {
#ifdef PLATFORM_OSX
    return getprogname();
#else
    return program_invocation_short_name;
#endif
}
// char logLevelStr[16];
// char logMaskStr[1024];

// Persistent handle for the log file so each log line is a single write()
// instead of a full fopen/write/fclose cycle.  On slow SD cards the per-line
// open/close metadata traffic was enough that a card-internal stall could
// block the (real-time) channel output thread mid-log and drop frames.
//
// logrotate rotates fppd.log by rename (no copytruncate and no signal to
// fppd), so we periodically stat() the path and reopen when the inode under
// our name changes or the file disappears.
//
// A recursive_timed_mutex guards the handle: recursive because the crash
// handler calls LogErr and may re-enter on the same thread; timed so that if
// another thread is wedged inside a write (e.g. stalled storage) a crashing
// thread gives up and falls back to stderr instead of hanging the process.
// Owner the log file should end up with, resolved once. Looked up rather than
// hardcoded to a uid, and returns false where there is no such user (macOS runs
// as the developer's own account, not fpp) so the chown is simply skipped there.
static bool logFileOwner(uid_t& uid, gid_t& gid) {
    static bool resolved = false;
    static bool haveOwner = false;
    static uid_t cachedUid = 0;
    static gid_t cachedGid = 0;
    if (!resolved) {
        resolved = true;
        struct passwd* pw = getpwnam("fpp");
        if (pw) {
            cachedUid = pw->pw_uid;
            cachedGid = pw->pw_gid;
            haveOwner = true;
        }
    }
    uid = cachedUid;
    gid = cachedGid;
    return haveOwner;
}

static std::recursive_timed_mutex logFileLock;
static FILE* persistentLogFile = nullptr;
static ino_t persistentLogIno = 0;
static dev_t persistentLogDev = 0;
static time_t lastRotationCheck = 0;

// Must be called with logFileLock held.
static void closePersistentLogFile() {
    if (persistentLogFile) {
        fclose(persistentLogFile);
        persistentLogFile = nullptr;
        persistentLogIno = 0;
        persistentLogDev = 0;
        lastRotationCheck = 0;
    }
}

// Must be called with logFileLock held.  Returns the persistent handle for
// logFileName, (re)opening it as needed, or nullptr if it cannot be opened.
static FILE* getPersistentLogFile(time_t now) {
    if (persistentLogFile && now != lastRotationCheck) {
        // At most once a second, check whether the file we have open is still
        // the one at logFileName (logrotate renames it, a user may delete it).
        lastRotationCheck = now;
        struct stat st;
        if ((stat(logFileName, &st) != 0) ||
            (st.st_ino != persistentLogIno) || (st.st_dev != persistentLogDev)) {
            closePersistentLogFile();
        }
    }
    if (!persistentLogFile) {
        bool existed = (access(logFileName, F_OK) == 0);
        persistentLogFile = fopen(logFileName, "a");
        if (persistentLogFile) {
            // Line buffered: each log line still hits the kernel immediately
            // (matching the old open/write/close behavior) but as one write().
            setvbuf(persistentLogFile, nullptr, _IOLBF, 8192);
            if (!existed) {
                // We just created the log, and fppd runs as root (fppd.service
                // sets no User=), so it would be root:root 0644 in an fpp-owned
                // directory -- locking out every writer that is not root. That
                // matters because fppd is no longer the only one: the
                // fppd_start/stop/restart scripts append here, and so do the PHP
                // breadcrumbs (backup, mp3gain) which run as Apache/fpp. Their
                // writes are best-effort and error-suppressed, so they would fail
                // SILENTLY.
                //
                // logrotate's `create 0664 fpp fpp` normally gets here first, but
                // it cannot cover every case: resetConfig.php deletes logs/*, and
                // a fresh install writes here before FPPINIT's setFileOwnership
                // runs. Whoever creates the file has to set the ownership, so do
                // it here too. Best-effort: as non-root the chown simply fails,
                // which is the case where we are already the right owner.
                uid_t uid;
                gid_t gid;
                if (logFileOwner(uid, gid) && fchown(fileno(persistentLogFile), uid, gid) == 0) {
                    fchmod(fileno(persistentLogFile), 0664);
                }
            }
            struct stat st;
            if (fstat(fileno(persistentLogFile), &st) == 0) {
                persistentLogIno = st.st_ino;
                persistentLogDev = st.st_dev;
            }
            lastRotationCheck = now;
        }
    }
    return persistentLogFile;
}

void FPPLogger::Init() {
    if (all.empty()) {
        all.push_back(&(FPPLogger::General));
        all.push_back(&(FPPLogger::ChannelOut));
        all.push_back(&(FPPLogger::ChannelData));
        all.push_back(&(FPPLogger::Command));
        all.push_back(&(FPPLogger::E131Bridge));
        all.push_back(&(FPPLogger::Effect));
        all.push_back(&(FPPLogger::MediaOut));
        all.push_back(&(FPPLogger::Playlist));
        all.push_back(&(FPPLogger::Schedule));
        all.push_back(&(FPPLogger::Sequence));
        all.push_back(&(FPPLogger::Settings));
        all.push_back(&(FPPLogger::Control));
        all.push_back(&(FPPLogger::Sync));
        all.push_back(&(FPPLogger::Plugin));
        all.push_back(&(FPPLogger::GPIO));
        all.push_back(&(FPPLogger::HTTP));
    }
}

bool FPPLogger::SetLevel(const char* name, const char* level) {
    LogLevel newLevel = LOG_ERR;
    bool found = false;
    if (level == nullptr || name == nullptr) {
        LogErr(VB_SETTING, "Log level (%s) and name (%s) must be specified\n", level, name);
        return false;
    }

    if (strcmp(level, "error") == 0) {
        newLevel = LOG_ERR;
        found = true;
    } else if (strcmp(level, "warn") == 0) {
        newLevel = LOG_WARN;
        found = true;
    } else if (strcmp(level, "info") == 0) {
        newLevel = LOG_INFO;
        found = true;
    } else if (strcmp(level, "debug") == 0) {
        newLevel = LOG_DEBUG;
        found = true;
    } else if (strcmp(level, "excess") == 0) {
        newLevel = LOG_EXCESSIVE;
        found = true;
    }

    if (!found) {
        LogErr(VB_SETTING, "Invalid name for LogLevel: %s\n", level);
        return false;
    } else {
        return SetLevel(std::string(name), newLevel);
    }
}

bool FPPLogger::SetLevel(std::string name, LogLevel level) {
    bool madeChange = false;
    std::string prefix("LogLevel_");
    if (name.compare(0, prefix.size(), prefix) == 0) {
        name = name.substr(prefix.size());
    }

    std::vector<FPPLoggerInstance*>::iterator it;
    Init(); /* Just incase */

    for (it = all.begin(); it != all.end(); it++) {
        FPPLoggerInstance* ptr = *it;
        if (name == ptr->name) {
            ptr->level = level;
            madeChange = true;
        }
    }
    if (!madeChange) {
        LogWarn(VB_SETTING, "Invalid name for LoggerInstance: %s\n", name.c_str());
    }

    return madeChange;
}

int FPPLogger::MinimumLogLevel() {
    int rc = LOG_ERR;
    Init(); /* Just incase */
    std::vector<FPPLoggerInstance*>::iterator it;

    for (it = all.begin(); it != all.end(); it++) {
        int level = (*it)->level;
        if (level > rc) {
            rc = level;
        }
    }

    return rc;
}

std::string FPPLogger::GetLogLevelString() {
    std::stringstream rc;
    LogLevel currentLevel = LOG_ERR;
    bool firstLevel = true;
    bool firstLogger = true;
    std::vector<FPPLoggerInstance*>::iterator it;

    for (it = all.begin(); it != all.end(); it++) {
        if ((currentLevel != (*it)->level) || firstLevel) {
            if (!firstLevel) {
                rc << ";";
            }
            firstLevel = false;
            firstLogger = true;
            rc << LogLevelToString((*it)->level) << ":";
            currentLevel = (*it)->level;
        }
        if (firstLogger) {
            firstLogger = false;
        } else {
            rc << ",";
        }
        rc << (*it)->name;
    }
    return rc.str();
}

void FPPLogger::SetAllLevel(LogLevel level) {
    std::vector<FPPLoggerInstance*>::iterator it;
    Init(); /* Just incase */

    for (it = all.begin(); it != all.end(); it++) {
        (*it)->level = level;
    }
}

std::string TruncateForLog(const std::string& s, size_t maxLen) {
    // Escape embedded newlines first (on the ORIGINAL string, not just the
    // truncated slice) - formatLogLines() below re-prefixes every physical
    // line it's given (deliberately, for attribution in this merged log
    // file), so an unescaped \n in otherwise-unbounded data (a Variable's
    // value, raw command output, etc.) turns one logical log entry into
    // several prefixed-but-uncorrelated-looking lines. A short multi-line
    // value is just as confusing as a long one, so this isn't gated behind
    // the length check below.
    std::string escaped;
    escaped.reserve(s.size());
    for (char c : s) {
        if (c == '\n') {
            escaped += "\\n";
        } else if (c == '\r') {
            escaped += "\\r";
        } else {
            escaped += c;
        }
    }
    if (escaped.size() <= maxLen) {
        return escaped;
    }
    return escaped.substr(0, maxLen) + "...(" + std::to_string(s.size()) + " bytes total)";
}

bool WillLog(int level, FPPLoggerInstance& facility) {
    // Don't log if we're not concerned about anything at this level
    if (facility.level < level)
        return false;

    return true;
}

void _LogWrite(const char* file, int line, int level, FPPLoggerInstance& facility, const std::string& str, ...) {
    // Same gate as the char* overload -- a ring-only line still has to reach it.
    if (!WillLog(level, facility) && !CrashLogRingWillCapture(level, facility))
        return;
    _LogWrite(file, line, level, facility, str.c_str());
}

// Append msg to out, applying prefix to EVERY physical line.
//
// The prefix used to be snprintf'd together with the caller's format string and
// handed to vfprintf, which applied it once per LogX() call rather than per
// line: any message containing \n produced several physical lines with only the
// first one identifying itself. That was survivable while fppd owned this file
// outright, but fppd.log is now a merged, sequential record -- fppd, the
// fppd_start/stop/restart scripts and the operation breadcrumbs all append to it
// -- and an unlabelled line there is an unattributable line.
//
// A message that does not end in \n gets one; previously it left the line open
// and the next log call concatenated onto it.
static void formatLogLines(std::string& out, const char* prefix, const char* msg) {
    if (!*msg) {
        out.append(prefix);
        out.append("\n");
        return;
    }
    const char* p = msg;
    while (*p) {
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        out.append(prefix);
        out.append(p, len);
        out.append("\n");
        if (!nl) {
            break;
        }
        p = nl + 1;
    }
}

void _LogWrite(const char* file, int line, int level, FPPLoggerInstance& facility, const char* format, ...) {
    // A line is formatted if it is either being logged normally OR retained in
    // the crash ring -- the ring is the whole point of formatting a line the
    // configured level would discard.
    const bool willLog = WillLog(level, facility);
    const bool willRing = CrashLogRingWillCapture(level, facility);
    if (!willLog && !willRing)
        return;

    va_list arg;

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    int ms = tv.tv_usec / 1000;

    uint64_t tid;
#ifdef PLATFORM_OSX
    pthread_threadid_np(NULL, &tid);
#else
    tid = gettid();
#endif

    // The "YYYY-MM-DD HH:MM:SS" field changes once per second, but building it
    // cost a localtime_r plus seven width-qualified integer conversions on
    // EVERY line -- together the largest single component of a log call.  That
    // is invisible on a desktop and not on the small ARM boards: measured on an
    // AM335x, localtime_r ~5us and the full prefix snprintf ~12us, against
    // ~1.7us to render the caller's actual message.
    //
    // So cache it, keyed on the second it describes.  thread_local rather than
    // shared: this is on every log call from every thread, and a lock or atomic
    // here would cost more than it saves, while a torn shared buffer would
    // corrupt timestamps.  Per-thread cost is one time_t and 20 bytes.
    //
    // A timezone change (DST) is picked up on the next second boundary, since
    // the key is the second itself.
    thread_local time_t cachedSec = (time_t)-1;
    thread_local char cachedDateTime[20]; // "YYYY-MM-DD HH:MM:SS"
    if (tv.tv_sec != cachedSec) {
        struct tm tm;
        localtime_r(&tv.tv_sec, &tm);
        snprintf(cachedDateTime, sizeof(cachedDateTime),
                 "%4d-%.2d-%.2d %.2d:%.2d:%.2d",
                 1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec);
        cachedSec = tv.tv_sec;
    }

    // The remaining numeric fields are emitted directly rather than through
    // snprintf; integer conversion is the expensive part of a format string,
    // and milliseconds are always exactly three digits here.
    char prefix[512];
    char* p = prefix;
    memcpy(p, cachedDateTime, 19);
    p += 19;
    *p++ = '.';
    *p++ = (char)('0' + (ms / 100) % 10);
    *p++ = (char)('0' + (ms / 10) % 10);
    *p++ = (char)('0' + ms % 10);
    *p++ = ' ';
    snprintf(p, sizeof(prefix) - (p - prefix), "%s(%llu) [%s] %s:%d: ",
             logProgramName(), tid, facility.name.c_str(), file, line);

    // Render the caller's message first so it can be split on newlines. The
    // stack buffer covers essentially every real log line; the heap path exists
    // so a long one (a dumped config blob) is not silently truncated.
    char stackBuf[1024];
    va_start(arg, format);
    int msgLen = vsnprintf(stackBuf, sizeof(stackBuf), format, arg);
    va_end(arg);

    const char* msg = stackBuf;
    std::string heapBuf;
    if (msgLen >= (int)sizeof(stackBuf)) {
        heapBuf.resize(msgLen + 1);
        va_start(arg, format);
        vsnprintf(&heapBuf[0], msgLen + 1, format, arg);
        va_end(arg);
        msg = heapBuf.c_str();
    }

    std::string out;
    out.reserve((msgLen > 0 ? (size_t)msgLen : 0) + 128);
    formatLogLines(out, prefix, msg);

    if (willRing) {
        CrashLogRingPush(out.data(), out.size());
    }
    if (!willLog) {
        // Retained for the crash report only; the configured level says this
        // line does not belong in the log file or on stdout.
        return;
    }

    // One fwrite per log call: it keeps the whole (possibly multi-line) message
    // contiguous, which matters now that shell writers append to this same file.
    bool wroteLogFile = false;
    if (logFileName[0]) {
        if (!strcmp(logFileName, "stderr")) {
            fwrite(out.data(), 1, out.size(), stderr);
        } else if (!strcmp(logFileName, "stdout")) {
            fwrite(out.data(), 1, out.size(), stdout);
        } else {
            std::unique_lock<std::recursive_timed_mutex> lock(logFileLock, std::defer_lock);
            if (lock.try_lock_for(std::chrono::seconds(2))) {
                FILE* logFile = getPersistentLogFile(tv.tv_sec);
                if (logFile) {
                    fwrite(out.data(), 1, out.size(), logFile);
                    wroteLogFile = true;
                } else {
                    fprintf(stderr, "Error: Unable to open log file for writing!\n");
                    fwrite(out.data(), 1, out.size(), stderr);
                    return;
                }
            } else {
                // Could not get the log file lock (holder likely wedged on
                // stalled storage); don't hang here - emit to stderr instead.
                fwrite(out.data(), 1, out.size(), stderr);
                return;
            }
        }
    }
    // Once the line is in a real log file, a second copy on stdout is a pure
    // duplicate: fppd runs as `fppd -f` under systemd, so logToStdOut stays set
    // and journald captured every line a second time -- doubling the volume for
    // a copy that does not outlive the boot, while fppd.log is what the Support
    // Zip ships and what users are asked for. Suppress it at the source, here.
    //
    // Do NOT instead mute the stream in fppd.service: fppd's stdout is inherited
    // by every child it forks (cape detection, plugin callbacks), so muting it
    // there discards their output too. That was tried, and reverted.
    //
    // `fppd -l stdout` still logs to stdout, and nothing suppresses stdout when
    // no real log file is in use (early startup, before SetLogFile).
    //
    // The duplicate-suppression only applies when stdout is a pipe to journald.
    // When a person runs `fppd -f` in a terminal, stdout is a TTY and the copy
    // is a genuine on-screen echo of the log file, not a journald duplicate, so
    // keep echoing every line -- that is the interactive foreground behavior.
    if (strcmp(logFileName, "stdout") && logToStdOut &&
        (!wroteLogFile || stdoutIsInteractive())) {
        fwrite(out.data(), 1, out.size(), stdout);
    }
}

void SetLogFile(const char* filename, bool toStdOut) {
    std::unique_lock<std::recursive_timed_mutex> lock(logFileLock);
    closePersistentLogFile();
    strcpy(logFileName, filename);
    logToStdOut = toStdOut;
}

std::string LogLevelToString(LogLevel level) {
    if (level == LOG_WARN) {
        return "warn";
    } else if (level == LOG_DEBUG) {
        return "debug";
    } else if (level == LOG_INFO) {
        return "info";
    } else if (level == LOG_EXCESSIVE) {
        return "excess";
    } else if (level == LOG_ERR) {
        return "error";
    } else {
        return "unknown";
    }
}

/*
 * Parse a string like debug:schedule,player;excess:mqtt for log level
 */
bool SetLogLevelComplex(std::string& input) {
    LogDebug(VB_SETTING, "Attempting to parse Log Levels from %s\n", input.c_str());

    if (input.length() == 0) {
        LogWarn(VB_SETTING, "Ignoring Empty String sent to SetLogLevelComplex");
        return false;
    }
    // Check if simple method
    if (input.find(':') == std::string::npos) {
        LogDebug(VB_SETTING, "Using simple loging for input \"%s\" as no colon found.\n", input.c_str());
        return SetLogLevel(input.c_str());
    }
    if (input.find(';') == std::string::npos) {
        input = input + ';';
    }

    bool madeChange = false;
    std::istringstream ss(input);
    std::string token1;

    while (std::getline(ss, token1, ';')) {
        // token1 should be debug:scheduler,player

        LogDebug(VB_SETTING, "Outer Prase is %s\n", token1.c_str());
        size_t pos = token1.find(':');
        if (pos != std::string::npos) {
            std::string level = token1.substr(0, pos);
            std::string loggers = token1.substr(pos + 1);
            std::istringstream st(loggers);
            std::string logger;
            if (loggers.find(',') == std::string::npos) {
                loggers = loggers + ",";
            }
            while (std::getline(st, logger, ',')) {
                LogDebug(VB_SETTING, "Attempting to set %s to %s \n", logger.c_str(), level.c_str());
                madeChange = FPPLogger::INSTANCE.SetLevel(logger.c_str(), level.c_str()) || madeChange;
            }
        } else {
            LogErr(VB_SETTING, "Invalid Log Level Token: %s\n", token1.c_str());
        }
    }

    return madeChange;
}
bool SetLogLevelComplex(const char* input) {
    std::string s = input;
    return SetLogLevelComplex(s);
}

bool SetLogLevel(const char* newLevel) {
    LogErr(VB_SETTING, "Using Legacy Global Logging to set all to same Log Level of %s\n", newLevel);
    if (!strcmp(newLevel, "warn")) {
        FPPLogger::INSTANCE.SetAllLevel(LOG_WARN);
    } else if (!strcmp(newLevel, "debug")) {
        FPPLogger::INSTANCE.SetAllLevel(LOG_DEBUG);
    } else if (!strcmp(newLevel, "info")) {
        FPPLogger::INSTANCE.SetAllLevel(LOG_INFO);
    } else if (!strcmp(newLevel, "excess")) {
        FPPLogger::INSTANCE.SetAllLevel(LOG_INFO);
    } else if (!strcmp(newLevel, "error")) {
        FPPLogger::INSTANCE.SetAllLevel(LOG_ERR);
    } else {
        LogErr(VB_SETTING, "Unknown Log Level: %s\n", newLevel);
        return false;
    }

    return true;
}

int loggingToFile(void) {
    if ((logFileName[0]) &&
        (strcmp(logFileName, "stderr")) &&
        (strcmp(logFileName, "stdout")))
        return 1;

    return 0;
}

void logVersionInfo(void) {
    LogErr(VB_ALL, "=========================================\n");
    LogErr(VB_ALL, "FPP %s\n", getFPPVersion());
    LogErr(VB_ALL, "Branch: %s\n", getFPPBranch());
    int o = open("/proc/device-tree/model", O_RDONLY);
    if (o > 0) {
        char buf[256];
        int i = read(o, buf, 255);
        int c = i;
        while (i > 0) {
            i = read(o, buf, 255 - c);
            c += i > 0 ? i : 0;
        }
        if (c > 0) {
            buf[c] = 0;
            LogErr(VB_ALL, "Model: %s\n", buf);
        }
        close(o);
    }
    // Kernel matters for triage far more often than it looks: the Pi's V4L2
    // decoder wedge (issues #2695/#2727) and several DRM/KMS bugs are fixed or
    // broken per kernel, and asking a user for `uname -a` days after the fact
    // is unreliable -- the box may have been updated since. Log it with the
    // rest of the version banner so every fppd.log answers it on its own.
    struct utsname un;
    if (uname(&un) == 0) {
        LogErr(VB_ALL, "Kernel: %s %s (%s)\n", un.sysname, un.release, un.machine);
    }
    LogErr(VB_ALL, "=========================================\n");
}
