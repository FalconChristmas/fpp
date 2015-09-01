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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>

#include <sstream>

#include "common.h"
#include "log.h"

/*
 * Get the current time down to the microsecond
 */
long long GetTime(void)
{
	struct timeval now_tv;
	gettimeofday(&now_tv, NULL);
	return now_tv.tv_sec * 1000000LL + now_tv.tv_usec;
}

/*
 * Check to see if the specified directory exists
 */
int DirectoryExists(const char * Directory)
{
	DIR* dir = opendir(Directory);
	if (dir)
	{
		closedir(dir);
		return 1;
	}
	else
	{
		return 0;
	}
}

/*
 * Check if the specified file exists or not
 */
int FileExists(const char * File)
{
	struct stat sts;
	if (stat(File, &sts) == -1 && errno == ENOENT)
	{
		return 0;
	}
	else
	{
		return 1;
	}
	
}

/*
 * Dump memory block in hex and human-readable formats
 */
void HexDump(char *title, void *data, int len) {
	int l = 0;
	int i = 0;
	int x = 0;
	int y = 0;
	unsigned char *ch = (unsigned char *)data;
	unsigned char str[17];
	char tmpStr[90];

	sprintf( tmpStr, "%s: (%d bytes)\n", title, len);
	LogInfo(VB_ALL, tmpStr);

	while (l < len) {
		if ( x == 0 ) {
			sprintf( tmpStr, "%06x: ", i );
		}

		if ( x < 16 ) {
			sprintf( tmpStr + strlen(tmpStr), "%02x ", *ch & 0xFF );
			str[x] = *ch;
			x++;
			i++;
		} else {
			sprintf( tmpStr + strlen(tmpStr), "   " );
			for( ; x > 0 ; x-- ) {
				if ( isgraph( str[16-x] ) || str[16-x] == ' ' ) {
					sprintf( tmpStr + strlen(tmpStr), "%c", str[16-x]);
				} else {
					sprintf( tmpStr + strlen(tmpStr), "." );
				}
			}

			sprintf( tmpStr + strlen(tmpStr), "\n" );
			LogInfo(VB_ALL, tmpStr);
			x = 0;

			sprintf( tmpStr, "%06x: ", i );
			sprintf( tmpStr + strlen(tmpStr), "%02x ", *ch & 0xFF );
			str[x] = *ch;
			x++;
			i++;
		}

		l++;
		ch++;
	}
	for( y = x; y < 16 ; y++ ) {
		sprintf( tmpStr + strlen(tmpStr), "   " );
	}
	sprintf( tmpStr + strlen(tmpStr), "   " );
	for( y = 0; y < x ; y++ ) {
		if ( isgraph( str[y] ) || str[y] == ' ' ) {
			sprintf( tmpStr + strlen(tmpStr), "%c", str[y]);
		} else {
			sprintf( tmpStr + strlen(tmpStr), "." );
		}
	}

	sprintf( tmpStr + strlen(tmpStr), "\n" );
	LogInfo(VB_ALL, tmpStr);
}

/*
 * Get IP address on specified network interface
 */
