#ifndef _CONTROLRECV_H
#define _CONTROLRECV_H

/* Functions for controlling us */
int  InitControlSocket(void);
void ProcessControlPacket(void);
void ShutdownControlSocket(void);

#endif /* _CONTROLRECV_H */
