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

#include "common_mini.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#if defined(__APPLE__)
#include <copyfile.h>
#else
#include <sys/sendfile.h>
#endif

#include <arpa/inet.h>
#include <curl/curl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <ctype.h>
#include <cxxabi.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <ifaddrs.h>
#include <iomanip>
#include <list>
#include <map>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sstream>
#include <unistd.h>
#include <utility>
#include <vector>

/*
 * Get the current time down to the microsecond
 */
long long GetTime(void) {
    struct timeval now_tv;
    gettimeofday(&now_tv, NULL);
    return now_tv.tv_sec * 1000000LL + now_tv.tv_usec;
}
long long GetTimeMicros(void) {
    struct timeval now_tv;
    gettimeofday(&now_tv, NULL);
    return now_tv.tv_sec * 1000000LL + now_tv.tv_usec;
}

long long GetTimeMS(void) {
    struct timeval now_tv;
    gettimeofday(&now_tv, NULL);
    return now_tv.tv_sec * 1000LL + now_tv.tv_usec / 1000;
}

std::string GetTimeStr(std::string fmt) {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream sstr;
    sstr << std::put_time(&tm, fmt.c_str());

    return sstr.str();
}

std::string GetDateStr(std::string fmt) {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream sstr;
    sstr << std::put_time(&tm, fmt.c_str());

    return sstr.str();
}

/*
 * Check to see if the specified directory exists
 */
int DirectoryExists(const char* Directory) {
    DIR* dir = opendir(Directory);
    if (dir) {
        closedir(dir);
        return 1;
    } else {
        return 0;
    }
}
int DirectoryExists(const std::string& Directory) {
    return DirectoryExists(Directory.c_str());
}

/*
 * Check if the specified file exists or not
 */
int FileExists(const char* File) {
    struct stat sts;
    if (stat(File, &sts) == -1) {
        return 0;
    } else {
        return 1;
    }
}
int FileExists(const std::string& f) {
    return FileExists(f.c_str());
}

int Touch(const std::string& File) {
    int fd = open(File.c_str(), O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK, 0666);
    if (fd < 0)
        return 0;

    close(fd);

    return 1;
}

/*
 * Get IP address on specified network interface
 */
int GetInterfaceAddress(const char* interface, char* addr, char* mask, char* gw) {
    int fd;
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;

    /* I want IP address attached to E131interface */
    strncpy(ifr.ifr_name, (const char*)interface, IFNAMSIZ - 1);

    if (addr) {
        if (ioctl(fd, SIOCGIFADDR, &ifr))
            strcpy(addr, "127.0.0.1");
        else
            strcpy(addr, inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));
    }

    if (mask) {
        if (ioctl(fd, SIOCGIFNETMASK, &ifr))
            strcpy(mask, "255.255.255.255");
        else
            strcpy(mask, inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));
    }

    if (gw) {
        *gw = 0;

        FILE* f;
        char line[100];
        char* iface;
        char* dest;
        char* route;
        char* saveptr;

        f = fopen("/proc/net/route", "r");

        if (f) {
            while (fgets(line, 100, f)) {
                iface = strtok_r(line, " \t", &saveptr);
                dest = strtok_r(NULL, " \t", &saveptr);
                route = strtok_r(NULL, " \t", &saveptr);

                if ((iface && dest && route) &&
                    (!strcmp(iface, interface)) &&
                    (!strcmp(dest, "00000000"))) {
                    char* end;
                    int ng = strtol(route, &end, 16);
                    struct in_addr addr;
                    addr.s_addr = ng;
                    strcpy(gw, inet_ntoa(addr));
                }
            }
            fclose(f);
        }
    }

    close(fd);

    return 0;
}

/*
 *
 */

/*
 *
 */

static unsigned char bitLookup[16] = {
    0x0,
    0x8,
    0x4,
    0xc,
    0x2,
    0xa,
    0x6,
    0xe,
    0x1,
    0x9,
    0x5,
    0xd,
    0x3,
    0xb,
    0x7,
    0xf,
};

/*
 * Reverse bits in a byte
 */
uint8_t ReverseBitsInByte(uint8_t n) {
    return (bitLookup[n & 0b1111] << 4) | bitLookup[n >> 4];
}

