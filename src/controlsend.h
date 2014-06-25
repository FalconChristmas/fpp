#ifndef _CONTROL_H
#define _CONTROL_H

/* Functions for controlling other FPP instances */
int  InitSyncMaster(void);
void SendSeqSyncStartPacket(const char *filename);
void SendSeqSyncStopPacket();
void SendSeqSyncPacket(int frames, float seconds);
void ShutdownSync(void);

#endif /* _CONTROL_H */
