#ifndef _FPPD_H
#define _FPPD_H

void CreateDaemon(void);
void PlayerProcess(void);
void CheckExistanceOfDirectoriesAndFiles();

enum fppModes {
	PLAYER_MODE = 0,
	BRIDGE_MODE
};

#endif
