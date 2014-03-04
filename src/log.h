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
	VB_GENERIC     = 0x00000001,
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
	VB_ALL         = 0x7FFFFFFF,
	VB_MOST        = 0x7FFFFFFB
} LogFacility;

typedef enum {
	LOG_ERR      = 1,
	LOG_WARN,
	LOG_INFO,
	LOG_DEBUG
} LogLevel;

extern int logLevel;
extern int logMask;

#define LogErr(facility, format, args...)   _LogWrite(__FILE__, __LINE__, LOG_ERR, facility, format, ## args)
#define LogInfo(facility, format, args...)  _LogWrite(__FILE__, __LINE__, LOG_INFO, facility, format, ## args)
#define LogWarn(facility, format, args...)  _LogWrite(__FILE__, __LINE__, LOG_WARN, facility, format, ## args)
#define LogDebug(facility, format, args...) _LogWrite(__FILE__, __LINE__, LOG_DEBUG, facility, format, ## args)

// Legacy Logging macro, logs at Generic Info level
#define LogWrite(format, args...) _LogWrite(__FILE__, __LINE__, LOG_INFO, VB_GENERIC, format, ## args)

void _LogWrite(char *file, int line, int level, int facility, const char *format, ...);

#endif //__LOG_H__
