#ifndef _COMMON_H
#define _COMMON_H

long long GetTime(void);
int       DirectoryExists(char * Directory);
int       FileExists(char * File);
void      HexDump(char *title, void *data, int len);
char     *GetInterfaceAddress(char *interface);

#endif
