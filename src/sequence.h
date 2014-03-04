#ifndef _SEQUENCE_H
#define _SEQUENCE_H

#define FPPD_MAX_CHANNELS 65536

extern unsigned long seqFileSize;
extern int  seqDuration;
extern int  seqSecondsElapsed;
extern int  seqSecondsRemaining;
extern char seqData[FPPD_MAX_CHANNELS];

int   OpenSequenceFile(const char *filename);
int   IsSequenceRunning(void);
void  ReadSequenceData(void);
void  SendSequenceData(void);
void  SendBlankingData(void);
void  CloseSequenceFile(void);

#endif /* _SEQUENCE_H */
