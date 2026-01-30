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
#include <unistd.h>
#include <utility>
#include <vector>

#include "common.h"
#include "fppversion.h"
#include "log.h"

/*
 * Dump memory block in hex and human-readable formats
 */
void HexDump(const char* title, const void* data, int len, FPPLoggerInstance& facility, int perLine) {
    int l = 0;
    int i = 0;
    int x = 0;
    int y = 0;
    unsigned char* ch = (unsigned char*)data;
    unsigned char* str = new unsigned char[perLine + 1];

    int maxLen = perLine * 7 + 20;
    char* tmpStr = new char[maxLen];

    if (strlen(title)) {
        snprintf(tmpStr, maxLen, "%s: (%d bytes)\n", title, len);
        LogInfo(facility, tmpStr);
    }

    while (l < len) {
        if (x == 0) {
            snprintf(tmpStr, maxLen, "%06x: ", i);
        }

        if (x < perLine) {
            snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), "%02x ", *ch & 0xFF);
            str[x] = *ch;
            x++;
            i++;
        } else {
            snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), "   ");
            for (; x > 0; x--) {
                if (str[perLine - x] == '%' || str[perLine - x] == '\\') {
                    // these are escapes for the Log call, so don't display them
                    snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), ".");
                } else if (isgraph(str[perLine - x]) || str[perLine - x] == ' ') {
                    snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), "%c", str[perLine - x]);
                } else {
                    snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), ".");
                }
            }

            snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), "\n");
            LogInfo(facility, tmpStr);
            x = 0;

            snprintf(tmpStr, maxLen, "%06x: ", i);
            snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), "%02x ", *ch & 0xFF);
            str[x] = *ch;
            x++;
            i++;
        }

        l++;
        ch++;
    }
    for (y = x; y < perLine; y++) {
        snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), "   ");
    }
    snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), "   ");
    for (y = 0; y < x; y++) {
        if (str[y] == '%' || str[y] == '\\') {
            // these are escapes for the Log call, so don't display them
            snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), ".");
        } else if (isgraph(str[y]) || str[y] == ' ') {
            snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), "%c", str[y]);
        } else {
            snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), ".");
        }
    }

    snprintf(tmpStr + strlen(tmpStr), maxLen - strlen(tmpStr), "\n");
    LogInfo(facility, tmpStr);
    delete[] tmpStr;
    delete[] str;
}

int CheckForHostSpecificFile(const std::string& hostname, std::string& filename) {
    std::string localFilename = filename;

    int len = localFilename.length();
    int extIdx = 0;

    // Check for 3 or 4-digit extension
    if (localFilename[len - 4] == '.') {
        extIdx = len - 4;
    } else if (localFilename[len - 5] == '.') {
        extIdx = len - 5;
    }

    if (extIdx) {
        // Preserve the extension including the dot
        std::string ext = localFilename.substr(extIdx);
        localFilename = localFilename.substr(0, extIdx);
        localFilename += "-";
        localFilename += hostname;
        localFilename += ext;

        if (FileExists(localFilename)) {
            LogDebug(VB_SEQUENCE, "Found %s to use instead of %s\n",
                     localFilename.c_str(), filename.c_str());
            filename = localFilename;
            return 1;
        } else {
            // Replace hyphen with an underscore and recheck
            localFilename[extIdx] = '_';
            if (FileExists(localFilename)) {
                LogDebug(VB_SEQUENCE, "Found %s to use instead of %s\n",
                         localFilename.c_str(), filename.c_str());
                filename = localFilename;
                return 1;
            }
        }
    }

    return 0;
}
int CheckForHostSpecificFile(const char* hostname, char* filename) {
    std::string f = filename;
    if (CheckForHostSpecificFile(hostname, f)) {
        strcpy(filename, f.c_str());
        return 1;
    }
    return 0;
}
char* FindInterfaceForIP(char* ip) {
    struct ifaddrs *ifaddr, *ifa;
    int family, s, n;
    char host[NI_MAXHOST];
    char interfaceIP[16];
    static char interface[10] = "";

    if (getifaddrs(&ifaddr) == -1) {
        LogErr(VB_SETTING, "Error getting interfaces list: %s\n",
               strerror(errno));
        return interface;
    }

    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if (ifa->ifa_addr == NULL)
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                LogErr(VB_SETTING, "getnameinfo failed");
                freeifaddrs(ifaddr);
                return interface;
            }

            if (!strcmp(host, ip)) {
                strcpy(interface, ifa->ifa_name);
                freeifaddrs(ifaddr);
                return interface;
            }
        }
    }

    return interface;
}

