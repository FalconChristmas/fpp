/*
 *   Log handler for Falcon Player (FPP)
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
 
#ifndef __LOG_H__
#define __LOG_H__

///////////////////////////////////////////////////////////////////////////////
// LogFacility is a mask allowing multiple log facilities on at the same time.
///////////////////////////////////////////////////////////////////////////////
// WARNING: Do not update this enum by placing new items in the middle unless
//          you also update the VB_MOST mask at the end.  The default of
//          VB_MOST does not include VB_CHANNELDATA which can be very verbose.
///////////////////////////////////////////////////////////////////////////////
typedef enum {
	VB_NONE        = 0x00000000,
	VB_GENERAL     = 0x00000001,
	VB_CHANNELOUT  = 0x00000002,
	VB_CHANNELDATA = 0x00000004,
	VB_COMMAND     = 0x00000008,
	VB_E131BRIDGE  = 0x00000010,
	VB_EFFECT      = 0x00000020,
	VB_EVENT       = 0x00000040,
	VB_MEDIAOUT    = 0x00000080,
	VB_PLAYLIST    = 0x00000100,
	VB_SCHEDULE    = 0x00000200,
	VB_SEQUENCE    = 0x00000400,
	VB_SETTING     = 0x00000800,
	VB_CONTROL     = 0x00001000,
	VB_SYNC        = 0x00002000,
	VB_PLUGIN      = 0x00004000,
	VB_GPIO        = 0x00008000,
	VB_HTTP        = 0x00010000,
	VB_ALL         = 0x7FFFFFFF,
	VB_MOST        = 0x7FFFFFFB
} LogFacility;

typedef enum {
	LOG_ERR      = 1,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG,
	LOG_EXCESSIVE
} LogLevel;

extern int logLevel;
extern char logLevelStr[16];
extern int logMask;
extern char logMaskStr[1024];

#define LogErr(facility, format, args...)   _LogWrite(__FILE__, __LINE__, LOG_ERR, facility, format, ## args)
#define LogInfo(facility, format, args...)  _LogWrite(__FILE__, __LINE__, LOG_INFO, facility, format, ## args)
#define LogWarn(facility, format, args...)  _LogWrite(__FILE__, __LINE__, LOG_WARN, facility, format, ## args)
#define LogDebug(facility, format, args...) _LogWrite(__FILE__, __LINE__, LOG_DEBUG, facility, format, ## args)
#define LogExcess(facility, format, args...) _LogWrite(__FILE__, __LINE__, LOG_EXCESSIVE, facility, format, ## args)

void _LogWrite(const char *file, int line, int level, int facility, const char *format, ...);
bool WillLog(int level, int facility);

void SetLogFile(const char *filename, bool toStdOut = true);
int SetLogLevel(const char *newLevel);
int SetLogMask(const char *newMask);
int loggingToFile(void);
void logVersionInfo(void);


#define LogMaskIsSet(x)  (logMask & x)
#define LogLevelIsSet(x) (logLevel >= x)

#endif //__LOG_H__
