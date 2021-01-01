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

#include "fpp-pch.h"

#include <stdbool.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>

//int logLevel = LOG_INFO;
//int logMask  = VB_MOST;

//Create Global Instance
FPPLogger FPPLogger::INSTANCE = FPPLogger();

char logFileName[1024] = "";
bool logToStdOut = true;
//char logLevelStr[16];
//char logMaskStr[1024];

void FPPLogger::Init() {
   if (all.empty()) {
      all.push_back(&(FPPLogger::General));
      all.push_back(&(FPPLogger::ChannelOut));
      all.push_back(&(FPPLogger::ChannelData));
      all.push_back(&(FPPLogger::Command));
      all.push_back(&(FPPLogger::E131Bridge));
      all.push_back(&(FPPLogger::Effect));
      all.push_back(&(FPPLogger::Event));
      all.push_back(&(FPPLogger::MediaOut));
      all.push_back(&(FPPLogger::Playlist));
      all.push_back(&(FPPLogger::Schedule));
      all.push_back(&(FPPLogger::Sequence));
      all.push_back(&(FPPLogger::Settings));
      all.push_back(&(FPPLogger::Control));
      all.push_back(&(FPPLogger::Sync));
      all.push_back(&(FPPLogger::Plugin));
      all.push_back(&(FPPLogger::GPIO));
      all.push_back(&(FPPLogger::HTTP));
   }
}

bool FPPLogger::SetLevel(char *name, char *level) {
   LogLevel newLevel = LOG_ERR;
   bool found = false;

   if (strcmp(level, "error") == 0) {
      newLevel = LOG_ERR;
      found = true;
   } else if (strcmp(level, "warn") == 0) {
      newLevel = LOG_WARN;
      found = true;
   } else if (strcmp(level, "info") == 0) {
      newLevel = LOG_INFO;
      found = true;
   } else if (strcmp(level, "debug") == 0) {
      newLevel = LOG_DEBUG;
      found = true;
   } else if (strcmp(level, "excess") == 0) {
      newLevel = LOG_EXCESSIVE;
      found = true;
   } 

   if (!found) {
      LogErr(VB_SETTING, "Invalid name for LogLevel: %s\n", level);
      return false;
   } else {
      return SetLevel(std::string(name), newLevel);
   }

}

bool FPPLogger::SetLevel(std::string name, LogLevel level) {
   bool madeChange = false;
   std::string prefix("LogLevel_");
   if (name.compare(0, prefix.size(), prefix) == 0) {
      name = name.substr(prefix.size());
   }

   std::vector<FPPLoggerInstance*>::iterator it;
   Init(); /* Just incase */

   for (it = all.begin(); it != all.end(); it++) {
      FPPLoggerInstance *ptr = *it;
      if (name == ptr->name) {
         ptr->level = level;
	 madeChange = true;
      }
   }
   if (! madeChange)
      LogWarn(VB_SETTING, "Invalid name for LogLevel: %s\n", name.c_str());

   return madeChange;
}

int FPPLogger::MinimumLogLevel() {
   int rc = LOG_ERR;
   Init(); /* Just incase */
   std::vector<FPPLoggerInstance*>::iterator it;

   for (it = all.begin(); it != all.end(); it++) {
      int level = (*it)->level;
      if (level > rc) {
         rc = level;
      }
   }

   return rc;
}

void FPPLogger::SetAllLevel(LogLevel level) {
   std::vector<FPPLoggerInstance*>::iterator it;
   Init(); /* Just incase */

   for (it = all.begin(); it != all.end(); it++) {
      (*it)->level = level;
   }
}


bool WillLog(int level, FPPLoggerInstance &facility) {

    // Don't log if we're not concerned about anything at this level
    if (facility.level < level)
        return false;
    
    return true;
}

void _LogWrite(const char *file, int line, int level, FPPLoggerInstance &facility, const char *format, ...)
{
	// Don't log if we're not concerned about anything at this level
	if (!(WillLog(level, facility)))
		return;

	va_list arg;
    
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    
	struct tm tm;
	char timeStr[512];

	localtime_r(&tv.tv_sec, &tm);
    int ms = tv.tv_usec / 1000;
    
	int size = snprintf(timeStr, sizeof(timeStr),
                    "%4d-%.2d-%.2d %.2d:%.2d:%.2d.%.3d (%ld) %s:%d [%s]: %s",
					1900+tm.tm_year,
					tm.tm_mon+1,
					tm.tm_mday,
					tm.tm_hour,
					tm.tm_min,
					tm.tm_sec,
                    ms,
                    syscall(SYS_gettid), file, line, facility.name.c_str(), format);

	if (logFileName[0]) {
		FILE *logFile;

        bool close = true;
		if (!strcmp(logFileName, "stderr")) {
			logFile = stderr;
            close = false;
		} else if (!strcmp(logFileName, "stdout")) {
			logFile = stdout;
            close = false;
        } else {
			logFile = fopen(logFileName, "a");
			if ( ! logFile ) {
				fprintf(stderr, "Error: Unable to open log file for writing!\n");
				va_start(arg, format);
				vfprintf(stderr, timeStr, arg);
				va_end(arg);
				return;
			}
		}

		va_start(arg, format);
		vfprintf(logFile, timeStr, arg);
		va_end(arg);

        if (close)  {
			fclose(logFile);
        }
	}
    if (strcmp(logFileName, "stdout") && logToStdOut) {
		va_start(arg, format);
		vfprintf(stdout, timeStr, arg);
		va_end(arg);
	}
}

void SetLogFile(const char *filename, bool toStdOut)
{
	strcpy(logFileName, filename);
    logToStdOut = toStdOut;
}

std::string LogLevelToString(LogLevel level) {
   if (level == LOG_WARN) {
      return "warn";
   } else if (level == LOG_DEBUG) {
      return "debug";
   } else if (level == LOG_INFO) {
      return "info";
   } else if (level == LOG_EXCESSIVE) {
      return "excess";
   } else {
      return "unknown";
   }
}

int SetLogLevel(const char *newLevel)
{
	LogErr(VB_SETTING, "Using Legacy Global Logging to set all to same Log Level of %s\n", newLevel);
	if (!strcmp(newLevel, "warn")) {
		FPPLogger::INSTANCE.SetAllLevel(LOG_WARN);
	} else if (!strcmp(newLevel, "debug")) {
		FPPLogger::INSTANCE.SetAllLevel(LOG_DEBUG);
	} else if (!strcmp(newLevel, "info")) {
		FPPLogger::INSTANCE.SetAllLevel(LOG_INFO);
	} else if (!strcmp(newLevel, "excess")) {
		FPPLogger::INSTANCE.SetAllLevel(LOG_EXCESSIVE);
	} else {
		LogErr(VB_SETTING, "Unknown Log Level: %s\n", newLevel);
		return 0;
	}

	return 1;
}

int loggingToFile(void)
{
	if ((logFileName[0]) &&
		(strcmp(logFileName, "stderr")) &&
		(strcmp(logFileName, "stdout")))
		return 1;

	return 0;
}

void logVersionInfo(void) {
	LogErr(VB_ALL, "=========================================\n");
	LogErr(VB_ALL, "FPP %s\n", getFPPVersion());
	LogErr(VB_ALL, "Branch: %s\n", getFPPBranch());
	LogErr(VB_ALL, "=========================================\n");
}

