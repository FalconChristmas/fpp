/*
 *   Common functions for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
 
#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>

#include <vector>
#include <string>


/////////////////////////////////////////////////////////////////////////////
// printf macros for printing bitmaps
//
// Macros from https://stackoverflow.com/a/25108449

#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16             PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64    \
    PRINTF_BINARY_PATTERN_INT32             PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)

/////////////////////////////////////////////////////////////////////////////


long long GetTime(void);
long long GetTimeMS(void);
int       DirectoryExists(const char * Directory);
int       FileExists(const char * File);
int       FileExists(const std::string &File);
int       Touch(const std::string &File);
void      HexDump(const char *title, const void *data, int len);
int       GetInterfaceAddress(const char *interface, char *addr, char *mask, char *gw);
char     *FindInterfaceForIP(char *ip);
int       CheckForHostSpecificFile(const char *hostname, char *filename);
int       CheckForHostSpecificFile(const std::string &hostname, std::string &filename);
int       DateStrToInt(const char *str);
int       GetCurrentDateInt(int daysOffset = 0);
int       CurrentDateInRange(int startDate, int endDate);
void      CloseOpenFiles(void);

std::string GetFileContents(const std::string &filename);
void TrimWhiteSpace(std::string &s);

uint8_t   ReverseBitsInByte(uint8_t n);

bool SetFilePerms(const std::string &filename);
bool SetFilePerms(const char *file);

#ifndef PLATFORM_OSX
#include <jsoncpp/json/json.h>
void      MergeJsonValues(Json::Value &a, Json::Value &b);
Json::Value LoadJsonFromFile(const std::string &filename);
Json::Value LoadJsonFromString(const std::string &str);
bool LoadJsonFromString(const std::string &str, Json::Value &root);
bool LoadJsonFromFile(const std::string &filename, Json::Value &root);
bool LoadJsonFromFile(const char *filename, Json::Value &root);
std::string SaveJsonToString(const Json::Value &root, const std::string &indentation = "");
bool SaveJsonToString(const Json::Value &root, std::string &str, const std::string &indentation);
bool SaveJsonToFile(const Json::Value &root, const std::string &filename, const std::string &indentation = "\t");
bool SaveJsonToFile(const Json::Value &root, const char *filename, const char *indentation = "\t");
#endif

std::string tail(std::string const& source, size_t const length);
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
std::vector<std::string> split(const std::string &s, char delim);

// splits the string on , but also honors any double/single quotes so commas within strings are preserved
std::vector<std::string> splitWithQuotes(const std::string &s, char delim = ',');

bool startsWith(const std::string &str, const std::string &prefix);
bool endsWith(const std::string& str, const std::string& suffix);
bool contains(const std::string &str, const std::string &v);
void replaceAll(std::string& str, const std::string& from, const std::string& to);
bool replaceStart(std::string& str, const std::string& from, const std::string& to = "");
bool replaceEnd(std::string& str, const std::string& from, const std::string& to = "");
void toUpper(std::string& str);
void toLower(std::string& str);
std::string toUpperCopy(const std::string& str);
std::string toLowerCopy(const std::string& str);


// URL Helpers
bool urlGet(const std::string url, std::string &resp);
bool urlPost(const std::string url, const std::string data, std::string &resp);
bool urlPut(const std::string url, const std::string data, std::string &resp);
bool urlDelete(const std::string url, const std::string data, std::string &resp);
bool urlDelete(const std::string url, std::string &resp);

#endif
