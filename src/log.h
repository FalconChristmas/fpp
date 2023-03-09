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

#include <string>
#include <vector>

/* 
 * Define the allowed levels of Logging
 */
typedef enum {
    LOG_ERR = 1,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
    LOG_EXCESSIVE
} LogLevel;

/* 
 * A Logger Instance. 
 * In the future, this could be adjusted to allow different Instances
 * to go to different files
 */

class FPPLoggerInstance {
public:
    FPPLoggerInstance(std::string name) { this->name = name; }
    LogLevel level = LOG_INFO;
    std::string name;
};

/*
 * Class to hold all the Instances
 */
class FPPLogger {
public:
    static FPPLogger INSTANCE;
    const std::vector<FPPLoggerInstance*>& allInstances() { return all; }
    void Init();
    bool SetLevel(std::string name, LogLevel level);
    bool SetLevel(const char* name, const char* level);
    void SetAllLevel(LogLevel level);
    int MinimumLogLevel();
    std::string GetLogLevelString();
    FPPLoggerInstance General = FPPLoggerInstance("General");
    FPPLoggerInstance ChannelOut = FPPLoggerInstance("ChannelOut");
    FPPLoggerInstance ChannelData = FPPLoggerInstance("ChannelData");
    FPPLoggerInstance Command = FPPLoggerInstance("Command");
    FPPLoggerInstance E131Bridge = FPPLoggerInstance("E131Bridge");
    FPPLoggerInstance Effect = FPPLoggerInstance("Effect");
    FPPLoggerInstance MediaOut = FPPLoggerInstance("MediaOut");
    FPPLoggerInstance Playlist = FPPLoggerInstance("Playlist");
    FPPLoggerInstance Schedule = FPPLoggerInstance("Schedule");
    FPPLoggerInstance Sequence = FPPLoggerInstance("Sequence");
    FPPLoggerInstance Settings = FPPLoggerInstance("Settings");
    FPPLoggerInstance Control = FPPLoggerInstance("Control");
    FPPLoggerInstance Sync = FPPLoggerInstance("Sync");
    ;
    FPPLoggerInstance Plugin = FPPLoggerInstance("Plugin");
    FPPLoggerInstance GPIO = FPPLoggerInstance("GPIO");
    FPPLoggerInstance HTTP = FPPLoggerInstance("HTTP");

private:
    std::vector<FPPLoggerInstance*> all;
};

#define VB_GENERAL FPPLogger::INSTANCE.General
#define VB_ALL FPPLogger::INSTANCE.General /* Over Load */
#define VB_CHANNELOUT FPPLogger::INSTANCE.ChannelOut
#define VB_CHANNELDATA FPPLogger::INSTANCE.ChannelData
#define VB_COMMAND FPPLogger::INSTANCE.Command
#define VB_E131BRIDGE FPPLogger::INSTANCE.E131Bridge
#define VB_EFFECT FPPLogger::INSTANCE.Effect
#define VB_MEDIAOUT FPPLogger::INSTANCE.MediaOut
#define VB_PLAYLIST FPPLogger::INSTANCE.Playlist
#define VB_SCHEDULE FPPLogger::INSTANCE.Schedule
#define VB_SEQUENCE FPPLogger::INSTANCE.Sequence
#define VB_SETTING FPPLogger::INSTANCE.Settings
#define VB_CONTROL FPPLogger::INSTANCE.Control
#define VB_SYNC FPPLogger::INSTANCE.Sync
#define VB_PLUGIN FPPLogger::INSTANCE.Plugin
#define VB_GPIO FPPLogger::INSTANCE.GPIO
#define VB_HTTP FPPLogger::INSTANCE.HTTP

#define EXCESSIVE_LOG_LEVEL_WARNING "A Log Level is set to Excessive"
#define DEBUG_LOG_LEVEL_WARNING "A Log Level is set to Debug"

//extern int logLevel;
//extern char logLevelStr[16];
//extern int logMask;
//extern char logMaskStr[1024];

#define LogErr(facility, format, args...) _LogWrite(__FILE__, __LINE__, LOG_ERR, facility, format, ##args)
#define LogInfo(facility, format, args...) _LogWrite(__FILE__, __LINE__, LOG_INFO, facility, format, ##args)
#define LogWarn(facility, format, args...) _LogWrite(__FILE__, __LINE__, LOG_WARN, facility, format, ##args)
#define LogDebug(facility, format, args...) _LogWrite(__FILE__, __LINE__, LOG_DEBUG, facility, format, ##args)
#define LogExcess(facility, format, args...) _LogWrite(__FILE__, __LINE__, LOG_EXCESSIVE, facility, format, ##args)

void _LogWrite(const char* file, int line, int level, FPPLoggerInstance& facility, const char* format, ...);
void _LogWrite(const char* file, int line, int level, FPPLoggerInstance& facility, const std::string &str, ...);
bool WillLog(int level, FPPLoggerInstance& facility);

void SetLogFile(const char* filename, bool toStdOut = true);
int loggingToFile(void);
void logVersionInfo(void);

bool SetLogLevel(const char* newLevel);
bool SetLogLevelComplex(std::string& input);  /*parse debug:schedule,player;excess:mqtt*/
bool SetLogLevelComplex(const char* input);   /*parse debug:schedule,player;excess:mqtt*/
std::string LogLevelToString(LogLevel level); /* Convert Level to String Equivilanent */
