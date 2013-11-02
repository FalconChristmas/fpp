#ifndef _E131_H
#define _E131_H


#define MAX_UNIVERSE_COUNT    128
#define E131_HEADER_LENGTH    126
#define MAX_STEPS_OUT_OF_SYNC 2

#define E131_DEST_PORT				5568
#define E131_SOURCE_PORT			58301
			
#define E131_STATUS_IDLE      0
#define E131_STATUS_READING   1
#define E131_STATUS_EOF       2

#define E131_UNIVERSE_INDEX   113
#define E131_SEQUENCE_INDEX   111
#define E131_COUNT_INDEX   		123
#define STEP_SIZE_OFFSET      10
#define CHANNEL_DATA_OFFSET		28

#define E131_TYPE_MULTICAST		0
#define E131_TYPE_UNICAST			1


typedef struct{
		int active;
		int universe;
    int size;
    int startAddress;
		int type;
    char unicastAddress[16];
		unsigned long bytesReceived;
}UniverseEntry;

extern int E131status;
extern int E131sequenceFramesSent;

int IsSequenceRunning(void);
char * GetE131LocalAddressFromInterface();
void E131_Initialize();
int E131_InitializeNetwork();
int E131_OpenSequenceFile(const char * file);
void E131_CloseSequenceFile();
void E131_SetTimer(int us);
void E131_ReadData(void);
void E131_Send(void);
void E131_SendPixelnetDMXdata();
void LoadUniversesFromFile();
void UniversesPrint();
void ResetBytesReceived();
void WriteBytesReceivedFile();
void SendBlankingData(void);


#endif
