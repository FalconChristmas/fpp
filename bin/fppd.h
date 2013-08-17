#ifndef _FPPD_H
#define _FPPD_H

void CreateDaemon(void);
void PlayerProcess(void);
void CheckExistanceOfDirectoriesAndFiles();

#define PLAYER_MODE											0
#define E131_PIXELNET_DMX_BRIDGE_MODE		1

#endif