#ifndef _FPPD_H
#define _FPPD_H

void CreateDaemon(void);
void PlayerProcess(void);
void CheckExistanceOfDirectoriesAndFiles();

enum fppModes {
	DEFAULT_MODE = 0,
	PLAYER_MODE = 0,
	BRIDGE_MODE,
	MAX_MODES
};

#endif
