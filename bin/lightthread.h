#ifndef _LIGHTTHREAD_H
#define _LIGHTTHREAD_H

extern float RefreshRate;
extern int   DefaultLightDelay;
extern int   LightDelay;

int StartLightThread(void);
int IsLightThreadRunning(void);

#endif
