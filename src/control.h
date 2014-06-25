#ifndef _CONTROL_H
#define _CONTROL_H

#include <sys/types.h>

#define FPP_CTRL_ADDR "239.255.50.69"
#define FPP_CTRL_PORT 32320

#define CTRL_PKT_CMD   0
#define CTRL_PKT_SYNC  1

typedef struct __attribute__((packed)) {
	char     fppd[4];        // 'FPPD'
	uint8_t  pktType;        // Control Packet Type
	uint16_t extraDataLen;   // Length of extra data following Control Packet
} ControlPkt;

typedef struct {
	char   command[1];       // Null-terminated Command to process
	                         // (data may continue past this header)
} CommandPkt;

#define SYNC_PKT_START 0
#define SYNC_PKT_STOP  1
#define SYNC_PKT_SYNC  2

#define SYNC_FILE_SEQ  0
#define SYNC_FILE_VID  1

typedef struct __attribute__((packed)) {
	uint8_t  pktType;        // Sync Packet Type
	uint8_t  fileType;       // File Type being synced
	uint32_t frameNumber;    // Sequence frames displayed on Master
	float    secondsElapsed; // Seconds elapsed in file on Master
	char     filename[1];    // Null-terminated File Name to play on Slave
	                         // (data may continue past this header)
} SyncPkt;

#endif /* _CONTROL_H */