static void ReplaceString(std::string& str, std::string pattern, std::string replacement) {
    std::size_t found = str.find(pattern);
    while (found != std::string::npos) {
        str.replace(found, pattern.length(), replacement);

        found = str.find(pattern);
    }
}
std::string ReplaceKeywords(std::string str, std::map<std::string, std::string>& keywords) {
    std::string key;

    for (auto& k : keywords) {
        key = std::string("%");
        key += k.first;
        key += std::string("%");

        ReplaceString(str, key, k.second);
    }

    std::time_t currTime = std::time(NULL);
    struct tm now;
    localtime_r(&currTime, &now);
    char tmpStr[20];

    snprintf(tmpStr, sizeof(tmpStr), "%02d:%02d:%02d", now.tm_hour, now.tm_min, now.tm_sec);
    ReplaceString(str, "%TIME%", tmpStr);

    snprintf(tmpStr, sizeof(tmpStr), "%04d-%02d-%02d", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday);
    ReplaceString(str, "%DATE%", tmpStr);

    ReplaceString(str, "%FPP_VERSION%", getFPPVersionTriplet());
    ReplaceString(str, "%FPP_SOURCE_VERSION%", getFPPVersion());
    ReplaceString(str, "%FPP_BRANCH%", getFPPBranch());

    return str;
}

/////////////////////////////////////////////////////////////////////////////
/*
 * Merge the contens of Json::Value b into Json::Value a
 */
void MergeJsonValues(Json::Value& a, Json::Value& b) {
    if (!a.isObject() || !b.isObject())
        return;

    Json::Value::Members memberNames = b.getMemberNames();

    for (unsigned int i = 0; i < memberNames.size(); i++) {
        std::string key = memberNames[i];

        if ((a[key].type() == Json::objectValue) &&
            (b[key].type() == Json::objectValue)) {
            MergeJsonValues(a[key], b[key]);
        } else {
            a[key] = b[key];
        }
    }
}

/*
 *
 */
Json::Value LoadJsonFromFile(const std::string& filename) {
    Json::Value root;

    LoadJsonFromFile(filename, root);

    return root;
}

/*
 *
 */
Json::Value LoadJsonFromString(const std::string& str) {
    Json::Value root;
    bool result = LoadJsonFromString(str, root);

    return root;
}

/*
 *
 */
bool LoadJsonFromString(const std::string& str, Json::Value& root) {
    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    std::string errors;

    builder["collectComments"] = false;

    bool success = reader->parse(str.c_str(), str.c_str() + str.size(), &root, &errors);
    delete reader;

    if (!success) {
        LogErr(VB_GENERAL, "Error parsing JSON string in LoadJsonFromString(): '%s'\n", str.c_str());
        Json::Value empty;
        root = empty;
        return false;
    }

    LogDebug(VB_GENERAL, "LoadJsonFromString() loaded: '%s'\n", str.c_str());

    return true;
}

bool LoadJsonFromFile(const std::string& filename, Json::Value& root) {
    if (!FileExists(filename)) {
        LogErr(VB_GENERAL, "JSON File %s does not exist\n", filename.c_str());
        return false;
    }

    std::string jsonStr = GetFileContents(filename);

    return LoadJsonFromString(jsonStr, root);
}

