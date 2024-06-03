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
#include <list>
#include <string>
#include <vector>

#include "common_mini.h"
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
void HexDump(const char* title, const void* data, int len, FPPLoggerInstance& facility, int perLine = 16);
int CheckForHostSpecificFile(const std::string& hostname, std::string& filename);
int CheckForHostSpecificFile(const char* hostname, char* filename);
std::string ReplaceKeywords(std::string str, std::map<std::string, std::string>& keywords);
char* FindInterfaceForIP(char* ip);

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

std::string getSimpleHTMLTTag(const std::string& html, const std::string& searchStr, const std::string& skipStr, const std::string& endStr);
std::string getSimpleXMLTag(const std::string& xml, const std::string& tag);

// URL Helpers
bool urlHelper(const std::string method, const std::string& url, const std::string& data, std::string& resp, const std::list<std::string>& headers, const unsigned int timeout = 30);
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

std::string getPlatform();

inline std::string toStdStringAndFree(char* v) {
    std::string s = v;
    free(v);
    return s;
}

void SetThreadName(const std::string& name);