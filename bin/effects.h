// Effect Sequence file format and header definition
#ifndef EFFECTS_H_
#define EFFECTS_H_

#include <stdio.h>

//typedef struct eseqheader {
//} eSeqHeader;

typedef struct fppeffect {
	char     *name;
	FILE     *fp;
	int       stepSize;
	int       startChannel;
} FPPeffect;

extern FPPeffect *effects[];

int  InitEffects(void);
void CloseEffects(void);
int  StartEffect(char *effectName, int startChannel);
int  StopEffect(int effectID);
void StopEffects(void);
int  OverlayEffects(char *channelData);

#endif