bool LoadJsonFromFile(const char* filename, Json::Value& root) {
    std::string filenameStr = filename;

    return LoadJsonFromFile(filenameStr, root);
}

std::string SaveJsonToString(const Json::Value& root, const std::string& indentation) {
    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = indentation;

    std::string result = Json::writeString(wbuilder, root);

    return result;
}

bool SaveJsonToString(const Json::Value& root, std::string& str, const std::string& indentation) {
    str = SaveJsonToString(root, indentation);

    if (str.empty())
        return false;

    return true;
}

bool SaveJsonToFile(const Json::Value& root, const std::string& filename, const std::string& indentation) {
    std::string str;

    bool result = SaveJsonToString(root, str, indentation);

    if (!result) {
        LogErr(VB_GENERAL, "Error converting Json::Value to std::string()\n");
        return false;
    }

    result = PutFileContents(filename, str);
    if (!result) {
        return false;
    }

    return true;
}

bool SaveJsonToFile(const Json::Value& root, const char* filename, const char* indentation) {
    std::string filenameStr = filename;
    std::string indentationStr = indentation;

    return SaveJsonToFile(root, filenameStr, indentationStr);
}

std::string getSimpleHTMLTTag(const std::string& html, const std::string& searchStr, const std::string& skipStr, const std::string& endStr) {
    std::string value;
    std::string tStr;
    std::size_t fStart = html.find(searchStr);
    std::size_t fEnd;

    if (fStart != std::string::npos) {
        tStr = html.substr(fStart + searchStr.size());
        fStart = tStr.find(skipStr);
        if (fStart != std::string::npos) {
            fStart += skipStr.size();
            fEnd = tStr.substr(fStart).find(endStr);
            fEnd += fStart;
            if (fEnd > fStart) {
                value = tStr.substr(fStart, fEnd - fStart);
                TrimWhiteSpace(value);
                replaceAll(value, std::string("  "), std::string(" "));
            }
        }
    }

    return value;
}

std::string getSimpleXMLTag(const std::string& xml, const std::string& tag) {
    std::string value;
    std::string sSearch = "<";
    sSearch += tag + ">";

    std::string eSearch = "</";
    eSearch += tag + ">";

    std::size_t fStart = xml.find(sSearch);
    std::size_t fEnd = xml.find(eSearch);

    if ((fStart != std::string::npos) &&
        (fEnd != std::string::npos) &&
        (fEnd > fStart)) {
        value = xml.substr(fStart + sSearch.length(), fEnd - fStart - sSearch.length());
        TrimWhiteSpace(value);
    }

    return value;
}

// URL Helpers
size_t urlWriteData(void* buffer, size_t size, size_t nmemb, void* userp) {
    std::string* str = (std::string*)userp;

    str->append(static_cast<const char*>(buffer), size * nmemb);

    return size * nmemb;
}