/*
 * Convert a string of the form "YYYY-MM-DD to an integer YYYYMMDD
 */
int DateStrToInt(const char* str) {
    if ((!str) || (str[4] != '-') || (str[7] != '-') || (str[10] != 0x0))
        return 0;

    int result = 0;
    char tmpStr[11];

    strcpy(tmpStr, str);

    result += atoi(str) * 10000;   // Year
    result += atoi(str + 5) * 100; // Month
    result += atoi(str + 8);       // Day

    return result;
}

/*
 * Get the current date in an integer form YYYYMMDD
 */
int GetCurrentDateInt(int daysOffset) {
    time_t currTime = time(NULL) + (daysOffset * 86400);
    struct tm now;
    int result = 0;

    localtime_r(&currTime, &now);

    result += (now.tm_year + 1900) * 10000;
    result += (now.tm_mon + 1) * 100;
    result += (now.tm_mday);

    return result;
}
std::string secondsToTime(int i) {
    std::stringstream sstr;

    if (i > (24 * 60 * 60)) {
        int days = i / (24 * 60 * 60);
        sstr << days;
        if (days == 1)
            sstr << " day, ";
        else
            sstr << " days, ";

        i = i % (24 * 60 * 60);
    }

    if (i > (60 * 60)) {
        int hour = i / (60 * 60);
        sstr << std::setw(2) << std::setfill('0') << hour;
        sstr << ":";
        i = i % (60 * 60);
    }

    int min = i / 60;
    int sec = i % 60;
    sstr << std::setw(2) << std::setfill('0') << min;
    sstr << ":";
    sstr << std::setw(2) << std::setfill('0') << sec;
    return sstr.str();
}
/*
 * Close all open file descriptors (used after fork())
 */
void CloseOpenFiles(void) {
    int maxfd = sysconf(_SC_OPEN_MAX);

    for (int fd = 3; fd < maxfd; fd++) {
        if (fcntl(fd, F_GETFD) != -1) {
            bool doClose = false;
            struct stat statBuf;
            if (fstat(fd, &statBuf) == 0) {
                // it's a file or unix domain socket or similar
                doClose = true;
            }
            struct sockaddr_in address;
            memset(&address, 0, sizeof(address));
            socklen_t addrLen = sizeof(address);
            getsockname(fd, (struct sockaddr*)&address, &addrLen);
            if (address.sin_family) {
                // its a tcp/udp socket of some sort
                doClose = true;
            }
            if (doClose) {
                // if it's not a file or socket, we cannot close it
                // On OSX, that may be a "NPOLICY" descriptor which
                // if closed will terminate the process immediately
                close(fd);
            }
        }
    }
}

int DateInRange(time_t when, int startDate, int endDate) {
    struct tm dt;
    localtime_r(&when, &dt);

    int dateInt = 0;
    dateInt += (dt.tm_year + 1900) * 10000;
    dateInt += (dt.tm_mon + 1) * 100;
    dateInt += (dt.tm_mday);

    return DateInRange(dateInt, startDate, endDate);
}

/*
 * Check to see if current date int is in the range specified
 */
int CurrentDateInRange(int startDate, int endDate) {
    int currentDate = GetCurrentDateInt();

    return DateInRange(currentDate, startDate, endDate);
}

int DateInRange(int currentDate, int startDate, int endDate) {
    if ((startDate < 10000) || (endDate < 10000)) {
        startDate = startDate % 10000;
        endDate = endDate % 10000;
        currentDate = currentDate % 10000;

        if (endDate < startDate) {
            if (currentDate <= endDate)
                currentDate += 10000;

            endDate += 10000; // next year
        }
    }

    if ((startDate < 100) || (endDate < 100)) {
        startDate = startDate % 100;
        endDate = endDate % 100;
        currentDate = currentDate % 100;

        if (endDate < startDate) {
            if (currentDate <= endDate)
                currentDate += 100;

            endDate += 100; // next month
        }
    }

    if ((startDate == 0) && (endDate == 0))
        return 1;

    if ((startDate <= currentDate) && (currentDate <= endDate))
        return 1;

    return 0;
}