int GetInterfaceAddress(char *interface, char *addr, char *mask, char *gw)
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;

	/* I want IP address attached to E131interface */
	strncpy(ifr.ifr_name, (const char *)interface, IFNAMSIZ-1);

	if (addr)
	{
		if (ioctl(fd, SIOCGIFADDR, &ifr))
			strcpy(addr, "127.0.0.1");
		else
			strcpy(addr, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	}

	if (mask)
	{
		if (ioctl(fd, SIOCGIFNETMASK, &ifr))
			strcpy(mask, "255.255.255.255");
		else
			strcpy(mask, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	}

	if (gw)
	{
		FILE *f;
		char  line[100];
		char *iface;
		char *dest;
		char *route;
		char *saveptr;

		f = fopen("/proc/net/route", "r");

		if (f)
		{
			while (fgets(line, 100, f))
			{
				iface = strtok_r(line, " \t", &saveptr);
				dest  = strtok_r(NULL, " \t", &saveptr);
				route = strtok_r(NULL, " \t", &saveptr);

				if ((iface && dest && route) &&
					(!strcmp(iface, interface)) &&
					(!strcmp(dest, "00000000")))
				{
					char *end;
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
char *FindInterfaceForIP(char *ip)
{
	struct ifaddrs *ifaddr, *ifa;
	int family, s, n;
	char host[NI_MAXHOST];
	char interfaceIP[16];
	static char interface[10] = "";

	if (getifaddrs(&ifaddr) == -1)
	{
		LogErr(VB_SETTING, "Error getting interfaces list: %s\n",
			strerror(errno));
		return interface;
	}

	for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++)
	{
		if (ifa->ifa_addr == NULL)
			continue;

		if (ifa->ifa_addr->sa_family == AF_INET)
		{
			s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
							host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if (s != 0)
			{
				LogErr(VB_SETTING, "getnameinfo failed");
				freeifaddrs(ifaddr);
				return interface;
			}

			if (!strcmp(host, ip))
			{
				strcpy(interface, ifa->ifa_name);
				freeifaddrs(ifaddr);
				return interface;
			}
		}
	}

	return interface;
}

/*
 *
 */
int CheckForHostSpecificFile(const char *hostname, char *filename)
{
	char localFilename[2048];
	strcpy(localFilename, filename);

	char ext[6];
	char *ptr = 0;
	int len = strlen(localFilename);

	// Check for 3 or 4-digit extension
	if (localFilename[len - 4] == '.')
	    ptr = &localFilename[len - 4];
	else if (localFilename[len - 5] == '.')
	    ptr = &localFilename[len - 5];

	if (ptr)
	{
		// Preserve the extension including the dot
		strcpy(ext, ptr);

		*ptr = 0;
		strcat(ptr, "-");
		strcat(ptr, hostname);
		strcat(ptr, ext);

		if (FileExists(localFilename))
		{
			LogDebug(VB_SEQUENCE, "Found %s to use instead of %s\n",
				localFilename, filename);
			strcpy(filename, localFilename);
		}
		else
		{
			// Replace hyphen with an underscore and recheck
			*ptr = '_';
			if (FileExists(localFilename))
			{
				LogDebug(VB_SEQUENCE, "Found %s to use instead of %s\n",
					localFilename, filename);
				strcpy(filename, localFilename);
			}
		}
	}
}

/*
 * Convert a string of the form "YYYY-MM-DD to an integer YYYYMMDD
 */
int DateStrToInt(const char *str)
{
	if ((!str) || (str[4] != '-') || (str[7] != '-') || (str[10] != 0x0))
		return 0;

	int result = 0;
	char tmpStr[11];

	strcpy(tmpStr, str);

	result += atoi(str    ) * 10000; // Year
	result += atoi(str + 5) *   100; // Month
	result += atoi(str + 8)        ; // Day

	return result;
}

/*
 * Get the current date in an integer form YYYYMMDD
 */
int GetCurrentDateInt(int daysOffset)
{
	time_t currTime = time(NULL) + (daysOffset * 86400);
	struct tm now;
	int result = 0;
	
	localtime_r(&currTime, &now);

	result += (now.tm_year + 1900) * 10000;
	result += (now.tm_mon + 1)     *   100;
	result += (now.tm_mday)               ;

	return result;
}

/*
 * Check to see if current date int is in the range specified
 */
int CurrentDateInRange(int startDate, int endDate)
{
	int currentDate = GetCurrentDateInt();

	if ((startDate < 10000) || (endDate < 10000))
	{
		startDate = startDate % 10000;
		endDate = endDate % 10000;
		currentDate = currentDate % 10000;
	}

	if ((startDate < 100) || (endDate < 100))
	{
		startDate = startDate % 100;
		endDate = endDate % 100;
		currentDate = currentDate % 100;
	}

	if ((startDate == 0) && (endDate == 0))
		return 1;

	if ((startDate <= currentDate) && (currentDate <= endDate))
		return 1;

	return 0;
}

std::string tail(std::string const& source, size_t const length)
{
	if (length >= source.size())
		return source;

	return source.substr(source.size() - length);
}

/*
 * Helpers to split a string on the specified character delimiter
 */
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }

    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;

    split(s, delim, elems);

    return elems;
}