bool urlHelper(const std::string method, const std::string& url, const std::string& data, std::string& resp, const unsigned int timeout) {
    return urlHelper(method, url, data, resp, std::list<std::string>(), timeout);
}
bool urlHelper(const std::string method, const std::string& url, const std::string& data, std::string& resp, const std::list<std::string>& extraHeaders, const unsigned int timeout) {
    CURL* curl = curl_easy_init();
    std::string userAgent = "FPP/";
    userAgent += getFPPVersionTriplet();

    resp = "";

    if (!curl) {
        LogDebug(VB_GENERAL, "Unable to create curl instance in urlHelper()\n");
        return false;
    }

    CURLcode status;
    status = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, urlWriteData);
    if (status != CURLE_OK) {
        LogErr(VB_GENERAL, "curl_easy_setopt() Error setting write callback function: %s\n", curl_easy_strerror(status));
        curl_easy_cleanup(curl);
        return false;
    }

    status = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    if (status != CURLE_OK) {
        LogErr(VB_GENERAL, "curl_easy_setopt() Error setting class pointer: %s\n", curl_easy_strerror(status));
        curl_easy_cleanup(curl);
        return false;
    }

    status = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (status != CURLE_OK) {
        LogErr(VB_GENERAL, "curl_easy_setopt() Error setting URL: %s\n", curl_easy_strerror(status));
        curl_easy_cleanup(curl);
        return false;
    }

    struct curl_slist* headers = NULL;
    if (startsWith(data, "{") && endsWith(data, "}")) {
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }
    for (auto& h : extraHeaders) {
        headers = curl_slist_append(headers, h.c_str());
    }
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    if ((method == "POST") || (method == "PUT")) {
        status = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        if (status != CURLE_OK) {
            LogErr(VB_GENERAL, "curl_easy_setopt() Error setting postfields data: %s\n", curl_easy_strerror(status));
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return false;
        }
    }
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

    if (method == "POST")
        curl_easy_setopt(curl, CURLOPT_POST, 1);
    else if (method == "PUT")
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    else if (method == "DELETE")
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());

    LogDebug(VB_GENERAL, "Calling %s %s\n", method.c_str(), url.c_str());

    status = curl_easy_perform(curl);
    if (status != CURLE_OK) {
        LogErr(VB_GENERAL, "curl_easy_perform() failed (%s): %s\n", url.c_str(), curl_easy_strerror(status));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }

    LogDebug(VB_GENERAL, "%s %s resp: %s\n", method.c_str(), url.c_str(), resp.c_str());

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return true;
}

bool urlHelper(const std::string method, const std::string& url, std::string& resp, const unsigned int timeout) {
    std::string data;
    return urlHelper(method, url, data, resp, timeout);
}

bool urlGet(const std::string url, std::string& resp) {
    std::string data;
    return urlHelper("GET", url, resp);
}

bool urlPost(const std::string url, const std::string data, std::string& resp) {
    return urlHelper("POST", url, data, resp);
}

bool urlPut(const std::string url, const std::string data, std::string& resp) {
    return urlHelper("PUT", url, data, resp);
}

bool urlDelete(const std::string url, const std::string data, std::string& resp) {
    return urlHelper("DELETE", url, data, resp);
}

bool urlDelete(const std::string url, std::string& resp) {
    std::string data;
    return urlHelper("DELETE", url, data, resp);
}

static const std::string BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static inline bool isBase64(uint8_t c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64Encode(uint8_t const* buf, unsigned int bufLen) {
    std::string ret;
    int i = 0;
    int j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    while (bufLen--) {
        char_array_3[i++] = *(buf++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); i++) {
                ret += BASE64_CHARS[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++) {
            ret += BASE64_CHARS[char_array_4[j]];
        }

        while ((i++ < 3)) {
            ret += '=';
        }
    }
    return ret;
}
std::vector<uint8_t> base64Decode(std::string const& encodedString) {
    int in_len = encodedString.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    uint8_t char_array_4[4], char_array_3[3];
    std::vector<uint8_t> ret;

    while (in_len-- && (encodedString[in_] != '=') && isBase64(encodedString[in_])) {
        char_array_4[i++] = encodedString[in_];
        in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                char_array_4[i] = BASE64_CHARS.find(char_array_4[i]);
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++) {
                ret.push_back(char_array_3[i]);
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++) {
            char_array_4[j] = 0;
        }
        for (j = 0; j < 4; j++) {
            char_array_4[j] = BASE64_CHARS.find(char_array_4[j]);
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) {
            ret.push_back(char_array_3[j]);
        }
    }
    return ret;
}

static std::function<void(bool)> SHUTDOWN_HOOK;
void ShutdownFPPD(bool restart) {
    if (SHUTDOWN_HOOK) {
        SHUTDOWN_HOOK(restart);
    }
}
void RegisterShutdownHandler(const std::function<void(bool)> hook) {
    SHUTDOWN_HOOK = hook;
}