std::string tail(std::string const& source, size_t const length) {
    if (length >= source.size())
        return source;

    return source.substr(source.size() - length);
}

/*
 * Helpers to split a string on the specified character delimiter
 */
std::vector<std::string>& split(const std::string& s, char delim, std::vector<std::string>& elems) {
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }

    return elems;
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> elems;

    split(s, delim, elems);

    return elems;
}

inline std::string dequote(const std::string& s) {
    if ((s[0] == '\'' || s[0] == '"') && s[0] == s[s.length() - 1] && s.length() > 2) {
        return s.substr(1, s.length() - 2);
    }
    return s;
}

std::vector<std::string> splitWithQuotes(const std::string& s, char delim) {
    std::vector<std::string> ret;
    const char* mystart = s.c_str();
    bool instring = false;
    for (const char* p = mystart; *p; p++) {
        if (*p == '"' || *p == '\'') {
            instring = !instring;
        } else if (*p == delim && !instring) {
            ret.push_back(dequote(std::string(mystart, p - mystart)));
            mystart = p + 1;
        }
    }
    ret.push_back(dequote(std::string(mystart)));
    return ret;
}

std::string GetFileContents(const std::string& filename) {
    FILE* fd = fopen(filename.c_str(), "r");
    std::string contents;
    if (fd != nullptr) {
        flock(fileno(fd), LOCK_SH);
        fseeko(fd, 0, SEEK_END);
        size_t sz = ftello(fd);
        fseeko(fd, 0, SEEK_SET);
        if (sz == 0) {
            char buf[4097];
            size_t i = fread(buf, 1, 4096, fd);
            while (i > 0) {
                buf[i] = 0;
                contents += buf;
                i = fread(buf, 1, 4096, fd);
            }
        } else {
            contents.resize(sz);
            fread(&contents[0], contents.size(), 1, fd);
        }
        flock(fileno(fd), LOCK_UN);
        fclose(fd);
        int x = contents.size() - 1;
        for (; x > 0; x--) {
            if (contents[x] != 0) {
                break;
            }
        }
        contents.resize(x + 1);
    }
    return contents;
}

bool PutFileContents(const std::string& filename, const std::string& str) {
    FILE* fd = fopen(filename.c_str(), "w");
    if (fd != nullptr) {
        flock(fileno(fd), LOCK_EX);
        fwrite(&str[0], str.size(), 1, fd);
        flock(fileno(fd), LOCK_UN);
        fclose(fd);
        SetFilePerms(filename);
        return true;
    }
    fprintf(stderr, "ERROR: Unable to open %s for writing.\n", filename.c_str());

    return false;
}
bool CopyFileContents(const std::string& srcFile, const std::string& destFile) {
    int input, output;
    if ((input = open(srcFile.c_str(), O_RDONLY)) == -1) {
        fprintf(stderr, "ERROR: Unable to open %s for reading.\n", srcFile.c_str());
        return false;
    }
    if ((output = creat(destFile.c_str(), 0660)) == -1) {
        close(input);
        return false;
    }
    // Here we use kernel-space copying for performance
#if defined(__APPLE__)
    int result = fcopyfile(input, output, 0, COPYFILE_ALL);
#else
    off_t bytesCopied = 0;
    struct stat fileinfo = { 0 };
    fstat(input, &fileinfo);
    int result = sendfile(output, input, &bytesCopied, fileinfo.st_size);
#endif
    close(input);
    close(output);
    if (result < 0) {
        fprintf(stderr, "ERROR: Unable to open %s for reading.\n", srcFile.c_str());
    }
    return result == 0;
}

bool SetFilePerms(const std::string& filename, bool exBit) {
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    if (exBit) {
        mode |= S_IRWXU | S_IRWXG | S_IXOTH;
    }
    chmod(filename.c_str(), mode);
#ifndef PLATFORM_OSX
    struct passwd* pwd = getpwnam("fpp");
    if (pwd) {
        chown(filename.c_str(), pwd->pw_uid, pwd->pw_gid);
    }
#endif

    return true;
}

bool SetFilePerms(const char* file, bool exBit) {
    const std::string filename = file;
    return SetFilePerms(filename, exBit);
}

