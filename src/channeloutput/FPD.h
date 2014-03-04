#ifndef _FPD_H
#define _FPD_H

#include "channeloutput.h"

/* Config routine exposed so we can re-load the config on demand */
void SendFPDConfig();

/* Expose our interface */
extern FPPChannelOutput FPDOutput;

#endif /* #ifndef _FPD_H   */
