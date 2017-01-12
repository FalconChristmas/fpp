/*
 *   Common functions for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#include <vector>
#include <string>

#include <jsoncpp/json/json.h>

long long GetTime(void);
int       DirectoryExists(const char * Directory);
int       FileExists(const char * File);
void      HexDump(char *title, void *data, int len);
int       GetInterfaceAddress(char *interface, char *addr, char *mask, char *gw);
char     *FindInterfaceForIP(char *ip);
int       CheckForHostSpecificFile(const char *hostname, char *filename);
int       DateStrToInt(const char *str);
int       GetCurrentDateInt(int daysOffset = 0);
int       CurrentDateInRange(int startDate, int endDate);

void      MergeJsonValues(Json::Value &a, Json::Value &b);

std::string tail(std::string const& source, size_t const length);
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
std::vector<std::string> split(const std::string &s, char delim);

#endif
