#ifndef _E131_H
#define _E131_H

#include "channeloutput.h"

#define MAX_UNIVERSE_COUNT    128
#define E131_HEADER_LENGTH    126
#define MAX_STEPS_OUT_OF_SYNC 2

#define E131_DEST_PORT        5568
#define E131_SOURCE_PORT      58301

#define E131_UNIVERSE_INDEX   113
#define E131_SEQUENCE_INDEX   111
#define E131_COUNT_INDEX      123

#define E131_TYPE_MULTICAST   0
#define E131_TYPE_UNICAST     1

typedef struct {
	int           active;
	int           universe;
	int           size;
	int           startAddress;
	int           type;
	char          unicastAddress[16];
	unsigned long bytesReceived;
} UniverseEntry;

// FIXME, these should be in e131bridge.c, not here
void  ResetBytesReceived();
void  WriteBytesReceivedFile();

/* Expose our interface */
extern FPPChannelOutput E131Output;

#endif
