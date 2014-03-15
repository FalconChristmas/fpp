#ifndef _MEMORYMAPCONTROL_H
#define _MEMORYMAPCONTROL_H

#define FPPCHANNELMEMORYMAPMAJORVER 1
#define FPPCHANNELMEMORYMAPMINORVER 0
#define FPPCHANNELMEMORYMAPSIZE     65536

#define FPPCHANNELMEMORYMAPDATAFILE "/tmp/FPPChannelData"
#define FPPCHANNELMEMORYMAPCTRLFILE "/tmp/FPPChannelCtrl"

/*
 * Header block on channel data memory map control interface file.
 * We want the size of this to equal 256 bytes so we have room for
 * expansion and to allow the header and up to 255 map block definitions
 * in a 64K memory mapped file.
 */
typedef struct {
    unsigned char   majorVersion;   // major version of memory map layout
	unsigned char   minorVersion;   // minor version of memory map layout
	unsigned char   totalBlocks;    // number of blocks defined in config file
	unsigned char   testMode;       // 0/1, read by fppd, 1 == copy all channels
	unsigned char   filler[252];    // filler for future use
} FPPChannelMemoryMapControlHeader;

typedef struct {
	unsigned char   isActive;       // 0/1 set by client, read by fppd
	char            blockName[32];  // null-terminated string, set by fppd
	long            startChannel;   // 1-based start channel
	long            channelCount;   // number of channels to copy
	unsigned char   filler[215];    // filler for future use
} FPPChannelMemoryMapControlBlock;

#endif /* _MEMORYMAPCONTROL_H */
