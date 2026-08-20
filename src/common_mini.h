#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2024 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <stdint.h>
#include <sys/types.h>

#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

/*
 * This file contains common utilities that only depend on the standard
 * C++ runtime  (no json, no curl, no logging, etc...).  Useful for
 * startup services that need to be small and fast
 */

long long GetTime();
long long GetTimeMicros();
long long GetTimeMS();
std::string GetTimeStr(std::string fmt);
std::string GetDateStr(std::string fmt);

// Thread-safe replacements for non-reentrant POSIX APIs.  Several of the C
// library functions (strerror, rand, getpwnam, ...) return pointers into
// shared per-process static buffers or use shared global state and are not
// safe to call from multiple threads.  FPP is heavily threaded (main loop,
// ping thread, HTTP server pool, channel output thread, ...) so these
// wrappers should be preferred everywhere.

// Thread-safe strerror(): wraps strerror_r() using a per-thread buffer.  The
// returned pointer is valid until the next FPPstrerror() call on the same
// thread, which matches how strerror() was already being used (formatted
// immediately into a log message), so it is a drop-in replacement.
const char* FPPstrerror(int errnum);

// Thread-safe rand(): each thread gets its own PRNG, so no global state is
// shared and no srand() seeding is required.  Returns a value in [0, RAND_MAX].
int FPPrand();

// Thread-safe getpwnam()/getpwuid(): wraps getpwnam_r().  Returns false if the
// user does not exist.  Any of the out pointers may be null if not needed.
bool GetUserIds(const std::string& username, uid_t* uid, gid_t* gid, std::string* homedir = nullptr);

int DirectoryExists(const char* Directory);
int DirectoryExists(const std::string& Directory);
int FileExists(const char* File);
int FileExists(const std::string& File);
uint64_t FileTimestamp(const std::string& File);
int Touch(const std::string& File);
int GetInterfaceAddress(const char* interface, char* addr, char* mask, char* gw);
int DateStrToInt(const char* str);
int GetCurrentDateInt(int daysOffset = 0);
int CurrentDateInRange(int startDate, int endDate);
int DateInRange(time_t when, int startDate, int endDate);
int DateInRange(int currentDate, int startDate, int endDate);
void CloseOpenFiles(int daemonMode = 0);
std::string secondsToTime(int i);

// Tee this process's stdout (and that of everything it forks) into `log`, one
// prefixed line at a time, while still echoing each raw line to the stdout we
// started with (journald under systemd, the terminal by hand).  For the small
// startup services -- fppinit, fpprtc -- which printf() rather than link the
// full logger.  See the comment on the definition.
void teeOutput(const std::string& log, const std::string& program, const std::string& facility, pid_t pid);

std::string GetFileContents(const std::string& filename);
std::string GetFileExtension(const std::string& filename);

bool PutFileContents(const std::string& filename, const std::string& str);
bool CopyFileContents(const std::string& srcFile, const std::string& destFile);
void TrimWhiteSpace(std::string& s);

uint8_t ReverseBitsInByte(uint8_t n);

bool SetFilePerms(const std::string& filename, bool exBit = false);
bool SetFilePerms(const char* file, bool exBit = false);

std::string tail(std::string const& source, size_t const length);
std::vector<std::string>& split(const std::string& s, char delim, std::vector<std::string>& elems);
std::vector<std::string> split(const std::string& s, char delim);

// splits the string on , but also honors any double/single quotes so commas within strings are preserved
std::vector<std::string> splitWithQuotes(const std::string& s, char delim = ',');

// Parse an "HH:MM:SS" (or "HH:MM", seconds defaulting to 0) time string.
// Returns false and leaves the outputs untouched when the string doesn't carry
// at least an hour and a minute.  Callers used to index split()'s vector blind,
// which walked off the end of it for a short value such as "8:00".
bool ParseTimeString(const std::string& str, int& hour, int& minute, int& second);

bool startsWith(const std::string& str, const std::string& prefix);
bool endsWith(const std::string& str, const std::string& suffix);

// ethernet interfaces that carry show traffic (eth0, eno1, enp3s0, enx<mac>);
// the single predicate for fppinit's qdisc/EEE setup and UDPOutput's pacing
bool IsShowEthernetInterface(const std::string& dev);

// path to the tc binary (location varies across distros)
std::string TcPath();
// install the fq qdisc used for UDP output pacing on an ethernet interface
bool InstallShowTrafficQdisc(const std::string& dev);
// does the interface have a routable IPv4 address (i.e. will it carry routed
// UDP)?  169.254/16 link-local doesn't count: FPP's catch-all networkd config
// puts one on every unconfigured interface, including NICs dedicated to raw
// layer-2 outputs (ColorLight).
bool InterfaceHasRoutableIPv4(const std::string& dev);
bool contains(const std::string& str, const std::string& v);
void replaceAll(std::string& str, const std::string& from, const std::string& to);
bool replaceStart(std::string& str, const std::string& from, const std::string& to = "");
bool replaceEnd(std::string& str, const std::string& from, const std::string& to = "");
void toUpper(std::string& str);
void toLower(std::string& str);
std::string toUpperCopy(const std::string& str);
std::string toLowerCopy(const std::string& str);

// Tools for getting settings directly from the settings file
// Most programs should use the utilities in settings.h, but
// for boot utilities, these are quicker and have less dependencies
bool getRawSetting(const std::string& str, std::string& value);
int getRawSettingInt(const std::string& str, int def);
void setRawSetting(const std::string& str, const std::string& value);
std::map<std::string, std::string> loadSettingsFile(const std::string& filename);

