#ifndef _CHANNELOUTPUT_H
#define _CHANNELOUTPUT_H

#include <pthread.h>

#define FPPD_MAX_CHANNEL_OUTPUTS   64

typedef struct fppChannelOutput {
	unsigned int     maxChannels;

	int              (*open)(char *device, void **privDataPtr);
	int              (*close)(void *data);
	int              (*isConfigured)(void);
	int              (*isActive)(void *data);
	int              (*send)(void *data, char *channelData, int channelCount);
} FPPChannelOutput;

typedef struct fppChannelOutputInstance {
	unsigned int      startChannel;
	unsigned int      channelCount;
	FPPChannelOutput *output;
	void             *privData;
} FPPChannelOutputInstance;

extern char            channelData[];
extern pthread_mutex_t channelDataLock;
extern unsigned long   channelOutputFrame;


int  InitializeChannelOutputs(void);
int  SendChannelData(char *channelData);
int  CloseChannelOutputs(void);
void ResetChannelOutputFrameNumber(void);
int  LoadChannelRemapData(void);

#endif /* _CHANNELOUTPUT_H */