std::string GetFileExtension(const std::string& filename) {
    if (filename.find_last_of(".") != std::string::npos)
        return filename.substr(filename.find_last_of(".") + 1);
    return "";
}

void SetThreadName(const std::string& name) {
#ifdef PLATFORM_OSX
    pthread_setname_np(name.c_str());
#else
    pthread_setname_np(pthread_self(), name.c_str());
#endif
}

std::string getPlatform() {
    std::string platform = GetFileContents("/etc/fpp/platform");
    TrimWhiteSpace(platform);
    return platform;
}


/////////////////////////////////////////////////////////////////////////////
// Unrolled 32x32 bitwise transform from Hacker's Delight
//
// Bit flips an array along the diagonal from MSbit on LS uint32_t to LS bit on MS uint32_t
//
// src[00] = 0b00000000000000000000000000000001
// src[01] = 0b00000000000000000000000000000010
// ...
// src[30] = 0b00000000000000000000000000000000
// src[31] = 0b00000000000000000000000000000000
// becomes
// dst[00] = 0b00000000000000000000000000000000
// dst[01] = 0b00000000000000000000000000000000
// ...
// dst[30] = 0b01000000000000000000000000000000
// dst[31] = 0b10000000000000000000000000000000

#define tr32swap(a0, a1, j, m) t = (a0 ^ (a1 >> j)) & m; \
                           a0 = a0 ^ t; \
                           a1 = a1 ^ (t << j);

