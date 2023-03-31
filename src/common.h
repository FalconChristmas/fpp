#pragma once
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

#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "log.h"

/////////////////////////////////////////////////////////////////////////////
// printf macros for printing bitmaps
//
// Macros from https://stackoverflow.com/a/25108449

#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i) \
    (((i)&0x80ll) ? '1' : '0'),       \
        (((i)&0x40ll) ? '1' : '0'),   \
        (((i)&0x20ll) ? '1' : '0'),   \
        (((i)&0x10ll) ? '1' : '0'),   \
        (((i)&0x08ll) ? '1' : '0'),   \
        (((i)&0x04ll) ? '1' : '0'),   \
        (((i)&0x02ll) ? '1' : '0'),   \
        (((i)&0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8 PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8), PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16 PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64 \
    PRINTF_BINARY_PATTERN_INT32 PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)

/////////////////////////////////////////////////////////////////////////////

long long GetTime(void);
long long GetTimeMicros(void);
long long GetTimeMS(void);
std::string GetTimeStr(std::string fmt);
std::string GetDateStr(std::string fmt);

int DirectoryExists(const char* Directory);
int DirectoryExists(const std::string& Directory);
int FileExists(const char* File);
int FileExists(const std::string& File);
int Touch(const std::string& File);
void HexDump(const char* title, const void* data, int len, FPPLoggerInstance& facility, int perLine = 16);
int GetInterfaceAddress(const char* interface, char* addr, char* mask, char* gw);
char* FindInterfaceForIP(char* ip);
int CheckForHostSpecificFile(const char* hostname, char* filename);
int CheckForHostSpecificFile(const std::string& hostname, std::string& filename);
int DateStrToInt(const char* str);
int GetCurrentDateInt(int daysOffset = 0);
int CurrentDateInRange(int startDate, int endDate);
int DateInRange(time_t when, int startDate, int endDate);
int DateInRange(int currentDate, int startDate, int endDate);
void CloseOpenFiles(void);
std::string secondsToTime(int i);

std::string GetFileContents(const std::string& filename);
std::string GetFileExtension(const std::string& filename);

bool PutFileContents(const std::string& filename, const std::string& str);
void TrimWhiteSpace(std::string& s);

uint8_t ReverseBitsInByte(uint8_t n);

bool SetFilePerms(const std::string& filename, bool exBit = false);
bool SetFilePerms(const char* file, bool exBit = false);

void MergeJsonValues(Json::Value& a, Json::Value& b);
Json::Value LoadJsonFromFile(const std::string& filename);
Json::Value LoadJsonFromString(const std::string& str);
bool LoadJsonFromString(const std::string& str, Json::Value& root);
bool LoadJsonFromFile(const std::string& filename, Json::Value& root);
bool LoadJsonFromFile(const char* filename, Json::Value& root);
std::string SaveJsonToString(const Json::Value& root, const std::string& indentation = "");
bool SaveJsonToString(const Json::Value& root, std::string& str, const std::string& indentation);
bool SaveJsonToFile(const Json::Value& root, const std::string& filename, const std::string& indentation = "\t");
bool SaveJsonToFile(const Json::Value& root, const char* filename, const char* indentation = "\t");

std::string tail(std::string const& source, size_t const length);
std::vector<std::string>& split(const std::string& s, char delim, std::vector<std::string>& elems);
std::vector<std::string> split(const std::string& s, char delim);

// splits the string on , but also honors any double/single quotes so commas within strings are preserved
std::vector<std::string> splitWithQuotes(const std::string& s, char delim = ',');

bool startsWith(const std::string& str, const std::string& prefix);
bool endsWith(const std::string& str, const std::string& suffix);
bool contains(const std::string& str, const std::string& v);
void replaceAll(std::string& str, const std::string& from, const std::string& to);
bool replaceStart(std::string& str, const std::string& from, const std::string& to = "");
bool replaceEnd(std::string& str, const std::string& from, const std::string& to = "");
std::string ReplaceKeywords(std::string str, std::map<std::string, std::string>& keywords);
void toUpper(std::string& str);
void toLower(std::string& str);
std::string toUpperCopy(const std::string& str);
std::string toLowerCopy(const std::string& str);

std::string getSimpleHTMLTTag(const std::string& html, const std::string& searchStr, const std::string& skipStr, const std::string& endStr);
std::string getSimpleXMLTag(const std::string& xml, const std::string& tag);

// URL Helpers
bool urlHelper(const std::string method, const std::string& url, const std::string& data, std::string& resp, const unsigned int timeout = 30);
bool urlHelper(const std::string method, const std::string& url, std::string& resp, const unsigned int timeout = 30);
bool urlGet(const std::string url, std::string& resp);
bool urlPost(const std::string url, const std::string data, std::string& resp);
bool urlPut(const std::string url, const std::string data, std::string& resp);
bool urlDelete(const std::string url, const std::string data, std::string& resp);
bool urlDelete(const std::string url, std::string& resp);

std::string base64Encode(uint8_t const* bytes_to_encode, unsigned int in_len);
std::vector<uint8_t> base64Decode(std::string const& encodedString);

void ShutdownFPPD(bool restart = false);
void RegisterShutdownHandler(const std::function<void(bool)> hook);

void GetCurrentFPPDStatus(Json::Value& result);

inline std::string toStdStringAndFree(char* v) {
    std::string s = v;
    free(v);
    return s;
}