/////////////////////////////////////////////////////////////////////////////
// trim from start (in place)
static inline void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
                return !std::isspace(ch);
            }));
}
// trim from end (in place)
static inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
                return !std::isspace(ch);
            }).base(),
            s.end());
}
// trim from both ends (in place)
void TrimWhiteSpace(std::string& s) {
    ltrim(s);
    rtrim(s);
}

bool startsWith(const std::string& str, const std::string& prefix) {
    return ((prefix.size() <= str.size()) && std::equal(prefix.begin(), prefix.end(), str.begin()));
}
bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}
bool contains(const std::string& str, const std::string& v) {
    return str.find(v) != std::string::npos;
}
void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // ...
    }
}

bool replaceStart(std::string& str, const std::string& from, const std::string& to) {
    if (!startsWith(str, from))
        return false;

    str.replace(0, from.size(), to);
    return true;
}

bool replaceEnd(std::string& str, const std::string& from, const std::string& to) {
    if (!endsWith(str, from))
        return false;

    str.replace(str.size() - from.size(), from.size(), to);
    return true;
}

void toUpper(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
}
void toLower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
}
std::string toUpperCopy(const std::string& str) {
    std::string cp = str;
    toUpper(cp);
    return cp;
}
std::string toLowerCopy(const std::string& str) {
    std::string cp = str;
    toLower(cp);
    return cp;
}

bool getRawSetting(const std::string& setting, std::string& value) {
    if (!FileExists("/home/fpp/media/settings")) {
        return false;
    }
    std::string c = GetFileContents("/home/fpp/media/settings");
    size_t idx = c.find(setting + " = ");
    if (idx != std::string::npos) {
        int offset = idx + setting.size() + 3;
        size_t idx2 = c.find("\r", offset);
        size_t idx3 = c.find("\n", offset);
        if (idx2 == std::string::npos) {
            idx2 = idx3;
        }
        if (idx3 != std::string::npos && idx3 < idx2) {
            idx2 = idx3;
        }
        std::string s = c.substr(offset, idx2 - offset);
        TrimWhiteSpace(s);
        value = dequote(s);
        return true;
    }
    return false;
}
int getRawSettingInt(const std::string& setting, int def) {
    if (!FileExists("/home/fpp/media/settings")) {
        return def;
    }
    std::string c = GetFileContents("/home/fpp/media/settings");
    size_t idx = c.find(setting + " = ");
    if (idx != std::string::npos) {
        int offset = idx + setting.size() + 4;
        int ch1 = c[offset];
        if (ch1 == '-') {
            ++offset;
            ch1 = c[offset] - '0';
            ch1 = 0 - ch1;
        } else {
            ch1 = ch1 - '0';
        }
        int ch = ch1;
        ++offset;
        uint8_t ch2 = c[offset];
        while (ch2 != '\"' && ch2 != '\n' && ch2 != '\r') {
            ch *= 10;
            ch += ch2 - '0';
            ++offset;
            ch2 = c[offset];
        }
        return ch;
    }
    return def;
}

void setRawSetting(const std::string& setting, const std::string& value) {
    std::string content = GetFileContents("/home/fpp/media/settings");
    std::vector<std::string> lines = split(content, '\n');
    content = "";
    for (auto& line : lines) {
        if (line.find(setting + " = ") == std::string::npos) {
            content += line;
            content += "\n";
        }
    }
    content += setting;
    content += " = \"";
    content += value;
    content += "\"\n";
    PutFileContents("/home/fpp/media/settings", content);
}

std::map<std::string, std::string> loadSettingsFile(const std::string& filename) {
    std::map<std::string, std::string> ret;
    std::string content = GetFileContents(filename);
    std::vector<std::string> lines = split(content, '\n');
    for (auto& line : lines) {
        size_t idx = line.find("=");
        if (idx != std::string::npos) {
            std::string key = line.substr(0, idx);
            std::string value = line.substr(idx + 1);
            TrimWhiteSpace(key);
            TrimWhiteSpace(value);
            if (!value.empty() && value[0] == '"') {
                value = value.substr(1);
                value = value.substr(0, value.size() - 1);
            }
            ret[key] = value;
        }
    }
    return ret;
}