void TransposeBits32x32(uint32_t *dst, uint32_t *src) {
   uint32_t m, t;
   uint32_t a0, a1, a2, a3, a4, a5, a6, a7,
            a8, a9, a10, a11, a12, a13, a14, a15,
            a16, a17, a18, a19, a20, a21, a22, a23,
            a24, a25, a26, a27, a28, a29, a30, a31;

   a0  = src[ 0];  a1  = src[ 1];  a2  = src[ 2];  a3  = src[ 3];
   a4  = src[ 4];  a5  = src[ 5];  a6  = src[ 6];  a7  = src[ 7];
   a8  = src[ 8];  a9  = src[ 9];  a10 = src[10];  a11 = src[11];
   a12 = src[12];  a13 = src[13];  a14 = src[14];  a15 = src[15];
   a16 = src[16];  a17 = src[17];  a18 = src[18];  a19 = src[19];
   a20 = src[20];  a21 = src[21];  a22 = src[22];  a23 = src[23];
   a24 = src[24];  a25 = src[25];  a26 = src[26];  a27 = src[27];
   a28 = src[28];  a29 = src[29];  a30 = src[30];  a31 = src[31];

   m = 0x0000FFFF;
   tr32swap(a0,  a16, 16, m)
   tr32swap(a1,  a17, 16, m)
   tr32swap(a2,  a18, 16, m)
   tr32swap(a3,  a19, 16, m)
   tr32swap(a4,  a20, 16, m)
   tr32swap(a5,  a21, 16, m)
   tr32swap(a6,  a22, 16, m)
   tr32swap(a7,  a23, 16, m)
   tr32swap(a8,  a24, 16, m)
   tr32swap(a9,  a25, 16, m)
   tr32swap(a10, a26, 16, m)
   tr32swap(a11, a27, 16, m)
   tr32swap(a12, a28, 16, m)
   tr32swap(a13, a29, 16, m)
   tr32swap(a14, a30, 16, m)
   tr32swap(a15, a31, 16, m)

   m = 0x00FF00FF;
   tr32swap(a0,  a8,   8, m)
   tr32swap(a1,  a9,   8, m)
   tr32swap(a2,  a10,  8, m)
   tr32swap(a3,  a11,  8, m)
   tr32swap(a4,  a12,  8, m)
   tr32swap(a5,  a13,  8, m)
   tr32swap(a6,  a14,  8, m)
   tr32swap(a7,  a15,  8, m)
   tr32swap(a16, a24,  8, m)
   tr32swap(a17, a25,  8, m)
   tr32swap(a18, a26,  8, m)
   tr32swap(a19, a27,  8, m)
   tr32swap(a20, a28,  8, m)
   tr32swap(a21, a29,  8, m)
   tr32swap(a22, a30,  8, m)
   tr32swap(a23, a31,  8, m)

   m = 0x0F0F0F0F;
   tr32swap(a0,  a4,   4, m)
   tr32swap(a1,  a5,   4, m)
   tr32swap(a2,  a6,   4, m)
   tr32swap(a3,  a7,   4, m)
   tr32swap(a8,  a12,  4, m)
   tr32swap(a9,  a13,  4, m)
   tr32swap(a10, a14,  4, m)
   tr32swap(a11, a15,  4, m)
   tr32swap(a16, a20,  4, m)
   tr32swap(a17, a21,  4, m)
   tr32swap(a18, a22,  4, m)
   tr32swap(a19, a23,  4, m)
   tr32swap(a24, a28,  4, m)
   tr32swap(a25, a29,  4, m)
   tr32swap(a26, a30,  4, m)
   tr32swap(a27, a31,  4, m)

   m = 0x33333333;
   tr32swap(a0,  a2,   2, m)
   tr32swap(a1,  a3,   2, m)
   tr32swap(a4,  a6,   2, m)
   tr32swap(a5,  a7,   2, m)
   tr32swap(a8,  a10,  2, m)
   tr32swap(a9,  a11,  2, m)
   tr32swap(a12, a14,  2, m)
   tr32swap(a13, a15,  2, m)
   tr32swap(a16, a18,  2, m)
   tr32swap(a17, a19,  2, m)
   tr32swap(a20, a22,  2, m)
   tr32swap(a21, a23,  2, m)
   tr32swap(a24, a26,  2, m)
   tr32swap(a25, a27,  2, m)
   tr32swap(a28, a30,  2, m)
   tr32swap(a29, a31,  2, m)

   m = 0x55555555;
   tr32swap(a0,  a1,   1, m)
   tr32swap(a2,  a3,   1, m)
   tr32swap(a4,  a5,   1, m)
   tr32swap(a6,  a7,   1, m)
   tr32swap(a8,  a9,   1, m)
   tr32swap(a10, a11,  1, m)
   tr32swap(a12, a13,  1, m)
   tr32swap(a14, a15,  1, m)
   tr32swap(a16, a17,  1, m)
   tr32swap(a18, a19,  1, m)
   tr32swap(a20, a21,  1, m)
   tr32swap(a22, a23,  1, m)
   tr32swap(a24, a25,  1, m)
   tr32swap(a26, a27,  1, m)
   tr32swap(a28, a29,  1, m)
   tr32swap(a30, a31,  1, m)

   dst[ 0] = a0;   dst[ 1] = a1;   dst[ 2] = a2;   dst[ 3] = a3;
   dst[ 4] = a4;   dst[ 5] = a5;   dst[ 6] = a6;   dst[ 7] = a7;
   dst[ 8] = a8;   dst[ 9] = a9;   dst[10] = a10;  dst[11] = a11;
   dst[12] = a12;  dst[13] = a13;  dst[14] = a14;  dst[15] = a15;
   dst[16] = a16;  dst[17] = a17;  dst[18] = a18;  dst[19] = a19;
   dst[20] = a20;  dst[21] = a21;  dst[22] = a22;  dst[23] = a23;
   dst[24] = a24;  dst[25] = a25;  dst[26] = a26;  dst[27] = a27;
   dst[28] = a28;  dst[29] = a29;  dst[30] = a30;  dst[31] = a31;
}
/////////////////////////////////////////////////////////////////////////////

