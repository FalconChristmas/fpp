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
 
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "common.h"

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
int DirectoryExists(char * Directory)
{
	DIR* dir = opendir(Directory);
	if (dir)
	{
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
int FileExists(char * File)
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

	fprintf( stdout, "%s: (%d bytes)\n", title, len);
	while (l < len) {
		if ( x == 0 ) {
			fprintf( stdout, "%06x: ", i );
		}

		if ( x < 16 ) {
			fprintf( stdout, "%02x ", *ch & 0xFF );
			str[x] = *ch;
			x++;
			i++;
		} else {
			fprintf( stdout, "   " );
			for( ; x > 0 ; x-- ) {
				if ( isgraph( str[16-x] ) || str[16-x] == ' ' ) {
					fprintf( stdout, "%c", str[16-x]);
				} else {
					fprintf( stdout, "." );
				}
			}

			fprintf( stdout, "\n" );
			x = 0;

			fprintf( stdout, "%06x: ", i );
			fprintf( stdout, "%02x ", *ch & 0xFF );
			str[x] = *ch;
			x++;
			i++;
		}

		l++;
		ch++;
	}
	for( y = x; y < 16 ; y++ ) {
		fprintf( stdout, "   " );
	}
	fprintf( stdout, "   " );
	for( y = 0; y < x ; y++ ) {
		if ( isgraph( str[y] ) || str[y] == ' ' ) {
			fprintf( stdout, "%c", str[y]);
		} else {
			fprintf( stdout, "." );
		}
	}

	fprintf( stdout, "\n" );
}

/*
 * Get IP address on specified network interface
 */
char *GetInterfaceAddress(char *interface) {
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;

	/* I want IP address attached to E131interface */
	strncpy(ifr.ifr_name, (const char *)interface, IFNAMSIZ-1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);

	/* return duplicate of result */
	return strdup(inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
}

