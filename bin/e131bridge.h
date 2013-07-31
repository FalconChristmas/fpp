#ifndef _E131_BRIDGE_H
#define _E131_BRIDGE_H

#define  BRIDGE_INVALID_UNIVERSE_INDEX								0xFF

void Bridge_Initialize();
void Bridge_InitializeSockets();
void Bridge_Process();
void Bridge_StoreData(int universe);
int Bridge_GetIndexFromUniverseNumber(int universe);

#endif
