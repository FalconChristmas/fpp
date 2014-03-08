#ifndef _SERIALUTIL_H
#define _SERIALUTIL_H

int SerialOpen(char *device, int baud, char *mode);
int SerialClose(int fd);
int SerialSendBreak(int fd, int duration);

#endif /* _SERIALUTIL_H */
